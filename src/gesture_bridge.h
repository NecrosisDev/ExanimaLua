#pragma once
// powers.gesture(kind): the player plays a cast motion and nothing else.
// 179E40(char, motion, actionId) is how powers start one: refused while a +1230 callback says busy;
// otherwise it stores actionId at +1240, plays the motion and sets action state +DF4 = 0x20. While +DF4 is 0x20,
// attack and block presses go only to +1230, so this installs a callback that releases on any event (returns -1;
// the caller then runs 179DE0, back to idle) and ends the gesture itself with 179DE0 after `hold` ms. actionId 0:
// the NPC engage path's dispatcher 1D3790 answers 0x4000000 for it (only player characters are used here anyway).
// Both calls run as jobs on the game's ring 0x499990.
namespace gestures {
using Start=char(*)(uintptr_t,int,uint32_t);using End=void(*)(uintptr_t);
static bool possible=false;static Start start=nullptr;static End end=nullptr;
struct Job {std::atomic<bool> busy{false};int motion=0;bool ending=false;uintptr_t player=0;};
static Job job;static std::atomic<int> wanted{0};static uintptr_t activePlayer=0;static uint64_t startedAt=0;static bool active=false;
constexpr uint64_t hold=1200;
static int released(void*,uint32_t){return -1;}
static void available(){
 const unsigned char startPrefix[]={0x53,0x57,0x56,0x41,0x54,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xcb,0x89,0xd6,0x44};
 const unsigned char endPrefix[]={0x53,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0x31,0xd2,0x48,0xc7,0x83,0x30,0x12};
 possible=gamebridge::operation(base+0x179e40,startPrefix,sizeof startPrefix)&&gamebridge::operation(base+0x179de0,endPrefix,sizeof endPrefix)&&readable(base+0x499990,8);
 if(possible){start=(Start)(base+0x179e40);end=(End)(base+0x179de0);}
}
// "channel" 0x22 (the Blast-type push motion), "thrust" 0x27 (Shock's).
static int motionOf(const std::string& kind){return kind=="channel"?0x22:kind=="thrust"?0x27:0;}
static bool request(const std::string& kind){const int m=motionOf(kind);if(!possible||!m)return false;int none=0;return wanted.compare_exchange_strong(none,m);}
static unsigned char run(void*){
 __try{
  const uintptr_t p=job.player;uintptr_t current=0;
  if(p&&copyWorldMemory(base+0x4f0830,&current,8)&&current==p){
   if(job.ending){uintptr_t cb=0;if(copyWorldMemory(p+0x1230,&cb,8)&&cb==uintptr_t(released))end(p);}
   else{uintptr_t cb=1;uint32_t state=1;
    if(copyWorldMemory(p+0x1230,&cb,8)&&!cb&&copyWorldMemory(p+0xdf4,&state,4)&&!state&&start(p,job.motion,0)){
     *(uintptr_t*)(p+0x1238)=0;*(uintptr_t*)(p+0x1230)=uintptr_t(released);activePlayer=p;startedAt=GetTickCount64();active=true;}}
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){}
 job.busy=false;return 0;
}
// Each pulse: start a wanted gesture for the living, standing player, or end one that has run its time.
static void deliver(scripting::Host& owner){
 if(!possible||job.busy.load())return;
 if(active&&GetTickCount64()-startedAt>=hold){active=false;if(!job.busy.exchange(true)){job.ending=true;job.player=activePlayer;if(!nativecalls::enqueue(run))job.busy=false;}return;}
 const int m=wanted.exchange(0);if(!m)return;
 const auto p=nativecalls::livePlayer();
 if(!p||!nativecalls::standing(p)||!gamebridge::backupDone){owner.report("runtime","info","powers.gesture skipped: the player is not standing and free");return;}
 if(job.busy.exchange(true))return;job.ending=false;job.motion=m;job.player=p;if(!nativecalls::enqueue(run))job.busy=false;
}
}
