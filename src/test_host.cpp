#include "script_host.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include "test_assert.h"
static const char* caps="events.item_use,items.read,containers.read,containers.open,doors.unlock";
static std::string file(const char* path){std::ifstream f(path);assert(f);std::stringstream s;s<<f.rdbuf();return s.str();}
static bool load(scripting::Host& h,const char* id,const std::string& source,const char* required=caps){return h.load(id,source.data(),source.size(),required);}
int main(){
 using namespace scripting;
 Host h;const auto keyring=file("keyring.lua");assert(load(h,"keyring",keyring));
 Host core(0);assert(load(core,"pure","assert(api.version == 1)",""));
 assert(!load(core,"unavailable",keyring));
 Event e;e.item=1424;e.model="keyring_a";e.container=true;e.target="door";e.doorLocked=true;e.requiredKey=90;
 e.entries={{11,true,false},{90,true,false},{120,true,false}};
 assert(h.dispatch(e).action==Action::unlockDoor);
 e.entries[1].removed=true;assert(!h.dispatch(e).handled);e.entries[1].removed=false;
 e.entries[1].key=false;assert(!h.dispatch(e).handled);e.entries[1].key=true;
 e.requiredKey=999;assert(!h.dispatch(e).handled);e.requiredKey=90;
 e.storageOpen=true;assert(h.dispatch(e).handled&&h.dispatch(e).action==Action::none);e.storageOpen=false;
 e.requiredKey=e.item;assert(h.dispatch(e).handled&&h.dispatch(e).action==Action::none);e.requiredKey=90;
 e.target="inventory";e.targetItem=e.item;assert(h.dispatch(e).action==Action::openStorage);
 e.targetItem=11;assert(h.dispatch(e).handled&&h.dispatch(e).action==Action::none);
 e.target="door";e.model="other";assert(!h.dispatch(e).handled);e.model="keyring_a";
 assert(load(h,"keyring",keyring)&&h.size()==1);
 assert(!load(h,"keyring","this is invalid Lua !")&&h.handles(e.model));
 assert(h.dispatch(e).action==Action::unlockDoor);
 assert(!load(h,"missing",keyring,"does.not.exist"));
 assert(!load(h,"missing",keyring,"events.item_use"));
 Host faults;
 assert(load(faults,"broken","events.on('item.use','keyring_a',function(e) e:unlock(); error('stop') end)"));
 assert(faults.dispatch(e).action==Action::none&&!faults.handles(e.model));
 assert(faults.error().find("stop")!=std::string::npos);
 assert(load(faults,"good",keyring));assert(faults.dispatch(e).action==Action::unlockDoor);
 Host chain;
 assert(load(chain,"broken","events.on('item.use','keyring_a',function(e) error('first addon failed') end)"));
 assert(load(chain,"observer","events.on('item.use','keyring_a',function(e) return false end)"));
 assert(load(chain,"good",keyring));assert(chain.dispatch(e).action==Action::unlockDoor);
 assert(load(faults,"isolated","assert(leaked == nil); leaked = 7; assert(os == nil and io == nil and package == nil and debug == nil and loadfile == nil and setmetatable == nil)"));
 assert(load(faults,"second","assert(leaked == nil)"));
 Host stale;
 assert(load(stale,"stale","local old; events.on('item.use','keyring_a',function(e) if old then old:unlock() else old=e end return true end)"));
 assert(stale.dispatch(e).handled);assert(!stale.dispatch(e).handled);
 assert(stale.error().find("expired event")!=std::string::npos);
 Host limits;
 assert(!load(limits,"loop-load","while true do end"));
 assert(limits.error().find("budget")!=std::string::npos);
 assert(!load(limits,"memory","local x=string.rep('x',16*1024*1024)"));
 assert(load(limits,"loop-call","events.on('item.use','keyring_a',function(e) while true do end end)"));
 assert(!limits.dispatch(e).handled&&!limits.handles(e.model));
 assert(load(limits,"caught","events.on('item.use','keyring_a',function(e) while true do pcall(function() while true do end end) end end)"));
 assert(!limits.dispatch(e).handled);
 Host denied;
 assert(load(denied,"denied","events.on('item.use','keyring_a',function(e) e:unlock(); return true end)","events.item_use,items.read"));
 assert(denied.dispatch(e).action==Action::none);
 assert(h.unload("keyring")&&!h.handles(e.model)&&h.size()==0);
 std::puts("LUA_HOST_PASSED: actual keyring script, isolation, dispatch, staged actions, stale handles, capability denial, failed reload retention, unload, memory/instruction limits");
}
