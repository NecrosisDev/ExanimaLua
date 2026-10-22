#pragma once
// gear.use: double-click a weapon or apparel item, then click it on something in the world, the way keys and
// repair orbs are used. The inventory's double-click 0x10ee45 asks the
// gate 0x102310(item, flags) whether an item can be used; plain gear (no device chunk 0x10000) gets 0 and nothing
// happens. While a Lua addon listens, the gate's detour answers 2 for gear (weapon chunk 0x4000 or apparel chunk
// 0x1000, no device chunk) after the game's own gate said 0, so the game's generic use starter 0x11cd50 puts the
// item on the cursor. The click then reaches the world-use detour 0x11cb70 (game_bridge.h useBody), which hands
// gear to `use` here instead of the game: the game itself has no use for gear there, and its consumable branch
// (kind 0x10 -> 0x11c310) must never run for it.
namespace gear {
using Gate=unsigned char(*)(uint32_t,uint32_t);
static Gate originalGate=nullptr;static bool gateCreated=false,installed=false;
static std::atomic<bool> listening{false};
static bool isGear(uint32_t item,std::string& kind){
 if(!item||inventorybridge::chunk(item,0x10000))return false;
 if(inventorybridge::chunk(item,0x4000)){kind="weapon";return true;}
 if(inventorybridge::chunk(item,0x1000)){kind="apparel";return true;}
 if(inventorybridge::chunk(item,0x8000)){kind="shield";return true;}   // shield chunk; no condition bytes are read for it
 return false;
}
static bool carriedOnly(uint32_t item);
// Only gear in the pack: worn and held items keep the game's answer, so they are never lifted onto the cursor.
static unsigned char gate(uint32_t item,uint32_t flags){
 const auto result=originalGate(item,flags);
 if(result||!listening.load(std::memory_order_relaxed))return result;
 std::string kind;bool gearItem=false;try{gearItem=isGear(item,kind)&&carriedOnly(item);}catch(...){}
 return gearItem?2:result;
}
static bool install(){
 if(installed)return true;
 // Five bytes (push rbx; push rdi; push rsi; push r12) end on an instruction boundary; no RIP-relative operand.
 const unsigned char prefix[]={0x53,0x57,0x56,0x41,0x54,0x48,0x8d,0x64,0x24,0xd8,0x89,0xcb,0x89,0xd6,0x31,0xff};
 if(!gateCreated){if(!gamebridge::operation(base+0x102310,prefix,sizeof prefix)||MH_CreateHook((void*)(base+0x102310),(void*)gate,(void**)&originalGate)!=MH_OK)return false;gateCreated=true;}
 const auto s=MH_EnableHook((void*)(base+0x102310));installed=s==MH_OK||s==MH_ERROR_ENABLED;return installed;
}
// Repair jobs (wear type 10, dirt type 9) on the inventory control's job list, as the orb and brush add them; the
// control's own tick 0x119550 animates them over two seconds and refreshes the item. Set by restoreAvailable().
static bool restorePossible=false;
static bool restore(uint32_t item,const std::string& kind,int wear,int dirt);
static bool carriedOnly(uint32_t item);
// Whether the item is in the player's pack (not worn, not in a weapon slot).
static bool carriedOnly(uint32_t item){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t player=0,stats=0;scripting::InventoryView view;
 if(!m.get(base+npc::off::controlled,player)||!player||!m.get(player+0x10a0,stats)||!stats||!inventorybridge::rows(stats,view))return false;
 for(const auto& row:view.worn)if(row.item==item)return false;
 for(size_t i=0;i<view.weapons.size();i+=2)if(view.weapons[i]==item)return false;
 for(const auto& row:view.carried)if(row.item==item&&!(row.state&0xc80100))return true;
 return false;
}
// target.heat: the distance (cm) from the picked object to the nearest burning fire within 300 cm, NaN for none.
// A fire is a node of the player's sector whose body is the game's fire-effect class (0x30e9e8), the same test the
// fire-source bridge uses for the fires it lights (world_emitter_bridge.h fireNodes); torches carried by NPCs and
// the player's own light are not fire effects of the sector. Reads only.
static float nearestFire(uintptr_t player,const std::array<float,3>& at){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t body=0,sector=0;float best=NAN;
 if(!m.get(player+0x18,body)||!body||!m.get(body+0x130,sector)||!sector)return best;
 std::vector<uintptr_t> nodes;if(!worlddevices::children(sector,nodes,8192))return best;
 for(const auto node:nodes){uintptr_t effect=0,owner=0;std::array<float,3> p{};
  if(!node||!m.get(node+0x160,effect)||!effect||!scripting::classMatches(effect,base+0x30e9e8,copyWorldMemory)||!m.get(effect+0x18,owner)||owner!=node||!m.get(node+0x64,p))continue;
  const float dx=p[0]-at[0],dy=p[1]-at[1],dz=p[2]-at[2],d=std::sqrt(dx*dx+dy*dy+dz*dz);
  if(std::isfinite(d)&&d<=300.f&&!(d>=best))best=d;}
 return best;
}
// event:ignite_target: every fire comes from F2A70; 1494D0(node, size)
// builds one at a live scene node's position (+0x64) as a free scene object, size u16 × 0.01. It allocates and
// registers a body, so it runs as a job on the game's ring 0x499990, never inside this click. Saves rebuild fire
// bodies (F2ED0), so a node that already has one is refused: a child or nearby fire-effect body whose controller
// +0x158 is 0x3DD0E002 (the test 1C9820 uses), and the job checks again before lighting.
constexpr uint32_t fireTag=0x3DD0E002;
static bool burning(uintptr_t node,const std::array<float,3>& at){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t sector=0;if(!m.get(node+0x130,sector)||!sector)return true;
 std::vector<uintptr_t> nodes;if(!worlddevices::children(sector,nodes,8192))return true;
 for(const auto other:nodes){uintptr_t body=0;uint32_t tag=0;std::array<float,3> p{};
  if(!other||!m.get(other+0x160,body)||!body||!scripting::classMatches(body,base+0x30e9e8,copyWorldMemory)||!m.get(body+0x158,tag)||tag!=fireTag||!m.get(other+0x64,p))continue;
  const float dx=p[0]-at[0],dy=p[1]-at[1],dz=p[2]-at[2];if(std::sqrt(dx*dx+dy*dy+dz*dz)<150.f)return true;}
 std::vector<uintptr_t> children;if(!worlddevices::children(node,children,64))return true;
 for(const auto child:children){uintptr_t body=0;uint32_t tag=0;if(child&&m.get(child+0x160,body)&&body&&m.get(body+0x158,tag)&&tag==fireTag)return true;}
 return false;
}
struct IgniteJob {std::atomic<bool> busy{false};uintptr_t node=0,sector=0,world=0;std::array<float,3> at{};};
static IgniteJob igniteJob;static uintptr_t pickedNode=0;
static bool sameScene(uintptr_t node,uintptr_t& sector,uintptr_t& world){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t owner=0;
 return m.get(base+npc::off::world,world)&&world&&m.get(node+0x130,sector)&&sector&&m.get(sector+0x220,owner)&&owner==world;
}
static unsigned char runIgnite(void*){
 __try{
  uintptr_t sector=0,world=0;std::array<float,3> at{};
  if(sameScene(igniteJob.node,sector,world)&&sector==igniteJob.sector&&world==igniteJob.world&&copyWorldMemory(igniteJob.node+0x64,at.data(),12)&&at==igniteJob.at&&
     !burning(igniteJob.node,at)&&worldemitters::fire)worldemitters::fire(igniteJob.node,100);   // size 1.0
 }__except(EXCEPTION_EXECUTE_HANDLER){}
 igniteJob.busy=false;return 0;
}
static bool ignite(uintptr_t node){
 if(!node||!worldemitters::firePossible||!worldemitters::fire||!gamebridge::backupDone||igniteJob.busy.exchange(true))return false;
 igniteJob.node=node;if(!sameScene(node,igniteJob.sector,igniteJob.world)||!copyWorldMemory(node+0x64,igniteJob.at.data(),12)||burning(node,igniteJob.at)){igniteJob.busy=false;return false;}
 if(!nativecalls::enqueue(runIgnite)){igniteJob.busy=false;return false;}return true;
}
// The world target under the cursor, as the key path finds a door: focus, pick, scene node, then its class.
static void target(scripting::GearUse& event,uintptr_t control){
 event.targetKind="other";
 const auto mouse=*(uintptr_t*)(base+0x49a960);if(!readable(mouse,0x20))return;
 const auto focus=gamebridge::api.focus(mouse);if(focus!=*(uintptr_t*)(control+0x18))return;
 alignas(16) unsigned char hit[64]{};*(uint32_t*)(hit+0x34)=1;
 if(!gamebridge::api.pick(focus,float(*(int*)(mouse+0x18)),float(*(int*)(mouse+0x1c)),hit))return;
 const auto node=gamebridge::api.object(*(uintptr_t*)hit);if(!readable(node,0x190))return;
 const int kind=gamebridge::api.interaction(node);uint32_t record=0;uintptr_t body=0;
 copyWorldMemory(node+0x180,&record,4);copyWorldMemory(node+0x160,&body,8);
 // 0x118f80: 0 none, 3 item, 4 door, 0x20 lever, 0x30 button, 8 corpse; placed scenery has no record or no body.
 event.targetKind=kind==4?"door":kind==3?"item":kind==0x20?"lever":kind==0x30?"button":kind==8?"corpse":(!record||!body)?"object":"other";
 event.targetRecord=record;
 char name[17]{};if(copyWorldMemory(node+0x140,name,16)){size_t n=0;while(n<16&&name[n]&&name[n]>=0x20&&name[n]<0x7f)++n;if(n==strnlen(name,16))event.targetName.assign(name,n);}
 uintptr_t player=0;std::array<float,3> at{},centre{};
 if(copyWorldMemory(node+0x64,at.data(),12)&&copyWorldMemory(base+npc::off::controlled,&player,8)&&player&&copyWorldMemory(player+npc::off::root,centre.data(),12)){
  const float dx=at[0]-centre[0],dy=at[1]-centre[1],dz=at[2]-centre[2];const float d=std::sqrt(dx*dx+dy*dy+dz*dz);if(std::isfinite(d))event.distance=d;
  event.heat=nearestFire(player,at);
  uintptr_t sector=0,world=0;
  if(event.targetKind=="object"&&worldemitters::firePossible&&sameScene(node,sector,world)&&!burning(node,at)){event.ignitable=true;pickedNode=node;}}
}
// Called from useBody for an item on the cursor; true when this took the click (the game's own use is skipped).
static bool use(uintptr_t control,uint32_t item){
 std::string kind;if(!listening.load()||!isGear(item,kind))return false;
 scripting::GearUse event;event.item=item;event.kind=kind;if(gamebridge::api.getChunk)event.model=gamebridge::model(item);
 scripting::ItemStats stats;if(kind!="shield"&&itemstats::lookup&&itemstats::readAt(item,stats,true))event.condition=stats.condition;
 event.carried=carriedOnly(item);pickedNode=0;target(event,control);
 const auto result=host.dispatchGear(event);
 if(result.handled&&result.ignite&&!ignite(event.ignitable?pickedNode:0))host.report("runtime","warn","ignite_target refused: fire building unavailable, no save backup, a fire already burns there, or one is still being lit");
 if(result.handled&&result.restore){
  if(!restorePossible||!gamebridge::backupDone||!restore(item,kind,result.wear,result.dirt))host.report("runtime","warn","restore_condition refused: repair jobs unavailable, no save backup, or the item changed");
 }
 return true;   // gear never has a game use here
}
// character.hit armour_id: equipment refresh ECA40 folds worn apparel into each part's rows in wear order, the
// last piece with coverage on that part on top; the worn rows are taken
// to be in that order (inferred). Coverage for part p is the apparel word at payload +0x150 + 4p.
static uint32_t outermostArmour(const scripting::InventoryView& view,int part){
 if(part<0||part>=18||!itemstats::lookup)return 0;
 for(auto row=view.worn.rbegin();row!=view.worn.rend();++row){scripting::ItemStats stats;
  if(row->item&&itemstats::readAt(row->item,stats,true)&&stats.armour&&stats.a.coverage[size_t(part)]>0)return row->item;}
 return 0;
}
static bool condition(uint32_t item,scripting::Condition& out){
 scripting::ItemStats stats;if(!itemstats::lookup||!itemstats::readAt(item,stats,true))return false;out=stats.condition;return out.known;
}
// item.use event:consume(): the used item leaves the pack on the next pulse through the same row removal the
// consumable path uses (inventory_bridge.h compact + finish, then a full read-back), the way the game's own use
// code removes a spent orb or salve during the use. Pack items only: worn, held or bagged items are refused.
static std::vector<uint32_t> consumeQueue;
static void consumeLater(uint32_t item){if(item&&consumeQueue.size()<8)consumeQueue.push_back(item);}
static void consumeNow(scripting::Host& owner){
 if(consumeQueue.empty())return;const auto items=std::move(consumeQueue);consumeQueue.clear();
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t player=0,stats=0;
 if(!worldPulse||!inventorybridge::possible||inventorybridge::broken||!gamebridge::backupDone||!m.get(base+npc::off::controlled,player)||!player||!m.get(player+0x10a0,stats)||!stats){
  owner.report("runtime","warn","consume refused: no playing character, inventory writes unavailable, or no save backup");return;}
 for(const auto item:items){
  scripting::InventoryView current;if(!inventorybridge::rows(stats,current)){owner.report("runtime","warn","consume refused: the pack is unreadable");return;}
  size_t index=current.carried.size();
  for(size_t i=0;i<current.carried.size();++i)if(current.carried[i].item==item&&!(current.carried[i].state&0xc80100)){index=i;break;}
  if(index==current.carried.size()||carrieditems::referenced(current,item)){owner.report("runtime","warn","consume refused: the used item is no longer a plain pack item");continue;}
  if(!inventorybridge::compact(stats,index)||!inventorybridge::finish(stats,player,item,false)){inventorybridge::fault();return;}
  auto expected=current.carried;expected.erase(expected.begin()+std::ptrdiff_t(index));scripting::InventoryView after;
  if(!inventorybridge::rows(stats,after)||after.carried!=expected||after.worn!=current.worn||after.weapons!=current.weapons){inventorybridge::fault();return;}
 }
}
// items.add_wear(id, {wear, dirt}): raise an item's wear/dirt through the same timed job rows restore_condition uses
// (the control's tick 0x119550 moves the byte from->to over two seconds and refreshes the item, whichever way it
// goes). Instanced weapons and apparel only, the game's unrepairable mark refused, clamped to 255, 32 per pulse,
// applied on the pulse after the request (never inside a hit).
static bool addJob(uintptr_t control,uint32_t type,uint32_t item,int from,int to);
struct Wear {uint32_t item=0;int wear=0,dirt=0;};
static std::vector<Wear> wearQueue;
static bool queueWear(uint32_t item,int wear,int dirt){
 if(!restorePossible||!(item&0xf0000000u)||wearQueue.size()>=32)return false;
 for(auto& w:wearQueue)if(w.item==item){w.wear=(std::min)(255,w.wear+wear);w.dirt=(std::min)(255,w.dirt+dirt);return true;}
 wearQueue.push_back({item,wear,dirt});return true;
}
// A worn piece's wear feeds the armour layer rows only when equipment refresh ECA40(character, 0) rebuilds them
// after the job has moved the byte (about 2 s) the player's are rebuilt with
// it, as a job on the ring 0x499990. NPCs' worn items are not read, so their rows wait for the game's own refresh.
using Refresh=void(*)(uintptr_t,int);static bool refreshPossible=false;static uint64_t refreshAt=0;static uintptr_t refreshPlayer=0;
static std::atomic<bool> refreshBusy{false};
static void refreshAvailable(){
 const unsigned char prefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0xa4,0x24,0x00,0xff,0xff,0xff,0x48,0x89,0x9d,0x20};
 refreshPossible=gamebridge::operation(base+0xeca40,prefix,sizeof prefix);
}
static unsigned char runRefresh(void*){
 __try{uintptr_t p=0;if(copyWorldMemory(base+0x4f0830,&p,8)&&p&&p==refreshPlayer)((Refresh)(base+0xeca40))(p,0);}__except(EXCEPTION_EXECUTE_HANDLER){}
 refreshBusy=false;return 0;
}
static void refreshWorn(){
 if(!refreshAt||GetTickCount64()<refreshAt)return;
 if(refreshBusy.exchange(true))return;refreshAt=0;
 const auto p=nativecalls::livePlayer();if(!p||p!=refreshPlayer||!nativecalls::enqueue(runRefresh))refreshBusy=false;
}
static void wearNow(scripting::Host& owner){
 refreshWorn();
 if(wearQueue.empty())return;const auto items=std::move(wearQueue);wearQueue.clear();
 scripting::InventoryView worn;{uintptr_t p=0,stats=0;if(!(copyWorldMemory(base+npc::off::controlled,&p,8)&&p&&copyWorldMemory(p+0x10a0,&stats,8)&&stats&&inventorybridge::rows(stats,worn)))worn={};refreshPlayer=p;}
 uintptr_t control=0;if(!restorePossible||!gamebridge::backupDone||!worldPulse||!copyWorldMemory(base+0x4efdd0,&control,8)||!control){owner.report("runtime","warn","items.add_wear skipped: repair jobs unavailable, no save backup, or no session");return;}
 for(const auto& w:items){std::string kind;if(!isGear(w.item,kind)||kind=="shield")continue;
  const auto chunk=inventorybridge::chunk(w.item,kind=="weapon"?0x4000:0x1000);int32_t mark=0;
  if(!chunk||!copyWorldMemory(chunk+8+(kind=="weapon"?0x4:0x1ac),&mark,4)||mark==-1)continue;
  scripting::ItemStats stats;if(!itemstats::readAt(w.item,stats,true)||!stats.condition.known)continue;
  const int wear=(std::min)(255,int(stats.condition.wear)+w.wear),dirt=(std::min)(255,int(stats.condition.dirt)+w.dirt);
  if(wear>int(stats.condition.wear)&&!addJob(control,10,w.item,int(stats.condition.wear),wear))break;
  if(dirt>int(stats.condition.dirt)&&!addJob(control,9,w.item,int(stats.condition.dirt),dirt))break;
  if(refreshPossible&&kind=="apparel"&&wear>int(stats.condition.wear))for(const auto& row:worn.worn)if(row.item==w.item)refreshAt=GetTickCount64()+2300;}
}
static void deliver(scripting::Host& owner){listening=installed&&owner.handlesGear();consumeNow(owner);wearNow(owner);}
// The job list of the inventory control (*0x4efdd0, the same object 0x11cb70 receives): count i32 +0x108, a Free
// Pascal dynamic array of 0x20-byte rows at +0x110 {u32 type, u32 item, u32 start ms (0x4997d0), i32 from, i32 to,
// pad, u64 node}. The orb's adder 0x11c580 grows it with DynArraySetLength 0x0b8e0(&array, typeinfo 0x312a38, 1,
// &length) and writes the last row; this does the same with the caller's amounts and without 0x11c1f0, which would
// use up the orb. The tick 0x119550 animates from->to over two seconds, rewrites the byte, refreshes the item
// (0x119380 / 0x1192a0, 0x10d9c0, 0x107bc0) and removes finished rows.
using SetLength=void(*)(void**,void*,int64_t,int64_t*);
static SetLength setLength=nullptr;
static void restoreAvailable(){
 const unsigned char prefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0xa4,0x24,0x70,0xff,0xff,0xff,0x48,0x89,0x5d,0x98};
 restorePossible=installed&&gamebridge::operation(base+0xb8e0,prefix,sizeof prefix)&&readable(base+0x312a38,8)&&readable(base+0x4997d0,4)&&readable(base+0x4efdd0,8);
 if(restorePossible)setLength=(SetLength)(base+0xb8e0);
 refreshAvailable();
}
static bool addJob(uintptr_t control,uint32_t type,uint32_t item,int from,int to){
 __try{
  int32_t count=*(int32_t*)(control+0x108);if(count<0||count>=1024)return false;
  int64_t length=int64_t(count)+1;setLength((void**)(control+0x110),(void*)(base+0x312a38),1,&length);
  const auto rows=*(uint8_t**)(control+0x110);if(!rows)return false;
  uint8_t* row=rows+size_t(count)*0x20;std::memset(row,0,0x20);
  *(uint32_t*)row=type;*(uint32_t*)(row+4)=item;*(uint32_t*)(row+8)=*(uint32_t*)(base+0x4997d0);*(int32_t*)(row+0xc)=from;*(int32_t*)(row+0x10)=to;
  *(int32_t*)(control+0x108)=count+1;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool restore(uint32_t item,const std::string& kind,int wear,int dirt){
 uintptr_t control=0;if(kind=="shield"||!restorePossible||!copyWorldMemory(base+0x4efdd0,&control,8)||!control||!carriedOnly(item))return false;
 // The orb's refusals: a weapon whose payload +4 is -1, or apparel whose +0x1ac is -1, cannot be repaired.
 const auto chunk=inventorybridge::chunk(item,kind=="weapon"?0x4000:0x1000);int32_t mark=0;
 if(!chunk||!copyWorldMemory(chunk+8+(kind=="weapon"?0x4:0x1ac),&mark,4)||mark==-1)return false;
 scripting::ItemStats stats;if(!itemstats::readAt(item,stats,true)||!stats.condition.known)return false;
 bool ok=true;
 if(wear>=0&&unsigned(wear)<stats.condition.wear)ok=addJob(control,10,item,int(stats.condition.wear),wear);
 if(ok&&dirt>=0&&unsigned(dirt)<stats.condition.dirt)ok=addJob(control,9,item,int(stats.condition.dirt),dirt);
 return ok;
}
}
