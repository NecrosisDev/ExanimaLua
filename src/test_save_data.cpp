#include "plugin.cpp"
#include <cstdio>
#include <filesystem>
#include "test_assert.h"
// A slot package in miniature: row 0 is the header, then named entries, each {u32 offset, u32 size, name[16]}.
struct Row {uint32_t offset=0,size=0;uint64_t name[2]{};};
static std::vector<Row> rows;static HANDLE file=INVALID_HANDLE_VALUE;static std::vector<uint8_t> package(0x40);
static int writes=0,reads=0,starts=0,entries=0;
static void sync(){const int count=int(rows.size());std::memcpy(package.data()+8,&count,4);const auto table=(uintptr_t)rows.data();std::memcpy(package.data()+0x10,&table,8);std::memcpy(package.data()+0x18,&file,8);}
static void writerFixture(){++writes;}
// The game's reader returns 1 for a loaded slot, 0 when playerdata is missing and 0xffffffff for an old file.
static uint64_t readerResult=1;
static uint64_t readerFixture(uint32_t){++reads;return readerResult;}
static void startFixture(uintptr_t){++starts;}
static int loads=0;
static unsigned char loadFixture(uintptr_t,int){++loads;return 1;}
static unsigned char putFixture(uintptr_t at,const uint64_t* name,const void* data){
 assert(at==(uintptr_t)package.data());++entries;const auto& buffer=*(const savedata::Buffer*)data;
 Row* row=nullptr;for(auto& r:rows)if(r.name[0]==name[0]&&r.name[1]==name[1])row=&r;
 if(!row){rows.push_back({});row=&rows.back();row->name[0]=name[0];row->name[1]=name[1];row->offset=uint32_t(GetFileSize(file,nullptr));}
 row->size=buffer.size;SetFilePointer(file,LONG(row->offset),nullptr,FILE_BEGIN);DWORD n=0;WriteFile(file,buffer.data,buffer.size,&n,nullptr);sync();return 1;
}
static bool load(const char* id,const std::string& source,const char* caps){return host.load(id,source.data(),source.size(),caps);}
int main(){
 using namespace scripting;
 const auto path=std::filesystem::temp_directory_path()/("lua-save-"+std::to_string(GetCurrentProcessId())+".rsg");
 file=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_TEMPORARY,nullptr);assert(file!=INVALID_HANDLE_VALUE);
 DWORD n=0;WriteFile(file,"game playerdata",15,&n,nullptr);rows={{},{0,15,{0x6164726579616c70ull,0x6174ull}}};sync();
 std::vector<uint8_t> image(0x500000);base=(uintptr_t)image.data();const auto at=(uintptr_t)package.data();std::memcpy(image.data()+0x4f0ca0,&at,8);
 savedata::originalWriter=writerFixture;savedata::originalReader=readerFixture;savedata::originalStart=startFixture;savedata::originalLoad=loadFixture;savedata::put=putFixture;
 host.capabilities(saveData|timerEvents);host.setSaveApi(savedata::get,savedata::set);
 // A save with no addon data gets no entry.
 savedata::written();assert(writes==1&&entries==0&&rows.size()==2);
 assert(!load("loading","save_data.set('x')","storage.save"));assert(!load("denied","save_data.get()","timers"));
 assert(load("quest","quest=save_data.get()","storage.save"));
 assert(load("writer","timers.after(0,function()assert(save_data.get()==nil);assert(save_data.set('stage=3'));assert(save_data.get()=='stage=3');assert(not pcall(save_data.set,string.rep('x',4097)))end)","storage.save,timers"));
 host.setTime(1000);host.runTimers();assert(host.enabled("writer"));
 savedata::written();assert(writes==2&&entries==1&&rows.size()==3);
 // Death or a later load: the entry comes back with the playerdata it was saved beside.
 savedata::clear();std::string value;assert(!savedata::get("writer",value));
 assert(savedata::loaded(0xffffffff)==1&&reads==1&&savedata::get("writer",value)&&value=="stage=3");
 // Previews and slot listing run the same reader without loading: the live data stays.
 const std::string other="other";assert(savedata::set("writer",&other));savedata::loaded(0x100010);savedata::loaded(0);
 assert(reads==3&&savedata::get("writer",value)&&value=="other");
 // A failed load clears, whether it fails in the reader or before it.
 readerResult=0xffffffff;savedata::loaded(0xffffffff);assert(!savedata::get("writer",value));readerResult=1;
 assert(savedata::set("writer",&other));assert(savedata::loading(0,7)==1&&loads==1&&!savedata::get("writer",value));
 savedata::loaded(0xffffffff);assert(savedata::get("writer",value)&&value=="stage=3");
 // A new game forgets the loaded game's data before it starts.
 savedata::started(0);assert(starts==1&&!savedata::get("writer",value));
 // Clearing every value still rewrites an existing entry, so the old data cannot come back.
 savedata::written();assert(entries==2);savedata::loaded(0xffffffff);assert(!savedata::get("writer",value));
 // A damaged entry loads as nothing and is counted.
 const auto before=savedata::faults.load();SetFilePointer(file,LONG(rows[2].offset),nullptr,FILE_BEGIN);WriteFile(file,"BAD!",4,&n,nullptr);
 savedata::loaded(0xffffffff);assert(!savedata::get("writer",value)&&savedata::faults.load()==before+1);
 // Limits: 32 addons and 32 KB in all.
 savedata::clear();const std::string big(4000,'x');unsigned accepted=0;for(int i=0;i<40;++i)accepted+=savedata::set("addon"+std::to_string(i),&big);
 assert(accepted==8);
 CloseHandle(file);std::filesystem::remove(path);
 std::puts("LUA_SAVE_DATA_PASSED");
}
