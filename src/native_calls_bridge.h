#pragma once
// Staged native calls for Lua (player.damage, player.push, levels.drive). A callback stages them; they run after it
// returns without error, on the presentation pulse, which is the game's window thread and its game-flow thread
// (static reading: message pump 0x1e30 -> frame 0x263c0 on one thread). Never from a
// worker and never inside a physics contact callback (neither has a reentrancy guard).
//   0x159700(character, float balance, float health, char can_fall)  damage applier (runs this runtime's own
//            combat detour, so character.hit sees it; follows the game's 0x4f0900 override)
//   0x158870(character, float3* direction, float amount, char toward)  push-back
//   0x14a3e0(scene = world + 0x220, u32 record, u32 source 0, u32 state)  drive a door/device record:
//            doors 0 close, 1 open, 0xffffffff toggle (door flag 0x80 swaps 0/1); buttons, kind 6/7 devices
namespace nativecalls {
using Damage=unsigned char(*)(uintptr_t,float,float,char);using Push=void(*)(uintptr_t,const float*,float,char);
using Drive=void(*)(uintptr_t,uint32_t,uint32_t,uint32_t);
static Damage damage=nullptr;static Push push=nullptr;static Drive drive=nullptr;
static bool damagePossible=false,pushPossible=false,drivePossible=false;
static void available(){
 const unsigned char damagePrefix[]={0x53,0x56,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xcb,0x40,0x30,0xf6};
 const unsigned char pushPrefix[]={0x53,0x56,0x48,0x8d,0x64,0x24,0xc8,0x48,0x89,0xcb,0x0f,0x28,0xca,0x44,0x88,0xce};
 const unsigned char drivePrefix[]={0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x41,0x56,0x48,0x8d,0x64,0x24,0xb8,0x48,0x89};
 damagePossible=gamebridge::operation(base+0x159700,damagePrefix,sizeof damagePrefix);if(damagePossible)damage=(Damage)(base+0x159700);
 pushPossible=gamebridge::operation(base+0x158870,pushPrefix,sizeof pushPrefix);if(pushPossible)push=(Push)(base+0x158870);
 drivePossible=gamebridge::operation(base+0x14a3e0,drivePrefix,sizeof drivePrefix)&&readable(base+0x4f0bd0,8);if(drivePossible)drive=(Drive)(base+0x14a3e0);
}
// The controlled character, alive (death stage 0, health > 0) and in a session; 0 otherwise.
static uintptr_t livePlayer(){
 uintptr_t player=0;uint32_t stage=1;float health=0;
 if(!worldPulse||!copyWorldMemory(base+0x4f0830,&player,8)||!player||!copyWorldMemory(player+scripting::lifeStageAt,&stage,4)||stage!=0||
    !copyWorldMemory(player+npc::off::health,&health,4)||!(health>0))return 0;
 return player;
}
static bool callDamage(uintptr_t c,float b,float h,bool fall){__try{damage(c,b,h,fall?1:0);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool callPush(uintptr_t c,const float* d,float a){__try{push(c,d,a,0);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool callDrive(uintptr_t scene,uint32_t record,uint32_t state){__try{drive(scene,record,0,state);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
// Travel and place run as jobs on the game's own frame queue 0x499990 (0x1a890 adds, the frame 0x263c0 drains
// it next frame on the game thread, exactly as level exits do). Travel copies what the exit job 0x1250e0 does for
// a level outside the locales table: name to 0x2b7690, arrival entry to 0x2ba6a0, end drags 0x119150, close
// windows 0x139430(1), migrate player/followers/items 0x180640, save 0x17f7f0(0), flow state 0x4eff70 = 0, then
// schedule the loader 0x124ff0 with 0x1246d0. Place calls 0x180160(player, transform) on a scratch copy (it
// changes it): basis row 2 is the facing (+0xa48), the translation at +0x24.
using Job=unsigned char(*)(void*);using Enqueue=void(*)(uintptr_t,Job,void*);using Void0=void(*)();using Close=void(*)(unsigned char);
using Save=void(*)(int);using Schedule=void(*)(void*,unsigned char,unsigned char);using Place=void(*)(uintptr_t,float*);
static bool travelPossible=false,placePossible=false;
struct Pending {std::atomic<bool> busy{false};char name[16]{};uint32_t entry=0;float at[3]{};};
static Pending travelJob,placeJob;
static bool travelAvailable(){
 const unsigned char queue[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0x48,0x89,0xd6,0x4c,0x89};
 const unsigned char drags[]={0x48,0x8d,0x64,0x24,0xd8,0x48,0x8b,0x05};const unsigned char close[]={0x53,0x56,0x48,0x8d,0x64,0x24,0xd8,0x40,0x88,0xce};
 const unsigned char migrate[]={0x48,0x8d,0x64,0x24,0xd8,0xe8};const unsigned char save[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0xa0,0x48,0x89,0x5d,0xc8,0x48,0x89,0x7d};
 const unsigned char schedule[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xd0,0x48,0x89,0xcb,0x40,0x88,0xd6,0x44,0x88};const unsigned char loader[]={0x48,0x8d,0x64,0x24,0xd8,0x83,0x3d};
 const unsigned char place[]={0x53,0x56,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xc8,0x48,0x89,0xd6,0x48,0x8b,0x90};
 const bool queueOk=gamebridge::operation(base+0x1a890,queue,sizeof queue)&&readable(base+0x499990,8);
 travelPossible=queueOk&&gamebridge::operation(base+0x119150,drags,sizeof drags)&&gamebridge::operation(base+0x139430,close,sizeof close)&&
  gamebridge::operation(base+0x180640,migrate,sizeof migrate)&&gamebridge::operation(base+0x17f7f0,save,sizeof save)&&
  gamebridge::operation(base+0x1246d0,schedule,sizeof schedule)&&gamebridge::operation(base+0x124ff0,loader,sizeof loader)&&readable(base+0x2b7690,16)&&readable(base+0x2ba6a0,4);
 placePossible=queueOk&&gamebridge::operation(base+0x180160,place,sizeof place);
 return travelPossible;
}
static int32_t flowState(){int32_t flow=0;return copyWorldMemory(base+0x4eff70,&flow,4)?flow:-1;}
static unsigned char runTravel(void*){
 __try{
  if(flowState()==3){
   std::memcpy((void*)(base+0x2b7690),travelJob.name,16);*(uint32_t*)(base+0x2ba6a0)=travelJob.entry;
   ((Void0)(base+0x119150))();((Close)(base+0x139430))(1);((Void0)(base+0x180640))();((Save)(base+0x17f7f0))(0);
   *(int32_t*)(base+0x4eff70)=0;((Schedule)(base+0x1246d0))((void*)(base+0x124ff0),0,0);
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){}
 travelJob.busy=false;return 0;
}
// The player must be standing: death stage 0, health > 0 and no ragdoll/dead action bits, since 0x180160 resets state.
static bool standing(uintptr_t player){uint32_t action=0;return player&&copyWorldMemory(player+npc::off::action,&action,4)&&!(action&0x800)&&(action&0xa00)!=0xa00;}
static unsigned char runPlace(void*){
 __try{
  const auto player=livePlayer();float forward[3]{};
  if(player&&standing(player)&&copyWorldMemory(player+0xa48,forward,12)){
   float t[12]{};const float right[3]={forward[2],0.f,-forward[0]};
   t[0]=right[0];t[1]=right[1];t[2]=right[2];t[3]=0;t[4]=1;t[5]=0;t[6]=forward[0];t[7]=forward[1];t[8]=forward[2];
   t[9]=placeJob.at[0];t[10]=placeJob.at[1];t[11]=placeJob.at[2];((Place)(base+0x180160))(player,t);
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){}
 placeJob.busy=false;return 0;
}
static bool enqueue(Job job){__try{((Enqueue)(base+0x1a890))(base+0x499990,job,nullptr);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool commit(const scripting::NativeCall& call){
 if(!gamebridge::backupDone)return false;
 switch(call.kind){
  case scripting::NativeCall::travel:{
   if(!travelPossible||!worldPulse||flowState()!=3||!livePlayer()||travelJob.busy.exchange(true))return false;
   std::memset(travelJob.name,0,16);std::memcpy(travelJob.name,call.name,strnlen(call.name,16));travelJob.entry=call.record;
   if(!enqueue(runTravel)){travelJob.busy=false;return false;}return true;}
  case scripting::NativeCall::place:{
   const auto p=livePlayer();if(!placePossible||!p||!standing(p)||placeJob.busy.exchange(true))return false;
   std::memcpy(placeJob.at,call.values,12);if(!enqueue(runPlace)){placeJob.busy=false;return false;}return true;}
  case scripting::NativeCall::damage:{const auto p=livePlayer();return damage&&p&&callDamage(p,call.values[0],call.values[1],call.flag);}
  // characters.damage: only a character in this pulse's roster that is not the player and not dead.
  case scripting::NativeCall::damageCharacter:{
   if(!damage||!worldPulse||!call.target)return false;const auto roster=captureCharacters();if(!roster||!roster->ready)return false;
   for(const auto& c:roster->characters)if(c.nativeCharacter==call.target){
    if((c.playerKnown&&c.player)||(c.stateKnown&&c.dead))return false;return callDamage(call.target,call.values[0],call.values[1],call.flag);}
   return false;}
  case scripting::NativeCall::push:{const auto p=livePlayer();return push&&p&&callPush(p,call.values,call.values[3]);}
  case scripting::NativeCall::drive:{uintptr_t world=0;if(!drive||!worldPulse||!copyWorldMemory(base+0x4f0bd0,&world,8)||!world)return false;return callDrive(world+0x220,call.record,call.state);}
  default:break;
 }
 return false;
}
}
