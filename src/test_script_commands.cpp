#include "plugin.cpp"
#include <cstdio>
#include "test_assert.h"
// A script VM in miniature: argument count at +0x50, then {text, length} pairs from +0x58.
struct Vm {std::vector<uint8_t> bytes=std::vector<uint8_t>(0x200);std::vector<std::string> texts;
 uintptr_t call(std::initializer_list<const char*> arguments){
  texts.assign(arguments.begin(),arguments.end());const int count=int(texts.size());std::memcpy(bytes.data()+0x50,&count,4);
  for(size_t i=0;i<texts.size();++i){const char* text=texts[i].data();const uint32_t size=uint32_t(texts[i].size());std::memcpy(bytes.data()+0x58+i*0x10,&text,8);std::memcpy(bytes.data()+0x60+i*0x10,&size,4);}
  return (uintptr_t)bytes.data();
 }
};
static bool load(const char* id,const std::string& source,const char* caps){return host.load(id,source.data(),source.size(),caps);}
int main(){
 using namespace scripting;Vm vm;
 scriptcommands::registered=true;runtimeEnabled=true;host.capabilities(scriptEvents|scriptFlags|timerEvents);host.setFlagApi(scriptcommands::getFlag,scriptcommands::setFlag);
 // Nobody listens: LuaEvent returns 0 and queues nothing; LuaFlag reads 0 for an unknown flag.
 assert(scriptcommands::luaEvent(vm.call({"quest_started"}))==0&&scriptcommands::luaFlag(vm.call({"door_open"}))==0);
 assert(!load("denied","events.on('script.event',function()end)","timers"));
 assert(load("quest",R"(local n=0
events.on('script.event',function(e)
 n=n+1
 if n==1 then assert(e.name=='quest_started' and e.value=='2')
 elseif n==2 then assert(e.name=='met_derrin' and e.value==nil)
 else error('a third event reached Lua') end
end))","events.script"));
 scriptcommands::deliver(host,true);assert(scriptcommands::listening);
 assert(scriptcommands::luaEvent(vm.call({"quest_started","2"}))==1&&scriptcommands::luaEvent(vm.call({"met_derrin"}))==1);
 assert(scriptcommands::luaEvent(vm.call({}))==0&&scriptcommands::luaEvent(vm.call({"a_name_that_is_far_too_long_for_the_limit"}))==0);
 // LuaEvent() makes the game's tokenizer run on into the following script text: not a name, so refused.
 assert(scriptcommands::luaEvent(vm.call({");Foo(x"}))==0);
 scriptcommands::deliver(host,false);scriptcommands::deliver(host,true);assert(host.enabled("quest"));   // held outside a session: dropped
 assert(scriptcommands::luaEvent(vm.call({"quest_started","2"}))==1);scriptcommands::deliver(host,true);assert(host.enabled("quest"));
 assert(scriptcommands::luaEvent(vm.call({"met_derrin"}))==1);scriptcommands::deliver(host,true);assert(host.enabled("quest"));
 assert(scriptcommands::luaEvent(vm.call({"third"}))==1);scriptcommands::deliver(host,true);assert(!host.enabled("quest"));
 // Flags: an addon sets them, the game's conditions read them synchronously.
 assert(load("flags","timers.after(0,function()assert(script_flags.set('door_open',3));assert(script_flags.get('door_open')==3);assert(not script_flags.set('bad name',1));assert(not pcall(script_flags.set,'x',1.5));assert(script_flags.set('gone',1) and script_flags.set('gone',nil) and script_flags.get('gone')==nil)end)","script.flags,timers"));
 host.setTime(1000);host.runTimers();assert(host.enabled("flags"));
 assert(scriptcommands::luaFlag(vm.call({"door_open "}))==3);   // the tokenizer keeps trailing spaces
 runtimeEnabled=false;assert(scriptcommands::luaFlag(vm.call({"door_open"}))==0);runtimeEnabled=true;
 assert(scriptcommands::luaFlag(vm.call({"door_open"}))==3&&scriptcommands::luaFlag(vm.call({"gone"}))==0&&scriptcommands::luaFlag(vm.call({}))==0);
 unsigned accepted=0;const int32_t one=1;for(int i=0;i<300;++i)accepted+=scriptcommands::setFlag("f"+std::to_string(i),&one);assert(accepted==255);
 // A name already in the game's table is never registered twice.
 struct Row {const char* name;int32_t length,pad;void* fn;};Row rows[2]={{"GameEvent",9,0,nullptr},{"LuaEvent",8,0,nullptr}};
 assert(scriptcommands::present((uintptr_t)rows,2,scriptcommands::eventName)&&!scriptcommands::present((uintptr_t)rows,1,scriptcommands::eventName));
 std::puts("LUA_SCRIPT_COMMANDS_PASSED");
}
