#pragma once
// The Lua side of the shared owners: title.add_row/remove_row/set_enabled and levels.serve/clear_served. Every row
// and served member belongs to one addon ID, and is dropped when that addon is unloaded, replaced or disabled.
namespace sharedlua {
// Title rows: the native action only marks the row pressed; Lua runs at the next pulse on the presentation thread.
struct Row {std::string owner,label;std::atomic<bool> pressed{false};bool used=false;};
static std::array<Row,titlerows::maxExtra> rows;
static void pressed(void* context){static_cast<Row*>(context)->pressed=true;}
static bool addRow(const std::string& owner,const std::string& label,int after,int actsAs){
 for(auto& r:rows)if(r.used&&r.label==label&&r.owner!=owner)return false;   // another addon's (or mod's) label
 Row* slot=nullptr;for(auto& r:rows)if(r.used&&r.label==label)slot=&r;
 if(!slot)for(auto& r:rows)if(!r.used){slot=&r;break;}
 if(!slot||!titlerows::registerRow(label.c_str(),after,pressed,slot,actsAs))return false;
 slot->owner=owner;slot->label=label;slot->pressed=false;slot->used=true;return true;
}
static bool removeRow(const std::string& owner,const std::string& label){
 for(auto& r:rows)if(r.used&&r.owner==owner&&r.label==label){titlerows::unregisterRow(label.c_str());r.used=false;r.pressed=false;return true;}
 return false;
}
static bool enableRow(const std::string& owner,const std::string& label,bool enabled){
 for(auto& r:rows)if(r.used&&r.owner==owner&&r.label==label)return titlerows::setRowEnabled(label.c_str(),enabled);
 return false;
}
// Served members, keyed "package file|member" in lower case (the package's own file name, such as resource.rpk),
// read by the priority-50 member provider. Only that package's member is replaced, never a same-named one elsewhere.
struct Served {std::string owner;std::shared_ptr<const std::vector<uint8_t>> bytes;};
static std::map<std::string,Served> served;static SRWLOCK servedLock=SRWLOCK_INIT;static size_t servedBytes=0;
static constexpr size_t servedMax=16,servedTotalMax=16*1024*1024;
static std::atomic<size_t> servedCount{0};
static bool provide(void*,uintptr_t package,int,const char* name16,uint32_t archiveType,uintptr_t buffer);
static bool claims(void*,const char* name16);
static std::string lower(std::string s){for(auto& c:s)c=char(std::tolower((unsigned char)c));return s;}
static std::string key(const std::string& package,const std::string& member){return lower(package)+"|"+lower(member);}
// The file name part of a package's path (package +0x18), or empty.
static std::string packageName(uintptr_t package){
 char path[260]{};uintptr_t at=0;
 if(!copyWorldMemory(package+0x18,&at,8)||!at)return {};
 size_t n=0;for(;n+1<sizeof path;++n){char c=0;if(!copyWorldMemory(at+n,&c,1)||!c)break;path[n]=c;}path[n]=0;
 const char* name=path;for(const char* p=path;*p;++p)if(*p=='\\'||*p=='/')name=p+1;return name;
}
static bool serve(const std::string& owner,const std::string& package,const std::string& name,const std::string& bytes){
 if(name.empty()||name.size()>16||package.empty()||package.size()>64||bytes.empty()||bytes.size()>4*1024*1024)return false;
 // The runtime's own provider (priority 50) is registered, and the member read hooked, at the first serve.
 static bool providing=false;if(!providing)providing=memberreads::addProvider(provide,claims,nullptr,50);if(!providing)return false;
 auto copy=std::make_shared<const std::vector<uint8_t>>(bytes.begin(),bytes.end());bool ok=false;const std::string id=key(package,name);
 AcquireSRWLockExclusive(&servedLock);
 const auto found=served.find(id);
 if(found!=served.end()&&found->second.owner!=owner){}
 else{const size_t old=found!=served.end()?found->second.bytes->size():0;
  if((found!=served.end()||served.size()<servedMax)&&servedBytes-old+bytes.size()<=servedTotalMax){served[id]={owner,copy};servedBytes=servedBytes-old+bytes.size();ok=true;}}
 servedCount=served.size();ReleaseSRWLockExclusive(&servedLock);return ok;
}
static void clearServed(const std::string& owner){
 AcquireSRWLockExclusive(&servedLock);
 for(auto i=served.begin();i!=served.end();)if(i->second.owner==owner){servedBytes-=i->second.bytes->size();i=served.erase(i);}else ++i;
 servedCount=served.size();ReleaseSRWLockExclusive(&servedLock);
}
static bool fill(uintptr_t buffer,const uint8_t* data,size_t size){__try{std::memcpy(*(void**)buffer,data,size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
// Archive type 0 only (as the editor's redirect): other types are compressed or special and keep their own bytes.
static bool provide(void*,uintptr_t package,int,const char* name16,uint32_t archiveType,uintptr_t buffer){
 if(archiveType!=0||!servedCount.load(std::memory_order_relaxed))return false;
 const std::string id=key(packageName(package),name16);std::shared_ptr<const std::vector<uint8_t>> bytes;
 AcquireSRWLockShared(&servedLock);const auto found=served.find(id);if(found!=served.end())bytes=found->second.bytes;ReleaseSRWLockShared(&servedLock);
 return bytes&&memberreads::resizeBuffer(buffer,bytes->size())&&fill(buffer,bytes->data(),bytes->size());
}
// Shadow reporting has only the member name: any package with that member served counts.
static bool claims(void*,const char* name16){
 const std::string suffix="|"+lower(name16);bool found=false;AcquireSRWLockShared(&servedLock);
 for(const auto& s:served)if(s.first.size()>suffix.size()&&!s.first.compare(s.first.size()-suffix.size(),suffix.size(),suffix))found=true;
 ReleaseSRWLockShared(&servedLock);return found;
}
// Startup overrides, declared in an addon's manifest:
//   [script.overrides]
//   "Resource.rpk/charroles.rdb" = "assets/charroles.rdb"
// read when EMTK submits the addon (at game start, before the game reads files it reads only once, such as the
// role database at session setup 0x12ad2d), from the addon's own folder (EMTK loads each addon from mods/<id>,
// emtk-startup.patch load_profile_script_for). Served at priority 60: after levels.serve, before the editor's 100.
// They stay for the process: the game does not re-read most of these files, so dropping them later would not undo
// anything. A missing, oversized or misplaced file is logged and the game's own member stays.
static std::map<std::string,Served> overrides;static SRWLOCK overridesLock=SRWLOCK_INIT;static std::atomic<size_t> overrideCount{0};
static bool provideOverride(void*,uintptr_t package,int,const char* name16,uint32_t archiveType,uintptr_t buffer){
 if(archiveType!=0||!overrideCount.load(std::memory_order_relaxed))return false;
 const std::string id=key(packageName(package),name16);std::shared_ptr<const std::vector<uint8_t>> bytes;
 AcquireSRWLockShared(&overridesLock);const auto found=overrides.find(id);if(found!=overrides.end())bytes=found->second.bytes;ReleaseSRWLockShared(&overridesLock);
 return bytes&&memberreads::resizeBuffer(buffer,bytes->size())&&fill(buffer,bytes->data(),bytes->size());
}
static bool overrideClaims(void*,const char* name16){
 const std::string suffix="|"+lower(name16);bool found=false;AcquireSRWLockShared(&overridesLock);
 for(const auto& s:overrides)if(s.first.size()>suffix.size()&&!s.first.compare(s.first.size()-suffix.size(),suffix.size(),suffix))found=true;
 ReleaseSRWLockShared(&overridesLock);return found;
}
// An override whose member the package does not list is added under that name: the name lookup answers a
// synthetic index for it (member_reads_bridge.h find), and this serves it.
static std::vector<std::string> added;   // index -> override key; only ever appended
static int addedIndex(uintptr_t package,const char* name16){
 char name[17]{};std::memcpy(name,name16,16);const auto k=key(packageName(package),name);
 AcquireSRWLockShared(&overridesLock);int index=-1;for(size_t i=0;i<added.size();++i)if(added[i]==k){index=int(i);break;}ReleaseSRWLockShared(&overridesLock);
 return index;
}
static bool addedRead(uintptr_t package,unsigned index,uintptr_t buffer){
 std::shared_ptr<const std::vector<uint8_t>> bytes;std::string k;
 AcquireSRWLockShared(&overridesLock);if(index<added.size()){k=added[index];const auto f=overrides.find(k);if(f!=overrides.end())bytes=f->second.bytes;}ReleaseSRWLockShared(&overridesLock);
 if(!bytes||k.compare(0,k.find('|'),lower(packageName(package)))!=0)return false;
 return memberreads::resizeBuffer(buffer,bytes->size())&&fill(buffer,bytes->data(),bytes->size());
}
// The [script.overrides] rows of a manifest: "package/member" = "relative path" (the only form EMTK manifests use here).
static std::vector<std::pair<std::string,std::string>> overrideRows(const std::string& toml){
 std::vector<std::pair<std::string,std::string>> found;bool inside=false;size_t at=0;
 while(at<toml.size()){
  size_t end=toml.find('\n',at);if(end==std::string::npos)end=toml.size();std::string line=toml.substr(at,end-at);at=end+1;
  while(!line.empty()&&(line.back()=='\r'||line.back()==' '||line.back()=='\t'))line.pop_back();
  size_t first=line.find_first_not_of(" \t");if(first==std::string::npos||line[first]=='#')continue;line=line.substr(first);
  if(line[0]=='['){inside=line=="[script.overrides]";continue;}
  if(!inside)continue;
  const size_t k0=line.find('"'),k1=k0==std::string::npos?k0:line.find('"',k0+1),eq=k1==std::string::npos?k1:line.find('=',k1+1);
  const size_t v0=eq==std::string::npos?eq:line.find('"',eq+1),v1=v0==std::string::npos?v0:line.find('"',v0+1);
  if(k0!=0||v1==std::string::npos)continue;
  found.push_back({line.substr(k0+1,k1-k0-1),line.substr(v0+1,v1-v0-1)});
 }
 return found;
}
// Reads and registers the overrides of addon `id` from mods/<id>; logs each problem through `report`.
static void loadOverrides(const std::string& id,const std::filesystem::path& mods,scripting::Host& owner){
 std::error_code error;const auto folder=std::filesystem::weakly_canonical(mods/std::filesystem::u8path(id),error);
 if(error||folder.parent_path()!=std::filesystem::weakly_canonical(mods,error))return;
 std::ifstream input(folder/L"manifest.toml",std::ios::binary);if(!input)return;
 const std::string toml{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
 const auto declared=overrideRows(toml);if(declared.empty())return;
 static bool providing=false;if(!providing)providing=memberreads::addProvider(provideOverride,overrideClaims,nullptr,60);
 if(!providing){owner.report(id.c_str(),"warn","[script.overrides] ignored: member reads are unavailable (another mod owns 0x6f2d0)");return;}
 for(const auto& row:declared){
  const auto slash=row.first.find('/');const std::string package=slash==std::string::npos?"":row.first.substr(0,slash),member=slash==std::string::npos?"":row.first.substr(slash+1);
  std::string why;std::vector<uint8_t> bytes;
  const auto path=std::filesystem::weakly_canonical(folder/std::filesystem::u8path(row.second),error);
  if(package.empty()||package.size()>64||member.empty()||member.size()>16||member.find('/')!=std::string::npos)why="the key must be \"Package.rpk/member\" with a member name of at most 16 characters";
  else if(error||path.native().compare(0,folder.native().size()+1,(folder/L"").native())!=0)why="the file must be inside the addon folder";
  else{std::ifstream file(path,std::ios::binary);if(!file)why="the file is missing";
   else{bytes.assign(std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>());if(bytes.size()<8||bytes.size()>8*1024*1024)why="the file must be 8 bytes .. 8 MiB";}}
  if(why.empty()){
   AcquireSRWLockExclusive(&overridesLock);const auto k=key(package,member);const auto found=overrides.find(k);
   if(found!=overrides.end()&&found->second.owner!=id)why="another addon already overrides it";
   else{overrides[k]={id,std::make_shared<const std::vector<uint8_t>>(std::move(bytes))};overrideCount=overrides.size();
    if(std::find(added.begin(),added.end(),k)==added.end()&&added.size()<0x10000)added.push_back(k);}
   ReleaseSRWLockExclusive(&overridesLock);
   if(why.empty()&&!memberreads::synthetic){memberreads::synthetic=addedIndex;memberreads::syntheticRead=addedRead;
    if(!memberreads::hookFind())owner.report(id.c_str(),"warn","new file names cannot be added (the package name lookup 0x1faf0 is not the game's own); replacing existing files still works");}
  }
  owner.report(id.c_str(),why.empty()?"info":"warn",why.empty()?("override "+row.first+" ready").c_str():("override "+row.first+" not used: "+why+"; the game's own file stays").c_str());
 }
}
// assets.read: a file under mods/<id> (EMTK loads each addon from that folder), never outside it.
static std::filesystem::path modsDirectory;
static bool readAsset(const std::string& owner,const std::string& relative,std::string& out,std::string& why){
 if(modsDirectory.empty()){why="the addon folder is unknown";return false;}
 std::error_code error;const auto folder=std::filesystem::weakly_canonical(modsDirectory/std::filesystem::u8path(owner),error);
 if(error||folder.parent_path()!=std::filesystem::weakly_canonical(modsDirectory,error)){why="the addon folder is unknown";return false;}
 const auto path=std::filesystem::weakly_canonical(folder/std::filesystem::u8path(relative),error);
 if(error||path.native().compare(0,folder.native().size()+1,(folder/L"").native())!=0){why="the path must stay inside the addon folder";return false;}
 const auto size=std::filesystem::file_size(path,error);if(error){why="the file is missing";return false;}
 if(size>8*1024*1024){why="the file is larger than 8 MiB";return false;}
 std::ifstream file(path,std::ios::binary);if(!file){why="the file could not be opened";return false;}
 out.assign(std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>());return true;
}
// assets.write: a file under mods/<id>/cache/ only (created as needed), 8 MiB at most, written whole through a
// temporary name. With a manifest override pointing at it ([script.overrides] "Resource.rpk/humanoid.rsq" =
// "cache/humanoid.rsq"), what an addon computed in one session is served from the next game start, before the game's
// first read.
static bool writeAsset(const std::string& owner,const std::string& relative,const std::string& bytes,std::string& why){
 if(modsDirectory.empty()){why="the addon folder is unknown";return false;}
 if(bytes.size()>8*1024*1024){why="at most 8 MiB";return false;}
 std::error_code error;const auto folder=std::filesystem::weakly_canonical(modsDirectory/std::filesystem::u8path(owner),error);
 if(error||folder.parent_path()!=std::filesystem::weakly_canonical(modsDirectory,error)){why="the addon folder is unknown";return false;}
 const auto cache=folder/L"cache";const auto path=std::filesystem::weakly_canonical(folder/std::filesystem::u8path(relative),error);
 if(error||path.native().compare(0,cache.native().size()+1,(cache/L"").native())!=0){why="the path must be inside the addon's cache/ folder";return false;}
 std::filesystem::create_directories(path.parent_path(),error);if(error){why="the folder could not be made";return false;}
 const auto temporary=path.native()+L".new";
 {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);if(!out){why="the file could not be opened";return false;}out.write(bytes.data(),std::streamsize(bytes.size()));if(!out){why="the file could not be written";return false;}}
 std::filesystem::rename(temporary,path,error);if(error){std::filesystem::remove(temporary,error);why="the file could not be replaced";return false;}
 return true;
}
// levels.read: a member of a package in the game folder (the mods folder's parent), read from the file itself, so
// nothing served applies. The .rpk layout: u32 magic 01 0c bf af, u32 table length, 32-byte rows {name[16],
// u32 offset, u32 size, u32 type, u32}, data at 8 + table length + offset.
static bool readPackage(const std::string& package,const std::string& member,std::string& out,std::string& why){
 if(modsDirectory.empty()){why="the game folder is unknown";return false;}
 const auto path=modsDirectory.parent_path()/std::filesystem::u8path(package);std::ifstream in(path,std::ios::binary);
 if(!in){why="no such package in the game folder";return false;}
 unsigned char head[8]{};if(!in.read((char*)head,8)||head[0]!=1||head[1]!=0x0c||head[2]!=0xbf||head[3]!=0xaf){why="not a package file";return false;}
 uint32_t length=0;std::memcpy(&length,head+4,4);if(length%32||length>(64u<<20)){why="not a package file";return false;}
 std::vector<char> table(length);if(!in.read(table.data(),length)){why="the package index could not be read";return false;}
 const std::string wanted=lower(member);
 for(uint32_t o=0;o<length;o+=32){
  if(lower(std::string(table.data()+o,strnlen(table.data()+o,16)))!=wanted)continue;
  uint32_t offset=0,size=0,type=0;std::memcpy(&offset,table.data()+o+16,4);std::memcpy(&size,table.data()+o+20,4);std::memcpy(&type,table.data()+o+24,4);
  if(type!=0){why="that member is compressed";return false;}if(size>8u*1024*1024){why="the member is larger than 8 MiB";return false;}
  out.resize(size);in.seekg(std::streamoff(8)+length+offset);if(size&&!in.read(out.data(),size)){out.clear();why="the member could not be read";return false;}
  return true;
 }
 why="no such member in that package";return false;
}
static void forget(const std::string& owner){
 for(auto& r:rows)if(r.used&&r.owner==owner){titlerows::unregisterRow(r.label.c_str());r.used=false;r.pressed=false;}
 clearServed(owner);
}
static void deliver(scripting::Host& owner){
 for(auto& r:rows)if(r.used&&r.pressed.exchange(false))owner.notifyTitle(r.owner,r.label);
}
}
