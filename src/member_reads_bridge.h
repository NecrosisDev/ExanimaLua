#pragma once
#include <set>
// Shared owner of the package member read 0x6f2d0(package, index, buffer) (exports add_member_provider & co.): vtable
// +0xd0 of both package classes, also behind the read by name (+0xe0). The table entry is *(package+0x10) + index*32
// {name[16], offset +0x10, size +0x14, type +0x18}; the buffer {data +0, length +8, capacity +0xc, type +0x10} is
// resized with the game's own 0x21260(buffer, size) (types 0/1, the heap kinds). Facts: the editor's hotload.h and
// panel.inl, which this replaces as the hook owner when both are present.
// Providers are asked in ascending priority; the first that returns true has filled the buffer and no later one is
// asked, so two providers never both write one member. No lock is held while a provider runs: the list is an
// immutable snapshot swapped on change, and an in-flight count lets removal wait until the provider is idle.
namespace memberreads {
using Read=char(*)(uintptr_t,unsigned,void*);using Resize=char(*)(void*,unsigned);
using Provider=bool(*)(void* context,uintptr_t package,int index,const char* name16,uint32_t archiveType,uintptr_t buffer);
using Claims=bool(*)(void* context,const char* name16);
struct Entry {Provider serve=nullptr;Claims claims=nullptr;void* context=nullptr;int priority=0;};
struct List {std::vector<Entry> entries;};
static Read original=nullptr;static Resize resize=nullptr;static std::atomic<bool> ready{false};
static std::atomic<const List*> current{nullptr};static SRWLOCK writer=SRWLOCK_INIT;
static std::vector<const List*> retired;   // old snapshots, freed when no read is in flight
static std::atomic<int> inFlight{0};static thread_local bool insideProvider=false;
static std::atomic<uint64_t> served{0},faults{0};
// Shadowed serves (a later provider claimed a member an earlier one served), reported from the pulse.
static SRWLOCK shadowLock=SRWLOCK_INIT;static std::array<std::array<char,17>,16> shadowed{};static size_t shadowCount=0;
static void publish(std::vector<Entry> entries){
 std::stable_sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){return a.priority<b.priority;});
 const List* next=new List{std::move(entries)};const List* old=current.exchange(next);if(old)retired.push_back(old);
}
static void reclaim(){if(inFlight.load())return;for(const List* l:retired)delete l;retired.clear();}
static bool installHook();
static bool addProvider(Provider serve,Claims claims,void* context,int priority){
 if(!serve||!ready||!installHook())return false;AcquireSRWLockExclusive(&writer);
 const List* now=current.load();std::vector<Entry> entries=now?now->entries:std::vector<Entry>{};bool ok=entries.size()<16;
 for(const auto& e:entries)if(e.serve==serve&&e.context==context)ok=false;
 if(ok){entries.push_back({serve,claims,context,priority});publish(std::move(entries));}
 ReleaseSRWLockExclusive(&writer);return ok;
}
// Returns after the provider can no longer run. From inside a provider it removes the entry and returns at once.
static bool removeProvider(Provider serve,void* context){
 AcquireSRWLockExclusive(&writer);const List* now=current.load();std::vector<Entry> entries=now?now->entries:std::vector<Entry>{};
 const auto end=std::remove_if(entries.begin(),entries.end(),[&](const Entry& e){return e.serve==serve&&e.context==context;});
 const bool found=end!=entries.end();if(found){entries.erase(end,entries.end());publish(std::move(entries));}
 ReleaseSRWLockExclusive(&writer);
 if(found&&!insideProvider){while(inFlight.load())Sleep(0);AcquireSRWLockExclusive(&writer);reclaim();ReleaseSRWLockExclusive(&writer);}
 return found;
}
static bool resizeBuffer(uintptr_t buffer,size_t size){
 __try{
  if(!resize||!buffer||size>0x7fffffff||!readable(buffer,0x14))return false;
  if((*(const uint32_t*)(buffer+0x10)&0x3f)>1)return false;   // only the heap buffer kinds the game resizes itself
  if(!resize((void*)buffer,unsigned(size)))return false;
  return *(const uint32_t*)(buffer+8)==size&&(!size||*(void**)buffer);
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static int call(const Entry& e,uintptr_t package,int index,const char* name,uint32_t type,uintptr_t buffer){
 __try{return e.serve(e.context,package,index,name,type,buffer)?1:0;}__except(EXCEPTION_EXECUTE_HANDLER){return -1;}
}
static bool claimed(const Entry& e,const char* name){__try{return e.claims&&e.claims(e.context,name);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool entryName(uintptr_t package,unsigned index,char (&name)[17],uint32_t& type){
 __try{
  if(!readable(package,0x20))return false;const unsigned count=*(const unsigned*)(package+8);const uintptr_t table=*(const uintptr_t*)(package+0x10);
  if(index>=count||!readable(table+uintptr_t(index)*32,32))return false;
  std::memcpy(name,(const void*)(table+uintptr_t(index)*32),16);name[16]=0;type=*(const uint32_t*)(table+uintptr_t(index)*32+0x18);return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// New member names (files a package does not list): the name lookup 0x1faf0(package, name16) -> index, vtable
// +0xc8 of every package, is detoured to answer a synthetic index (syntheticBase + n) for a name the runtime adds,
// after the game's own search said -1; this read then serves those indices itself and never passes them to the
// game, whose own read would index past its table. Callers only pass the index on to this read (+0xe0 reads by name
// through both); the remove-row slot +0xd8 (0x1fbe0) is not hooked and is not known to be used on such names.
constexpr unsigned syntheticBase=0x7f000000u;
static char memberReadServed(uintptr_t package,unsigned index,void* buffer);
using Find=int(*)(uintptr_t,const char*);using Synthetic=int(*)(uintptr_t,const char*);using SyntheticRead=bool(*)(uintptr_t,unsigned,uintptr_t);
static Find originalFind=nullptr;static bool findCreated=false,findHooked=false;
static Synthetic synthetic=nullptr;static SyntheticRead syntheticRead=nullptr;
static int find(uintptr_t package,const char* name){
 const int index=originalFind(package,name);
 if(index>=0||!synthetic)return index;
 int added=-1;__try{added=synthetic(package,name);}__except(EXCEPTION_EXECUTE_HANDLER){added=-1;}
 return added>=0?int(syntheticBase+unsigned(added)):index;
}
static bool hookFind(){
 if(findHooked)return true;
 const unsigned char prefix[]={0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x64,0x24,0xe0};
 if(!findCreated){if(!titlerows::untouched(base+0x1faf0,prefix,sizeof prefix)||MH_CreateHook((void*)(base+0x1faf0),(void*)find,(void**)&originalFind)!=MH_OK)return false;findCreated=true;}
 const auto s=MH_EnableHook((void*)(base+0x1faf0));findHooked=s==MH_OK||s==MH_ERROR_ENABLED;return findHooked;
}
// Edits applied to a member after it is filled (by a provider or the game): level placements (level_placements.h).
// `wants` is asked first with the member name; `edit` then gets the bytes and returns true when it changed them.
using Wants=bool(*)(const char*);using Edit=bool(*)(const char*,std::string&);
static std::atomic<Wants> wants{nullptr};static Edit edit=nullptr;static std::atomic<uint64_t> edited{0};
static bool wanted(const char* name){__try{const auto w=wants.load();return w&&w(name);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool view(uintptr_t buffer,const char*& data,uint32_t& length){
 __try{if(!readable(buffer,0x14))return false;data=*(const char**)buffer;length=*(const uint32_t*)(buffer+8);
  return data&&length&&length<=64u*1024*1024&&readable(uintptr_t(data),length);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool copyBytes(void* to,const void* from,size_t n){__try{std::memcpy(to,from,n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool rewrite(const char* name,uintptr_t buffer){
 const char* data=nullptr;uint32_t length=0;if(!view(buffer,data,length))return false;
 std::string bytes(length,'\0');if(!copyBytes(&bytes[0],data,length))return false;
 bool changed=false;try{changed=edit&&edit(name,bytes);}catch(...){return false;}if(!changed)return false;
 if(!resizeBuffer(buffer,bytes.size())||!view(buffer,data,length)||length!=bytes.size()||!copyBytes(const_cast<char*>(data),bytes.data(),bytes.size())){++faults;return false;}
 ++edited;return true;
}
static char memberRead(uintptr_t package,unsigned index,void* buffer){
 if(index>=syntheticBase&&index<syntheticBase+0x10000){bool ok=false;__try{ok=syntheticRead&&syntheticRead(package,index-syntheticBase,uintptr_t(buffer));}__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}return ok?1:0;}
 if(wants.load(std::memory_order_relaxed)){
  char member[17]{};uint32_t kind=0;
  if(entryName(package,index,member,kind)&&wanted(member)){
   // Fill as usual (providers, else the game), then edit what was read.
   const char filled=current.load(std::memory_order_relaxed)?memberReadServed(package,index,buffer):original(package,index,buffer);
   if(filled)rewrite(member,uintptr_t(buffer));return filled;
  }
 }
 return memberReadServed(package,index,buffer);
}
static char memberReadServed(uintptr_t package,unsigned index,void* buffer){
 if(!current.load(std::memory_order_relaxed))return original(package,index,buffer);
 ++inFlight;const List* list=current.load();char name[17]{};uint32_t type=0;int result=0;size_t winner=0;
 if(list&&!list->entries.empty()&&entryName(package,index,name,type)){
  const bool outer=insideProvider;insideProvider=true;
  for(size_t i=0;i<list->entries.size()&&result!=1;++i){result=call(list->entries[i],package,int(index),name,type,uintptr_t(buffer));if(result<0)++faults;winner=i;}
  if(result==1){++served;
   for(size_t i=winner+1;i<list->entries.size();++i)if(claimed(list->entries[i],name)){
    AcquireSRWLockExclusive(&shadowLock);if(shadowCount<shadowed.size())std::memcpy(shadowed[shadowCount++].data(),name,17);ReleaseSRWLockExclusive(&shadowLock);break;}
  }
  insideProvider=outer;
 }
 --inFlight;
 return result==1?1:original(package,index,buffer);
}
static std::set<std::string> shadowReported;
static void deliver(scripting::Host& owner){
 std::vector<std::string> names;AcquireSRWLockExclusive(&shadowLock);for(size_t i=0;i<shadowCount;++i)names.push_back(shadowed[i].data());shadowCount=0;ReleaseSRWLockExclusive(&shadowLock);
 for(const auto& n:names)if(shadowReported.size()<256&&shadowReported.insert(n).second)
  owner.report("runtime","info",("Member "+n+" was served by a higher-priority provider; a later provider also had it").c_str());
 if(TryAcquireSRWLockExclusive(&writer)){reclaim();ReleaseSRWLockExclusive(&writer);}
}
static const unsigned char readPrefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0xa0,0x48,0x89,0x5d,0xd8};
// Whether serving can be offered (the member read untouched, the resize matching); nothing is hooked here.
static std::string available(){
 if(ready)return {};
 const unsigned char resizePrefix[]={0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0x89,0xd6};
 if(!titlerows::untouched(base+0x6f2d0,readPrefix,sizeof readPrefix))return "member read 0x6f2d0 is already detoured or differs; level serving is left to the mod that owns it";
 if(!gamebridge::operation(base+0x21260,resizePrefix,sizeof resizePrefix))return "buffer resize 0x21260 did not match";
 resize=(Resize)(base+0x21260);ready=true;return {};
}
static std::atomic<bool> hooked{false};static SRWLOCK installLock=SRWLOCK_INIT;
// Hooked at the first provider, after checking again that nobody took the entry meanwhile. A failed enable
// leaves the hook disabled (never removed) and serving unavailable.
static bool installHook(){
 if(hooked)return true;AcquireSRWLockExclusive(&installLock);
 if(!hooked&&ready){
  if(!titlerows::untouched(base+0x6f2d0,readPrefix,sizeof readPrefix))ready=false;
  else{const auto c=MH_CreateHook((void*)(base+0x6f2d0),(void*)memberRead,(void**)&original);
   if(c==MH_OK||c==MH_ERROR_ALREADY_CREATED){const auto s=MH_EnableHook((void*)(base+0x6f2d0));if(s==MH_OK||s==MH_ERROR_ENABLED)hooked=true;else{MH_DisableHook((void*)(base+0x6f2d0));ready=false;}}
   else ready=false;}
 }
 ReleaseSRWLockExclusive(&installLock);return hooked;
}
}
