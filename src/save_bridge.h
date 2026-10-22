#pragma once
// Addon data kept inside the save itself, as one entry named "#exanimalua" in the slot package (global
// 0x4f0ca0; rows of {u32 offset, u32 size, name[16]} at +0x10, count at +8, file handle at +0x18). It is
// written right after the story playerdata writer 0x182d10 and read right after its reader 0x182850, so the
// level-entry checkpoint copy (.rcp) carries the data of that same moment. Entries the game does not know
// are ignored by it, so removing the runtime leaves an unread entry and nothing else.
namespace savedata {
using Writer=void(*)();using Reader=uint64_t(*)(uint32_t);using Put=unsigned char(*)(uintptr_t,const uint64_t*,const void*);
using Start=void(*)(uintptr_t);using Load=unsigned char(*)(uintptr_t,int);
struct Buffer {const void* data;uint32_t size;};
static Writer originalWriter=nullptr;static Reader originalReader=nullptr;static Put put=nullptr;static Start originalStart=nullptr;static Load originalLoad=nullptr;
static bool writerCreated=false,readerCreated=false,startCreated=false,loadCreated=false,writerOn=false,readerOn=false,startOn=false,loadOn=false;
static SRWLOCK storeLock=SRWLOCK_INIT;static std::map<std::string,std::string> store;
static std::atomic<bool> wanted{false};
static std::atomic<unsigned> faults{0};
constexpr size_t addonLimit=4096,totalLimit=32768,addonsMax=32;
inline constexpr uint64_t entryName[2]={0x616d696e61786523ull,0x000000000061756cull};   // "#exanimalua"
inline constexpr uint32_t magic=0x41554c45,version=1;   // "ELUA"
static std::string encode(const std::map<std::string,std::string>& values){
 std::string out;auto u32=[&](uint32_t v){out.append((const char*)&v,4);};
 u32(magic);u32(version);u32(uint32_t(values.size()));
 for(const auto& [id,data]:values){out.push_back(char(id.size()));out+=id;u32(uint32_t(data.size()));out+=data;}
 return out;
}
static bool decode(const std::string& bytes,std::map<std::string,std::string>& values){
 values.clear();size_t at=0;auto u32=[&](uint32_t& v){if(bytes.size()-at<4)return false;std::memcpy(&v,bytes.data()+at,4);at+=4;return true;};
 uint32_t m=0,v=0,n=0;if(!u32(m)||!u32(v)||!u32(n)||m!=magic||v!=version||n>addonsMax)return false;
 for(uint32_t i=0;i<n;++i){
  if(at>=bytes.size())return false;const size_t idSize=uint8_t(bytes[at++]);if(!idSize||bytes.size()-at<idSize)return false;
  std::string id=bytes.substr(at,idSize);at+=idSize;uint32_t size=0;if(!u32(size)||size>addonLimit||bytes.size()-at<size)return false;
  values[id]=bytes.substr(at,size);at+=size;
 }
 return at==bytes.size();
}
// The package's row for our entry: -1 when absent, -2 when the package cannot be read.
static int findRow(uintptr_t package,uint32_t& offset,uint32_t& size,HANDLE& file){
 __try{
  const int count=*(const int*)(package+8);const auto rows=*(const uintptr_t*)(package+0x10);file=*(const HANDLE*)(package+0x18);
  if(count<0||count>65536||(count&&!rows))return -2;
  for(int i=1;i<count;++i){const auto row=rows+uintptr_t(i)*0x18;
   if(*(const uint64_t*)(row+8)==entryName[0]&&*(const uint64_t*)(row+0x10)==entryName[1]){offset=*(const uint32_t*)row;size=*(const uint32_t*)(row+4);return i;}
  }
  return -1;
 }__except(EXCEPTION_EXECUTE_HANDLER){return -2;}
}
static uintptr_t package(){__try{return *(const uintptr_t*)(base+0x4f0ca0);}__except(EXCEPTION_EXECUTE_HANDLER){return 0;}}
static bool putCall(uintptr_t at,const Buffer& buffer){__try{put(at,entryName,&buffer);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static void written(){
 originalWriter();
 if(!wanted.load(std::memory_order_relaxed))return;
 const auto at=package();uint32_t offset=0,size=0;HANDLE file=nullptr;if(!at){++faults;return;}
 std::string bytes;AcquireSRWLockShared(&storeLock);const bool empty=store.empty();if(!empty)bytes=encode(store);ReleaseSRWLockShared(&storeLock);
 const int row=findRow(at,offset,size,file);if(row==-2){++faults;return;}
 if(empty&&row<0)return;   // never add an entry to a save that has no addon data
 if(empty)bytes=encode({});
 if(!putCall(at,{bytes.data(),uint32_t(bytes.size())}))++faults;
}
// The reader also runs for the character-screen preview (0x100010) and slot listing (0); only the load
// itself passes 0xffffffff, and it succeeds with a positive 32-bit result.
static uint64_t loaded(uint32_t argument){
 const auto result=originalReader(argument);
 if(argument!=0xffffffffu)return result;
 std::map<std::string,std::string> values;
 if(int32_t(result)>=1){
  const auto at=package();uint32_t offset=0,size=0;HANDLE file=nullptr;
  if(at&&findRow(at,offset,size,file)>=0&&file&&size>=12&&size<=totalLimit+4096){
   std::string bytes(size,'\0');DWORD read=0;
   if(SetFilePointer(file,LONG(offset),nullptr,FILE_BEGIN)==INVALID_SET_FILE_POINTER||!ReadFile(file,bytes.data(),size,&read,nullptr)||read!=size||!decode(bytes,values)){values.clear();++faults;}
  }
 }
 AcquireSRWLockExclusive(&storeLock);store=std::move(values);ReleaseSRWLockExclusive(&storeLock);
 return result;
}
static void clear(){AcquireSRWLockExclusive(&storeLock);store.clear();ReleaseSRWLockExclusive(&storeLock);}
// A new story game (0x126050) loads nothing, and a failed load (0x182340) falls back to a fresh level: in both,
// the last loaded game's data must not be saved into the slot.
static void started(uintptr_t argument){clear();originalStart(argument);}
static unsigned char loading(uintptr_t mode,int slot){clear();return originalLoad(mode,slot);}
static bool get(const std::string& id,std::string& out){
 AcquireSRWLockShared(&storeLock);const auto found=store.find(id);const bool ok=found!=store.end();if(ok)out=found->second;ReleaseSRWLockShared(&storeLock);return ok;
}
static bool set(const std::string& id,const std::string* value){
 if(value&&value->size()>addonLimit)return false;
 AcquireSRWLockExclusive(&storeLock);bool ok=true;
 if(!value)store.erase(id);
 else{
  size_t total=12;for(const auto& [key,data]:store)if(key!=id)total+=5+key.size()+data.size();
  ok=(store.count(id)||store.size()<addonsMax)&&total+5+id.size()+value->size()<=totalLimit;
  if(ok)store[id]=*value;
 }
 ReleaseSRWLockExclusive(&storeLock);if(ok)wanted=true;return ok;
}
static bool hook(uintptr_t at,const unsigned char* prefix,size_t size,void* detour,void** trampoline,bool& created,bool& enabled){
 if(enabled)return true;
 if(!created){if(!gamebridge::operation(at,prefix,size)||MH_CreateHook((void*)at,detour,trampoline)!=MH_OK)return false;created=true;}
 const auto status=MH_EnableHook((void*)at);enabled=status==MH_OK||status==MH_ERROR_ENABLED;return enabled;
}
static bool install(){
 const unsigned char writer[]={0x53,0x48,0x8d,0x64,0x24,0xe0,0x48,0x8b,0x0d,0x93,0xdf,0x36,0x00};
 const unsigned char reader[]={0x53,0x57,0x56,0x41,0x54,0x48,0x8d,0x64,0x24,0xa8,0x89,0xcb};
 const unsigned char entry[]={0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x41,0x56,0x48,0x8d,0x64,0x24,0xc8,0x48,0x89,0xce,0x48,0x89,0xd7,0x4c,0x89,0xc3};
 if(!gamebridge::operation(base+0x26fe80,entry,sizeof entry))return false;
 const unsigned char start[]={0x53,0x57,0x56,0x41,0x54,0x48,0x8d,0xa4,0x24,0x08,0xff,0xff,0xff};
 const unsigned char load[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0xa4,0x24,0x50,0xff,0xff,0xff,0x48,0x89,0x9d,0x78,0xff,0xff,0xff};
 put=(Put)(base+0x26fe80);
 // Reader and resets first: data must never be written by a runtime that cannot read or reset it.
 return hook(base+0x182850,reader,sizeof reader,(void*)loaded,(void**)&originalReader,readerCreated,readerOn)&&
  hook(base+0x126050,start,sizeof start,(void*)started,(void**)&originalStart,startCreated,startOn)&&
  hook(base+0x182340,load,sizeof load,(void*)loading,(void**)&originalLoad,loadCreated,loadOn)&&
  hook(base+0x182d10,writer,sizeof writer,(void*)written,(void**)&originalWriter,writerCreated,writerOn);
}
}
