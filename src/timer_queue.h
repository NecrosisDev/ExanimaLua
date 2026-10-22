#pragma once
#include <cstdint>
#include <vector>
#include <limits>
namespace scripting {
struct Timer {uint64_t id=0,due=0,interval=0;int reference=0;};
class TimerQueue {
 std::vector<Timer> timers;
public:
 static constexpr size_t limit=64;
 const std::vector<Timer>& entries()const{return timers;}
 void clear(){timers.clear();}
 bool add(const Timer& timer){if(timers.size()>=limit)return false;timers.push_back(timer);return true;}
 int cancel(uint64_t id){for(auto it=timers.begin();it!=timers.end();++it)if(it->id==id){const int ref=it->reference;timers.erase(it);return ref;}return 0;}
 bool take(uint64_t now,Timer& result){
  auto selected=timers.end();
  for(auto it=timers.begin();it!=timers.end();++it)if(it->due<=now&&(selected==timers.end()||it->due<selected->due||(it->due==selected->due&&it->id<selected->id)))selected=it;
  if(selected==timers.end())return false;result=*selected;
  // Rearm before the callback so cancelling a running repeat uses the same path.
  if(result.interval&&now<=(std::numeric_limits<uint64_t>::max)()-result.interval)selected->due=now+result.interval;
  else{result.interval=0;timers.erase(selected);}
  return true;
 }
};
}
