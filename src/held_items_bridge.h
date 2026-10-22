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
// Characters a staged order may touch: the level has settled (aiPulseReady: the same gate AI writes use) and the
// character has been in the roster for 3 s, so a round or level that is still spawning is never changed mid-setup.
// Orders for younger characters wait (10 s at most).
namespace settling {
static std::unordered_map<uintptr_t,uint64_t> seen;static uint64_t lastSweep=0;
static bool ready(uintptr_t character,uint64_t now){
 auto it=seen.find(character);if(it==seen.end()){seen[character]=now;return false;}
 return aiPulseReady&&now>=it->second+3000;
}
static void sweep(const scripting::CharacterSnapshot* roster,uint64_t now){
 if(now<lastSweep+1000||!roster||!roster->ready)return;lastSweep=now;
 for(auto it=seen.begin();it!=seen.end();){bool present=false;for(const auto& c:roster->characters)present=present||c.nativeCharacter==it->first;
  if(!present)it=seen.erase(it);else ++it;}
 for(const auto& c:roster->characters)if(c.nativeCharacter&&!seen.count(c.nativeCharacter))seen[c.nativeCharacter]=now;
}
}
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
static bool dropNow(const Job& j){
 bool shield=false;if(!still(j,shield))return false;
 if(shield){releaseShield(j.character,j.item);++done;return true;}
 *(uint32_t*)(j.item+0xc)&=~0x80000u;
 uintptr_t callback=0,node=0;if(copyWorldMemory(j.item+0x720,&callback,8)&&callback&&gamebridge::executable(callback)&&copyWorldMemory(j.item+0x18,&node,8))((Callback)callback)(node);
 release(j.character,j.item,j.hand);++done;return true;
}
static void work(){dropNow(job);}
static unsigned char run(void*){
 __try{work();}__except(EXCEPTION_EXECUTE_HANDLER){++faults;}
 busy=false;return 0;
}
// The session pulse: resolve the addons' orders against this pulse's roster, then start at most one job.
struct Waiting {scripting::DropOrder order;uint64_t until=0;};static std::vector<Waiting> waiting;
static void pulse(scripting::Host& host,bool session){
 std::vector<scripting::DropOrder> fresh;host.collectDrops(fresh);
 if(!session||!possible||!gamebridge::backupDone){jobs.clear();waiting.clear();return;}
 const uint64_t now=GetTickCount64();for(const auto& f:fresh)if(waiting.size()<32)waiting.push_back({f,now+10000});
 if(!waiting.empty()){const auto roster=captureCharacters();const scripting::CharacterMemory m{copyWorldMemory};settling::sweep(roster,now);
  std::vector<scripting::DropOrder> wanted;
  for(auto it=waiting.begin();it!=waiting.end();){bool young=false;
   if(roster&&roster->ready)for(const auto& c:roster->characters)if(c.nativeCharacter&&scripting::targets(it->order,c)&&!settling::ready(c.nativeCharacter,now))young=true;
   if(young&&now<it->until){++it;continue;}if(!young)wanted.push_back(it->order);it=waiting.erase(it);}
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
struct Job {uintptr_t character=0,part=0;int hand=-1;};   // hand: the arm's hand (0 right, 1 left), dropped before the cut
static std::deque<Job> jobs;static std::atomic<bool> busy{false};static Job job;static std::atomic<unsigned> faults{0},done{0};static uintptr_t knownWorld=0;
static bool cutRows(uintptr_t from,uintptr_t to){
 int32_t count=0;uintptr_t rows=0;bool any=false;
 if(!copyWorldMemory(from+0x31c,&count,4)||count<1||count>64||!copyWorldMemory(from+0x320,&rows,8)||!rows||!readable(rows,size_t(count)*0x68))return false;
 for(int32_t i=0;i<count;++i){const uintptr_t row=rows+uintptr_t(i)*0x68;uintptr_t other=0;if(!copyWorldMemory(row+0x30,&other,8)||other!=to)continue;
  *(uint32_t*)(row+0x38)|=0x100000u;any=true;}
 return any;
}
static void work(){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t parent=0;bool listed=false;
 for(const auto at:{std::pair<uintptr_t,uintptr_t>{0x910,0x918}}){int32_t count=0;uintptr_t list=0;
  if(!m.get(job.character+at.first,count)||count<1||count>64||!m.get(job.character+at.second,list))continue;
  for(int32_t i=0;i<count;++i){uintptr_t p=0;if(m.get(list+uintptr_t(i)*8,p)&&p==job.part)listed=true;}}
 if(!listed||!m.get(job.part+0x428,parent)||!parent||parent==job.part)return;
 // A cut arm first lets go of what its hand holds (the death path's drop), so no grip row ties a held item to a limb
 // the solver no longer holds.
 if(job.hand>=0&&helditems::possible){uintptr_t item=0;if(m.get(job.character+0xe88+uintptr_t(job.hand)*8,item)&&item)helditems::dropNow({job.character,item,uint32_t(job.hand),false});}
 const bool a=cutRows(job.part,parent),b=cutRows(parent,job.part);if(!a&&!b)return;
 for(auto& s:combat::severed){uintptr_t empty=0;if(s.compare_exchange_strong(empty,job.part)){++combat::severedCount;break;}}
 ++done;
}
static unsigned char run(void*){
 __try{work();}__except(EXCEPTION_EXECUTE_HANDLER){++faults;}
 busy=false;return 0;
}
static void forget(){for(auto& s:combat::severed)s=0;combat::severedCount=0;}
struct Waiting {scripting::SeverOrder order;uint64_t until=0;};static std::vector<Waiting> waiting;
static void pulse(scripting::Host& host,bool session){
 std::vector<scripting::SeverOrder> fresh;host.collectSevers(fresh);
 uintptr_t world=0;if(!session||!copyWorldMemory(base+npc::off::world,&world,8)||!world){jobs.clear();waiting.clear();if(knownWorld){forget();knownWorld=0;}return;}
 if(world!=knownWorld){forget();knownWorld=world;jobs.clear();waiting.clear();}
 if(!gamebridge::backupDone)return;
 const uint64_t now=GetTickCount64();for(const auto& f:fresh)if(waiting.size()<32)waiting.push_back({f,now+10000});
 if(!waiting.empty()){const auto roster=captureCharacters();const scripting::CharacterMemory m{copyWorldMemory};settling::sweep(roster,now);
  std::vector<scripting::SeverOrder> wanted;
  for(auto it=waiting.begin();it!=waiting.end();){bool young=false;
   if(roster&&roster->ready)for(const auto& c:roster->characters)if(c.nativeCharacter&&scripting::targets(it->order,c)&&!settling::ready(c.nativeCharacter,now))young=true;
   if(young&&now<it->until){++it;continue;}if(!young)wanted.push_back(it->order);it=waiting.erase(it);}
  if(roster&&roster->ready)for(const auto& w:wanted)for(const auto& c:roster->characters){
   if(!c.nativeCharacter||!scripting::targets(w,c))continue;
   // The part, by its index +0x468, in the character's body parts (the array at *(+0x918), count +0x910).
   uintptr_t found=0;
   for(const auto at:{std::pair<uintptr_t,uintptr_t>{0x910,0x918}}){if(found)break;
    int32_t count=0;uintptr_t list=0;if(!m.get(c.nativeCharacter+at.first,count)||count<1||count>64||!m.get(c.nativeCharacter+at.second,list))continue;
    for(int32_t i=0;i<count;++i){uintptr_t p=0;int32_t index=-1;if(m.get(list+uintptr_t(i)*8,p)&&p&&m.get(p+0x468,index)&&index==w.part-1){found=p;break;}}}
   const int hand=(w.part==12||w.part==13)?0:(w.part==15||w.part==16)?1:-1;
   if(found&&jobs.size()<16)jobs.push_back({c.nativeCharacter,found,hand});}}
 if(busy.load()||jobs.empty())return;
 job=jobs.front();jobs.pop_front();busy=true;if(!nativecalls::enqueue(run))busy=false;
}
}
// characters.play_motion(target, motion): the game's motion player 0x179ee0(character, motion, blend 0x1e, 0) on a
// target, as a game-ring job (docs/reviews/play-motion-review.md). 0x179ee0 indexes the character's motion table
// (+0x9b0, a dynamic array of motion records built from the RSQ sets) without a bounds check, so the job checks the
// index against that array's length (at -8) and the entry first. Motions 48+ are those an addon spliced into
// humanoid.rsq (Ground Fighters). The character update 0x169200 may play its own motion over it next tick.
namespace motions {
using Play=void(*)(uintptr_t,uint32_t,uint32_t,char);static Play play=nullptr;static bool possible=false;
static void available(){
 const unsigned char prefix[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0x89,0xd6,0x44,0x89,0xc7};
 possible=gamebridge::operation(base+0x179ee0,prefix,sizeof prefix);if(possible)play=(Play)(base+0x179ee0);
}
struct Job {uintptr_t character=0;uint32_t motion=0;};
static std::deque<Job> jobs;static std::atomic<bool> busy{false};static Job job;static std::atomic<unsigned> faults{0},done{0};
static bool valid(uintptr_t character,uint32_t motion){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t table=0,entry=0;int64_t high=-1;
 if(!m.get(character+0x9b0,table)||!table||!m.get(table-8,high)||high<0||high>4096||int64_t(motion)>high)return false;
 return m.get(table+uintptr_t(motion)*8,entry)&&entry&&readable(entry,8);
}
static void work(){
 uint32_t action=0;if(!copyWorldMemory(job.character+0xdf4,&action,4)||(action&0x800)||!valid(job.character,job.motion))return;
 play(job.character,job.motion,0x1e,0);++done;
}
static unsigned char run(void*){
 __try{work();}__except(EXCEPTION_EXECUTE_HANDLER){++faults;}
 busy=false;return 0;
}
static void pulse(scripting::Host& host,bool session){
 std::vector<scripting::MotionOrder> wanted;host.collectMotions(wanted);
 if(!session||!possible||!gamebridge::backupDone){jobs.clear();return;}
 if(!wanted.empty()){const auto roster=captureCharacters();const uint64_t now=GetTickCount64();settling::sweep(roster,now);
  if(roster&&roster->ready)for(const auto& w:wanted)for(const auto& c:roster->characters){
   if(!c.nativeCharacter||(c.stateKnown&&c.dead)||!scripting::targets(w,c)||!settling::ready(c.nativeCharacter,now))continue;
   if(jobs.size()<16)jobs.push_back({c.nativeCharacter,w.motion});}}
 if(busy.load()||jobs.empty())return;
 job=jobs.front();jobs.pop_front();busy=true;if(!nativecalls::enqueue(run))busy=false;
}
}
