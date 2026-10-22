#pragma once
namespace carrieditems {
using Clone=uint32_t(*)(uint32_t,uintptr_t);using Allocate=void(*)(void**,size_t);using Free=void(*)(void*);
using Equip=char(*)(uintptr_t,uint32_t,uint32_t,unsigned,void*);using Hide=void(*)(uintptr_t,uint32_t,int);
using Shield=char(*)(uintptr_t,uint32_t,uint32_t,uintptr_t);using RemoveAction=char(*)(void*);using Queue=void(*)(uintptr_t,char(*)(void*),void*);
static Clone clone=nullptr,convert=nullptr;static Allocate allocate=nullptr;static Free release=nullptr;static Equip equip=nullptr;static Hide hide=nullptr;
static uintptr_t displacedCallback=0;
static Shield shield=nullptr;static RemoveAction removeAction=nullptr;static Queue enqueue=nullptr;static Hide unhide=nullptr;
static bool weaponsPossible=false,bagsPossible=false,shieldsPossible=false,unequipPossible=false;
static std::atomic<bool> actionFault{false};
struct Bag {uint64_t inventory=0;size_t row=0;uint32_t item=0;containerbridge::Record source;scripting::ContainerView view;};
static std::vector<Bag> bags;
static bool available(bool weapons){
 const unsigned char clonePrefix[]={0x53,0x57,0x56,0x41,0x54,0x48,0x8d,0x64,0x24,0xa8,0x89,0xce,0x48,0x89,0xd7,0x41};
 if(!weapons)return containerbridge::available()&&gamebridge::operation(base+0x1035c0,clonePrefix,sizeof clonePrefix);
 const unsigned char equipPrefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0x90,0x48,0x89,0x5d,0xb0,0x48,0x89,0x7d};
 const unsigned char allocatePrefix[]={0x53,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0x48,0x89,0xd1,0xff,0x15,0x96,0xc7};
 const unsigned char freePrefix[]={0x48,0x8d,0x64,0x24,0xd8,0xff,0x15,0x45,0xc7,0x29,0x00,0x90,0x48,0x8d,0x64,0x24};
 const unsigned char hidePrefix[]={0x53,0x8b,0x01,0x83,0xe8,0x01,0x85,0xc0,0x7c,0x62,0x41,0xb9,0xff,0xff,0xff,0xff};
 const unsigned char displacedPrefix[]={0x49,0x83,0x78,0x20,0x00,0x0f,0x84,0x69,0x00,0x00,0x00,0x49,0x8b,0x40,0x20,0x8b};
 const unsigned char convertPrefix[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xc0,0x89,0xcb,0x48,0x89,0xd6,0x89,0xdf};
 return gamebridge::operation(base+0x135810,equipPrefix,sizeof equipPrefix)&&gamebridge::operation(base+0xed70,allocatePrefix,sizeof allocatePrefix)&&
  gamebridge::operation(base+0xedd0,freePrefix,sizeof freePrefix)&&gamebridge::operation(base+0x1115a0,hidePrefix,sizeof hidePrefix)&&
  gamebridge::operation(base+0x24a950,displacedPrefix,sizeof displacedPrefix)&&gamebridge::operation(base+0x102700,convertPrefix,sizeof convertPrefix);
}
static bool availableAction(bool removing){
 const unsigned char unhidePrefix[]={0x53,0x8b,0x01,0x83,0xe8,0x01,0x85,0xc0,0x7c,0x62,0x41,0xb9,0xff,0xff,0xff,0xff};
 if(!gamebridge::operation(base+0x111530,unhidePrefix,sizeof unhidePrefix))return false;
 if(removing){
  const unsigned char removePrefix[]={0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x48,0x8d,0x64,0x24,0x90,0x48,0x89,0x4c,0x24};
  const unsigned char queuePrefix[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0x48,0x89,0xd6,0x4c,0x89};
  const unsigned char allocatePrefix[]={0x53,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0x48,0x89,0xd1,0xff,0x15,0x96,0xc7};
  const unsigned char freePrefix[]={0x48,0x8d,0x64,0x24,0xd8,0xff,0x15,0x45,0xc7,0x29,0x00,0x90,0x48,0x8d,0x64,0x24};
  return gamebridge::operation(base+0x1345d0,removePrefix,sizeof removePrefix)&&gamebridge::operation(base+0x1a890,queuePrefix,sizeof queuePrefix)&&
   gamebridge::operation(base+0xed70,allocatePrefix,sizeof allocatePrefix)&&gamebridge::operation(base+0xedd0,freePrefix,sizeof freePrefix);
 }
 const unsigned char shieldPrefix[]={0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x48,0x8d,0x64,0x24,0xb0,0x48,0x89,0xcb,0x89};
 const unsigned char hidePrefix[]={0x53,0x8b,0x01,0x83,0xe8,0x01,0x85,0xc0,0x7c,0x62,0x41,0xb9,0xff,0xff,0xff,0xff};
 const unsigned char convertPrefix[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xc0,0x89,0xcb,0x48,0x89,0xd6,0x89,0xdf};
 return gamebridge::operation(base+0x135b00,shieldPrefix,sizeof shieldPrefix)&&gamebridge::operation(base+0x1115a0,hidePrefix,sizeof hidePrefix)&&gamebridge::operation(base+0x102700,convertPrefix,sizeof convertPrefix);
}
static bool checkFault(){if(!actionFault.load())return true;if(!inventorybridge::broken)inventorybridge::fault();return false;}
static bool queuesIdle(){
 if(!checkFault())return false;
 if(!weaponsPossible&&!shieldsPossible&&!unequipPossible)return true;
 const scripting::CharacterMemory m{copyWorldMemory};
 for(unsigned rva:{0x49b9d0u,0x499990u}){uint32_t read=0,write=0;if(!m.get(base+rva,read)||!m.get(base+rva+4,write)||read>=128||write>=128||read!=write)return false;}
 return true;
}
static bool referenced(const scripting::InventoryView& view,uint32_t id){
 for(const auto& row:view.worn)if(row.item==id)return true;
 for(size_t i=0;i<view.weapons.size();i+=2)if(view.weapons[i]==id)return true;return false;
}
static const scripting::ContainerView* readBag(uint64_t ticket,size_t index){
 using namespace inventorybridge;
 if(!bagsPossible||!ticket||ticket!=target.view.ticket||!identity(target)||index>=target.view.carried.size())return nullptr;
 scripting::InventoryView current;if(!rows(target.stats,current)||current.carried!=target.view.carried||current.worn!=target.view.worn||current.weapons!=target.view.weapons)return nullptr;
 for(const auto& bag:bags)if(bag.inventory==ticket&&bag.row==index)return &bag.view;
 if(bags.size()>=8)return nullptr;
 const auto item=current.carried[index];if(!item.item||(item.state&0xc80100)||referenced(current,item.item)||!containerbridge::closed(item.item))return nullptr;
 if(item.item&0xc0000000u){size_t copies=0;for(const auto& row:current.carried)copies+=row.item==item.item;if(copies!=1)return nullptr;}
 Bag bag;bag.inventory=ticket;bag.row=index;bag.item=item.item;
 if(!containerbridge::record(item.item,bag.source,true)||!scripting::containerView(bag.source.bytes,bag.view))return nullptr;
 bags.push_back(std::move(bag));return &bags.back().view;
}
static bool cloneCall(uint32_t id,uintptr_t db,uint32_t& out){__try{out=clone(id,db);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool assignRow(uintptr_t stats,size_t index,const scripting::InventoryRow& row){
 __try{const int count=*(int*)(stats+0xf0);auto data=*(scripting::InventoryRow**)(stats+0xf8);
  if(count<1||count>4096||index>=size_t(count)||!readable((uintptr_t)data,size_t(count)*12))return false;data[index]=row;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
struct BagPlan {
 Bag chosen;scripting::InventoryRow row;containerbridge::Record record;
 std::vector<scripting::InventoryRow> entries;size_t offset=0;uint32_t actual=0;bool shared=false;
};
static bool prepareBag(const scripting::InventoryView& current,uint64_t ticket,size_t index,BagPlan& plan){
 if(index>=current.carried.size())return false;
 const Bag* found=nullptr;for(const auto& bag:bags)if(bag.inventory==ticket&&bag.row==index){found=&bag;break;}if(!found)return false;
 plan.chosen=*found;plan.row=current.carried[index];plan.actual=plan.row.item;plan.shared=(plan.row.item&0xc0000000u)==0;
 if(plan.row.item!=plan.chosen.item||(plan.row.state&0xc80100)||referenced(current,plan.row.item)||!containerbridge::closed(plan.row.item)||
  !containerbridge::record(plan.row.item,plan.record,true)||plan.record.db!=plan.chosen.source.db||plan.record.bytes!=plan.chosen.source.bytes||
  !scripting::containerRows(plan.record.bytes,plan.entries,plan.offset))return false;
 if(plan.shared){
  const scripting::CharacterMemory m{copyWorldMemory};uint32_t size=0;uintptr_t db=0;const auto metadata=inventorybridge::chunk(plan.row.item,0x10000000);
  if(!metadata||!m.get(metadata+4,size)||size<32||!m.get(base+0x4ef930,db)||!db)return false;
  for(const auto& row:plan.entries)if(inventorybridge::chunk(row.item,0x20000))return false;
 }return true;
}
static bool bagFits(const BagPlan& bag,uint32_t item){
 uint16_t capacity=0,maximum=0;std::memcpy(&capacity,bag.record.bytes.data()+bag.offset+0x10,2);std::memcpy(&maximum,bag.record.bytes.data()+bag.offset+0x12,2);
 return inventorybridge::fits(bag.entries,item,capacity,maximum);
}
static bool materializeBag(BagPlan& plan){
 if(!plan.shared)return true;
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t db=0;
 if(!m.get(base+0x4ef930,db)||!db||!cloneCall(plan.row.item,db,plan.actual)||(plan.actual&0xc0000000u)!=0x80000000u||
  !containerbridge::record(plan.actual,plan.record,true)||plan.record.db!=db)return false;
 std::vector<scripting::InventoryRow> cloned;size_t at=0;
 // Native clone metadata belongs to the instance, not the shared source copy.
 return scripting::containerRows(plan.record.bytes,cloned,at)&&cloned==plan.entries;
}
static bool publishBag(const BagPlan& plan,const std::vector<uint8_t>& next){
 containerbridge::Record latest;
 if(!containerbridge::closed(plan.actual)||!containerbridge::record(plan.actual,latest,true)||latest.db!=plan.record.db||latest.bytes!=plan.record.bytes)return false;
 struct Buffer{const void* data;uint32_t size,pad;} buffer{next.data(),uint32_t(next.size()),0};
 if(!containerbridge::publish(latest.db,latest.slot,&buffer))return false;
 containerbridge::Record after;return containerbridge::record(plan.actual,after,true)&&after.bytes==next;
}
static bool commitBag(const inventorybridge::Target& owner,const scripting::InventoryView& current,const scripting::InventoryChange& change){
 using namespace inventorybridge;using namespace scripting;
 if(!bagsPossible||!clone||worlditems::published)return false;
 BagPlan source;if(!prepareBag(current,change.inventory,change.bag,source))return false;
 const bool transfer=change.op==InventoryOp::bagTransfer,taking=change.op==InventoryOp::bagTake;
 const auto& rowsIn=(taking||transfer)?source.entries:current.carried;
 if(change.row>=rowsIn.size()||(!taking&&!transfer&&change.row==change.bag)||(taking&&current.carried.size()>=4096))return false;
 const auto item=rowsIn[change.row];if(!worlditems::ordinary(item.item,item.state)||referenced(current,item.item)||item.item==source.row.item)return false;
 BagPlan destination;
 if(transfer){
  if(change.bag==change.destinationBag||!prepareBag(current,change.inventory,change.destinationBag,destination)||
   (!source.shared&&!destination.shared&&source.actual==destination.actual)||!bagFits(destination,item.item))return false;
 }else if(taking){
  const CharacterMemory m{copyWorldMemory};int maximum=0;if(!m.get(base+0x2b7290,maximum)||maximum<1||!fits(current.carried,item.item,400,unsigned(maximum)))return false;
 }else if(!bagFits(source,item.item))return false;
 auto next=source.record.bytes;
 if(!changeContainerRow(next,change.row,(taking||transfer)?nullptr:&item)||next.size()>1024*1024)return false;
 auto destinationNext=destination.record.bytes;
 if(transfer&&(!changeContainerRow(destinationNext,0,&item)||destinationNext.size()>1024*1024))return false;
 InventoryView last;BagPlan latestSource,latestDestination;
 if(!identity(owner)||!rows(owner.stats,last)||last.carried!=current.carried||last.worn!=current.worn||last.weapons!=current.weapons||
  !prepareBag(current,change.inventory,change.bag,latestSource)||(transfer&&!prepareBag(current,change.inventory,change.destinationBag,latestDestination)))return false;
 worlditems::published=true;
 if(!materializeBag(source)||(transfer&&!materializeBag(destination))||(transfer&&source.actual==destination.actual))return fault();
 next=source.record.bytes;if(!changeContainerRow(next,change.row,(taking||transfer)?nullptr:&item)||next.size()>1024*1024)return fault();
 if(transfer){
  destinationNext=destination.record.bytes;if(!changeContainerRow(destinationNext,0,&item)||destinationNext.size()>1024*1024||!publishBag(destination,destinationNext))return fault();
 }
 // Re-resolve the source slot after destination publication can move database storage.
 if(!publishBag(source,next))return fault();
 auto expected=current.carried;
 if(source.shared){expected[change.bag].item=source.actual;if(!assignRow(owner.stats,change.bag,expected[change.bag]))return fault();}
 if(transfer&&destination.shared){expected[change.destinationBag].item=destination.actual;if(!assignRow(owner.stats,change.destinationBag,expected[change.destinationBag]))return fault();}
 if(taking){if(!add(owner.stats,item))return fault();auto appended=item;appended.x=40;appended.y=4;expected.push_back(appended);}
 else if(!transfer){if(!compact(owner.stats,change.row))return fault();expected.erase(expected.begin()+change.row);}
 InventoryView result;if(!rows(owner.stats,result)||result.carried!=expected||result.worn!=current.worn||result.weapons!=current.weapons||!finish(owner.stats,owner.player,item.item,false))return fault();
 for(const auto* plan:{&source,transfer?&destination:nullptr})if(plan&&plan->shared&&(!finish(owner.stats,owner.player,plan->row.item,false)||!finish(owner.stats,owner.player,plan->actual,false)))return fault();
 target={};bags.clear();return true;
}
static int equipCall(const inventorybridge::Target& owner,const scripting::InventoryRow& row,unsigned hand,uint32_t& actual){
 __try{
  actual=convert(row.item,*(uintptr_t*)(base+0x4ef930));if(!actual||(actual&0xc0000000u)==0xc0000000u)return -1;
  void* action=nullptr;void* callback=nullptr;allocate(&action,0x48);allocate(&callback,0x28);if(!action||!callback){if(callback)release(callback);if(action)release(action);return -1;}
  std::memset(action,0,0x48);std::memset(callback,0,0x28);
  *(uintptr_t*)((uint8_t*)action+0x30)=(uintptr_t)callback;*((uint8_t*)action+0x40)=1;
  *(int*)callback=1;*(uintptr_t*)((uint8_t*)callback+0x10)=displacedCallback;*(uintptr_t*)((uint8_t*)callback+0x20)=owner.stats+0xf0;
  if(!equip(owner.player,actual,row.state,hand,action)){release(callback);release(action);return 0;}
  // Native completion owns both allocations and the displaced-item callback from this point.
  return 1;
 }__except(EXCEPTION_EXECUTE_HANDLER){return -1;}
}
static bool hideCall(uintptr_t stats,uint32_t item){__try{hide(stats+0xf0,item,1);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool commitWeapon(const inventorybridge::Target& owner,const scripting::InventoryView& current,const scripting::InventoryChange& change){
 using namespace inventorybridge;using namespace scripting;
 if(!weaponsPossible||!equip||!allocate||!release||!hide||!convert||!displacedCallback||worlditems::published||change.row>=current.carried.size()||change.hand>1||!queuesIdle())return false;
 uintptr_t sector=0;std::array<float,3> position{};if(!worlditems::actor(owner,sector,position))return false;
 const auto row=current.carried[change.row];if(!worlditems::ordinary(row.item,row.state)||referenced(current,row.item))return false;
 const CharacterMemory m{copyWorldMemory};const auto weapon=chunk(row.item,0x4000);uint32_t size=0,flags=0;
 if(!weapon||!m.get(weapon+4,size)||size<0x50||!m.get(weapon+8,flags)||(flags&0x30)==0||chunk(row.item,0x8000)||chunk(row.item,0x100))return false;
 for(uint32_t id:{row.item,current.weapons[0],current.weapons[2]})if(id){size_t copies=0;for(const auto& carried:current.carried)copies+=carried.item==id;if(copies!=1)return false;}
 for(unsigned hand=0;hand<2;++hand){uintptr_t part=0;if(!m.get(owner.player+fp::game::character::handParts+hand*8,part)||!part||!readable(part,0x20))return false;}
 if(change.hand==1){
  if((flags&0x30)!=0x10||!current.weapons[0])return false;
  const auto primary=chunk(current.weapons[0],0x4000);uint32_t primaryFlags=0;if(!primary||!m.get(primary+8,primaryFlags)||(primaryFlags&0x30)!=0x10||chunk(current.weapons[0],0x100))return false;
  uint16_t rating=0;if(!m.get(owner.stats+0xc0,rating))return false;const unsigned count=rating/5000;if(count>13)return false;
  bool dual=false;for(unsigned i=0;i<count;++i){uint8_t skill=0;if(!m.get(owner.stats+0xc2+i,skill))return false;dual|=skill==8;}if(!dual)return false;
 }
 uintptr_t db=0,library=0;if(!m.get(base+0x4ef930,db)||!db||!m.get(owner.world+0x1db8,library)||!library)return false;
 worlditems::published=true;uint32_t actual=0;const auto result=equipCall(owner,row,change.hand,actual);if(result<0)return fault();if(!result)return false;
 auto expected=current.carried;expected[change.row].item=actual;
 if(!assignRow(owner.stats,change.row,expected[change.row])||!hideCall(owner.stats,actual))return fault();expected[change.row].state|=0x100;
 InventoryView after;if(!rows(owner.stats,after)||after.carried!=expected||after.worn!=current.worn||!finish(owner.stats,owner.player,row.item,false)||
  (actual!=row.item&&!finish(owner.stats,owner.player,actual,false)))return fault();
 target={};host.report("runtime","info","Native hand equip queued; the engine owns displacement and completion");return true;
}
static bool unhideCall(uintptr_t stats,uint32_t item){__try{unhide(stats+0xf0,item,1);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool uniqueCarried(const scripting::InventoryView& view,uint32_t id){size_t count=0;for(const auto& row:view.carried)count+=row.item==id;return count==1;}
static bool held(const inventorybridge::Target& owner,const scripting::InventoryView& current,unsigned hand){
 const scripting::CharacterMemory m{copyWorldMemory};int32_t hands=0,count=0;uintptr_t body=0,node=0;uint32_t id=0;
 if(!(hand<2&&current.weapons[hand*2]&&m.get(owner.player+0xe70,hands)&&hands>int(hand)&&hands<=2&&
  m.get(owner.player+fp::game::character::heldItems+hand*8,body)&&body&&readable(body,0x440)&&m.get(body+0x18,node)&&node&&
  m.get(node+0x180,id)&&id==current.weapons[hand*2]&&m.get(owner.player+0x920,count)&&count>=0&&count<=2))return false;
 bool owned=false;for(int i=0;i<count;++i){uintptr_t item=0;if(!m.get(owner.player+0x928+size_t(i)*8,item)||!item||!readable(item,0x440))return false;owned|=item==body;}
 for(unsigned offset:{0xbf8u,0xc00u}){uintptr_t item=0;if(!m.get(owner.player+offset,item))return false;owned|=item==body;}return owned;
}
struct Removal {alignas(uintptr_t) uint8_t action[0x48];uintptr_t world,scene,player,stats,body;uint32_t item,hand;};
static char removeQueued(void* pointer){
 __try{
  const auto packet=(Removal*)pointer;const scripting::CharacterMemory m{copyWorldMemory};uintptr_t world=0,scene=0,player=0,stats=0,body=0;uint32_t id=0;
  if(!m.get(base+npc::off::world,world)||world!=packet->world||!m.get(base+0x49b300,scene)||scene!=packet->scene||
   !m.get(base+npc::off::controlled,player)||player!=packet->player||!m.get(player+0x10a0,stats)||stats!=packet->stats||
   !m.get(player+fp::game::character::heldItems+packet->hand*8,body)||body!=packet->body||!m.get(stats+0x64+packet->hand*8,id)||id!=packet->item)actionFault.store(true);
  else removeAction(packet->action);
  release(pointer);
 }__except(EXCEPTION_EXECUTE_HANDLER){actionFault.store(true);}
 // This callback can outlive Lua; it calls no host, logger or addon state.
 return 0;
}
static bool removeCall(const inventorybridge::Target& owner,unsigned hand){
 __try{
  void* memory=nullptr;allocate(&memory,sizeof(Removal));if(!memory)return false;std::memset(memory,0,sizeof(Removal));auto packet=(Removal*)memory;
  *(uint32_t*)packet->action=7;*(uintptr_t*)(packet->action+8)=owner.player;*(unsigned*)(packet->action+0x3c)=hand;packet->action[0x40]=packet->action[0x41]=1;
  packet->world=owner.world;packet->scene=owner.scene;packet->player=owner.player;packet->stats=owner.stats;
  packet->hand=hand;packet->item=owner.view.weapons[hand*2];packet->body=*(uintptr_t*)(owner.player+fp::game::character::heldItems+hand*8);
  enqueue(base+0x49b9d0,removeQueued,packet);return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool commitUnequip(const inventorybridge::Target& owner,const scripting::InventoryView& current,const scripting::InventoryChange& change){
 using namespace inventorybridge;using namespace scripting;
 if(!unequipPossible||!allocate||!release||!enqueue||!removeAction||!unhide||worlditems::published||change.hand>1||!queuesIdle()||!held(owner,current,change.hand))return false;
 if(current.weapons[0]&&current.weapons[0]==current.weapons[2])return false;
 uintptr_t sector=0;std::array<float,3> position{};if(!worlditems::actor(owner,sector,position))return false;
 const auto id=current.weapons[change.hand*2];if(!uniqueCarried(current,id)||chunk(id,0x100))return false;
 for(uint32_t other:{current.weapons[0],current.weapons[2]})if(other&&!uniqueCarried(current,other))return false;
 auto expected=current.carried;bool found=false;for(auto& row:expected)if(row.item==id){if(!(row.state&0x100)||!worlditems::ordinary(id,row.state&~0x100u))return false;row.state&=~0x100u;found=true;}
 if(!found)return false;worlditems::published=true;
 if(!removeCall(owner,change.hand)||!unhideCall(owner.stats,id))return fault();
 InventoryView after;if(!rows(owner.stats,after)||after.carried!=expected||after.worn!=current.worn||!finish(owner.stats,owner.player,id,false))return fault();
 target={};return true;
}
static int shieldCall(const inventorybridge::Target& owner,const scripting::InventoryRow& row,uint32_t& actual){
 __try{actual=convert(row.item,*(uintptr_t*)(base+0x4ef930));if(!actual||(actual&0xc0000000u)==0xc0000000u)return -1;return shield(owner.player,actual,row.state,0)?1:0;}
 __except(EXCEPTION_EXECUTE_HANDLER){return -1;}
}
static bool commitShield(const inventorybridge::Target& owner,const scripting::InventoryView& current,const scripting::InventoryChange& change){
 using namespace inventorybridge;using namespace scripting;
 if(!shieldsPossible||!shield||!convert||!hide||!unhide||worlditems::published||change.row>=current.carried.size()||!queuesIdle())return false;
 uintptr_t sector=0;std::array<float,3> position{};if(!worlditems::actor(owner,sector,position))return false;
 const auto row=current.carried[change.row];if(!worlditems::ordinary(row.item,row.state)||referenced(current,row.item)||!uniqueCarried(current,row.item)||chunk(row.item,0x4000))return false;
 const CharacterMemory m{copyWorldMemory};auto data=chunk(row.item,0x8000);uint32_t size=0,flags=0;
 if(!data||!m.get(data+4,size)||size<0x10||!m.get(data+8,flags))return false;
 if(flags&0x2000000){uintptr_t archetype=0;uint32_t skills=0;if(!m.get(owner.player+0xaf8,archetype)||!archetype||!m.get(archetype+0x58,skills)||!(skills&0x10000))return false;}
 if(current.weapons[0]){data=chunk(current.weapons[0],0x4000);if(!data||!m.get(data+8,flags)||(flags&0x30)!=0x10||chunk(current.weapons[0],0x100))return false;}
 uintptr_t part=0,node=0,library=0,db=0;
 if(!m.get(owner.player+fp::game::character::handParts+8,part)||!part||!m.get(part+0x18,node)||!node||!readable(node,0x198)||
  !m.get(owner.world+0x1db8,library)||!library||!m.get(base+0x4ef930,db)||!db)return false;
 const auto previous=current.weapons[2];auto expected=current.carried;
 if(previous){
  if(previous==current.weapons[0]||!uniqueCarried(current,previous)||!held(owner,current,1)||chunk(previous,0x100))return false;
  bool hidden=false;for(auto& item:expected)if(item.item==previous){hidden=(item.state&0x100)!=0;item.state&=~0x100u;}if(!hidden)return false;
 }else{uintptr_t heldBody=0;if(!m.get(owner.player+fp::game::character::heldItems+8,heldBody)||heldBody)return false;}
 worlditems::published=true;uint32_t actual=0;const int result=shieldCall(owner,row,actual);if(result<0)return fault();if(!result)return false;
 expected[change.row].item=actual;expected[change.row].state|=0x100;
 if(!assignRow(owner.stats,change.row,{actual,row.state,row.x,row.y})||!hideCall(owner.stats,actual)||(previous&&!unhideCall(owner.stats,previous)))return fault();
 InventoryView after;if(!rows(owner.stats,after)||after.carried!=expected||after.worn!=current.worn||!finish(owner.stats,owner.player,row.item,false)||
  (actual!=row.item&&!finish(owner.stats,owner.player,actual,false)))return fault();
 target={};return true;
}
static bool commit(const inventorybridge::Target& owner,const scripting::InventoryView& current,const scripting::InventoryChange& change){
 if(change.op==scripting::InventoryOp::unequipWeapon)return commitUnequip(owner,current,change);
 if(change.op==scripting::InventoryOp::equipShield)return commitShield(owner,current,change);
 if(change.op==scripting::InventoryOp::equipWeapon)return commitWeapon(owner,current,change);
 if(change.op==scripting::InventoryOp::bagTake||change.op==scripting::InventoryOp::bagStore||change.op==scripting::InventoryOp::bagTransfer)return commitBag(owner,current,change);
 return worlditems::commit(owner,current,change);
}
}
