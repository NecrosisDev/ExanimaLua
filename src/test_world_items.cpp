#include "plugin.cpp"
#include <cstdio>
#include "test_assert.h"
template<class T>static void put(uintptr_t at,T value){std::memcpy((void*)at,&value,sizeof value);}
struct Item {std::vector<uint8_t> node=std::vector<uint8_t>(0x210),body=std::vector<uint8_t>(0x500);uintptr_t at(){return (uintptr_t)node.data();}};
static std::vector<std::unique_ptr<Item>> models;static std::vector<uintptr_t> children;static std::vector<scripting::InventoryRow> carried;
static uintptr_t world=0,player=0,stats=0,sector=0;static std::vector<uint8_t> look(0x30);static int removed=0,created=0,released=0,switched=0,notified=0;static bool releaseFault=false,nullCreate=false,ignoreRemove=false;
static std::string sequence;
static void syncChildren(){put(world+0x118,int32_t(children.size())-1);put(world+0x120,(uintptr_t)children.data());}
static void syncCarried(){put(stats+0xf0,int32_t(carried.size()));put(stats+0xf8,(uintptr_t)carried.data());}
static Item& item(uint32_t id,uint32_t state,bool registered){
 models.push_back(std::make_unique<Item>());auto& object=*models.back();const auto n=object.at();
 put(n+8,uint32_t(0x4001|(registered?0x40000:0)));put(n+0x118,int32_t(-1));put(n+0x128,sector);put(n+0x130,sector);put(n+0x138,n);put(n+0x160,(uintptr_t)object.body.data());
 put(n+0x180,id);put(n+0x184,state);put((uintptr_t)object.body.data()+0xc,uint32_t(0x12000000));
 const std::array<float,3> pos{30,0,0};put(n+0x64,pos);put(n+0x94,pos);children.push_back(n);syncChildren();return object;
}
static uintptr_t chunkFixture(uint32_t id,uint32_t tag){return (id==41||id==42||id==43||id==0x80000007u)&&tag==0x100000?(uintptr_t)look.data():0;}
static void appendFixture(uintptr_t at,uint32_t id,uint32_t state){assert(at==stats+0xf0);carried.resize(size_t(*(int*)at));carried.push_back({id,state,40,4});syncCarried();}
static void notifyFixture(uintptr_t at){assert(at==stats);++notified;}
static void recountFixture(uintptr_t at,uint32_t,int initial){assert(at==stats&&initial==0);}
static void switchFixture(uintptr_t at){assert(at==player);++switched;put(player+0xdc0,int32_t(31));}
static void removeFixture(uintptr_t node,unsigned mode){assert(mode==0);++removed;if(ignoreRemove)return;for(auto i=children.begin();i!=children.end();++i)if(*i==node){children.erase(i);break;}put(node+8,uint32_t(0x80004001));syncChildren();}
static uint32_t convertFixture(uint32_t id,uintptr_t db){assert(db==*(uintptr_t*)(base+0x4ef930));sequence+='c';return id==41?0x80000007u:id;}
static uintptr_t createFixture(uint32_t id,uint32_t state,uintptr_t at,uintptr_t flags){assert(at==sector&&flags==0);sequence+='n';if(nullCreate)return 0;++created;return item(id,state,false).at();}
static void initializeFixture(uintptr_t node){sequence+='i';const auto position=*(std::array<float,3>*)(node+0x94);put(node+0x64,position);}
static void registerFixture(uintptr_t at,uintptr_t* node,int last){assert(at==world&&node&&last==0);sequence+='r';put(*node+8,uint32_t(0x44001));}
static void releaseFixture(uintptr_t node,const void* pose){assert(node&&pose==nullptr);sequence+='p';++released;if(releaseFault)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);}
static bool load(const char* id,const std::string& source,const char* caps="world.read,inventory.read,weapons.switch,world.pickup,world.drop,timers"){return host.load(id,source.data(),source.size(),caps);}
static void pulse(uint64_t now){worldPulse=true;beginAiPulse("session",now);inventorybridge::target={};worlditems::beginPulse();worldCaptured=false;worldCopy={};}
static void tick(uint64_t now){pulse(now);host.setTime(now);host.runTimers();worldPulse=false;}
int main(int argc,char**){
 using namespace scripting;std::vector<uint8_t> image(0x600000),scene(0x1e80),actor(0x1400),actorNode(0x210),actorStats(0x150),behavior(0x8a0),manager(0x100),drag(0x30),database(0x40),library(0x30);
 base=(uintptr_t)image.data();world=(uintptr_t)scene.data();sector=world;player=(uintptr_t)actor.data();stats=(uintptr_t)actorStats.data();
 put(base+npc::off::world,world);put(base+npc::off::controlled,player);put(base+0x49b300,world);std::memcpy((void*)(world+0x140),"exanima01",9);put(world+8,uint32_t(0x11));put(world+0x220,world);put(world+0x1db8,(uintptr_t)library.data());
 put(player+0x10a0,stats);put(player+0x18,(uintptr_t)actorNode.data());put((uintptr_t)actorNode.data()+0x130,world);put(player+0xd40,(uintptr_t)behavior.data());put((uintptr_t)behavior.data()+fp::game::behaviour::view,world);
 put(base+0x49a960,(uintptr_t)manager.data());put(base+0x4efdd0,(uintptr_t)drag.data());put(base+0x4ef930,(uintptr_t)database.data());put(base+0x2b7290,int32_t(100));
 put(stats+0x64,uint32_t(42));put(stats+0x74,uint32_t(43));put((uintptr_t)look.data()+4,uint32_t(look.size()));put((uintptr_t)look.data()+0x24,uint8_t(20));syncCarried();auto& original=item(41,0x20000,true);const auto n=original.at();
 worldPossible=inventoryWritePossible=inventorybridge::possible=worlditems::switchPossible=worlditems::pickupPossible=worlditems::dropPossible=true;gamebridge::backupDone=true;
 inventorybridge::append=appendFixture;inventorybridge::notify=notifyFixture;inventorybridge::recount=recountFixture;inventorybridge::extraCommit=worlditems::commit;containerbridge::chunk=chunkFixture;
 worlditems::requestSwitch=switchFixture;worlditems::convert=convertFixture;worlditems::create=createFixture;worlditems::initialize=initializeFixture;worlditems::registerNode=registerFixture;worlditems::release=releaseFixture;worlditems::remove=removeFixture;
 host.capabilities(worldRead|inventoryRead|weaponSwitch|worldPickup|worldDrop|timerEvents);host.setWorldProvider(captureWorld);host.setInventoryApi(inventorybridge::capture,inventorybridge::commit);host.setWorldItemCapture(worlditems::capture);beginAiPulse("session",0);
 assert(load("rollback","timers.after(0,function()world.snapshot().objects[1]:pickup();error('discard')end)"));tick(3001);assert(removed==0&&carried.empty());
 assert(load("take","timers.after(0,function()assert(world.snapshot().objects[1]:pickup())end)"));tick(3002);assert(removed==1&&children.empty()&&carried.size()==1&&carried[0].item==41&&carried[0].state==0x20000);
 assert(load("drop","timers.after(0,function()local i=inventory.snapshot();assert(i.weapons[1].item_id==42 and i.alternate_weapons[1].item_id==43);assert(i.carried[1]:drop({80,50,0}))end)"));tick(3003);
 assert(created==1&&released==1&&sequence=="cnirp"&&*(int*)(stats+0xf0)==0&&children.size()==1);const auto dropped=children[0];assert(*(uint32_t*)(dropped+0x180)==0x80000007u&&*(uint32_t*)(dropped+0x184)==0x20000);
 assert(load("take-back","timers.after(0,function()world.snapshot().objects[1]:pickup()end)"));tick(3004);assert(removed==2&&carried.size()==1&&carried[0].item==0x80000007u&&children.empty());
 assert(load("switch","timers.after(0,function()assert(inventory.snapshot():switch_weapons())end)"));tick(3005);assert(switched==1&&*(int*)(player+0xdc0)==31&&*(uint32_t*)(stats+0x64)==42);
 pulse(3006);assert(!inventorybridge::capture());put(player+0xdc0,int32_t(0));put(stats+0x64,uint32_t(43));put(stats+0x74,uint32_t(42));worldPulse=false;
 assert(load("expired","timers.after(0,function()local r=inventory.snapshot().carried[1];timers.after(0,function()r:drop({0,0,0})end)end)"));tick(3007);tick(3008);assert(!host.enabled("expired")&&created==1);
 assert(load("denied","timers.after(0,function()local i=inventory.snapshot();assert(i.switch_weapons==nil and i.carried[1].drop==nil)end)","inventory.read,timers"));tick(3009);assert(host.enabled("denied"));
 assert(load("invalid","timers.after(0,function()inventory.snapshot().carried[1]:drop({0,0/0,0})end)"));tick(3010);assert(!host.enabled("invalid")&&created==1);
 assert(load("far","timers.after(0,function()inventory.snapshot().carried[1]:drop({201,0,0})end)"));tick(3011);assert(created==1&&!inventorybridge::broken);
 assert(load("equipped","timers.after(0,function()inventory.snapshot().carried[1]:drop({0,0,0})end)"));put(stats+0x64,uint32_t(0x80000007u));tick(3012);assert(created==1);put(stats+0x64,uint32_t(43));
 children.push_back(n);put(n+8,uint32_t(0x44001));syncChildren();pulse(3013);auto ticket=worlditems::capture(n);assert(ticket);auto view=inventorybridge::capture();assert(view);InventoryChange change;change.op=InventoryOp::pickup;change.inventory=view->ticket;change.worldItem=ticket;
 put(n+0x184,uint32_t(0x40000));assert(!inventorybridge::commit(change)&&removed==2);put(n+0x184,uint32_t(0x20000));put((uintptr_t)original.body.data()+0x420,player);assert(!inventorybridge::commit(change));put((uintptr_t)original.body.data()+0x420,uintptr_t(0));
 put(world+0x3d4,uint32_t(200));assert(!inventorybridge::commit(change));put(world+0x3d4,uint32_t(0));put(n+0x64,std::array<float,3>{201,0,0});assert(!inventorybridge::commit(change));put(n+0x64,std::array<float,3>{30,0,0});
 pulse(3014);view=inventorybridge::capture();assert(view);change.inventory=view->ticket;change.worldItem=worlditems::capture(n);assert(change.worldItem);assert(inventorybridge::commit(change));assert(!inventorybridge::commit(change));carried.resize(size_t(*(int*)(stats+0xf0)));
 if(argc>1){
  children.push_back(n);put(n+8,uint32_t(0x44001));syncChildren();pulse(3015);view=inventorybridge::capture();assert(view);change.inventory=view->ticket;change.worldItem=worlditems::capture(n);assert(change.worldItem);
  ignoreRemove=true;assert(!inventorybridge::commit(change)&&inventorybridge::broken&&aiWriteBroken);std::puts("LUA_WORLD_REMOVE_FAULT_PASSED");return 0;
 }
 host.capabilities(worldRead|inventoryRead|weaponSwitch|worldPickup|worldDrop|timerEvents|lifecycleEvents);host.setTime(10000);
 std::ifstream example("inventory-tools.lua",std::ios::binary);assert(example);const std::string source{std::istreambuf_iterator<char>(example),std::istreambuf_iterator<char>()};std::vector<Module> config={{"config","return {enabled=true,action='switch'}"}};
 const auto exampleLoad=[&](){return host.load("example",source.data(),source.size(),"inventory.read,timers,events.lifecycle",8*1024*1024,config,"world.read,weapons.switch,world.pickup,world.drop");};
 assert(exampleLoad());host.notifyPhase("session",1);tick(13501);assert(switched==2&&host.enabled("example"));put(player+0xdc0,int32_t(0));put(stats+0x64,uint32_t(42));put(stats+0x74,uint32_t(43));
 config[0].source="return {enabled=true,action='drop',item_id=2147483655}";assert(exampleLoad());host.notifyPhase("session",2);tick(17002);assert(created==2&&host.enabled("example")&&children.size()==1);carried.resize(size_t(*(int*)(stats+0xf0)));
 config[0].source="return {enabled=true,action='pickup',world_item_record=7}";assert(exampleLoad());host.notifyPhase("session",3);tick(20503);assert(removed==4&&host.enabled("example")&&children.empty());host.clear();
 host.capabilities(worldRead|inventoryRead|worldDrop|worldSpawn|timerEvents);sequence.clear();const int spawnedBefore=created,carriedBefore=*(int*)(stats+0xf0);
 assert(load("spawn","timers.after(0,function()assert(inventory.snapshot():spawn_item(42,{20,10,0}))end)","inventory.read,world.spawn,timers"));tick(20600);
 assert(created==spawnedBefore+1&&sequence=="cnirp"&&*(uint32_t*)(children.back()+0x180)==42&&*(uint32_t*)(children.back()+0x184)==0&&*(int*)(stats+0xf0)==carriedBefore);
 assert(load("spawn-local","timers.after(0,function()inventory.snapshot():spawn_item(0x80000007,{0,0,0})end)","inventory.read,world.spawn,timers"));tick(20601);assert(!host.enabled("spawn-local"));
 assert(load("spawn-far","timers.after(0,function()inventory.snapshot():spawn_item(42,{500,0,0})end)","inventory.read,world.spawn,timers"));tick(20602);
 assert(load("spawn-unknown","timers.after(0,function()inventory.snapshot():spawn_item(77,{0,0,0})end)","inventory.read,world.spawn,timers"));tick(20603);
 assert(load("spawn-denied","timers.after(0,function()assert(inventory.snapshot().spawn_item==nil)end)","inventory.read,timers"));tick(20604);assert(host.enabled("spawn-denied"));
 assert(created==spawnedBefore+1&&!inventorybridge::broken&&*(int*)(stats+0xf0)==carriedBefore);children.pop_back();syncChildren();host.clear();
 pulse(21000);view=inventorybridge::capture();assert(view);change={};change.op=InventoryOp::drop;change.inventory=view->ticket;change.position={30,40,0};nullCreate=true;assert(!inventorybridge::commit(change)&&!inventorybridge::broken);nullCreate=false;
 pulse(21001);view=inventorybridge::capture();assert(view);change.inventory=view->ticket;releaseFault=true;assert(!inventorybridge::commit(change)&&inventorybridge::broken&&aiWriteBroken);assert(!inventorybridge::capture());
 std::puts("LUA_WORLD_ITEMS_PASSED");
}
