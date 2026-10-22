#include "plugin.cpp"
#include <cstdio>
#include "test_assert.h"
template<class T>static void put(uintptr_t at,T value){std::memcpy((void*)at,&value,sizeof value);}
static std::vector<uint8_t> bytes,stats(0x130),look(0x30),armor(0x230);static std::vector<scripting::InventoryRow> carried;static std::vector<std::pair<uint32_t,uint32_t>> worn;
static uintptr_t database=0,table=0;static bool openChest=false,failAppend=false,failPublish=false,fitRefuse=false;static int published=0,appended=0,rebuilt=0,notified=0;
static uintptr_t stat(){return (uintptr_t)stats.data();}
static void syncRecord(){put(database+0x18,(uintptr_t)bytes.data());put(database+0x20,uint32_t(bytes.size()));put(table+12,uint32_t(bytes.size()));}
static void sync(){syncRecord();put(stat()+0xf0,int32_t(carried.size()));put(stat()+0xf8,(uintptr_t)carried.data());put(stat()+0x84,int32_t(worn.size()));put(stat()+0x88,(uintptr_t)worn.data());}
static int findFixture(uintptr_t db,int id){return db==database&&id==7?0:-1;}
static char openFixture(int){return openChest;}
static uintptr_t chunkFixture(uint32_t item,uint32_t tag){if(item<41||item>43)return 0;return tag==0x100000?(uintptr_t)look.data():tag==0x1000&&item!=41?(uintptr_t)armor.data():0;}
static void publishFixture(uintptr_t db,unsigned index,void* value){assert(db==database&&index==0);++published;if(failPublish)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);struct Buffer{const uint8_t* data;uint32_t size,pad;};const auto& buffer=*(Buffer*)value;bytes.assign(buffer.data,buffer.data+buffer.size);syncRecord();}
static void appendFixture(uintptr_t at,uint32_t item,uint32_t state){assert(at==stat()+0xf0);++appended;if(failAppend)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);const int count=*(int*)at;carried.resize(size_t(count));carried.push_back({item,state,40,4});sync();}
static void wearFixture(uintptr_t at,uint32_t item,uint32_t state){assert(at==stat());worn.push_back({item,state});sync();}
static void unwearFixture(uintptr_t at,uint32_t item){assert(at==stat());for(auto i=worn.begin();i!=worn.end();++i)if(i->first==item){worn.erase(i);sync();return;}}
static int fitFixture(uintptr_t at,uintptr_t,void* value){assert(at==stat());if(fitRefuse)return -1;struct Callback{void(*call)(uintptr_t,uint32_t,uint32_t);uintptr_t context;};const auto c=*(Callback*)value;for(const auto& row:worn)c.call(c.context,row.first,row.second);worn.clear();sync();return 0;}
static void notifyFixture(uintptr_t at){assert(at==stat());++notified;}
static void recountFixture(uintptr_t at,uint32_t item,int initial){assert(at==stat()&&item>=41&&item<=43&&initial==0);}
static void refreshFixture(uintptr_t at){assert(at==stat());}
static void rebuildFixture(uintptr_t,uintptr_t at){assert(at==stat());++rebuilt;}
static bool load(const char* id,const std::string& source,const std::string& caps="world.read,inventory.read,inventory.transfer,equipment.write,timers"){return host.load(id,source.data(),source.size(),caps);}
static void tick(uint64_t now){worldPulse=true;beginAiPulse("session",now);containerbridge::targets.clear();inventorybridge::target={};worldCaptured=false;worldCopy={};host.setTime(now);host.runTimers();worldPulse=false;}
static void settle(uint64_t now){worldPulse=true;beginAiPulse("session",now);containerbridge::targets.clear();inventorybridge::target={};}
int main(){
 using namespace scripting;std::vector<uint8_t> image(0x600000),world(0x1c30),player(0x1400),node(0x190),db(0x40),drag(0x30),behavior(0x8a0),manager(0x100);uint32_t slots[4]={7,0,0,0};
 base=(uintptr_t)image.data();const auto w=(uintptr_t)world.data(),p=(uintptr_t)player.data(),n=(uintptr_t)node.data();database=(uintptr_t)db.data();table=(uintptr_t)slots;
 put(base+npc::off::world,w);put(base+npc::off::controlled,p);put(base+0x49b300,w);std::memcpy(world.data()+0x140,"exanima01",9);
 put(p+0x10a0,stat());put(p+0xd40,(uintptr_t)behavior.data());put((uintptr_t)behavior.data()+fp::game::behaviour::view,w);put(base+0x49a960,(uintptr_t)manager.data());put(base+0x4efdd0,(uintptr_t)drag.data());put(base+0x2b7290,int32_t(100));
 const uintptr_t child=n;put(w+0x118,int32_t(0));put(w+0x120,(uintptr_t)&child);put(n+8,uint32_t(0x4000));put(n+0x180,uint32_t(0x80000007));put(base+0x4ef930,database);put(database+8,int32_t(1));put(database+0x10,table);
 bytes.resize(64);put((uintptr_t)bytes.data(),uint32_t(0x20000));put((uintptr_t)bytes.data()+4,uint32_t(56));put((uintptr_t)bytes.data()+16,uint16_t(80));put((uintptr_t)bytes.data()+18,uint16_t(30));put((uintptr_t)bytes.data()+28,uint32_t(2));
 InventoryRow chest[2]={{41,0x20000,17,19},{41,0x40000,23,29}};std::memcpy(bytes.data()+32,chest,sizeof chest);put((uintptr_t)bytes.data()+56,uint32_t(0x123));put((uintptr_t)bytes.data()+60,uint32_t(8));const auto original=bytes;
 put((uintptr_t)look.data()+4,uint32_t(look.size()));put((uintptr_t)look.data()+0x24,uint8_t(20));put((uintptr_t)armor.data()+4,uint32_t(armor.size()));sync();
 containerbridge::find=findFixture;containerbridge::replace=publishFixture;containerbridge::open=openFixture;containerbridge::chunk=chunkFixture;
 inventorybridge::append=appendFixture;inventorybridge::wear=wearFixture;inventorybridge::unwear=unwearFixture;inventorybridge::fit=fitFixture;inventorybridge::notify=notifyFixture;inventorybridge::refresh=refreshFixture;inventorybridge::rebuild=rebuildFixture;
 inventorybridge::recount=recountFixture;
 worldPossible=containerWritePossible=inventoryWritePossible=inventorybridge::possible=inventorybridge::equipmentPossible=true;gamebridge::backupDone=true;
 host.capabilities(worldRead|containerEdit|inventoryRead|inventoryTransfer|equipmentWrite|timerEvents);host.setWorldProvider(captureWorld);host.setContainerApi(containerbridge::capture,containerbridge::read,containerbridge::commit);host.setInventoryApi(inventorybridge::capture,inventorybridge::commit);beginAiPulse("session",0);
 assert(load("rollback","timers.after(0,function()world.snapshot().objects[1]:take_item(2);error('discard')end)"));tick(3001);assert(published==0&&carried.empty()&&bytes==original);
 assert(load("take","timers.after(0,function()local c=world.snapshot().objects[1];assert(c:contents()[2].state==0x40000);assert(c:take_item(2));assert(not pcall(c.take_item,c,1))end)"));tick(3002);
 assert(published==1&&appended==1&&carried.size()==1&&carried[0].state==0x40000&&*(int*)(stat()+0xf0)==1&&notified==1);
 std::vector<InventoryRow> chestRows;size_t offset=0;assert(containerRows(bytes,chestRows,offset)&&chestRows.size()==1&&chestRows[0].state==0x20000);
 assert(load("store","timers.after(0,function()local i=inventory.snapshot();local again=inventory.snapshot();assert(#again.carried==1);assert(i.carried[1]:store_in(world.snapshot().objects[1]))end)"));tick(3003);
 assert(*(int*)(stat()+0xf0)==0&&published==2&&containerRows(bytes,chestRows,offset)&&chestRows.size()==2&&chestRows[1].state==0x40000);
 carried.clear();sync();
 assert(load("expired","timers.after(0,function()local c=world.snapshot().objects[1];timers.after(0,function()c:take_item(1)end)end)"));tick(3004);tick(3005);assert(!host.enabled("expired")&&published==2);
 assert(load("denied","timers.after(0,function()assert(world.snapshot().objects[1].take_item==nil);assert(inventory.snapshot().carried[1]==nil)end)","world.read,inventory.read,timers"));tick(3006);assert(host.enabled("denied"));
 openChest=true;assert(load("open","timers.after(0,function()assert(world.snapshot().objects[1].take_item==nil)end)"));tick(3007);assert(host.enabled("open"));openChest=false;
 carried={{42,0x20000,1,2}};worn={{43,0x40000}};sync();
 assert(load("equip","timers.after(0,function()assert(inventory.snapshot().carried[1]:equip())end)"));tick(3008);
 assert(rebuilt==1&&worn.size()==1&&worn[0].first==42&&worn[0].second==0x20000&&*(int*)(stat()+0xf0)==1);carried.resize(1);assert(carried[0].item==43&&carried[0].state==0x40000);
 assert(load("unequip","timers.after(0,function()assert(inventory.snapshot().worn[1]:unequip())end)"));tick(3009);assert(rebuilt==2&&worn.empty()&&carried.size()==2&&carried[1].item==42);
 fitRefuse=true;assert(load("fit-refuse","timers.after(0,function()inventory.snapshot().carried[2]:equip()end)"));tick(3010);assert(rebuilt==2&&!inventorybridge::broken);fitRefuse=false;
 settle(3011);auto view=inventorybridge::capture();assert(view);auto ticket=view->ticket;const auto c=containerbridge::capture(n);assert(c);
 put(stat()+0x64,uint32_t(43));assert(!inventorybridge::commit({InventoryOp::store,ticket,c,0}));put(stat()+0x64,uint32_t(0));
 put(base+0x499860,int32_t(1));assert(!inventorybridge::capture());put(base+0x499860,int32_t(0));
 put((uintptr_t)manager.data()+0x70,int32_t(0xfb01));assert(!inventorybridge::capture());put((uintptr_t)manager.data()+0x70,int32_t(0));
 put(p+0x12fe,uint8_t(1));assert(!inventorybridge::capture());put(p+0x12fe,uint8_t(0));
 put(base+0x5b3a64,int32_t(1));assert(!inventorybridge::capture());put(base+0x5b3a64,int32_t(0));
 settle(3012);view=inventorybridge::capture();ticket=view->ticket;const auto changed=containerbridge::capture(n);bytes[8]^=1;sync();assert(!inventorybridge::commit({InventoryOp::take,ticket,changed,0}));bytes[8]^=1;sync();
 carried[0].state^=2;assert(!inventorybridge::commit({InventoryOp::take,ticket,changed,0}));carried[0].state^=2;
 settle(3013);view=inventorybridge::capture();ticket=view->ticket;const auto full=containerbridge::capture(n);put((uintptr_t)look.data()+0x24,uint8_t(90));assert(!inventorybridge::commit({InventoryOp::store,ticket,full,0}));put((uintptr_t)look.data()+0x24,uint8_t(20));
 settle(3014);view=inventorybridge::capture();ticket=view->ticket;const auto invalid=containerbridge::capture(n);
 assert(!inventorybridge::commit({InventoryOp::take,ticket,invalid,4096}));gamebridge::backupDone=false;assert(!inventorybridge::commit({InventoryOp::take,ticket,invalid,0}));gamebridge::backupDone=true;
 put(base+npc::off::controlled,p+8);assert(!inventorybridge::commit({InventoryOp::take,ticket,invalid,0}));put(base+npc::off::controlled,p);
 assert(load("mixed","timers.after(0,function()local c=world.snapshot().objects[1];c:take_item(1);c:add_item(42)end)","world.read,containers.edit,inventory.transfer,timers"));tick(3015);assert(!host.enabled("mixed")&&published==2);
 host.capabilities(worldRead|containerEdit|inventoryRead|inventoryTransfer|equipmentWrite|timerEvents|lifecycleEvents);host.setTime(10000);
 std::ifstream example("inventory-tools.lua",std::ios::binary);assert(example);const std::string source{std::istreambuf_iterator<char>(example),std::istreambuf_iterator<char>()};
 std::vector<Module> config={{"config","return {enabled=true,action='equip',item_id=42}"}};
 assert(host.load("example",source.data(),source.size(),"inventory.read,timers,events.lifecycle",8*1024*1024,config,"world.read,inventory.transfer,equipment.write"));host.notifyPhase("session",1);tick(13501);assert(host.enabled("example")&&rebuilt==3&&worn.size()==1&&worn[0].first==42);
 config[0].source="return {enabled=true,action='take',item_id=41,container_record=7}";
 assert(host.load("example-take",source.data(),source.size(),"inventory.read,timers,events.lifecycle",8*1024*1024,config,"world.read,inventory.transfer"));host.notifyPhase("session",2);tick(17002);assert(host.enabled("example-take")&&published==3);
 // The other example is invoked by the lifecycle notification too; disable it before fault checks.
 host.clear();carried.resize(size_t(*(int*)(stat()+0xf0)));sync();
 settle(17003);view=inventorybridge::capture();assert(view);ticket=view->ticket;const auto fault=containerbridge::capture(n);failAppend=true;assert(!inventorybridge::commit({InventoryOp::take,ticket,fault,0})&&inventorybridge::broken&&aiWriteBroken);assert(!inventorybridge::capture());
 std::puts("LUA_INVENTORY_PASSED");
}
