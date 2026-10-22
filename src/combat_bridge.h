#pragma once
// Damage reports from the game's shared damage applier 0x159700(character, balance, health, can_fall), which
// returns 1 when the hit knocks the character down. Its weapon caller, impact 0x159a60(attacker), is detoured
// too, so a hit inside it names the attacker. The detours only queue; Lua sees the hits at the next session
// pulse, after the pointers are matched against that pulse's character snapshot.
namespace gear {static uint32_t outermostArmour(const scripting::InventoryView& view,int part);}   // gear_bridge.h
namespace combat {
using Apply=unsigned char(*)(uintptr_t,float,float,char);using Impact=void(*)(uintptr_t);
// struck: the item whose body took the blow when it was not a character part (a shield or a weapon in the way).
struct Hit {uintptr_t character=0,attacker=0;float balance=0,health=0;bool downed=false;uint32_t weapon=0;int part=-1;uint32_t struck=0;};
static Apply original=nullptr;static Impact originalImpact=nullptr;
static bool hookCreated=false,impactCreated=false,installed=false,impactInstalled=false;
static std::atomic<bool> listening{false};
struct Strike {uintptr_t attacker=0;uint32_t weapon=0;int part=-1;uint32_t struck=0;bool applied=false;};
static thread_local Strike striking;
static SRWLOCK queueLock=SRWLOCK_INIT;
static std::array<Hit,64> queue;static size_t queued=0,lost=0;
// character_hits (v1): every weapon hit on a character part, for other mods (ExCamera's wound marks), recorded here
// whether or not an addon listens. Copies only: the struck part (attacker +0xc20, part index +0x468 0..17), the
// contact point (attacker +0xc28), the part's pose (part +0x2a0, three axes then the origin), the weapon body's
// motion (attacker +0xc38: mean of +0xf0 minus mean of +0x120, 4x3 floats each), the motion kind (+0x9b8 == 0xb
// thrust). A 64-entry ring under its own lock; readers copy by serial.
// v2 appends the attacker's weapon body (attacker +0xc38) and reports cut (1) or crush (2) when the runtime's
// damage-computation hooks saw the kinds of this hit (damagescale::onKinds, same thread, just before).
struct HitRecord {uint32_t size=0,serial=0;uint64_t victim=0,part=0;float point[3]{},direction[3]{};float pose[12]{};float health=0,balance=0;uint32_t kind=0,reserved=0;uint64_t weapon=0;};
static_assert(sizeof(HitRecord)==0x78);
struct SeenKinds {uintptr_t victim=0;float cut=0,crush=0;bool fresh=false;};static thread_local SeenKinds seenKinds;
static std::atomic<bool> hitsRead{false};
static SRWLOCK hitsLock=SRWLOCK_INIT;static std::array<HitRecord,64> hits;static uint32_t hitSerial=0;
static void recordHit(uintptr_t victim,uintptr_t attacker,float balance,float health){
 if(!attacker)return;HitRecord r;r.size=sizeof r;r.victim=victim;r.health=health;r.balance=balance;
 uintptr_t part=0;int32_t index=-1;
 if(!copyWorldMemory(attacker+0xc20,&part,8)||!part||!copyWorldMemory(part+0x468,&index,4)||index<0||index>=18)return;
 r.part=part;if(!copyWorldMemory(attacker+0xc28,r.point,12)||!copyWorldMemory(part+0x2a0,r.pose,48))return;
 uintptr_t weapon=0;float now[12]{},before[12]{};
 if(copyWorldMemory(attacker+0xc38,&weapon,8)&&weapon&&copyWorldMemory(weapon+0xf0,now,48)&&copyWorldMemory(weapon+0x120,before,48))
  for(int axis=0;axis<3;++axis){float a=0,b=0;for(int k=0;k<4;++k){a+=now[k*3+axis];b+=before[k*3+axis];}r.direction[axis]=(a-b)*0.25f;}
 r.weapon=weapon;
 uint32_t motion=0;if(copyWorldMemory(attacker+0x9b8,&motion,4)&&motion==0xb)r.kind=3;
 else if(seenKinds.fresh&&seenKinds.victim==victim){if(seenKinds.cut>seenKinds.crush&&seenKinds.cut>0)r.kind=1;else if(seenKinds.crush>0)r.kind=2;}
 seenKinds.fresh=false;
 AcquireSRWLockExclusive(&hitsLock);r.serial=++hitSerial;hits[(r.serial-1)%hits.size()]=r;ReleaseSRWLockExclusive(&hitsLock);
}
static int readHits(uint32_t after,HitRecord* out,int capacity){
 hitsRead=true;if(!out||capacity<=0)return 0;AcquireSRWLockShared(&hitsLock);int n=0;const uint32_t last=hitSerial;
 uint32_t first=last>hits.size()?last-uint32_t(hits.size())+1:1;if(after+1>first)first=after+1;
 for(uint32_t s=first;s<=last&&n<capacity;++s)out[n++]=hits[(s-1)%hits.size()];
 ReleaseSRWLockShared(&hitsLock);return n;
}
static unsigned char applied(uintptr_t character,float balance,float health,char canFall){
 const auto result=original(character,balance,health,canFall);
 __try{recordHit(character,striking.attacker,balance,health);}__except(EXCEPTION_EXECUTE_HANDLER){}
 if(listening.load(std::memory_order_relaxed)){
  AcquireSRWLockExclusive(&queueLock);
  if(queued<queue.size())queue[queued++]={character,striking.attacker,balance,health,result!=0,striking.weapon,striking.part,striking.struck};else ++lost;
  ReleaseSRWLockExclusive(&queueLock);striking.applied=true;
 }
 return result;
}
// At impact entry the attacker holds the delivering weapon body (+0xc38) and the struck body (+0xc20). The weapon's
// item record: walk the body's +0x428 parent link to the root (which points to itself), whose node (+0x18) holds the
// record ID at +0x180. The struck body's part index is +0x468 (-1 for bodies that are not character parts). Copies
// only; any failed read leaves the field unknown.
static uint32_t weaponRecord(uintptr_t body){
 for(int depth=0;body&&depth<8;++depth){uintptr_t parent=0;if(!copyWorldMemory(body+0x428,&parent,8)||!parent)return 0;
  if(parent==body){uintptr_t node=0;uint32_t record=0;return copyWorldMemory(body+0x18,&node,8)&&node&&copyWorldMemory(node+0x180,&record,4)?record:0;}
  body=parent;}
 return 0;
}
static Strike strikeOf(uintptr_t attacker){
 Strike s;s.attacker=attacker;if(!listening.load(std::memory_order_relaxed))return s;
 uintptr_t weapon=0,part=0;int index=-1;
 if(copyWorldMemory(attacker+0xc38,&weapon,8)&&weapon)s.weapon=weaponRecord(weapon);
 if(copyWorldMemory(attacker+0xc20,&part,8)&&part&&copyWorldMemory(part+0x468,&index,4)){
  if(index>=0&&index<18)s.part=index;else if(index<0)s.struck=weaponRecord(part);}   // not a body part: an item took it
 return s;
}
// A blow that met a shield or weapon and applied no damage still counts: it is queued with no victim and zero
// damage, and deliver() gives it to the item's holder when that is the player (an NPC's held items are not read).
static void impacted(uintptr_t attacker){
 const auto outer=striking;striking=strikeOf(attacker);originalImpact(attacker);
 if(striking.struck&&!striking.applied&&listening.load(std::memory_order_relaxed)){
  AcquireSRWLockExclusive(&queueLock);
  if(queued<queue.size())queue[queued++]={0,striking.attacker,0,0,false,striking.weapon,-1,striking.struck};else ++lost;
  ReleaseSRWLockExclusive(&queueLock);}
 striking=outer;
}
static size_t take(std::vector<Hit>& out){
 AcquireSRWLockExclusive(&queueLock);out.assign(queue.begin(),queue.begin()+ptrdiff_t(queued));const auto missed=lost;queued=lost=0;ReleaseSRWLockExclusive(&queueLock);
 return missed;
}
static void clear(){std::vector<Hit> unused;take(unused);}
static bool hook(uintptr_t at,const unsigned char* prefix,size_t size,void* detour,void** trampoline,bool& created,bool& enabled){
 if(enabled)return true;
 if(!created){if(!gamebridge::operation(at,prefix,size)||MH_CreateHook((void*)at,detour,trampoline)!=MH_OK)return false;created=true;}
 const auto status=MH_EnableHook((void*)at);enabled=status==MH_OK||status==MH_ERROR_ENABLED;return enabled;
}
// Hits without the impact detour still arrive, with no source or attacker.
static bool install(){
 const unsigned char apply[]={0x53,0x56,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xcb,0x40,0x30,0xf6};
 const unsigned char impact[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0xa4,0x24,0x90,0xfe,0xff,0xff};
 if(!hook(base+0x159700,apply,sizeof apply,(void*)applied,(void**)&original,hookCreated,installed))return false;
 hook(base+0x159a60,impact,sizeof impact,(void*)impacted,(void**)&originalImpact,impactCreated,impactInstalled);
 return true;
}
static const scripting::Character* find(const scripting::CharacterSnapshot& snapshot,uintptr_t character){
 for(const auto& c:snapshot.characters)if(character&&c.nativeCharacter==character)return &c;return nullptr;
}
// Hits on characters missing from this pulse's snapshot (left the level, list truncated) are dropped.
static void deliver(scripting::Host& owner,const scripting::CharacterSnapshot* snapshot){
 listening=installed&&owner.handlesCombat();
 std::vector<Hit> hits;const auto missed=take(hits);
 if(!listening||!snapshot||!snapshot->ready)return;
 if(missed)owner.report("runtime","warn","Combat events dropped: more than 64 hits in one pulse");
 std::vector<scripting::CombatHit> found;
 // The player's held and worn items, read once, to name the holder of a struck item.
 scripting::InventoryView held;bool heldRead=false;const scripting::Character* player=nullptr;
 for(const auto& c:snapshot->characters)if(c.player&&c.playerKnown)player=&c;
 auto playerHolds=[&](uint32_t item){
  if(!heldRead){heldRead=true;uintptr_t p=0,stats=0;if(!(inventorybridge::possible&&copyWorldMemory(base+npc::off::controlled,&p,8)&&p&&copyWorldMemory(p+0x10a0,&stats,8)&&stats&&inventorybridge::rows(stats,held)))held={};}
  return item&&carrieditems::referenced(held,item);};
 for(const auto& hit:hits){
  const scripting::Character* victim=hit.character?find(*snapshot,hit.character):(player&&playerHolds(hit.struck)?player:nullptr);if(!victim)continue;
  scripting::CombatHit event{victim->record,victim->name,victim->player&&victim->playerKnown,hit.downed,hit.health,hit.balance};
  event.handle=uint64_t(victim->nativeCharacter);event.attackerHandle=uint64_t(hit.attacker);
  // A shield (chunk 0x8000) or a weapon (0x4000) of the victim's own took the blow: blocked or parried.
  if(hit.struck&&(!victim->player||playerHolds(hit.struck))){
   if(inventorybridge::chunk(hit.struck,0x8000)){event.blocked=true;event.shield=hit.struck;}
   else if(inventorybridge::chunk(hit.struck,0x4000)){event.parried=true;event.parryWeapon=hit.struck;}
  }
  // The player's outermost worn piece covering the struck part (NPCs' worn items are not read).
  if(victim->player&&victim->playerKnown&&hit.part>=0){playerHolds(0);event.armour=gear::outermostArmour(held,hit.part);}
  if(impactInstalled){
   event.source=hit.attacker?"weapon":"impact";event.weapon=hit.weapon;event.part=hit.part;
   if(const auto attacker=find(*snapshot,hit.attacker)){event.attacker=true;event.attackerRecord=attacker->record;event.attackerName=attacker->name;event.attackerPlayer=attacker->player&&attacker->playerKnown;}
  }
  found.push_back(std::move(event));
 }
 if(!found.empty())owner.notifyHits(found);
}
}
