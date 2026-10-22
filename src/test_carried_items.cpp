#include "plugin.cpp"
#include <cstdio>
#include "test_assert.h"
template<class T>static void put(uintptr_t at,T value){std::memcpy((void*)at,&value,sizeof value);}
struct Database {
 std::vector<uint8_t> header=std::vector<uint8_t>(0x40),body;std::vector<std::array<uint32_t,4>> table;std::map<uint32_t,std::vector<uint8_t>> records;
 uintptr_t at(){return (uintptr_t)header.data();}
 void sync(){body.clear();table.clear();for(const auto& pair:records){table.push_back({pair.first,0,uint32_t(body.size()),uint32_t(pair.second.size())});body.insert(body.end(),pair.second.begin(),pair.second.end());}
  put(at()+8,int32_t(table.size()));put(at()+0x10,(uintptr_t)table.data());put(at()+0x18,(uintptr_t)body.data());put(at()+0x20,uint32_t(body.size()));}
};
static Database level,shared,persistent;static uintptr_t player=0,stats=0;static std::vector<scripting::InventoryRow> carried;
static std::vector<uint8_t> look(0x30),weapon(0x60),metadata(0x20),shieldRecord(0x20);static std::vector<void*> allocations;static void* action=nullptr;
static uint32_t nextClone=8;static int failPublicationAt=0;
static bool nativeRefuse=false,publicationFault=false,equipFault=false,cloneFault=false;static int equips=0,clones=0,publishes=0;static uint32_t chosen=0;static unsigned selectedHand=0;
static void sync(){put(stats+0xf0,int32_t(carried.size()));put(stats+0xf8,(uintptr_t)carried.data());}
static Database& database(uintptr_t db){assert(db==level.at()||db==shared.at()||db==persistent.at());return db==level.at()?level:db==shared.at()?shared:persistent;}
static int findFixture(uintptr_t db,int id){auto& d=database(db);for(size_t i=0;i<d.table.size();++i)if(d.table[i][0]==uint32_t(id))return int(i);return -1;}
static char openFixture(int id){return id==int(0x80000009u);}
static uintptr_t chunkFixture(uint32_t id,uint32_t tag){
 if(tag==0x100000)return (uintptr_t)look.data();
 if(tag==0x10000000)return (uintptr_t)metadata.data();
 if(tag==0x8000&&id==45)return (uintptr_t)shieldRecord.data();
 if(tag==0x4000&&(id==41||id==42||id==43))return (uintptr_t)weapon.data();
 if(tag==0x20000&&(id==7||id==0x80000007u||id==0x80000008u||id==0x80000009u||id==0x40000007u))return 1;
 return 0;
}
static void publishFixture(uintptr_t db,unsigned slot,void* pointer){
 ++publishes;if(publicationFault||(failPublicationAt&&publishes==failPublicationAt))RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);auto& d=database(db);assert(slot<d.table.size());const auto id=d.table[slot][0];
 struct Buffer{const uint8_t* data;uint32_t size,pad;};const auto& value=*(Buffer*)pointer;d.records[id].assign(value.data,value.data+value.size);d.sync();
}
static uint32_t cloneFixture(uint32_t id,uintptr_t db){assert(id==7&&db==level.at());++clones;const auto slot=nextClone++;level.records[slot]=shared.records[7];level.records[slot][24]=0x7b;if(cloneFault)level.records[slot][36]^=1;level.sync();return 0x80000000u|slot;}
static void appendFixture(uintptr_t at,uint32_t id,uint32_t state){assert(at==stats+0xf0);carried.resize(size_t(*(int*)at));carried.push_back({id,state,40,4});sync();}
static void notifyFixture(uintptr_t at){assert(at==stats);}
static void recountFixture(uintptr_t at,uint32_t,int n){assert(at==stats&&n==0);}
static uint32_t convertFixture(uint32_t id,uintptr_t db){assert(db==level.at());return id;}
static void allocateFixture(void** out,size_t bytes){*out=std::calloc(1,bytes);allocations.push_back(*out);}
static void freeFixture(void* ptr){auto i=std::find(allocations.begin(),allocations.end(),ptr);assert(i!=allocations.end());allocations.erase(i);std::free(ptr);}
static void hideFixture(uintptr_t at,uint32_t id,int count){assert(at==stats+0xf0&&count==1);for(auto& row:carried)if(row.item==id&&!(row.state&0x100)){row.state|=0x100;return;}assert(false);}
static char equipFixture(uintptr_t owner,uint32_t id,uint32_t,unsigned hand,void* payload){
 assert(owner==player&&hand<2);if(nativeRefuse)return 0;++equips;action=payload;chosen=id;selectedHand=hand;
 if(equipFault)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
 auto* a=(uint8_t*)action;assert(a[0x40]==1&&*(uintptr_t*)(a+0x20)==0);auto* callback=*(uint8_t**)(a+0x30);
 assert(*(int*)callback==1&&*(uintptr_t*)(callback+0x10)==carrieditems::displacedCallback&&*(uintptr_t*)(callback+0x20)==stats+0xf0);
 put(base+0x49b9d0+4,uint32_t(1));return 1;
}
static void complete(){
 assert(action);const auto previous=*(uint32_t*)(stats+0x64+selectedHand*8);
 for(auto& row:carried)if(row.item==previous&&(row.state&0x100)){row.state&=~0x100u;break;}
 put(stats+0x64+selectedHand*8,chosen);auto* callback=*(void**)((uint8_t*)action+0x30);freeFixture(callback);freeFixture(action);action=nullptr;
 put(base+0x49b9d0,uint32_t(0));put(base+0x49b9d0+4,uint32_t(0));
}
static void pulse(uint64_t now){worldPulse=true;beginAiPulse("session",now);inventorybridge::target={};worlditems::beginPulse();carrieditems::bags.clear();}
static void tick(uint64_t now){pulse(now);host.setTime(now);host.runTimers();worldPulse=false;}
static bool load(const char* id,const std::string& text,const char* caps="inventory.read,inventory.bags,weapons.equip,timers"){return host.load(id,text.data(),text.size(),caps);}
static std::vector<uint8_t> bagBytes(){
 std::vector<uint8_t> out(64);auto p=(uintptr_t)out.data();put(p,uint32_t(0x20000));put(p+4,uint32_t(56));put(p+16,uint16_t(80));put(p+18,uint16_t(30));put(p+28,uint32_t(2));
 scripting::InventoryRow entries[2]={{44,0x20000,17,19},{44,0x40000,23,29}};std::memcpy(out.data()+32,entries,24);put(p+56,uint32_t(0x123));put(p+60,uint32_t(8));return out;
}
static std::array<std::vector<uint8_t>,2> heldBodies{std::vector<uint8_t>(0x500),std::vector<uint8_t>(0x500)},heldNodes{std::vector<uint8_t>(0x210),std::vector<uint8_t>(0x210)};
static char(*queuedRemoval)(void*)=nullptr;static void* removalPayload=nullptr;static bool removalFault=false,shieldFault=false,shieldRefuse=false,shieldPending=false;static int removals=0,shields=0;
static void syncHeld(){
 put(player+0xe70,int32_t(2));int count=0;put(player+0xbf8,uintptr_t(0));put(player+0xc00,uintptr_t(0));
 for(size_t hand=0;hand<2;++hand){const auto id=*(uint32_t*)(stats+0x64+hand*8);const auto body=(uintptr_t)heldBodies[hand].data(),node=(uintptr_t)heldNodes[hand].data();
  put(body+0x18,node);put(node+0x180,id);put(player+fp::game::character::heldItems+hand*8,id?body:uintptr_t(0));
  if(id==45)put(player+0xbf8,body);else if(id)put(player+0x928+size_t(count++)*8,body);}
 put(player+0x920,int32_t(count));
}
static void unhideFixture(uintptr_t at,uint32_t id,int count){assert(at==stats+0xf0&&count==1);for(auto& row:carried)if(row.item==id&&(row.state&0x100)){row.state&=~0x100u;return;}assert(false);}
static void queueFixture(uintptr_t queue,char(*callback)(void*),void* payload){assert(queue==base+0x49b9d0);queuedRemoval=callback;removalPayload=payload;put(queue+4,uint32_t(1));}
static char removeActionFixture(void* pointer){
 if(removalFault)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
 auto* payload=(uint8_t*)pointer;assert(*(uint32_t*)payload==7&&*(uintptr_t*)(payload+8)==player&&payload[0x40]==1&&payload[0x41]==1&&*(uintptr_t*)(payload+0x30)==0);
 const auto hand=*(unsigned*)(payload+0x3c);assert(hand<2);++removals;put(stats+0x64+hand*8,uint32_t(0));
 if(!hand&&*(uint32_t*)(stats+0x6c)){put(stats+0x64,*(uint64_t*)(stats+0x6c));put(stats+0x6c,uint64_t(0));}
 syncHeld();return 0;
}
static void completeRemoval(){assert(queuedRemoval&&removalPayload);auto callback=queuedRemoval;auto payload=removalPayload;queuedRemoval=nullptr;removalPayload=nullptr;assert(callback(payload)==0);put(base+0x49b9d0+4,uint32_t(0));}
static char shieldFixture(uintptr_t owner,uint32_t id,uint32_t state,uintptr_t ui){
 assert(owner==player&&id==45&&ui==0);if(shieldRefuse)return 0;if(shieldFault)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
 ++shields;shieldPending=true;chosen=id;put(base+0x49b9d0+4,uint32_t(1));put((uintptr_t)shieldRecord.data()+0x10,state);return 1;
}
static void completeShield(){assert(shieldPending);shieldPending=false;put(stats+0x6c,chosen);put(stats+0x70,*(uint32_t*)(shieldRecord.data()+0x10));put(base+0x49b9d0+4,uint32_t(0));syncHeld();}
static void advanced(const std::string& mode){
 using namespace scripting;
 host.clear();carrieditems::unequipPossible=carrieditems::shieldsPossible=true;carrieditems::unhide=unhideFixture;carrieditems::removeAction=removeActionFixture;carrieditems::enqueue=queueFixture;carrieditems::shield=shieldFixture;
 host.capabilities(inventoryRead|inventoryBags|weaponEquip|weaponUnequip|shieldEquip|timerEvents|lifecycleEvents);
 const auto add=[](const char* id,const std::string& text){return load(id,text,"inventory.read,inventory.bags,weapons.equip,weapons.unequip,shields.equip,timers");};
 put((uintptr_t)shieldRecord.data()+4,uint32_t(shieldRecord.size()));
 carried={{41,0x100,1,2},{43,0x100,3,4},{45,0x20000,5,6}};sync();put(stats+0x64,uint32_t(41));put(stats+0x6c,uint32_t(43));syncHeld();
 const auto leftPart=*(uintptr_t*)(player+fp::game::character::handParts+8);put(leftPart+0x18,(uintptr_t)heldNodes[1].data());
 assert(add("unequip-rollback","timers.after(0,function()inventory.snapshot().weapons[1]:unequip();error('discard')end)"));tick(22001);assert(!queuedRemoval&&removals==0&&carried[0].state==0x100);
 assert(add("shield-refuse","timers.after(0,function()inventory.snapshot().carried[3]:equip_shield()end)"));shieldRefuse=true;tick(22002);assert(shields==0&&carried[1].state==0x100&&carried[2].state==0x20000);shieldRefuse=false;
 assert(load("shield","timers.after(0,function()inventory.snapshot().carried[3]:equip_shield()end)","inventory.read,shields.equip,timers"));tick(22003);assert(shields==1&&carried[1].state==0&&carried[2].state==0x20100&&*(uint32_t*)(stats+0x6c)==43);
 pulse(22004);assert(!inventorybridge::capture());host.clear();completeShield();assert(*(uint32_t*)(stats+0x6c)==45&&*(uint32_t*)(stats+0x70)==0x20000);
 assert(load("unequip-shield","timers.after(0,function()inventory.snapshot().weapons[2]:unequip()end)","inventory.read,weapons.unequip,timers"));tick(22005);assert(queuedRemoval&&carried[2].state==0x20000&&allocations.size()==1);host.clear();completeRemoval();assert(removals==1&&*(uint32_t*)(stats+0x6c)==0&&allocations.empty());
 put(stats+0x6c,uint32_t(43));carried[1].state=0x100;syncHeld();assert(add("unequip-primary","timers.after(0,function()inventory.snapshot().weapons[1]:unequip()end)"));tick(22006);assert(queuedRemoval&&carried[0].state==0);completeRemoval();assert(removals==2&&*(uint32_t*)(stats+0x64)==43&&*(uint32_t*)(stats+0x6c)==0&&carried[1].state==0x100);
 assert(load("new-denied","timers.after(0,function()local i=inventory.snapshot();assert(i.weapons[1].unequip==nil and i.carried[1].equip_shield==nil and i.carried[1].transfer_to==nil)end)","inventory.read,timers"));tick(22007);assert(host.enabled("new-denied"));
 assert(add("unequip-stale","timers.after(0,function()local h=inventory.snapshot().weapons[1];timers.after(0,function()h:unequip()end)end)"));tick(22008);tick(22009);assert(!host.enabled("unequip-stale")&&removals==2);
 if(mode=="remove-fault"||mode=="remove-transition"){
  assert(add("fault","timers.after(0,function()inventory.snapshot().weapons[1]:unequip()end)"));tick(22010);assert(queuedRemoval);
  if(mode=="remove-fault")removalFault=true;else put(base+0x49b300,uintptr_t(0));
  completeRemoval();assert(carrieditems::actionFault.load());assert(!carrieditems::checkFault()&&inventorybridge::broken&&aiWriteBroken);
  for(void* pointer:allocations)std::free(pointer);std::puts("LUA_QUEUED_REMOVAL_FAULT_PASSED");return;
 }
 if(mode=="shield-fault"){
  shieldFault=true;assert(add("fault","timers.after(0,function()inventory.snapshot().carried[3]:equip_shield()end)"));tick(22010);assert(inventorybridge::broken&&aiWriteBroken);std::puts("LUA_SHIELD_FAULT_PASSED");return;
 }
 put(stats+0x64,uint32_t(0));put(stats+0x6c,uint32_t(0));syncHeld();carried={{0x80000007u,0,1,2},{0x40000007u,0,3,4}};sync();level.records[7]=bagBytes();persistent.records[7]=bagBytes();level.sync();persistent.sync();const auto before=carried;
 if(mode=="bag-second-fault"){
  failPublicationAt=publishes+2;assert(add("fault","timers.after(0,function()local i=inventory.snapshot();i.carried[1]:transfer_to(2,i.carried[2])end)"));tick(22011);
  std::vector<InventoryRow> rows;size_t at=0;assert(containerRows(level.records[7],rows,at)&&rows.size()==2);assert(containerRows(persistent.records[7],rows,at)&&rows.size()==3);
  assert(inventorybridge::broken&&aiWriteBroken&&carried==before);std::puts("LUA_BAG_SECOND_PUBLICATION_FAULT_PASSED");return;
 }
 auto publicationCount=publishes;
 put((uintptr_t)persistent.records[7].data()+16,uint16_t(20));persistent.sync();assert(add("bag-capacity","timers.after(0,function()local i=inventory.snapshot();i.carried[1]:transfer_to(2,i.carried[2])end)"));tick(22010);assert(publishes==publicationCount);put((uintptr_t)persistent.records[7].data()+16,uint16_t(80));persistent.sync();
 assert(add("bag-rollback","timers.after(0,function()local i=inventory.snapshot();i.carried[1]:transfer_to(2,i.carried[2]);error('discard')end)"));tick(22011);assert(publishes==publicationCount);
 assert(add("bag-direct","timers.after(0,function()local i=inventory.snapshot();assert(i.carried[1]:transfer_to(2,i.carried[2]))end)"));tick(22012);assert(publishes==publicationCount+2&&carried==before);
 std::vector<InventoryRow> rows;size_t at=0;assert(containerRows(level.records[7],rows,at)&&rows.size()==1&&rows[0].state==0x20000);assert(containerRows(persistent.records[7],rows,at)&&rows.size()==3&&rows[2].state==0x40000&&rows[2].x==23&&rows[2].y==29);
 publicationCount=publishes;assert(add("bag-self","timers.after(0,function()local i=inventory.snapshot();i.carried[1]:transfer_to(1,i.carried[1])end)"));tick(22013);assert(publishes==publicationCount);
 pulse(22014);auto view=inventorybridge::capture();assert(view);const auto ticket=view->ticket;assert(carrieditems::readBag(ticket,0)&&carrieditems::readBag(ticket,1));InventoryChange change;change.op=InventoryOp::bagTransfer;change.inventory=ticket;change.bag=0;change.destinationBag=1;
 persistent.records[7][8]^=1;persistent.sync();assert(!inventorybridge::commit(change)&&publishes==publicationCount);persistent.records[7][8]^=1;persistent.sync();
 carried={{7,0,1,2},{7,0,3,4}};sync();nextClone=100;const auto sharedBefore=shared.records[7];assert(add("shared-direct","timers.after(0,function()local i=inventory.snapshot();i.carried[1]:transfer_to(2,i.carried[2])end)"));tick(22015);
 assert(carried[0].item==0x80000064u&&carried[1].item==0x80000065u&&shared.records[7]==sharedBefore);assert(containerRows(level.records[100],rows,at)&&rows.size()==1);assert(containerRows(level.records[101],rows,at)&&rows.size()==3&&rows[2].state==0x40000);
 host.clear();host.setTime(30000);std::ifstream example("inventory-tools.lua",std::ios::binary);assert(example);const std::string source{std::istreambuf_iterator<char>(example),std::istreambuf_iterator<char>()};
 std::vector<Module> config={{"config","return {enabled=true,action='bag_transfer',bag_item_id=2147483748,destination_bag_item_id=2147483749,item_id=44}"}};
 const auto exampleLoad=[&](){return host.load("advanced-example",source.data(),source.size(),"inventory.read,timers,events.lifecycle",8*1024*1024,config,"inventory.bags,weapons.unequip,shields.equip");};
 publicationCount=publishes;assert(exampleLoad());host.notifyPhase("session",5);tick(33501);assert(publishes==publicationCount+2&&host.enabled("advanced-example"));
 carried={{41,0x100},{42,0x100},{45,0x20000}};sync();put(stats+0x64,uint32_t(41));put(stats+0x6c,uint32_t(42));syncHeld();
 config[0].source="return {enabled=true,action='equip_shield',item_id=45}";assert(exampleLoad());host.notifyPhase("session",6);tick(37002);assert(shields==2&&host.enabled("advanced-example"));completeShield();
 config[0].source="return {enabled=true,action='unequip_weapon',hand='left'}";assert(exampleLoad());host.notifyPhase("session",7);tick(40503);assert(queuedRemoval&&host.enabled("advanced-example"));completeRemoval();assert(removals==3&&allocations.empty());
 std::puts("LUA_EQUIPMENT_AND_BAG_TRANSFERS_PASSED");host.clear();
}
int main(int argc,char** argv){
 using namespace scripting;std::vector<uint8_t> image(0x600000),scene(0x1e80),actor(0x1400),actorNode(0x210),actorStats(0x150),behavior(0x8a0),manager(0x100),drag(0x30),hand0(0x30),hand1(0x30),library(0x30);
 base=(uintptr_t)image.data();const auto world=(uintptr_t)scene.data();player=(uintptr_t)actor.data();stats=(uintptr_t)actorStats.data();
 put(base+npc::off::world,world);put(base+npc::off::controlled,player);put(base+0x49b300,world);std::memcpy((void*)(world+0x140),"exanima01",9);put(world+0x220,world);put(world+0x1db8,(uintptr_t)library.data());
 put(player+0x10a0,stats);put(player+0x18,(uintptr_t)actorNode.data());put((uintptr_t)actorNode.data()+0x130,world);put(player+0xd40,(uintptr_t)behavior.data());put((uintptr_t)behavior.data()+fp::game::behaviour::view,world);
 put(player+fp::game::character::handParts,(uintptr_t)hand0.data());put(player+fp::game::character::handParts+8,(uintptr_t)hand1.data());
 put(base+0x49a960,(uintptr_t)manager.data());put(base+0x4efdd0,(uintptr_t)drag.data());put(base+0x2b7290,int32_t(100));
 level.records[7]=shared.records[7]=persistent.records[7]=bagBytes();level.sync();shared.sync();persistent.sync();put(base+0x4ef930,level.at());put(base+0x4ef920,shared.at());put(base+0x4ef940,persistent.at());
 put((uintptr_t)look.data()+4,uint32_t(look.size()));put((uintptr_t)look.data()+0x24,uint8_t(20));put((uintptr_t)weapon.data()+4,uint32_t(weapon.size()));put((uintptr_t)weapon.data()+8,uint32_t(0x10));put((uintptr_t)metadata.data()+4,uint32_t(metadata.size()));
 inventorybridge::possible=inventoryWritePossible=carrieditems::weaponsPossible=carrieditems::bagsPossible=true;gamebridge::backupDone=true;
 inventorybridge::append=appendFixture;inventorybridge::notify=notifyFixture;inventorybridge::recount=recountFixture;inventorybridge::extraCommit=carrieditems::commit;inventorybridge::extraReady=carrieditems::queuesIdle;
 containerbridge::find=findFixture;containerbridge::replace=publishFixture;containerbridge::open=openFixture;containerbridge::chunk=chunkFixture;
 carrieditems::clone=cloneFixture;carrieditems::convert=convertFixture;carrieditems::allocate=allocateFixture;carrieditems::release=freeFixture;carrieditems::equip=equipFixture;carrieditems::hide=hideFixture;carrieditems::displacedCallback=base+0x24a950;
 host.capabilities(inventoryRead|inventoryBags|weaponEquip|timerEvents|lifecycleEvents);host.setInventoryApi(inventorybridge::capture,inventorybridge::commit);host.setBagRead(carrieditems::readBag);beginAiPulse("session",0);
 carried={{0x80000007u,0,9,10},{44,0x10000,21,22}};sync();
 assert(load("rollback","timers.after(0,function()local i=inventory.snapshot();i.carried[2]:store_in(i.carried[1]);error('discard')end)"));tick(3001);assert(publishes==0&&carried.size()==2);
 assert(load("store","timers.after(0,function()local i=inventory.snapshot();assert(#i.carried[1]:contents()==2);assert(i.carried[2]:store_in(i.carried[1]))end)"));tick(3002);assert(publishes==1&&*(int*)(stats+0xf0)==1);carried.resize(1);sync();
 std::vector<InventoryRow> entries;size_t offset=0;assert(containerRows(level.records[7],entries,offset)&&entries.size()==3&&entries[2].state==0x10000&&entries[2].x==21&&entries[2].y==22);
 assert(load("take","timers.after(0,function()local b=inventory.snapshot().carried[1];assert(b:contents()[2].state==0x40000);b:take_item(2)end)"));tick(3003);assert(carried.size()==2&&carried[1].item==44&&carried[1].state==0x40000&&carried[1].x==40);
 const auto originalShared=shared.records[7];carried={{7,0,1,2},{7,0,3,4},{44,0,5,6}};sync();
 assert(load("clone","timers.after(0,function()local i=inventory.snapshot();i.carried[3]:store_in(i.carried[2])end)"));tick(3004);carried.resize(size_t(*(int*)(stats+0xf0)));assert(clones==1&&shared.records[7]==originalShared&&carried[0].item==7&&carried[1].item==0x80000008u&&level.records[8][24]==0x7b);
 carried={{0x40000007u,0,1,2},{44,0,3,4}};sync();assert(load("persistent","timers.after(0,function()local i=inventory.snapshot();i.carried[2]:store_in(i.carried[1])end)"));tick(3005);assert(publishes==4);carried.resize(1);sync();
 assert(load("expired","timers.after(0,function()local b=inventory.snapshot().carried[1];timers.after(0,function()b:take_item(1)end)end)"));tick(3006);tick(3007);assert(!host.enabled("expired")&&publishes==4);
 assert(load("denied","timers.after(0,function()local i=inventory.snapshot();assert(i.carried[1].contents==nil and i.carried[1].take_item==nil and i.carried[1].equip_weapon==nil)end)","inventory.read,timers"));tick(3008);assert(host.enabled("denied"));
 assert(load("self","timers.after(0,function()local i=inventory.snapshot();i.carried[1]:store_in(i.carried[1])end)"));tick(3009);assert(publishes==4);
 pulse(3010);auto view=inventorybridge::capture();assert(view);assert(carrieditems::readBag(view->ticket,0));InventoryChange change;change.op=InventoryOp::bagTake;change.inventory=view->ticket;change.bag=0;change.row=1;
 persistent.records[7][8]^=1;persistent.sync();assert(!inventorybridge::commit(change)&&publishes==4);persistent.records[7][8]^=1;persistent.sync();
 carried={{0x80000009u,0},{44,0}};sync();pulse(3011);view=inventorybridge::capture();assert(view&&!carrieditems::readBag(view->ticket,0));
 carried={{41,0,11,12},{42,0x100,13,14},{43,0,15,16}};sync();put(stats+0x64,uint32_t(42));
 assert(load("right","timers.after(0,function()inventory.snapshot().carried[1]:equip_weapon('right')end)"));tick(3012);assert(equips==1&&chosen==41&&selectedHand==0&&allocations.size()==2&&(carried[0].state&0x100)&&*(uint32_t*)(stats+0x64)==42);
 pulse(3013);assert(!inventorybridge::capture());host.clear();complete();assert(allocations.empty()&&carried[1].state==0&&*(uint32_t*)(stats+0x64)==41);
 assert(load("left-no-skill","timers.after(0,function()inventory.snapshot().carried[3]:equip_weapon('left')end)"));tick(3014);assert(equips==1);
 put(stats+0xc0,uint16_t(5000));put(stats+0xc2,uint8_t(8));assert(load("left","timers.after(0,function()inventory.snapshot().carried[3]:equip_weapon('left')end)"));tick(3015);assert(equips==2&&selectedHand==1&&chosen==43);complete();assert(*(uint32_t*)(stats+0x6c)==43&&*(uint32_t*)(stats+0x64)==41);
 assert(load("bad-hand","timers.after(0,function()inventory.snapshot().carried[2]:equip_weapon('feet')end)"));tick(3016);assert(!host.enabled("bad-hand")&&equips==2);
 carried.push_back({42,0x20000});sync();assert(load("duplicate","timers.after(0,function()inventory.snapshot().carried[2]:equip_weapon('right')end)"));tick(3017);assert(equips==2);carried.pop_back();sync();
 nativeRefuse=true;assert(load("refuse","timers.after(0,function()inventory.snapshot().carried[2]:equip_weapon('right')end)"));tick(3018);assert(equips==2&&allocations.empty()&&carried[1].state==0&&!inventorybridge::broken);nativeRefuse=false;
 carried={{0x80000007u,0},{44,0}};sync();put(stats+0x64,uint32_t(0));put(stats+0x6c,uint32_t(0));
 if(argc>1){
  if(std::string(argv[1])=="remove-fault"||std::string(argv[1])=="remove-transition"||std::string(argv[1])=="shield-fault"||std::string(argv[1])=="bag-second-fault"){advanced(argv[1]);return 0;}
  if(std::string(argv[1])=="equip-fault"){
   carried={{41,0}};sync();equipFault=true;assert(load("fault","timers.after(0,function()inventory.snapshot().carried[1]:equip_weapon('right')end)"));tick(3019);
   assert(inventorybridge::broken&&aiWriteBroken&&carried[0].state==0);for(void* pointer:allocations)std::free(pointer);std::puts("LUA_HAND_EQUIP_FAULT_PASSED");return 0;
  }
  if(std::string(argv[1])=="clone-fault"){
   carried[0].item=7;sync();cloneFault=true;assert(load("fault","timers.after(0,function()local i=inventory.snapshot();i.carried[2]:store_in(i.carried[1])end)"));tick(3019);
   assert(inventorybridge::broken&&aiWriteBroken&&carried[0].item==7&&shared.records[7]==originalShared);std::puts("LUA_BAG_CLONE_FAULT_PASSED");return 0;
  }
  publicationFault=true;assert(load("fault","timers.after(0,function()local i=inventory.snapshot();i.carried[2]:store_in(i.carried[1])end)"));tick(3019);assert(inventorybridge::broken&&aiWriteBroken);std::puts("LUA_BAG_PUBLICATION_FAULT_PASSED");return 0;
 }
 put((uintptr_t)look.data()+0x24,uint8_t(90));assert(load("capacity","timers.after(0,function()local i=inventory.snapshot();i.carried[2]:store_in(i.carried[1])end)"));tick(3019);assert(publishes==4&&!inventorybridge::broken);put((uintptr_t)look.data()+0x24,uint8_t(20));
 assert(load("nested","timers.after(0,function()local i=inventory.snapshot();i.carried[1]:store_in(i.carried[2])end)"));tick(3020);assert(publishes==4);
 host.clear();host.setTime(10000);std::ifstream example("inventory-tools.lua",std::ios::binary);assert(example);const std::string source{std::istreambuf_iterator<char>(example),std::istreambuf_iterator<char>()};
 std::vector<Module> config={{"config","return {enabled=true,action='bag_store',bag_item_id=2147483655,item_id=44}"}};
 const auto exampleLoad=[&](){return host.load("example",source.data(),source.size(),"inventory.read,timers,events.lifecycle",8*1024*1024,config,"inventory.bags,weapons.equip");};
 assert(exampleLoad());host.notifyPhase("session",1);tick(13501);assert(publishes==5&&host.enabled("example"));carried.resize(size_t(*(int*)(stats+0xf0)));sync();
 config[0].source="return {enabled=true,action='bag_take',bag_item_id=2147483655,item_id=44}";assert(exampleLoad());host.notifyPhase("session",2);tick(17002);assert(publishes==6&&host.enabled("example")&&carried.size()==2);
 carried.push_back({41,0});sync();config[0].source="return {enabled=true,action='equip_weapon',item_id=41,hand='right'}";assert(exampleLoad());host.notifyPhase("session",3);tick(20503);assert(equips==3&&chosen==41&&host.enabled("example"));complete();host.clear();
 std::puts("LUA_CARRIED_ITEMS_PASSED");advanced("");
}
