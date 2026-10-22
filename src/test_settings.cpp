#include "script_host.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include "test_assert.h"
static bool load(scripting::Host& host,const char* id,const std::string& source,const char* caps="storage.settings"){
 return host.load(id,source.data(),source.size(),caps);
}
static std::filesystem::path fileNamed(const std::filesystem::path& root,const char* name){for(const auto& item:std::filesystem::recursive_directory_iterator(root))if(item.path().filename()==name)return item.path();return {};}
static std::string bytes(const std::filesystem::path& path){std::ifstream input(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(input),{});}
int main(){
 using namespace scripting;using namespace scripting::settings;
 const auto root=std::filesystem::temp_directory_path()/(L"Lua settings fixture "+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
 Host h(settingsStorage|timerEvents|events|items);assert(h.setSettingsRoot(root/L"lua"));
 assert(load(h,"addon",R"(assert(storage.get('missing',17)==17);storage.set('data',{flag=false,count=9223372036854775807,small=-9223372036854775808,number=1.0,list={1,'text',true},nested={label='line\nquote"',zero='a\0b'}});local copy=storage.get('data');copy.count=0;assert(storage.get('data').count==9223372036854775807);assert(storage.status()=='pending'))"));
 assert(h.flushSettings(1,true)==1);
 Host fresh(settingsStorage);assert(fresh.setSettingsRoot(root/L"lua"));
 assert(load(fresh,"addon",R"(local d=storage.get('data');assert(d.flag==false and d.count==9223372036854775807 and d.small==-9223372036854775808);assert(math.type(d.number)=='float');assert(d.list[2]=='text' and d.nested.zero=='a\0b');assert(storage.status()=='saved'))"));
 assert(load(h,"addon","storage.set('n',1)"));assert(h.flushSettings(1,true)==1);
 assert(!load(h,"addon","storage.set('n',2);error('bad reload')"));
 assert(load(h,"addon","assert(storage.get('n')==1);assert(not pcall(function()storage.set('bad',0/0)end));storage.set('kept',true);assert(not pcall(function()storage.set('bad',function()end)end));assert(storage.get('kept'))"));
 assert(!load(h,"invalid",R"(local t={};t.self=t;storage.set('x',t))"));
 for(const auto& source:{"storage.set('x',{[2]=1})","storage.set('x',{[1]=1,a=2})","storage.set('x',math.huge)","storage.set('x',string.rep('x',65536))","storage.set('x',string.char(192,128))","for i=1,129 do storage.set('k'..i,i)end","local t={};local p=t;for i=1,10 do p.next={};p=p.next end;storage.set('x',t)"})assert(!load(h,"invalid",source));
 Host denied(0);assert(!load(denied,"denied","storage.set('x',true)",""));
 assert(load(h,"addon","timers.after(0,function()storage.set('n',3);error('bad callback')end)","storage.settings,timers"));
 h.setTime(1);h.runTimers();assert(!h.enabled("addon"));
 assert(load(h,"addon","assert(storage.get('n')==1);events.on('item.use','test',function()storage.set('n',4);return false end)","storage.settings,events.item_use,items.read"));
 Event event;event.model="test";assert(!h.dispatch(event).handled);assert(h.flushSettings(1,true)==1);
 Host reread(settingsStorage);assert(reread.setSettingsRoot(root/L"lua"));assert(load(reread,"addon","assert(storage.get('n')==4);assert(storage.get('kept'));storage.set('data',nil)"));assert(reread.flushSettings(1,true)==1);
 assert(load(h,"addon","storage.set('n',8)"));assert(h.flushSettings(1,true)==0);
 assert(load(h,"addon","assert(storage.status()=='conflict')"));
 Host unchanged(settingsStorage);assert(unchanged.setSettingsRoot(root/L"lua"));assert(load(unchanged,"addon","assert(storage.get('n')==4 and storage.get('data')==nil)"));
 FileStore store(root/L"native");assert(store.ready());Document values;Revision before,one,two,three;std::string error;
 assert(store.read("one",values,before,error)&&!before.exists);Value number;number.kind=Value::integer;number.whole=1;values["n"]=number;
 assert(store.write("one",values,before,one,error)==Written::saved);
 assert(store.write("one",values,before,two,error)==Written::conflict);
 values["n"].whole=2;assert(store.write("one",values,one,two,error)==Written::saved);
 const auto current=fileNamed(root/L"native","settings.json");assert(!current.empty());const auto previous=current.parent_path()/L"settings.previous.json";
 assert(unpack("one",bytes(previous))["n"].whole==1);
 values["n"].whole=3;assert(store.write("one",values,two,three,error)==Written::saved);assert(unpack("one",bytes(previous))["n"].whole==2);
 const auto temporary=current.parent_path()/L"settings.tmp";std::filesystem::create_directory(temporary);
 assert(store.write("one",values,three,two,error)==Written::error);assert(unpack("one",bytes(current))["n"].whole==3);std::filesystem::remove(temporary);
 {std::ofstream output(current,std::ios::binary|std::ios::trunc);output<<"corrupt";}
 Document rejected;Revision bad;assert(!store.read("one",rejected,bad,error)&&bytes(current)=="corrupt");
 Host optional(settingsStorage);assert(optional.setSettingsRoot(root/L"native"));
 const std::string fallback="assert(not api.has_capability('storage.settings'))";
 assert(optional.load("one",fallback.data(),fallback.size(),"",8*1024*1024,{},"storage.settings"));
 assert(!load(optional,"one","storage.get('n')"));assert(bytes(current)=="corrupt");
 FileStore isolated(root/L"namespaces");assert(isolated.ready());
 for(const char* id:{"A","a","../outside"}){Document data;Revision revision,written;assert(isolated.read(id,data,revision,error));data["n"]=number;assert(isolated.write(id,data,revision,written,error)==Written::saved);}
 assert(std::filesystem::exists(root/L"namespaces")&&!std::filesystem::exists(root/L"outside"));
 Host fair(settingsStorage);const auto fairRoot=root/L"fair";assert(fair.setSettingsRoot(fairRoot));
 for(char name='a';name<='e';++name){const std::string id(1,name);assert(load(fair,id.c_str(),"storage.set('n',1)"));const auto encoded=std::string("6")+char('1'+name-'a');std::filesystem::create_directory(fairRoot/encoded/L"_"/L"settings.tmp");}
 assert(load(fair,"z","storage.set('n',1)"));
 for(uint64_t i=0;i<5;++i){fair.setTime(i*1000);assert(fair.flushSettings()==0);}
 fair.setTime(5000);assert(fair.flushSettings()==1);
 Host healthy(settingsStorage);assert(healthy.setSettingsRoot(fairRoot));assert(load(healthy,"z","assert(storage.get('n')==1)"));
 const auto extendedRoot=std::filesystem::path(std::wstring(2,L'\\')+L"?\\"+root.wstring())/L"extended";
 FileStore extended(extendedRoot);assert(extended.ready());
 const std::string longId(128,'L');Document longData;Revision longRevision,longPublished;
 assert(extended.read(longId,longData,longRevision,error));longData["n"]=number;
 assert(extended.write(longId,longData,longRevision,longPublished,error)==Written::saved);
 const std::string unicode=R"({"format":1,"addon":"u","values":{"text":"\ud83d\ude00"}})";assert(unpack("u",unicode)["text"].text=="\xf0\x9f\x98\x80");
 for(const auto& invalid:{R"({"format":2,"addon":"u","values":{}})",R"({"format":1,"addon":"wrong","values":{}})",R"({"format":1,"addon":"u","values":{"a":null}})",R"({"format":1,"addon":"u","values":{"a":1,"a":2}})",R"({"format":1,"addon":"u","values":{"a":"\ud800"}})"}){bool failed=false;try{unpack("u",invalid);}catch(...){failed=true;}assert(failed);}
 std::puts("LUA_SETTINGS_PASSED: typed JSON round-trip, UTF-8/limits, callback/reload transactions, copy isolation, atomic backup/replacement, conflict/corruption refusal and confined addon namespaces");
}
