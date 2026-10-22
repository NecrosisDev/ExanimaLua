#include "script_host.h"
#include "diagnostics.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include "test_assert.h"
struct Capture {int count=0;std::string id,level,message;};
static void capture(void* pointer,const char* id,const char* level,const char* message){
 auto& out=*static_cast<Capture*>(pointer);++out.count;out.id=id;out.level=level;out.message=message;
}
static bool load(scripting::Host& host,const char* id,const std::string& source,const std::vector<scripting::Module>& modules={}){
 return host.load(id,source.data(),source.size(),"",8*1024*1024,modules);
}
int main(){
 using namespace scripting;Host host(0);Capture output;host.setLogger(capture,&output);
 const std::vector<Module> modules={{"util.counter","loaded=(loaded or 0)+1;return {count=loaded}"},{"empty","return nil"},{"false_value","return false"}};
 assert(load(host,"modules","assert(api.version==1 and not api.has_capability('doors.unlock')); local a=require('util.counter');assert(a==require('util.counter') and a.count==1);assert(require('empty')==true and require('false_value')==false);assert(package==nil and io==nil);log.warn('hello')",modules));
 assert(output.id=="modules");
 assert(!load(host,"undeclared","require('../outside')",modules));assert(host.error().find("not declared")!=std::string::npos);
 assert(!load(host,"cycle","require('a')",{{"a","return require('b')"},{"b","return require('a')"}}));assert(host.error().find("cyclic")!=std::string::npos);
 assert(!load(host,"syntax","require('bad')",{{"bad","this is invalid Lua !"}}));
 assert(!load(host,"trace","require('nested')",{{"nested","local function inner() error('deep error') end;inner()"}}));
 assert(host.error().find("stack traceback")!=std::string::npos&&host.error().find("trace/nested")!=std::string::npos);
 assert(!load(host,"duplicate","",{{"x",""},{"x",""}}));
 assert(!load(host,"path","",{{"../x",""}}));
 assert(!load(host,"large","",{{"x",std::string(1024*1024+1,' ')}}));
 Host isolated;assert(load(isolated,"one","assert(require('x')==1)",{{"x","return 1"}}));
 assert(load(isolated,"two","assert(require('x')==2)",{{"x","return 2"}}));
 assert(!load(isolated,"one","error('failed replacement')",{{"x","return 3"}}));assert(isolated.size()==2);
 output.count=0;assert(load(host,"quota","for i=1,100 do log.info('message') end"));assert(output.count==33); // 32 script messages plus host load status
 Host callbacks;
 const std::string callback="local m=require('policy');events.on('item.use','test',function() return m.value end)";
 assert(callbacks.load("reload",callback.data(),callback.size(),"events.item_use,items.read",8*1024*1024,{{"policy","return {value=true}"}}));
 Event event;event.model="test";assert(callbacks.dispatch(event).handled);
 const std::string failed="require('policy')";
 assert(!callbacks.load("reload",failed.data(),failed.size(),"",8*1024*1024,{{"policy","error('bad replacement')"}}));
 assert(callbacks.dispatch(event).handled);
 Host granted;const std::string cap="assert(api.has_capability('items.read'));assert(not api.has_capability('doors.unlock'));assert(not api.has_capability('unknown'))";
 assert(granted.load("cap",cap.data(),cap.size(),"items.read"));
 Host optional;
 const std::string opt="assert(api.has_capability('items.read'));assert(not api.has_capability('unknown'))";
 assert(optional.load("optional",opt.data(),opt.size(),"",8*1024*1024,{},"items.read,unknown"));
 const std::string unavailable="assert(not api.has_capability('doors.unlock'))";
 assert(host.load("optional-core",unavailable.data(),unavailable.size(),"",8*1024*1024,{},"doors.unlock,unknown"));
 const auto path=std::filesystem::path("diagnostics-test.log");std::error_code error;
 std::filesystem::remove(path,error);std::filesystem::remove(path.string()+".previous",error);
 FileLog log(path,128);for(int i=0;i<20;++i)log.write("addon","info","line\nwith control");
 assert(std::filesystem::file_size(path)<128&&std::filesystem::file_size(path.string()+".previous")<128);
 std::ifstream input(path);std::stringstream content;content<<input.rdbuf();assert(content.str().find("line\\nwith control")!=std::string::npos);
 std::puts("LUA_TOOLS_PASSED: isolated declared modules, cache/cycles/limits, traceback, capabilities, bounded tagged logs and rotation");
}
