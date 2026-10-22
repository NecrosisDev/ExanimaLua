#pragma once
// characters.drop_held(target, side): a hand lets go of what it holds, the way the game drops a dead character's
// weapons (docs/reviews/drop-held-review.md). Static reading of Exanima.exe 0.9.5.2g:
//   0x156480 (death): for each item in the weapon list (character +0x928, count +0x920) it clears the item's
//     +0xc bit 0x80000, calls the item's release callback +0x720(item node) when set, then 0x155a20.
//   0x155a20(character, item, hand): clears heldItems[hand] (character +0xe88 + 8 hand), removes the item from the
//     weapon list, resets the grip (+0x1068, +0x1070), points the item's owner +0x580 back at itself and hands it
//     back to its own physics (0x1529e0, 0x1551b0, 0x155880), then refreshes the character (0x152100).
//   0x155bb0(character, item): the same for the shield (+0xe90, +0xbf8).
// The runtime does exactly that for one hand, as a job on the game's ring 0x499990, after checking again that the
// item is still in that hand, owned by that character, and that the character is not dying (+0xdf4 bit 0x800).
namespace helditems {
using Release=void(*)(uintptr_t,uintptr_t,uint32_t);using ReleaseShield=void(*)(uintptr_t,uintptr_t);using Callback=void(*)(uintptr_t);
static Release release=nullptr;static ReleaseShield releaseShield=nullptr;static bool possible=false;
static void available(){
 const unsigned char releasePrefix[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xc0,0x48,0x89,0xcb,0x48,0x89,0xd6,0x44,0x89};
 const unsigned char shieldPrefix[]={0x53,0x56,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xcb,0x48,0xc7,0x83,0x90,0x0e,0x00};
 possible=gamebridge::operation(base+0x155a20,releasePrefix,sizeof releasePrefix)&&gamebridge::operation(base+0x155bb0,shieldPrefix,sizeof shieldPrefix);
 if(possible){release=(Release)(base+0x155a20);releaseShield=(ReleaseShield)(base+0x155bb0);}
}
struct Job {uintptr_t character=0,item=0;uint32_t hand=0;bool shield=false;};
static std::deque<Job> jobs;static std::atomic<bool> busy{false};static Job job;static std::atomic<unsigned> faults{0},done{0};
// What the job checks again: the item is still in that hand and owned by this character, who is not dying.
static bool still(const Job& j,bool& shield){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t item=0,owner=0,shieldItem=0;uint32_t action=0;
 if(!m.get(j.character+0xdf4,action)||(action&0x800)||!m.get(j.character+0xe88+uintptr_t(j.hand)*8,item)||item!=j.item||
    !m.get(item+0x580,owner)||owner!=j.character)return false;
 shield=m.get(j.character+0xbf8,shieldItem)&&shieldItem==item;
 if(shield)return true;
 int32_t count=0;if(!m.get(j.character+0x920,count)||count<1||count>16)return false;
 for(int32_t i=0;i<count;++i){uintptr_t entry=0;if(m.get(j.character+0x928+uintptr_t(i)*8,entry)&&entry==item)return true;}
 return false;
}
static void work(){
 bool shield=false;if(!still(job,shield))return;
 if(shield){releaseShield(job.character,job.item);++done;return;}
 *(uint32_t*)(job.item+0xc)&=~0x80000u;
 uintptr_t callback=0,node=0;if(copyWorldMemory(job.item+0x720,&callback,8)&&callback&&gamebridge::executable(callback)&&copyWorldMemory(job.item+0x18,&node,8))((Callback)callback)(node);
 release(job.character,job.item,job.hand);++done;
}
static unsigned char run(void*){
 __try{work();}__except(EXCEPTION_EXECUTE_HANDLER){++faults;}
 busy=false;return 0;
}
// The session pulse: resolve the addons' orders against this pulse's roster, then start at most one job.
static void pulse(scripting::Host& host,bool session){
 std::vector<scripting::DropOrder> wanted;host.collectDrops(wanted);
 if(!session||!possible||!gamebridge::backupDone){jobs.clear();return;}
 if(!wanted.empty()){const auto roster=captureCharacters();const scripting::CharacterMemory m{copyWorldMemory};
  if(roster&&roster->ready)for(const auto& w:wanted)for(const auto& c:roster->characters){
   if(!c.nativeCharacter||(c.stateKnown&&c.dead)||!scripting::targets(w,c))continue;
   uintptr_t node=0;uint32_t record=0;bool lit=false;
   if(!scripting::heldItem(m,base,c.nativeCharacter,w.hand,node,record,lit))continue;
   uintptr_t item=0;if(!m.get(c.nativeCharacter+0xe88+uintptr_t(w.hand)*8,item)||!item)continue;
   if(jobs.size()<16)jobs.push_back({c.nativeCharacter,item,uint32_t(w.hand),false});}}
 if(busy.load()||jobs.empty())return;
 job=jobs.front();jobs.pop_front();busy=true;if(!nativecalls::enqueue(run))busy=false;
}
}
// characters.sever(target, part): the joint between an arm part and its parent (part +0x428) is cut the way the
// object solver 0x89d80 breaks a joint: flag 0x100000 on the joint rows (part +0x320, stride 0x68, count +0x31c;
// row +0x30 the other body, +0x38 flags), which the character solver 0x15da80 skips (no constraint, no muscle).
// Both rows are set: the part's row to its parent and the parent's row to the part. The part is recorded so blows on
// it (or below it) are not applied to its owner (combat_bridge.h). A level change or load forgets the record; the
// game rebuilds the joints then. docs/reviews/sever-review.md
namespace severing {
struct Job {uintptr_t character=0,part=0;};
static std::deque<Job> jobs;static std::atomic<bool> busy{false};static Job job;static std::atomic<unsigned> faults{0},done{0};static uintptr_t knownWorld=0;
static bool cutRows(uintptr_t from,uintptr_t to){
 int32_t count=0;uintptr_t rows=0;bool any=false;
 if(!copyWorldMemory(from+0x31c,&count,4)||count<1||count>64||!copyWorldMemory(from+0x320,&rows,8)||!rows||!readable(rows,size_t(count)*0x68))return false;
 for(int32_t i=0;i<count;++i){const uintptr_t row=rows+uintptr_t(i)*0x68;uintptr_t other=0;if(!copyWorldMemory(row+0x30,&other,8)||other!=to)continue;
  *(uint32_t*)(row+0x38)|=0x100000u;any=true;}
 return any;
}
static void work(){
 const scripting::CharacterMemory m{copyWorldMemory};int32_t count=0;uintptr_t list=0,parent=0;bool listed=false;
 if(!m.get(job.character+0x9d8,count)||count<1||count>64||!m.get(job.character+0x9e0,list))return;
 for(int32_t i=0;i<count;++i){uintptr_t p=0;if(m.get(list+uintptr_t(i)*8,p)&&p==job.part)listed=true;}
 if(!listed||!m.get(job.part+0x428,parent)||!parent||parent==job.part)return;
 const bool a=cutRows(job.part,parent),b=cutRows(parent,job.part);if(!a&&!b)return;
 for(auto& s:combat::severed){uintptr_t empty=0;if(s.compare_exchange_strong(empty,job.part)){++combat::severedCount;break;}}
 ++done;
}
static unsigned char run(void*){
 __try{work();}__except(EXCEPTION_EXECUTE_HANDLER){++faults;}
 busy=false;return 0;
}
static void forget(){for(auto& s:combat::severed)s=0;combat::severedCount=0;}
static void pulse(scripting::Host& host,bool session){
 std::vector<scripting::SeverOrder> wanted;host.collectSevers(wanted);
 uintptr_t world=0;if(!session||!copyWorldMemory(base+npc::off::world,&world,8)||!world){jobs.clear();if(knownWorld){forget();knownWorld=0;}return;}
 if(world!=knownWorld){forget();knownWorld=world;jobs.clear();}
 if(!gamebridge::backupDone)return;
 if(!wanted.empty()){const auto roster=captureCharacters();const scripting::CharacterMemory m{copyWorldMemory};
  if(roster&&roster->ready)for(const auto& w:wanted)for(const auto& c:roster->characters){
   if(!c.nativeCharacter||!scripting::targets(w,c))continue;
   int32_t count=0;uintptr_t list=0;if(!m.get(c.nativeCharacter+0x9d8,count)||count<1||count>64||!m.get(c.nativeCharacter+0x9e0,list))continue;
   for(int32_t i=0;i<count;++i){uintptr_t p=0;int32_t index=-1;if(m.get(list+uintptr_t(i)*8,p)&&p&&m.get(p+0x468,index)&&index==w.part-1){if(jobs.size()<16)jobs.push_back({c.nativeCharacter,p});break;}}}}
 if(busy.load()||jobs.empty())return;
 job=jobs.front();jobs.pop_front();busy=true;if(!nativecalls::enqueue(run))busy=false;
}
}
