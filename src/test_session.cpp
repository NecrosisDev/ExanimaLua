#include "runtime_session.h"
#include <cassert>
#include <cstdio>
#include "test_assert.h"
static std::vector<std::string> messages;
static void logMessage(void*,const char*,const char*,const char* message){messages.emplace_back(message);}
int main(){
 using namespace scripting;Host host(0);host.setLogger(logMessage);Session session(host);
 session.configure(lifecycleEvents,true);
 ScriptSource code{"addon","events.on('runtime.phase',function(e) log.info(e.phase..':'..e.generation) end);events.on('item.use','test',function() return true end)","events.lifecycle,events.item_use,items.read","",{}};
 assert(session.submit(code,true)&&session.pending("addon")&&!host.handles("test"));
 auto unknown=code;unknown.id="unknown";unknown.required="unknown";assert(!session.submit(unknown,true));assert(session.error().find("unknown required capability")!=std::string::npos);
 unknown.required="storage.settings";assert(!session.submit(unknown,true));assert(session.error().find("unavailable required capability")!=std::string::npos);
 assert(!session.begin(11,22,"menu",0,0));assert(session.pending("addon"));
 assert(session.begin(11,11,"menu",0,0));assert(!session.begin(11,11,"menu",0,0));assert(!session.settledMenu(499));session.end();
 assert(session.begin(11,11,"menu",0,500));assert(session.retryDue(500));assert(!session.retryDue(501));session.consume();session.end();
 assert(session.pending("addon")&&!host.handles("test"));
 session.configure(lifecycleEvents|itemApiCapabilities,false);
 assert(session.begin(11,11,"menu",0,600));session.consume();session.end();
 assert(!session.pending("addon")&&host.handles("test")&&messages.back()=="menu:0");
 assert(session.begin(11,11,"session",111,700));session.end();assert(messages.back()=="session:1");
 const auto count=messages.size();assert(session.begin(11,11,"session",111,800));session.end();assert(messages.size()==count);
 assert(!session.begin(22,22,"session",222,900));
 assert(session.begin(11,11,"session",222,900));session.end();assert(messages.back()=="session:2");
 assert(session.begin(11,11,"loading",0,1000));session.end();assert(messages.back()=="loading:2");
 auto broken=code;broken.source="error('broken reload')";assert(session.submit(broken,true));
 assert(session.begin(11,11,"menu",0,2000));session.end();assert(session.begin(11,11,"menu",0,2500));session.consume();session.end();
 assert(host.enabled("addon")&&host.handles("test"));
 Host faults(lifecycleEvents);faults.setLogger(logMessage);
 const std::string bad="events.on('runtime.phase',function(e) error('bad lifecycle') end)";
 const std::string good="events.on('runtime.phase',function(e) log.info(e.phase) end)";
 assert(faults.load("bad",bad.data(),bad.size(),"events.lifecycle"));assert(faults.load("good",good.data(),good.size(),"events.lifecycle"));
 faults.notifyPhase("session",1);assert(!faults.enabled("bad")&&faults.enabled("good")&&messages.back()=="session");
 session.clear();assert(host.size()==0&&!session.pending("addon"));
 assert(session.begin(22,22,"menu",0,3000));session.end();
 Host budget;Session bounded(budget);bounded.configure(0,false);
 ScriptSource large{"",std::string(1024*1024,' '),"","",{}};
 for(int i=0;i<16;++i){large.id="large"+std::to_string(i);assert(bounded.submit(large,true));}
 large.id="overflow";assert(!bounded.submit(large,true));
 std::puts("LUA_SESSION_PASSED: owner-thread gate, reentrancy, phase/generation events, pending capabilities, retry backoff, atomic reload retention, fault isolation, disable and source budget");
}
