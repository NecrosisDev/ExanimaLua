#pragma once
// characters.grant_powers(record_id, {power ids}): teach a live NPC powers so its brain starts casting them
// Powers live in the stats object char +0x10A0: known count +0x188, known list
// +0x190 (16-byte {u32 id, u32 state, u64 ptr}), ten prepared u32 slots +0x198. The NPC caster B7E20 reads only the
// prepared slots, so each power goes into a free slot and also into the known list (the game's own Summon Demon path
// writes the known list too, and overruns it when a prepared power has no known entry). The list grows in place as
// Power_Glean 1D34C0 does: +0x188 + 1, ReAllocMem 0EE10(&+0x190, count << 4) through the FPC memory manager, then
// {id, 0x9FFF, 0}. A newly filled slot's AI wait (char +0xD40 -> +0x9F4[i]) is zeroed, or the NPC waits about 30 s.
// Stats objects are refcounted (+8) and can be shared with a spawner: anything but 1 is refused. Runs as a job on
// the game ring 0x499990, after re-reading the character from the world list; the player is never edited.
namespace npcpowers {
using Realloc=void*(*)(void**,size_t);
static bool possible=false;
struct Grant {uint32_t record=0;std::array<uint32_t,10> ids{};size_t count=0;};
static std::vector<Grant> wanted;static std::mutex lock;
struct Job {std::atomic<bool> busy{false};uintptr_t character=0;uint32_t record=0;Grant grant;};
static Job job;static std::atomic<unsigned> refused{0},done{0};
static void available(){
 const unsigned char reallocPrefix[]={0x48,0x8d,0x64,0x24,0xd8,0xff,0x15,0x1d,0xc7,0x29,0x00};
 possible=gamebridge::operation(base+0xee10,reallocPrefix,sizeof reallocPrefix)&&readable(base+0x2ab538,8)&&readable(base+0x499990,8);
}
// From Lua: a valid id has a power byte and one of the schools the NPC evaluator 1D3790 knows (Force 0x200, Energy
// 0x400, Displacement 0x600); Mind is refused, as the game's NPC caster never casts it.
static bool validId(uint32_t id){const uint32_t school=id&0xff00;return (id&0xff)&&(school==0x200||school==0x400||school==0x600)&&!(id&~0x0E0FFFFFu);}
static bool request(uint32_t record,const std::vector<uint32_t>& ids){
 if(!possible||ids.empty()||ids.size()>10)return false;for(auto id:ids)if(!validId(id))return false;
 Grant g;g.record=record;for(auto id:ids)g.ids[g.count++]=id;std::lock_guard<std::mutex> l(lock);if(wanted.size()>=8)return false;wanted.push_back(g);return true;
}
static unsigned char run(void*){
 __try{
  const uintptr_t c=job.character;uintptr_t stats=0,player=0,ai=0;int32_t refs=0,count=0;uint32_t record=0;
  bool ok=c&&copyWorldMemory(base+0x4f0830,&player,8)&&c!=player&&copyWorldMemory(c+0x10a0,&stats,8)&&stats&&readable(stats,0x1c4)&&
   copyWorldMemory(stats+8,&refs,4)&&refs==1&&copyWorldMemory(stats+0x188,&count,4)&&count>=0&&count<256;
  // Queued one frame ago from a pulse snapshot of the same character; still playing the same level (flow state 3).
  if(ok){int32_t flow=0;ok=copyWorldMemory(base+0x4eff70,&flow,4)&&flow==3;(void)record;}
  if(!ok){++refused;job.busy=false;return 0;}
  copyWorldMemory(c+0xd40,&ai,8);
  for(size_t k=0;k<job.grant.count;++k){const uint32_t id=job.grant.ids[k];
   count=*(int32_t*)(stats+0x188);const uintptr_t list=*(uintptr_t*)(stats+0x190);bool known=false;
   for(int i=0;i<count&&list;++i)if((*(uint32_t*)(list+uintptr_t(i)*16)&0x0E0FFFFF)==(id&0x0E0FFFFF))known=true;
   if(!known){
    const int next=count+1;((Realloc)(base+0xee10))((void**)(stats+0x190),size_t(next)<<4);
    const uintptr_t grown=*(uintptr_t*)(stats+0x190);if(!grown){++refused;break;}
    *(uint32_t*)(grown+uintptr_t(count)*16)=id;*(uint32_t*)(grown+uintptr_t(count)*16+4)=0x9FFF;*(uint64_t*)(grown+uintptr_t(count)*16+8)=0;
    *(int32_t*)(stats+0x188)=next;}
   auto slots=(uint32_t*)(stats+0x198);int free=-1;bool prepared=false;
   for(int i=0;i<10;++i){if((slots[i]&0x0E0FFFFF)==(id&0x0E0FFFFF))prepared=true;if(!slots[i]&&free<0)free=i;}
   if(!prepared&&free>=0){slots[free]=id;if(ai&&readable(ai+0x9f4+4*size_t(free),4))*(uint32_t*)(ai+0x9f4+4*size_t(free))=0;}
  }
  ++done;
 }__except(EXCEPTION_EXECUTE_HANDLER){++refused;}
 job.busy=false;return 0;
}
// Each pulse: the first waiting grant whose character is in this pulse's snapshot goes to the game thread.
static void deliver(scripting::Host& owner,const scripting::CharacterSnapshot*(*roster)()){
 static unsigned toldRefused=0;if(refused!=toldRefused){toldRefused=refused;owner.report("runtime","warn","characters.grant_powers refused: the character is gone, is the player, or shares its stats with its spawner");}
 if(!possible||job.busy.load()||!gamebridge::backupDone)return;
 {std::lock_guard<std::mutex> l(lock);if(wanted.empty())return;}
 const auto snapshot=roster();if(!snapshot||!snapshot->ready)return;
 std::lock_guard<std::mutex> l(lock);
 for(auto it=wanted.begin();it!=wanted.end();++it){
  for(const auto& c:snapshot->characters)if(c.record==it->record&&c.nativeCharacter&&!(c.player&&c.playerKnown)){
   if(job.busy.exchange(true))return;job.character=c.nativeCharacter;job.record=it->record;job.grant=*it;wanted.erase(it);
   if(!nativecalls::enqueue(run)){job.busy=false;++refused;}return;}
 }
}
// A snapshot already in hand (tests).
static const scripting::CharacterSnapshot* givenSnapshot=nullptr;static const scripting::CharacterSnapshot* given(){return givenSnapshot;}
static void deliver(scripting::Host& owner,const scripting::CharacterSnapshot* snapshot){givenSnapshot=snapshot;deliver(owner,given);}
}
