#include "runtime_session.h"
#include <cassert>
#include <cstdio>
#include <chrono>
#include <thread>
#include "test_assert.h"
struct Message {std::string id,value;};static std::vector<Message> messages;
static void capture(void*,const char* id,const char*,const char* text){messages.push_back({id,text});}
static bool load(scripting::Host& h,const char* id,const std::string& source,const char* caps="timers"){
 return h.load(id,source.data(),source.size(),caps);
}
static int hits(const std::string& value){int result=0;for(const auto& m:messages)if(m.value==value)++result;return result;}
int main(){
 using namespace scripting;Host h(timerEvents);h.setLogger(capture);h.setTime(1000);
 assert(load(h,"basic","assert(timers.now()==1);timers.after(0,function() log.info('zero') end);timers.after(.1,function() log.info('once') end);local n=0;local id;id=timers.every(.1,function() n=n+1;log.info('repeat');if n==2 then assert(timers.cancel(id));assert(not timers.cancel(id)) end end)"));
 h.runTimers();assert(hits("zero")==0);h.setTime(1001);h.runTimers();assert(hits("zero")==1&&hits("once")==0);
 h.setTime(1100);h.runTimers();assert(hits("once")==1&&hits("repeat")==1);
 h.setTime(5000);h.runTimers();assert(hits("repeat")==2);h.runTimers();assert(hits("repeat")==2);
 h.setTime(1);assert(load(h,"clock","assert(timers.now()==5)"));
 for(const auto& source:{"timers.after(-1,function()end)","timers.after(0/0,function()end)","timers.after(math.huge,function()end)","timers.after(86401,function()end)","timers.every(0,function()end)","timers.every(.001,function()end)"})assert(!load(h,"invalid",source));
 Host denied(0);assert(!load(denied,"denied","timers.after(0,function()end)",""));
 assert(!load(h,"capacity","for i=1,65 do timers.after(1,function()end) end"));
 Host quota(timerEvents);quota.setLogger(capture);
 assert(load(quota,"quota","for i=1,64 do timers.after(0,function()log.info('quota')end) end"));
 quota.setTime(1);quota.runTimers();assert(hits("quota")==4);
 Host fair(timerEvents);fair.setLogger(capture);
 for(int i=0;i<20;++i)assert(load(fair,("fair"+std::to_string(i)).c_str(),"timers.after(0,function() log.info('fair') end)"));
 fair.setTime(1);fair.runTimers();assert(hits("fair")==16);fair.runTimers();assert(hits("fair")==20);
 Host recursive(timerEvents);recursive.setLogger(capture);
 assert(load(recursive,"recursive","local function again()log.info('recursive');timers.after(0,again)end;timers.after(0,again)"));
 recursive.setTime(1);recursive.runTimers();recursive.runTimers();assert(hits("recursive")==1);
 recursive.setTime(2);recursive.runTimers();assert(hits("recursive")==2);
 Host lifetime(timerEvents|lifecycleEvents);lifetime.setLogger(capture);
 assert(load(lifetime,"life","timers.after(0,function()log.info('old phase')end)"));
 lifetime.notifyPhase("menu",0,"life");lifetime.setTime(1);lifetime.runTimers();assert(hits("old phase")==1);
 assert(load(lifetime,"life","timers.after(0,function()log.info('cancelled phase')end);events.on('runtime.phase',function()timers.after(0,function()log.info('new phase')end)end)","timers,events.lifecycle"));
 lifetime.notifyPhase("session",1);lifetime.setTime(2);lifetime.runTimers();assert(hits("cancelled phase")==0&&hits("new phase")==1);
 assert(load(lifetime,"life","timers.after(1,function()log.info('retained')end)"));
 assert(!load(lifetime,"life","timers.after(0,function()log.info('bad replacement')end);error('reload rejected')"));
 lifetime.setTime(1002);lifetime.runTimers();assert(hits("retained")==1&&hits("bad replacement")==0);
 assert(load(lifetime,"life","timers.after(0,function()log.info('replaced')end)"));assert(load(lifetime,"life","assert(not timers.cancel(5))"));
 lifetime.setTime(2000);lifetime.runTimers();assert(hits("replaced")==0);
 Host isolated(timerEvents);isolated.setLogger(capture);
 assert(load(isolated,"first","assert(timers.after(0,function()log.info('owned')end)==1)"));
 assert(load(isolated,"second","assert(not timers.cancel(1));timers.after(0,function()log.info('second')end)"));
 assert(isolated.unload("first"));isolated.setTime(1);isolated.runTimers();assert(hits("owned")==0&&hits("second")==1);
 Host faults(timerEvents);faults.setLogger(capture);
 assert(load(faults,"broken","timers.after(0,function()error('timer failed')end);timers.after(0,function()log.info('must cancel')end)"));
 assert(load(faults,"good","timers.after(0,function()log.info('survives')end)"));
 faults.setTime(1);faults.runTimers();assert(!faults.enabled("broken")&&faults.enabled("good")&&hits("must cancel")==0&&hits("survives")==1);
 assert(load(faults,"loop","timers.after(0,function()while true do end end)"));faults.setTime(2);faults.runTimers();assert(!faults.enabled("loop"));
 Host stale(timerEvents|itemApiCapabilities);
 assert(load(stale,"stale","events.on('item.use','test',function(e)timers.after(0,function()e:unlock()end);return true end)","timers,events.item_use,items.read,doors.unlock"));
 Event event;event.model="test";event.target="door";event.doorLocked=true;assert(stale.dispatch(event).handled);
 stale.setTime(1);stale.runTimers();assert(!stale.enabled("stale")&&stale.error().find("expired event")!=std::string::npos);
 Host gated(timerEvents);gated.setLogger(capture);Session session(gated);session.configure(timerEvents,false);
 assert(load(gated,"gated","timers.after(0,function()log.info('gated')end)"));
 session.tickTimers();assert(hits("gated")==0);assert(!session.begin(1,2,"menu",0,2));session.tickTimers();assert(hits("gated")==0);
 assert(session.begin(1,1,"menu",0,2));session.tickTimers();session.end();assert(hits("gated")==0); // transition cancelled old timer
 assert(load(gated,"gated","timers.after(0,function()log.info('gated')end)"));
 assert(session.begin(1,1,"menu",0,3));session.tickTimers();session.end();assert(hits("gated")==1);
 session.clear();assert(gated.size()==0);
 TimerQueue queue;assert(queue.add({1,1,100,1}));Timer last;assert(queue.take((std::numeric_limits<uint64_t>::max)(),last)&&last.interval==0&&queue.entries().empty());
 // tasks.worker: a module run on a worker thread, result copied back to a main-thread callback; errors; cancel.
 {Host w(timerEvents);w.setLogger(capture);w.setTime(1);
  std::vector<Module> mods{{"gen","local util=require('util') return function(t) local s=0 for i=1,t.n do s=s+i end return {sum=s,name=util.mark(t.name),list={1,2,{3}}} end"},
   {"util","return {mark=function(s) return s..'!' end}"},{"bad","return function() error('boom') end"}};
  const std::string src="assert(tasks.worker('gen',{n=1000,name='x'},function(r,e) assert(not e);log.info('w'..r.sum..r.name..#r.list..r.list[3][1]) end));"
   "tasks.worker('bad',nil,function(r,e) log.info(e and e:find('boom') and 'werr' or 'wno') end);"
   "local h=tasks.worker('gen',{n=1,name='c'},function() log.info('wcancel') end);assert(tasks.cancel_worker(h));"
   "assert(not pcall(tasks.worker,'missing',1,function()end));assert(not pcall(tasks.worker,'gen',function()end,function()end))";
  assert(w.load("workers",src.data(),src.size(),"timers",8*1024*1024,mods));
  for(int i=0;i<1000&&(hits("w500500x!33")==0||hits("werr")==0);++i){std::this_thread::sleep_for(std::chrono::milliseconds(5));w.runTimers();}
  assert(hits("w500500x!33")==1&&hits("werr")==1&&hits("wcancel")==0&&hits("wno")==0);}
 std::puts("LUA_TIMERS_PASSED: delays, repeats, cancellation/ownership, fairness/budgets, no catch-up, lifecycle/reload/unload cleanup, fault limits, stale event rejection and owner-thread gating");
}
