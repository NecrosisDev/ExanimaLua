#include "plugin.cpp"
#include <cstdio>
#include "test_assert.h"
template<class T>static void put(uintptr_t at,T value){std::memcpy((void*)at,&value,sizeof value);}
struct Object {std::vector<uint8_t> root=std::vector<uint8_t>(0x210),child=std::vector<uint8_t>(0x210),body=std::vector<uint8_t>(0x620);uintptr_t childAddress=0;uint32_t id=0;uintptr_t at(){return(uintptr_t)root.data();}uintptr_t physics(){return(uintptr_t)body.data();}};
static std::map<std::pair<uint32_t,uint32_t>,std::vector<uint8_t>> definitions;static std::vector<std::unique_ptr<Object>> objects;static std::vector<uintptr_t> children;
static uintptr_t world=0,player=0,stats=0;static std::vector<scripting::InventoryRow> carried;static std::vector<uint8_t> window(0x180);
static int effects=0,unlocks=0,opens=0,presses=0;static bool opened=false,nullOpen=false,badFind=false;static std::string failure;
static void sync(){put(stats+0xf0,int32_t(carried.size()));put(stats+0xf8,(uintptr_t)carried.data());}
static std::vector<uint8_t>& chunk(uint32_t id,uint32_t tag,size_t size){auto& bytes=definitions[{id,tag}];bytes.resize(size);put((uintptr_t)bytes.data(),tag);put((uintptr_t)bytes.data()+4,uint32_t(size));return bytes;}
static uintptr_t chunkFixture(uint32_t id,uint32_t tag){auto i=definitions.find({id,tag});return i==definitions.end()?0:(uintptr_t)i->second.data();}
static void device(uint32_t id,uint8_t kind,uint8_t subtype,uint16_t flags=0){auto& bytes=chunk(id,0x10000,32);bytes[8]=kind;bytes[9]=subtype;put((uintptr_t)bytes.data()+10,flags);}
static Object& object(uint32_t id,bool physical){
 objects.push_back(std::make_unique<Object>());auto& o=*objects.back();o.id=id;o.childAddress=(uintptr_t)o.child.data();const auto at=o.at();
 put(at+8,uint32_t(physical?0x44011:0x44001));put(at+0x180,id);put(at+0x130,world);put(at+0x128,world);put(at+0x118,int32_t(physical?0:-1));put(at+0x120,(uintptr_t)&o.childAddress);
 put(at+0x64,std::array<float,3>{30,0,0});put(at+0x94,std::array<float,3>{30,0,0});put(o.childAddress+0x128,at);put(o.childAddress+0x130,world);put(o.childAddress+0x160,o.physics());put(o.childAddress+0x118,int32_t(-1));put(o.physics()+0x18,o.childAddress);put(o.physics()+0x4f0,id);put(o.physics()+0x5e0,id);
 if(definitions.find({id,0x40000})!=definitions.end())put(o.physics(),base+0x31ab38);
 children.push_back(at);put(world+0x118,int32_t(children.size())-1);put(world+0x120,(uintptr_t)children.data());return o;
}
static void notifyFixture(uintptr_t at){assert(at==stats);}
static void recountFixture(uintptr_t at,uint32_t,int count){assert(at==stats&&count==0);}
static void effectFixture(void* pointer,uintptr_t actor){
 assert(actor==player);auto& context=*(interactions::EffectContext*)pointer;assert(context.view==world&&context.source==0&&context.node==0&&context.row==0);++effects;
 if(failure=="effect-fault")RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);if(failure=="effect-noop")return;
 auto& d=definitions[{context.item,0x10000}];if(d[9]==8){const auto old=*(float*)(player+0xdc4);put(player+0xdc4,old+0.1f);}else{assert(d[9]==14);put(player+0xdf8,uint32_t(0x800000));put(player+0x12a0,int32_t(1800));}
}
static void unlockFixture(uintptr_t body){++unlocks;if(failure=="unlock-fault")RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);put(body+0x603,uint8_t(1));const auto node=*(uintptr_t*)(body+0x18),root=*(uintptr_t*)(node+0x128);put(root+0x184,*(uint32_t*)(root+0x184)&~1u);}
static uintptr_t openFixture(uint32_t id){++opens;if(nullOpen)return 0;opened=true;put(base+0x499860,int32_t(1));put((uintptr_t)window.data()+0x15c,id);return(uintptr_t)window.data();}
static char openCheckFixture(uint32_t){return opened;}
static uintptr_t findFixture(uintptr_t at,uint32_t id){assert(at==world);if(badFind)return objects[0]->at();for(auto& o:objects)if(o->id==id)return o->at();return 0;}
static void triggerFixture(uintptr_t at,uint32_t id,uint32_t source,uint32_t value){assert(at==world&&source==0&&value<=1);++presses;if(failure=="button-fault")RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);for(auto& o:objects)if(o->id==id)put(o->physics()+0x51c,uint8_t(value));}
static int wakes=0,commands=0,unlatches=0;static uintptr_t faultBody=0;
static void commandFixture(uintptr_t,uint32_t,uint32_t);
static void wakeFixture(uintptr_t body,int ticks){assert(ticks==256);++wakes;if(failure=="lever-wake-fault"||failure=="door-wake-fault"||failure=="light-wake-fault")RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);put(body+0x14,ticks);}
static void unlatchFixture(uintptr_t body){++unlatches;if(failure=="door-unlatch-fault"||failure=="slider-unlatch-fault")RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);put(body+0x604,uint8_t(0));put(body+0x5fe,uint8_t(0));
 if(*(uint8_t*)(body+0x5f8)){const bool mechanical=*(uint8_t*)(body+0x5fa)!=0;if(!mechanical){const auto list=*(uintptr_t*)(body+0x320);put(list+size_t(*(int*)(body+0x58c))*0x68+0x28,0.6f);}commandFixture(body,4,mechanical?300:80);}}
static void commandFixture(uintptr_t body,uint32_t command,uint32_t ticks){++commands;if(failure=="door-command-fault"||(failure=="pair-second-fault"&&body==faultBody))RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);if(failure=="door-command-noop")return;
 assert((command==4&&(ticks==80||ticks==100||ticks==300))||(command==8&&(ticks==100||ticks==300)));put(body+0x528,command);put(body+0x52c,ticks);const auto node=*(uintptr_t*)(body+0x18),root=*(uintptr_t*)(node+0x128);put(root+0x184,(*(uint32_t*)(root+0x184)&0xffff00u)|command|(ticks<<24));}
static void pulse(uint64_t now){worldPulse=true;beginAiPulse("session",now);inventorybridge::target={};worlditems::beginPulse();interactions::targets.clear();worldCaptured=false;worldCopy={};}
static void tick(uint64_t now){pulse(now);host.setTime(now);host.runTimers();worldPulse=false;carried.resize(size_t(*(int*)(stats+0xf0)));sync();}
static bool load(const char* id,const std::string& text,const char* caps="inventory.read,world.read,items.use,world.use,timers"){return host.load(id,text.data(),text.size(),caps);}
static const std::string keyed="local i=inventory.snapshot();local w=world.snapshot();for _,o in ipairs(w.objects)do if o.item_id==70 then for _,r in ipairs(i.carried)do if r.item_id==50 then assert(o:use_item(r));return end end end end";
static const std::string salve="for _,r in ipairs(inventory.snapshot().carried)do if r.item_id==60 then assert(r:use());return end end";
static const std::string button="for _,o in ipairs(world.snapshot().objects)do if o.item_id==72 then assert(o:use());return end end";
static const std::string leverUse="for _,o in ipairs(world.snapshot().objects)do if o.item_id==74 then assert(o:set_position(1));return end end";
static const std::string doorUse="for _,o in ipairs(world.snapshot().objects)do if o.item_id==70 then assert(o:set_open(true));return end end";
static void deviceTests(){
 using namespace scripting;const auto previousObjects=objects.size(),previousChildren=children.size();wakes=commands=unlatches=0;host.clear();host.setTime(40000);
 const auto makeDoor=[](uint32_t id)->Object&{chunk(id,0x40000,24);auto& o=object(id,true);put(o.physics()+0x520,1.0f);put(o.physics()+0x5c8,1.0f);put(o.physics()+0x5fe,uint8_t(1));return o;};
 auto& slider=makeDoor(0x80000050u);std::vector<uint8_t> tetras(0x68);put((uintptr_t)tetras.data()+0x24,1.0f);put((uintptr_t)tetras.data()+0x28,1.0f);
 put(slider.physics()+0x5f8,uint8_t(1));put(slider.physics()+0x5fa,uint8_t(1));put(slider.physics()+0x31c,int32_t(1));put(slider.physics()+0x320,(uintptr_t)tetras.data());
 auto& pair=makeDoor(0x80000051u);Object mate;const auto mateNode=(uintptr_t)mate.child.data(),mateBody=mate.physics();
 put(mateNode+0x128,pair.at());put(mateNode+0x130,world);put(mateNode+0x160,mateBody);put(mateNode+0x118,int32_t(-1));put(mateBody,base+0x31ab38);put(mateBody+0x18,mateNode);put(mateBody+0x5e0,pair.id);put(mateBody+0x520,-1.0f);put(mateBody+0x5c8,-1.0f);put(mateBody+0x5fe,uint8_t(1));
 std::array<uintptr_t,2> pairChildren{pair.childAddress,mateNode};put(pair.at()+0x118,int32_t(1));put(pair.at()+0x120,(uintptr_t)pairChildren.data());
 put(pair.physics()+0x607,uint8_t(1));put(pair.physics()+0x5e8,mateBody);put(mateBody+0x607,uint8_t(1));put(mateBody+0x5e8,pair.physics());faultBody=mateBody;
 device(0x80000052u,3,6);auto& lightLever=object(0x80000052u,true);put(lightLever.physics()+0x570,lightLever.id);put(lightLever.physics()+0x524,-0.5f);put(lightLever.physics()+0x528,1.0f);put(lightLever.physics()+0x530,-0.25f);
 std::vector<uint8_t> light(0x2e0);const auto lamp=(uintptr_t)light.data();put(lamp+8,uint32_t(5));put(lamp+0x128,lightLever.at());put(lamp+0x130,world);put(lamp+0x118,int32_t(-1));put(lamp+0x2dc,1.0f);
 std::array<uintptr_t,2> lightChildren{lightLever.childAddress,lamp};put(lightLever.at()+0x118,int32_t(1));put(lightLever.at()+0x120,(uintptr_t)lightChildren.data());put(lightLever.physics()+0x5c0,lamp);
 const auto script=[](int record,const char* method){return "timers.after(0,function()for _,o in ipairs(world.snapshot().objects)do if o.item_id=="+std::to_string(record)+" then assert(o:"+method+");return end end error('missing target')end)";};
 const auto run=[&](const char* id,int record,const char* method,uint64_t now,const char* caps="world.read,world.doors,timers"){assert(load(id,script(record,method),caps));tick(now);assert(host.enabled(id));};
 const auto rowsBefore=carried;
 if(!failure.empty()){
  if(failure=="light-wake-fault")assert(load("fault",script(82,"set_position(1)"),"world.read,world.levers,timers"));
  else assert(load("fault",script(failure=="pair-second-fault"?81:80,"set_open(true)"),"world.read,world.doors,timers"));
  tick(40001);assert(inventorybridge::broken&&aiWriteBroken&&carried==rowsBefore);
  if(failure=="pair-second-fault")assert(commands==2&&*(int*)(pair.physics()+0x528)==4);
  std::puts("LUA_DEVICE_FAULT_PASSED");return;
 }
 run("slider",80,"set_open(true)",40001);assert(commands==1&&*(int*)(slider.physics()+0x52c)==300&&*(float*)(tetras.data()+0x28)==1);
 run("slider-close",80,"set_open(false)",40002);assert(commands==2&&*(int*)(slider.physics()+0x528)==8);
 // Simulate a later native settled state; fixture request acceptance is not motion.
 put(slider.at()+0x184,uint32_t(0));put(slider.physics()+0x528,int32_t(0));put(slider.physics()+0x5fe,uint8_t(1));put(slider.physics()+0x5fa,uint8_t(0));
 run("manual-slider",80,"set_open(true)",40003);assert(commands==3&&*(int*)(slider.physics()+0x52c)==80&&*(float*)(tetras.data()+0x28)==0.6f);
 run("pair",81,"set_open(true)",40004);assert(commands==5&&*(int*)(pair.physics()+0x528)==4&&*(int*)(mateBody+0x528)==4);
 run("pair-close",81,"set_open(false)",40005);assert(commands==7&&*(int*)(pair.physics()+0x528)==8&&*(int*)(mateBody+0x528)==8);
 assert(load("metadata","timers.after(0,function()for _,o in ipairs(world.snapshot().objects)do if o.item_id==80 then assert(o.door_kind=='sliding' and o.door_leaves==1)elseif o.item_id==81 then assert(o.door_kind=='hinged' and o.door_leaves==2 and o.command=='closing' and o.moving)end end end)","world.read,world.doors,timers"));tick(40006);assert(host.enabled("metadata"));
 // The command clears at the target, but a hinge still swinging (squared tip speed above 0.015) is moving.
 *(int*)(pair.physics()+0x528)=0;*(int*)(mateBody+0x528)=0;*(float*)(pair.physics()+0x51c)=0.02f;
 assert(load("swinging","timers.after(0,function()for _,o in ipairs(world.snapshot().objects)do if o.item_id==81 then assert(o.command==nil and o.moving)end end end)","world.read,world.doors,timers"));tick(40007);assert(host.enabled("swinging"));
 *(float*)(pair.physics()+0x51c)=0.01f;
 assert(load("settled","timers.after(0,function()for _,o in ipairs(world.snapshot().objects)do if o.item_id==81 then assert(o.command==nil and not o.moving)end end end)","world.read,world.doors,timers"));tick(40008);assert(host.enabled("settled"));
 pulse(40007);const auto inv=inventorybridge::capture();assert(inv);const auto view=interactions::capture(pair.at());assert(view&&view->doorControl);InventoryChange change;change.inventory=inv->ticket;change.worldItem=view->ticket;change.op=InventoryOp::doorOpen;change.active=true;
 put(mateBody+0x604,uint8_t(1));assert(!inventorybridge::commit(change)&&commands==7);put(mateBody+0x604,uint8_t(0));
 put(mateBody+0x5e8,uintptr_t(0));assert(!inventorybridge::commit(change)&&commands==7);put(mateBody+0x5e8,pair.physics());
 put(mateNode+0x128,world);assert(!inventorybridge::commit(change));put(mateNode+0x128,pair.at());
 put(mateBody,uintptr_t(0));assert(!inventorybridge::commit(change));put(mateBody,base+0x31ab38);
 pulse(40008);const auto inv2=inventorybridge::capture();const auto sliderView=interactions::capture(slider.at());assert(inv2&&sliderView&&sliderView->doorControl);change.inventory=inv2->ticket;change.worldItem=sliderView->ticket;change.active=false;
 put(slider.physics()+0x58c,int32_t(1));assert(!inventorybridge::commit(change));put(slider.physics()+0x58c,int32_t(0));
 put((uintptr_t)tetras.data()+0x28,0.5f);assert(!inventorybridge::commit(change));put((uintptr_t)tetras.data()+0x28,0.6f);
 run("light",82,"set_position(1)",40009,"world.read,world.levers,timers");assert(*(int*)(lightLever.physics()+0x540)==1&&*(float*)(lamp+0x2dc)==1);
 assert(load("light-metadata","timers.after(0,function()for _,o in ipairs(world.snapshot().objects)do if o.item_id==82 then assert(o.light_control==true)end end end)","world.read,world.levers,timers"));tick(40010);assert(host.enabled("light-metadata"));
 pulse(40011);const auto inv3=inventorybridge::capture();const auto lightView=interactions::capture(lightLever.at());assert(inv3&&lightView);change.inventory=inv3->ticket;change.worldItem=lightView->ticket;change.op=InventoryOp::leverPosition;change.active=false;
 put(lamp+0x128,world);assert(!inventorybridge::commit(change));put(lamp+0x128,lightLever.at());
 put(lightLever.physics()+0x5c0,slider.at());assert(!inventorybridge::commit(change));put(lightLever.physics()+0x5c0,lamp);
 assert(carried==rowsBefore&&!inventorybridge::broken);host.clear();host.setTime(41000);std::ifstream example("inventory-tools.lua",std::ios::binary);assert(example);const std::string source{std::istreambuf_iterator<char>(example),std::istreambuf_iterator<char>()};
 std::vector<Module> config={{"config","return {enabled=true,action='set_open',world_item_record=80,open=false}"}};
 const auto exampleLoad=[&](){return host.load("example",source.data(),source.size(),"inventory.read,timers,events.lifecycle",8*1024*1024,config,"world.read,world.levers,world.doors");};
 assert(exampleLoad());host.notifyPhase("session",7);tick(44501);assert(commands==8&&host.enabled("example"));
 config[0].source="return {enabled=true,action='set_open',world_item_record=81,open=true}";assert(exampleLoad());host.notifyPhase("session",8);tick(48002);assert(commands==10&&host.enabled("example"));
 config[0].source="return {enabled=true,action='world_use',world_item_record=82}";assert(exampleLoad());host.notifyPhase("session",9);tick(51503);assert(*(int*)(lightLever.physics()+0x540)==0&&host.enabled("example"));
 std::vector<uint8_t> mateTetras(0x68),primaryTetras(0x68);put((uintptr_t)mateTetras.data()+0x24,1.0f);put((uintptr_t)mateTetras.data()+0x28,0.5f);put((uintptr_t)primaryTetras.data()+0x24,1.0f);put((uintptr_t)primaryTetras.data()+0x28,0.5f);
 put(mateBody+0x5f8,uint8_t(1));put(mateBody+0x5fa,uint8_t(1));put(mateBody+0x31c,int32_t(1));put(mateBody+0x320,(uintptr_t)mateTetras.data());
 run("mixed-pair",81,"set_open(false)",51504);assert(commands==12);
 put(pair.physics()+0x5f8,uint8_t(1));put(pair.physics()+0x5fa,uint8_t(1));put(pair.physics()+0x31c,int32_t(1));put(pair.physics()+0x320,(uintptr_t)primaryTetras.data());
 run("sliding-pair",81,"set_open(true)",51505);assert(commands==14&&*(int*)(mateBody+0x52c)==300&&*(int*)(pair.physics()+0x52c)==300);
 host.clear();objects.resize(previousObjects);children.resize(previousChildren);put(world+0x118,int32_t(children.size())-1);put(world+0x120,(uintptr_t)children.data());std::puts("LUA_SLIDING_PAIRED_LIGHT_DEVICES_PASSED");
}
static std::vector<uint8_t> portalWindowBytes(0x1b0),scannerNode(0x300);
static char(*emitterCallback)(void*)=nullptr;static void* emitterPayload=nullptr;static int emitterCalls=0;static std::vector<uint8_t> emitterController(0x40);
static void emitterQueue(uintptr_t queue,char(*callback)(void*),void* payload){
 assert(queue==base+0x499990&&!emitterCallback);if(failure=="emitter-queue-fault"||failure=="fire-queue-fault"||failure=="torch-queue-fault")RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);emitterCallback=callback;emitterPayload=payload;put(base+0x499994,(*(uint32_t*)(base+0x499990)+1)%128);
}
static void drainEmitter(){assert(emitterCallback);const auto callback=emitterCallback;emitterCallback=nullptr;assert(callback(emitterPayload)==0);put(base+0x499990,*(uint32_t*)(base+0x499994));emitterPayload=nullptr;}
static void emitterSignal(uintptr_t node,uint32_t active){
 ++emitterCalls;if(failure=="emitter-native-fault")RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);if(failure=="emitter-no-effect")return;
 if(!active&&failure=="emitter-stop-fault")RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);if(!active&&failure=="emitter-stop-noop")return;
 for(auto& o:objects)if(o->at()==node){
  if(active){put(node+0x118,int32_t(0));put(o->childAddress,uintptr_t(0));put(o->physics(),base+0x323da0);put(o->physics()+0xc,uint32_t(0x21000000));put(o->physics()+0x198,uint32_t(0));put(o->physics()+0xa0,(uintptr_t)emitterController.data());}
  else{put(o->physics()+0x198,uint32_t(1));put((uintptr_t)emitterController.data()+0x30,int32_t(10));}
  return;
 }assert(false);
}
static void emitterTests(){
 using namespace scripting;host.clear();host.setTime(70000);const auto previousObjects=objects.size(),previousChildren=children.size();
 std::vector<uint8_t> renderer(0x80),resources(0x30);put(world+0x3b8,(uintptr_t)renderer.data());put((uintptr_t)renderer.data()+0x10,(uintptr_t)resources.data());put(base+0x3243f0,0.1);put(base+0x324570,0.25);
 constexpr uint32_t raw=0x8000005cu;device(raw,6,13);auto& bytes=definitions[{raw,0x10000}];put((uintptr_t)bytes.data()+0x10,uint16_t(10));put((uintptr_t)bytes.data()+0x12,uint16_t(10));put((uintptr_t)bytes.data()+0x14,int16_t(4));
 auto& emitter=object(raw,false);put(emitter.at()+0x58,std::array<float,3>{1,0,0});worldemitters::possible=true;worldemitters::signal=emitterSignal;worldemitters::enqueue=emitterQueue;worldMutationReady=worldMutationsIdle;
 const auto source=[](const char* method){return std::string("timers.after(0,function()for _,o in ipairs(world.snapshot().objects)do if o.item_id==92 then assert(o.interaction_kind=='emitter' and o.device_kind=='electrical');assert(o:")+method+");return end end error('missing emitter')end)";};
 const auto run=[&](const char* id,const char* method,uint64_t now){assert(load(id,source(method),"world.read,world.devices,timers"));tick(now);};const auto initial=carried;
 if(!failure.empty()){
  run("fault","set_active(true)",70001);if(emitterCallback)drainEmitter();pulse(70002);
  if(failure.find("emitter-stop-")==0){run("stop-fault","set_active(false)",70003);drainEmitter();pulse(70004);}
  assert(inventorybridge::broken&&aiWriteBroken&&carried==initial);std::puts("LUA_EMITTER_FAULT_PASSED");return;
 }
 assert(load("capabilities","timers.after(0,function()for _,o in ipairs(world.snapshot().objects)do if o.item_id==92 then assert(o.set_active==nil and o.use==nil)end end end)","world.read,world.use,timers"));tick(70001);assert(host.enabled("capabilities"));
 assert(load("rollback",source("set_active(true)").replace(source("set_active(true)").find(";return"),7,";error('discard');return"),"world.read,world.devices,timers"));tick(70002);assert(!emitterCallback&&emitterCalls==0);
 pulse(70003);auto inv=inventorybridge::capture();assert(inv);auto view=interactions::capture(emitter.at());assert(view);InventoryChange change;change.inventory=inv->ticket;change.worldItem=view->ticket;change.op=InventoryOp::emitterSet;change.active=true;
 put((uintptr_t)bytes.data()+0x12,uint16_t(11));assert(!inventorybridge::commit(change));put((uintptr_t)bytes.data()+0x12,uint16_t(10));
 put(emitter.at()+0x180,uint32_t(92));pulse(70004);assert(!interactions::capture(emitter.at()));put(emitter.at()+0x180,raw);
 put((uintptr_t)bytes.data()+0x10,uint16_t(0));assert(!interactions::capture(emitter.at()));put((uintptr_t)bytes.data()+0x10,uint16_t(10));
 put((uintptr_t)bytes.data()+9,uint8_t(12));assert(!interactions::capture(emitter.at()));put((uintptr_t)bytes.data()+9,uint8_t(13));
 auto& duplicate=object(raw,false);pulse(70005);assert(!interactions::capture(emitter.at()));put(duplicate.at()+0x180,uint32_t(0));
 run("start","set_active(true)",70006);assert(emitterCallback&&worldemitters::pending&&emitterCalls==0);host.clear();pulse(70007);assert(!aiPulseReady&&!inventorybridge::capture());drainEmitter();pulse(70008);assert(aiPulseReady&&emitterCalls==1&&*(uint16_t*)(bytes.data()+0xc)==1&&carried==initial);
 run("noop","set_active(true)",70009);assert(!emitterCallback&&emitterCalls==1);
 assert(load("bad-argument",source("set_active(1)"),"world.read,world.devices,timers"));tick(70010);assert(!host.enabled("bad-argument")&&!emitterCallback);
 run("stop","use()",70011);assert(emitterCallback);drainEmitter();pulse(70012);assert(emitterCalls==2&&*(uint16_t*)(bytes.data()+0xc)==0&&!interactions::capture(emitter.at()));
 put(emitter.at()+0x118,int32_t(-1));pulse(70013);assert(interactions::capture(emitter.at()));
 run("stale","set_active(true)",70014);put(emitter.at()+0x64,std::array<float,3>{31,0,0});drainEmitter();pulse(70015);assert(emitterCalls==2&&!inventorybridge::broken);put(emitter.at()+0x64,std::array<float,3>{30,0,0});
 run("transition","set_active(true)",70016);put(base+0x49b300,uintptr_t(0));drainEmitter();put(base+0x49b300,world);pulse(70017);assert(emitterCalls==2&&!inventorybridge::broken);
 run("out-of-reach","set_active(true)",70018);put(player+npc::off::root,std::array<float,3>{300,0,0});drainEmitter();put(player+npc::off::root,std::array<float,3>{0,0,0});pulse(70019);assert(emitterCalls==2&&!inventorybridge::broken);
 std::ifstream example("inventory-tools.lua",std::ios::binary);assert(example);const std::string script{std::istreambuf_iterator<char>(example),std::istreambuf_iterator<char>()};std::vector<Module> config={{"config","return {enabled=true,action='set_active',world_item_record=92,active=true}"}};
 host.clear();host.setTime(71000);assert(host.load("example",script.data(),script.size(),"inventory.read,timers,events.lifecycle",8*1024*1024,config,"world.read,world.devices"));host.notifyPhase("session",12);tick(74501);assert(emitterCallback&&host.enabled("example"));drainEmitter();pulse(74502);assert(emitterCalls==3&&!inventorybridge::broken);
 host.clear();worldMutationReady=nullptr;worldemitters::possible=false;objects.resize(previousObjects);children.resize(previousChildren);put(world+0x118,int32_t(children.size())-1);put(world+0x120,(uintptr_t)children.data());std::puts("LUA_ELECTRICAL_EMITTERS_PASSED");
}
static int fireCalls=0,fireSize=-1;static uintptr_t createdFire=0;
static uintptr_t fireFixture(uintptr_t node,int size){
 ++fireCalls;fireSize=size;if(failure=="fire-native-fault")RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);if(failure=="fire-no-effect")return 0;
 auto& effect=object(0,false);createdFire=effect.at();put(createdFire+8,uint32_t(0x49001));put(createdFire+0x160,effect.physics());put(effect.physics(),failure=="fire-foreign-effect"?uintptr_t(0):base+0x30e9e8);put(effect.physics()+0x18,createdFire);
 put(createdFire+0x64,*(std::array<float,3>*)(node+0x64));if(failure=="fire-wrong-position")put(createdFire+0x64,std::array<float,3>{123,0,0});if(failure=="fire-source-mutated")put(node+0x184,uint32_t(2));return 0;
}
static void removeFireFixture(){children.erase(std::remove(children.begin(),children.end(),createdFire),children.end());put(world+0x118,int32_t(children.size())-1);put(world+0x120,(uintptr_t)children.data());}
static void fireTests(){
 using namespace scripting;host.clear();host.setTime(80000);const auto previousObjects=objects.size(),previousChildren=children.size();const auto initial=carried;
 std::vector<uint8_t> renderer(0x80),resources(0x30);put(world+0x3b8,(uintptr_t)renderer.data());put((uintptr_t)renderer.data()+0x10,(uintptr_t)resources.data());put(base+0x31b0b0,0.01);put(base+0x31b0b8,2.0f);
 constexpr uint32_t raw=0x8000005du;device(raw,6,12);auto& bytes=definitions[{raw,0x10000}];auto& source=object(raw,false);put(source.at()+0x58,std::array<float,3>{1,0,0});
 worldemitters::firePossible=true;worldemitters::fire=fireFixture;worldemitters::enqueue=emitterQueue;worldMutationReady=worldMutationsIdle;
 const std::string script="timers.after(0,function()for _,o in ipairs(world.snapshot().objects)do if o.item_id==93 then assert(o.interaction_kind=='fire_source' and o.device_kind=='fire' and o.set_active==nil);assert(o:use());return end end error('missing source')end)";
 const auto run=[&](const char* id,uint64_t now){assert(load(id,script,"world.read,world.devices,timers"));tick(now);};
 if(!failure.empty()){run("fault",80001);if(emitterCallback)drainEmitter();pulse(80002);assert(inventorybridge::broken&&aiWriteBroken&&carried==initial);std::puts("LUA_FIRE_FAULT_PASSED");return;}
 assert(load("denied","timers.after(0,function()for _,o in ipairs(world.snapshot().objects)do if o.item_id==93 then assert(o.use==nil)end end end)","world.read,world.use,timers"));tick(80001);assert(host.enabled("denied"));
 auto rollback=script;rollback.replace(rollback.find(";return"),7,";error('discard');return");assert(load("rollback",rollback,"world.read,world.devices,timers"));tick(80002);assert(!emitterCallback&&!fireCalls);
 put((uintptr_t)bytes.data()+0xc,uint16_t(201));pulse(80003);assert(!interactions::capture(source.at()));put((uintptr_t)bytes.data()+0xc,uint16_t(0));
 put(base+0x31b0b8,std::numeric_limits<float>::quiet_NaN());assert(!interactions::capture(source.at()));put(base+0x31b0b8,2.0f);
 put(source.at()+0x180,uint32_t(93));assert(!interactions::capture(source.at()));put(source.at()+0x180,raw);
 run("stale",80004);put(source.at()+0x64,std::array<float,3>{31,0,0});drainEmitter();pulse(80005);assert(!fireCalls&&!inventorybridge::broken);put(source.at()+0x64,std::array<float,3>{30,0,0});
 run("native-default",80006);assert(emitterCallback&&worldemitters::pending);host.clear();pulse(80007);assert(!aiPulseReady&&!inventorybridge::capture());drainEmitter();pulse(80008);assert(fireCalls==1&&fireSize==0&&carried==initial&&!inventorybridge::broken&&interactions::capture(source.at())->pressed);
 put(createdFire+0x64,std::array<float,3>{123,0,0});pulse(80008);assert(interactions::capture(source.at())->pressed);
 removeFireFixture();pulse(80009);assert(interactions::capture(source.at()));put((uintptr_t)bytes.data()+0xc,uint16_t(25));run("native-size",80010);drainEmitter();pulse(80011);assert(fireCalls==2&&fireSize==25&&!inventorybridge::broken);removeFireFixture();pulse(80012);
 // Burning sources remain observable, without an ignition method.
 worldemitters::fires={};put(createdFire+0x64,std::array<float,3>{30,0,0});children.push_back(createdFire);put(world+0x118,int32_t(children.size())-1);put(world+0x120,(uintptr_t)children.data());assert(interactions::capture(source.at())->pressed&&!interactions::capture(source.at())->igniteControl);removeFireFixture();pulse(80013);
 put(base+0x499994,(*(uint32_t*)(base+0x499990)+1)%128);run("busy-queue",80014);assert(!emitterCallback&&fireCalls==2&&!inventorybridge::broken);put(base+0x499994,*(uint32_t*)(base+0x499990));
 std::ifstream example("inventory-tools.lua",std::ios::binary);assert(example);const std::string exampleScript{std::istreambuf_iterator<char>(example),std::istreambuf_iterator<char>()};std::vector<Module> config={{"config","return {enabled=true,action='world_use',world_item_record=93}"}};
 host.clear();host.setTime(81000);assert(host.load("example",exampleScript.data(),exampleScript.size(),"inventory.read,timers,events.lifecycle",8*1024*1024,config,"world.read,world.devices"));host.notifyPhase("session",13);tick(84501);assert(emitterCallback&&host.enabled("example"));drainEmitter();pulse(84502);assert(fireCalls==3&&!inventorybridge::broken&&carried==initial);
 host.clear();worldMutationReady=nullptr;worldemitters::firePossible=false;worldemitters::fires={};objects.resize(previousObjects);children.resize(previousChildren);put(world+0x118,int32_t(children.size())-1);put(world+0x120,(uintptr_t)children.data());std::puts("LUA_FIRE_SOURCES_PASSED");
}
static int torchCalls=0,removeCalls=0,poseCalls=0,publishCalls=0;
static uintptr_t flame=0;
static uintptr_t torchFixture(uintptr_t node,uintptr_t data){
 ++torchCalls;assert(data==chunkFixture(*(uint32_t*)(node+0x180),0x10000)+8);
 if(failure=="torch-native-fault")RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);if(failure=="torch-no-effect")return 0;
 const auto count=objects.size();auto& o=object(0,false);flame=o.at();children.pop_back();put(world+0x118,int32_t(children.size())-1);put(world+0x120,(uintptr_t)children.data());assert(objects.size()==count+1);
 put(flame+8,uint32_t(0x9001));put(flame+0x128,node);put(flame+0x160,o.physics());put(o.physics(),failure=="torch-foreign"?uintptr_t(0):base+0x30e9e8);put(o.physics()+0x18,flame);
 for(auto& source:objects)if(source->at()==node){source->childAddress=flame;put(node+0x118,int32_t(0));put(node+0x120,(uintptr_t)&source->childAddress);break;}
 return flame;
}
static void poseFixture(uintptr_t node){assert(node==flame&&torchCalls>poseCalls);++poseCalls;}
static void publishFixture(uintptr_t owner,uintptr_t* node,int flags){assert(owner==world&&*node==flame&&flags==0&&poseCalls==publishCalls+1);++publishCalls;
 if(failure=="torch-publish-fault")RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);if(failure!="torch-unregistered")put(*node+8,*(uint32_t*)(*node+8)|0x40000);
}
static void extinguishFixture(uintptr_t owner,uintptr_t node,int mode){
 assert(owner==world&&mode==1);++removeCalls;if(failure=="extinguish-native-fault")RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);if(failure=="extinguish-noop")return;
 const auto slot=*(uint32_t*)(world+0x3d4);const auto at=world+0x3d8+size_t(slot)*0x18;put(at,node);put(at+8,*(uintptr_t*)(node+0x130));put(at+16,mode);put(world+0x3d4,(slot+1)%256);
 put(node+8,(*(uint32_t*)(node+8)&~0x40000u)|0x80000000u);put(node+0xe0,uint32_t(0));put(node+0xe4,uint32_t(1));put(node+0x130,*(uintptr_t*)(base+0x49b2e0));
}
static void flamesTests(){
 using namespace scripting;host.clear();host.setTime(90000);const auto previousObjects=objects.size(),previousChildren=children.size();const auto initial=carried;
 const auto probe=[](uintptr_t node){interactions::targets.clear();return interactions::capture(node);};
 std::vector<uint8_t> renderer(0x80),resources(0x30);put(world+0x3b8,(uintptr_t)renderer.data());put((uintptr_t)renderer.data()+0x10,(uintptr_t)resources.data());
 put(base+0x31b0b0,0.01);put(base+0x31b0b8,2.0f);put(base+0x31b0c0,0.25f);put(base+0x31b0c8,10.0f);put(base+0x49b2e0,uintptr_t(0));
 constexpr uint32_t raw=0x8000005eu;device(raw,8,12);auto& bytes=definitions[{raw,0x10000}];put((uintptr_t)bytes.data()+0x10,uint16_t(50));
 auto& source=object(raw,false);put(source.at()+8,uint32_t(0x44011));put(source.at()+0x58,std::array<float,3>{1,0,0});
 worldemitters::torchPossible=worldemitters::extinguishPossible=true;worldemitters::torch=torchFixture;worldemitters::remove=extinguishFixture;worldemitters::publish=publishFixture;worldemitters::pose=poseFixture;worldemitters::enqueue=emitterQueue;worldMutationReady=worldMutationsIdle;
 const auto script=[](const char* method){return std::string("timers.after(0,function()for _,o in ipairs(world.snapshot().objects)do if o.item_id==94 then assert(o.interaction_kind=='torch' and o.device_kind=='torch');assert(o:")+method+");return end end error('missing torch')end)";};
 const auto run=[&](const char* id,const char* method,uint64_t now){assert(load(id,script(method),"world.read,world.devices,timers"));tick(now);};
 const auto drained=[&](){put(source.at()+0x118,int32_t(-1));put(world+0x3d0,*(uint32_t*)(world+0x3d4));};
 if(!failure.empty()){
  run("fault","ignite()",90001);if(emitterCallback)drainEmitter();pulse(90002);
  if(failure.find("extinguish-")==0){assert(!inventorybridge::broken);run("stop-fault","extinguish()",90003);assert(emitterCallback);drainEmitter();pulse(90004);}
  assert(inventorybridge::broken&&aiWriteBroken&&carried==initial);std::puts("LUA_FLAMES_FAULT_PASSED");return;
 }
 assert(load("capabilities","timers.after(0,function()for _,o in ipairs(world.snapshot().objects)do if o.item_id==94 then assert(o.ignite==nil and o.extinguish==nil and o.use==nil)end end end)","world.read,world.use,timers"));tick(90001);assert(host.enabled("capabilities"));
 auto rollback=script("ignite()");rollback.replace(rollback.find(";return"),7,";error('discard');return");assert(load("rollback",rollback,"world.read,world.devices,timers"));tick(90002);assert(!emitterCallback&&!torchCalls);
 pulse(90003);assert(probe(source.at())->igniteControl);put((uintptr_t)bytes.data()+0x10,uint16_t(201));assert(!probe(source.at()));put((uintptr_t)bytes.data()+0x10,uint16_t(50));
 put(source.at()+0x160,source.physics());assert(!probe(source.at()));put(source.at()+0x160,uintptr_t(0));
 // Marker replacement is accepted only for one validated leaf; foreign/grouped markers refuse.
 put(source.at()+0x118,int32_t(0));put(source.childAddress+8,uint32_t(1));put(source.childAddress+0x160,uintptr_t(0));put(source.childAddress+0x138,source.childAddress);put(source.childAddress+0x168,int32_t(0));assert(probe(source.at()));
 put(source.childAddress+8,uint32_t(0x44001));assert(!probe(source.at()));put(source.childAddress+8,uint32_t(0x42001));assert(!probe(source.at()));put(source.childAddress+8,uint32_t(1));
 put(source.childAddress+0x168,int32_t(1));assert(!probe(source.at()));put(source.childAddress+0x168,int32_t(0));put(source.childAddress+0x138,world);assert(!probe(source.at()));put(source.childAddress+0x138,source.childAddress);
 run("stale-marker","ignite()",90004);put(source.childAddress+0xc,uint32_t(1));drainEmitter();pulse(90005);assert(!torchCalls&&!inventorybridge::broken);put(source.childAddress+0xc,uint32_t(0));
 run("marker-light","ignite()",90006);assert(emitterCallback);host.clear();pulse(90007);assert(!inventorybridge::capture());drainEmitter();pulse(90008);assert(torchCalls==1&&poseCalls==1&&publishCalls==1&&!inventorybridge::broken);auto view=probe(source.at());assert(view&&view->pressed&&view->extinguishControl&&!view->igniteControl);
 assert(load("lit-metadata","timers.after(0,function()for _,o in ipairs(world.snapshot().objects)do if o.item_id==94 then assert(o.lit and o.ignite==nil and o.use==nil and o.extinguish)end end end)","world.read,world.devices,timers"));tick(90009);assert(host.enabled("lit-metadata"));
 put(world+0x3d0,uint32_t(1));put(world+0x3d4,uint32_t(0));run("full-removal","extinguish()",90010);assert(!emitterCallback&&!removeCalls&&!inventorybridge::broken);put(world+0x3d0,uint32_t(0));
 auto stopRollback=script("extinguish()");stopRollback.replace(stopRollback.find(";return"),7,";error('discard');return");assert(load("stop-rollback",stopRollback,"world.read,world.devices,timers"));tick(90011);assert(!emitterCallback&&!removeCalls);
 run("stale-effect","extinguish()",90012);put(flame+0x128,world);drainEmitter();pulse(90013);assert(!removeCalls&&!inventorybridge::broken);put(flame+0x128,source.at());
 run("stop","extinguish()",90014);assert(emitterCallback);host.clear();drainEmitter();pulse(90015);assert(removeCalls==1&&!inventorybridge::broken&&*(uint32_t*)(flame+8)&0x80000000u);assert(!probe(source.at()));drained();pulse(90016);assert(probe(source.at())->igniteControl);
 run("light-no-marker","use()",90017);drainEmitter();pulse(90018);assert(torchCalls==2&&publishCalls==2&&!inventorybridge::broken);
 std::ifstream example("inventory-tools.lua",std::ios::binary);assert(example);const std::string exampleScript{std::istreambuf_iterator<char>(example),std::istreambuf_iterator<char>()};std::vector<Module> config={{"config","return {enabled=true,action='extinguish',world_item_record=94}"}};
 host.clear();host.setTime(91000);assert(host.load("example",exampleScript.data(),exampleScript.size(),"inventory.read,timers,events.lifecycle",8*1024*1024,config,"world.read,world.devices"));host.notifyPhase("session",14);tick(94501);assert(emitterCallback&&host.enabled("example"));drainEmitter();pulse(94502);assert(removeCalls==2&&!inventorybridge::broken);drained();
 // Fire-source extinction works without enabling source ignition or torch lighting.
 constexpr uint32_t fireRaw=0x8000005fu;device(fireRaw,6,12);auto& fireSource=object(fireRaw,false);put(fireSource.at()+0x58,std::array<float,3>{1,0,0});fireFixture(fireSource.at(),0);worldemitters::torchPossible=false;worldemitters::firePossible=false;worldemitters::fires={};pulse(94503);put(fireSource.at()+0x160,fireSource.physics());put(fireSource.physics(),base+0x30e9e8);put(fireSource.physics()+0x18,fireSource.at());assert(!probe(fireSource.at()));put(fireSource.at()+0x160,uintptr_t(0));auto fireView=probe(fireSource.at());assert(fireView&&fireView->pressed&&fireView->extinguishControl&&!fireView->igniteControl);
 assert(load("source-stop","timers.after(0,function()for _,o in ipairs(world.snapshot().objects)do if o.item_id==95 then assert(o:extinguish());return end end error('missing fire')end)","world.read,world.devices,timers"));tick(94504);assert(emitterCallback);drainEmitter();pulse(94505);assert(removeCalls==3&&!inventorybridge::broken&&carried==initial);
 host.clear();worldMutationReady=nullptr;worldemitters::torchPossible=worldemitters::extinguishPossible=false;worldemitters::fires={};objects.resize(previousObjects);children.resize(previousChildren);put(world+0x118,int32_t(children.size())-1);put(world+0x120,(uintptr_t)children.data());std::puts("LUA_EXTINGUISH_TORCHES_PASSED");
}
static uintptr_t terminalCrystal=0,scannerChild=0;static int portalCalls=0,scannerCalls=0;static bool declinePortal=false;
static char portalFixture(uintptr_t node){
 ++portalCalls;if(failure=="teleporter-fault")RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);if(declinePortal)return 0;
 const auto old=*(uint32_t*)(node+0x180);constexpr uint32_t fresh=0x80000990u;definitions[{fresh,0x10000}]=definitions[{old,0x10000}];put(node+0x180,fresh);
 if(failure=="teleporter-decline-after-clone")return 0;
 const auto w=(uintptr_t)portalWindowBytes.data();put(w+0x79,uint8_t(1));put(w+0xb8,*(uint32_t*)(base+0x2efaa0));put(w+0x180,failure=="teleporter-bad-window"?world:node);put(w+0x188,terminalCrystal);put(w+0x1a0,chunkFixture(fresh,0x10000)+8);return 1;
}
static char scannerFixture(uintptr_t){
 ++scannerCalls;if(failure=="scanner-fault")RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);put(base+0x4f0c60,uint32_t(1));if(failure=="scanner-no-effect")return 1;
 const auto root=*(uintptr_t*)(player+0x18);scannerChild=(uintptr_t)scannerNode.data();put(scannerChild,base+0x34c710);put(scannerChild+0x128,root);put(root+0x118,int32_t(0));put(root+0x120,(uintptr_t)&scannerChild);return 1;
}
static void terminalTests(){
 using namespace scripting;host.clear();host.setTime(60000);portalCalls=scannerCalls=0;
 std::vector<uint8_t> gui(0x140),owner(0xc0);uintptr_t window=(uintptr_t)portalWindowBytes.data();put(base+0x49a930,(uintptr_t)gui.data());put((uintptr_t)gui.data()+0x128,(uintptr_t)owner.data());put((uintptr_t)owner.data()+0xa8,int32_t(1));put((uintptr_t)owner.data()+0xb0,(uintptr_t)&window);
 put(base+0x2efaa0,uint32_t(0x1234));put(base+0x34ccc8,16.0f);put(base+0x34ccd0,10000.0f);put(base+0x34cac0,1.0f);put(player+0xadc,std::array<float,3>{30,16,0});
 const auto playerNode=*(uintptr_t*)(player+0x18);put(playerNode+0x118,int32_t(-1));put(playerNode+0x120,uintptr_t(0));
 device(0x8000005au,2,15);auto& data=definitions[{0x8000005au,0x10000}];data.resize(56);put((uintptr_t)data.data()+4,uint32_t(56));auto& portal=object(0x8000005au,false);
 put(portal.at()+0x118,int32_t(0));put(portal.childAddress+0x140,char('['));put(portal.childAddress+0x64,std::array<float,3>{30,0,1});auto& crystal=object(0x1af,false);put(crystal.at()+0x64,std::array<float,3>{30,0,1});terminalCrystal=crystal.at();
 device(0x8000005bu,2,18);auto& scanner=object(0x8000005bu,false);put(scanner.at()+0x64,std::array<float,3>{40,0,0});put(scanner.at()+0x118,int32_t(0));put(scanner.childAddress+0x140,char('['));put(scanner.childAddress+0x64,std::array<float,3>{40,0,1});auto& scanCrystal=object(500,false);put(scanCrystal.at()+0x64,std::array<float,3>{40,0,1});
 worlddevices::teleporterPossible=worlddevices::scannerPossible=true;worlddevices::teleporter=portalFixture;worlddevices::scanner=scannerFixture;worldMutationReady=worlddevices::idle;
 const auto source=[](int record){return "timers.after(0,function()for _,o in ipairs(world.snapshot().objects)do if o.item_id=="+std::to_string(record)+" then assert(o:use());return end end error('missing target')end)";};
 const auto initial=carried;
 if(!failure.empty()){const bool scan=failure.find("scanner-")==0;assert(load("fault",source(scan?91:90),scan?"world.read,world.devices,timers":"world.read,world.teleporters,timers"));tick(60001);assert(inventorybridge::broken&&aiWriteBroken&&carried==initial);std::puts("LUA_TERMINAL_FAULT_PASSED");return;}
 assert(load("capabilities","timers.after(0,function()for _,o in ipairs(world.snapshot().objects)do if o.item_id==90 or o.item_id==91 then assert(o.use==nil)end end end)","world.read,world.use,timers"));tick(60001);assert(host.enabled("capabilities"));
 assert(load("rollback","timers.after(0,function()for _,o in ipairs(world.snapshot().objects)do if o.item_id==90 then o:use();error('discard')end end end)","world.read,world.teleporters,timers"));tick(60002);assert(portalCalls==0);
 put(crystal.at()+0x184,uint32_t(0x550000));pulse(60003);assert(!interactions::capture(portal.at()));put(crystal.at()+0x184,uint32_t(0x080000));
 pulse(60004);auto inv=inventorybridge::capture();assert(inv);const auto p=interactions::capture(portal.at());assert(p);InventoryChange change;change.inventory=inv->ticket;change.worldItem=p->ticket;change.op=InventoryOp::teleporterUse;
 put(crystal.at()+0x64,std::array<float,3>{30,0,2});assert(!inventorybridge::commit(change)&&portalCalls==0);put(crystal.at()+0x64,std::array<float,3>{30,0,1});
 declinePortal=true;assert(load("decline",source(90),"world.read,world.teleporters,timers"));tick(60005);assert(portalCalls==1&&!inventorybridge::broken);declinePortal=false;
 assert(load("portal",source(90),"world.read,world.teleporters,timers"));tick(60006);assert(portalCalls==2&&worlddevices::teleporterLease&&carried==initial&&*(uint32_t*)(crystal.at()+0x184)==0x080000);
 host.clear();pulse(60007);assert(!aiPulseReady&&!inventorybridge::capture()&&!worlddevices::idle());
 put((uintptr_t)portalWindowBytes.data()+0x79,uint8_t(0));pulse(60008);assert(aiPulseReady&&worlddevices::idle()&&inventorybridge::capture());
 assert(load("scanner",source(91),"world.read,world.devices,timers"));tick(60009);assert(scannerCalls==1&&worlddevices::scannerOwner==player&&carried==initial);
 host.clear();pulse(60010);assert(!aiPulseReady&&!inventorybridge::capture());put(playerNode+0x118,int32_t(-1));pulse(60011);assert(aiPulseReady&&worlddevices::idle());
 std::ifstream example("inventory-tools.lua",std::ios::binary);assert(example);const std::string script{std::istreambuf_iterator<char>(example),std::istreambuf_iterator<char>()};std::vector<Module> config={{"config","return {enabled=true,action='world_use',world_item_record=2448}"}};
 host.setTime(61000);assert(host.load("example",script.data(),script.size(),"inventory.read,timers,events.lifecycle",8*1024*1024,config,"world.read,world.teleporters,world.devices"));host.notifyPhase("session",10);tick(64501);assert(portalCalls==3&&host.enabled("example"));put((uintptr_t)portalWindowBytes.data()+0x79,uint8_t(0));pulse(64502);
 host.clear();config[0].source="return {enabled=true,action='world_use',world_item_record=91}";assert(host.load("example",script.data(),script.size(),"inventory.read,timers,events.lifecycle",8*1024*1024,config,"world.read,world.devices"));host.notifyPhase("session",11);tick(68002);assert(scannerCalls==2&&host.enabled("example"));put(playerNode+0x118,int32_t(-1));pulse(68003);assert(worlddevices::idle());
 host.clear();worldMutationReady=nullptr;worlddevices::teleporterPossible=worlddevices::scannerPossible=false;std::puts("LUA_TELEPORTER_SCANNER_PASSED");
}
int main(int argc,char** argv){
 using namespace scripting;std::vector<uint8_t> image(0x600000),scene(0x1e80),actor(0x1400),actorNode(0x210),actorStats(0x150),behavior(0x8a0),manager(0x100),drag(0x30),gui(0xc0),audio(0x30);
 base=(uintptr_t)image.data();world=(uintptr_t)scene.data();player=(uintptr_t)actor.data();stats=(uintptr_t)actorStats.data();uintptr_t sector=world;
 put(base+npc::off::world,world);put(base+npc::off::controlled,player);put(base+0x49b300,world);std::memcpy((void*)(world+0x140),"exanima01",9);put(world+8,uint32_t(0x11));put(world+0x220,world);put(world+0x3c4,int32_t(0));put(world+0x3c8,(uintptr_t)&sector);
 put(player+0x10a0,stats);put(player+0x580,player);put(player+0x12a4,uint32_t(2));put(player+0x18,(uintptr_t)actorNode.data());put((uintptr_t)actorNode.data()+0x130,world);put(player+0xd40,(uintptr_t)behavior.data());put((uintptr_t)behavior.data()+fp::game::behaviour::view,world);
 put(base+0x49a960,(uintptr_t)manager.data());put(base+0x4efdd0,(uintptr_t)drag.data());put(base+0x499790,(uintptr_t)gui.data());put(base+0x4eec90,(uintptr_t)audio.data());put(base+0x4eebd0,(uintptr_t)audio.data());put(base+0x2ae6d0,int32_t(256));
 device(50,1,0);device(51,1,0);device(60,1,8);device(62,1,14);device(63,1,9);
 auto& d=chunk(0x80000046u,0x40000,16);put((uintptr_t)d.data()+12,uint32_t(50));auto& chest=chunk(0x80000047u,0x20000,32);put((uintptr_t)chest.data()+16,uint16_t(80));device(0x80000048u,5,0,2);device(0x80000049u,5,0,4);
 auto& door=object(0x80000046u,true);put(door.at()+0x184,uint32_t(1));put(door.physics()+0x604,uint8_t(1));object(0x80000047u,false);auto& press=object(0x80000048u,true);object(0x80000049u,true);
 device(0x8000004au,3,1);auto& lever=object(0x8000004au,true);put(lever.physics()+0x570,lever.id);put(lever.physics()+0x524,-0.5f);put(lever.physics()+0x528,1.0f);put(lever.physics()+0x530,-0.25f);
 put(door.physics()+0x520,1.0f);put(door.physics()+0x5c8,1.0f);put(door.physics()+0x5fe,uint8_t(1));
 carried={{50,0,1,2},{51,0,3,4},{60,0,5,6},{60,0,7,8},{62,0,9,10},{63,0,11,12}};sync();
 gamebridge::backupDone=true;worldPossible=inventoryWritePossible=inventorybridge::possible=interactions::consumePossible=interactions::keyPossible=interactions::openPossible=interactions::buttonPossible=true;
 containerbridge::chunk=chunkFixture;inventorybridge::notify=notifyFixture;inventorybridge::recount=recountFixture;inventorybridge::extraCommit=interactions::commit;
 interactions::effect=effectFixture;interactions::unlock=unlockFixture;interactions::open=openFixture;interactions::isOpen=openCheckFixture;interactions::find=findFixture;interactions::trigger=triggerFixture;
 interactions::leverPossible=interactions::doorPossible=true;interactions::wake=wakeFixture;interactions::unlatch=unlatchFixture;interactions::doorCommand=commandFixture;
 interactions::slidingPossible=interactions::pairedPossible=interactions::lightLeverPossible=true;
 host.capabilities(inventoryRead|worldRead|itemsUse|worldUse|worldLevers|worldDoors|worldTeleporters|worldDevices|timerEvents|lifecycleEvents);host.setInventoryApi(inventorybridge::capture,inventorybridge::commit);host.setWorldProvider(captureWorld);host.setInteractionCapture(interactions::capture);beginAiPulse("session",0);
 if(argc>1){failure=argv[1];if(failure.find("torch-")==0||failure.find("extinguish-")==0){flamesTests();return 0;}if(failure.find("fire-")==0){fireTests();return 0;}if(failure.find("emitter-")==0){emitterTests();return 0;}if(failure.find("teleporter-")==0||failure.find("scanner-")==0){terminalTests();return 0;}if(failure=="pair-second-fault"||failure=="slider-unlatch-fault"||failure=="light-wake-fault"){deviceTests();return 0;}const bool control=failure.find("door-")==0||failure=="lever-wake-fault";if(control){put(door.at()+0x184,uint32_t(0));put(door.physics()+0x604,uint8_t(0));}
  const auto source=control?(failure=="lever-wake-fault"?leverUse:doorUse):failure=="unlock-fault"?keyed:failure=="button-fault"?button:salve;
  assert(load("fault","timers.after(0,function()"+source+" end)","inventory.read,world.read,items.use,world.use,world.levers,world.doors,timers"));tick(3001);assert(inventorybridge::broken&&aiWriteBroken&&carried.size()==6);std::puts("LUA_INTERACTION_FAULT_PASSED");return 0;}
 assert(load("rollback","timers.after(0,function()inventory.snapshot().carried[4]:use();error('discard')end)"));tick(3001);assert(effects==0&&carried.size()==6);
 assert(load("consume","timers.after(0,function()inventory.snapshot().carried[4]:use()end)","inventory.read,items.use,timers"));tick(3002);assert(effects==1&&carried.size()==5&&carried[2].item==60&&carried[2].x==5&&carried[3].item==62);
 assert(load("elixir","timers.after(0,function()inventory.snapshot().carried[4]:use()end)","inventory.read,items.use,timers"));tick(3003);assert(effects==2&&carried.size()==4&&(*(uint32_t*)(player+0xdf8)&0x800000));
 assert(load("wrong","timers.after(0,function()local i=inventory.snapshot();world.snapshot().objects[1]:use_item(i.carried[2])end)"));tick(3004);assert(unlocks==0&&*(uint8_t*)(door.physics()+0x603)==0);
 assert(load("key","timers.after(0,function()"+keyed+" end)","inventory.read,world.read,items.use,timers"));tick(3005);assert(unlocks==1&&carried.size()==4&&*(uint8_t*)(door.physics()+0x603)==1);
 assert(load("press","timers.after(0,function()"+button+" end)","world.read,world.use,timers"));tick(3006);assert(presses==1&&*(uint8_t*)(press.physics()+0x51c)==1);
 assert(load("release","timers.after(0,function()local w=world.snapshot();assert(w.objects[3].pressed);w.objects[3]:set_pressed(false);assert(w.objects[4].use==nil)end)","world.read,world.use,timers"));tick(3007);assert(presses==2&&*(uint8_t*)(press.physics()+0x51c)==0);
 assert(load("denied","timers.after(0,function()local i=inventory.snapshot();local w=world.snapshot();assert(i.carried[1].use==nil and w.objects[1].use_item==nil and w.objects[3].use==nil)end)","inventory.read,world.read,timers"));tick(3008);assert(host.enabled("denied"));
 assert(load("expired","timers.after(0,function()local row=inventory.snapshot().carried[3];timers.after(0,function()row:use()end)end)"));tick(3009);tick(3010);assert(!host.enabled("expired")&&effects==2);
 carried[2].state=0x20000;assert(load("spent","timers.after(0,function()inventory.snapshot().carried[3]:use()end)"));tick(3011);assert(effects==2);carried[2].state=0;sync();
 put(player+0x12a4,uint32_t(0));assert(load("no-pool","timers.after(0,function()"+salve+" end)"));tick(3012);assert(effects==2);put(player+0x12a4,uint32_t(2));
 pulse(3013);auto view=inventorybridge::capture();assert(view);auto inventoryTicket=view->ticket;const auto target=interactions::capture(press.at());assert(target);InventoryChange change;change.op=InventoryOp::worldUse;change.inventory=inventoryTicket;change.worldItem=target->ticket;
 put(press.at()+0x64,std::array<float,3>{201,0,0});assert(!inventorybridge::commit(change)&&presses==2);put(press.at()+0x64,std::array<float,3>{30,0,0});
 definitions[{0x80000048u,0x10000}][10]^=2;assert(!inventorybridge::commit(change)&&presses==2);definitions[{0x80000048u,0x10000}][10]^=2;
 badFind=true;assert(!inventorybridge::commit(change)&&!inventorybridge::broken);badFind=false;
 // Native dispatch prioritizes a door chunk even if a device chunk also exists.
 chunk(0x80000048u,0x40000,16);interactions::keyPossible=interactions::doorPossible=false;interactions::targets.clear();assert(!interactions::capture(press.at()));
 interactions::keyPossible=interactions::doorPossible=true;definitions.erase({0x80000048u,0x40000});
 assert(load("null-open","timers.after(0,function()world.snapshot().objects[2]:use()end)"));nullOpen=true;tick(3014);assert(opens==1&&!inventorybridge::broken);nullOpen=false;
 assert(load("open","timers.after(0,function()world.snapshot().objects[2]:use()end)","world.read,world.use,timers"));tick(3015);assert(opens==2&&opened);pulse(3016);assert(!inventorybridge::capture());put(base+0x499860,int32_t(0));
 assert(load("already-open","timers.after(0,function()world.snapshot().objects[2]:use()end)"));tick(3017);assert(opens==2&&!inventorybridge::broken);opened=false;
 put(objects[1]->at()+0x184,uint32_t(1));assert(load("locked-chest","timers.after(0,function()world.snapshot().objects[2]:use()end)"));tick(3018);assert(opens==2);put(objects[1]->at()+0x184,uint32_t(0));
 host.clear();host.setTime(10000);std::ifstream example("inventory-tools.lua",std::ios::binary);assert(example);const std::string source{std::istreambuf_iterator<char>(example),std::istreambuf_iterator<char>()};
 std::vector<Module> config={{"config","return {enabled=true,action='use',item_id=60}"}};
 const auto exampleLoad=[&](){return host.load("example",source.data(),source.size(),"inventory.read,timers,events.lifecycle",8*1024*1024,config,"world.read,items.use,world.use");};
 assert(exampleLoad());host.notifyPhase("session",1);tick(13501);assert(effects==3&&host.enabled("example")&&carried.size()==3);
 put(door.at()+0x184,uint32_t(1));put(door.physics()+0x603,uint8_t(0));config[0].source="return {enabled=true,action='use_item',item_id=50,world_item_record=70}";assert(exampleLoad());host.notifyPhase("session",2);tick(17002);assert(unlocks==2&&host.enabled("example"));
 config[0].source="return {enabled=true,action='world_use',world_item_record=72}";assert(exampleLoad());host.notifyPhase("session",3);tick(20503);assert(presses==3&&host.enabled("example"));
 config[0].source="return {enabled=true,action='set_pressed',world_item_record=72,pressed=false}";assert(exampleLoad());host.notifyPhase("session",4);tick(24004);assert(presses==4&&host.enabled("example"));host.clear();
 assert(load("lever-rollback","timers.after(0,function()world.snapshot().objects[5]:set_position(1);error('discard')end)","world.read,world.levers,timers"));tick(24005);assert(wakes==0);
 assert(load("lever","timers.after(0,function()"+leverUse+" end)","world.read,world.levers,timers"));tick(24006);assert(wakes==1&&*(int*)(lever.physics()+0x540)==1&&*(uint8_t*)(lever.physics()+0x544)==1&&carried.size()==3);
 assert(load("lever-toggle","timers.after(0,function()local o=world.snapshot().objects[5];assert(o.interaction_kind=='lever' and o.lever_position==0.25 and type(o.position)=='table' and o.target_position==1);o:use()end)","world.read,world.levers,timers"));tick(24007);assert(wakes==2&&*(int*)(lever.physics()+0x540)==0);
 assert(load("lever-invalid","timers.after(0,function()world.snapshot().objects[5]:set_position(2)end)","world.read,world.levers,timers"));tick(24008);assert(!host.enabled("lever-invalid")&&wakes==2);
 assert(load("lever-denied","timers.after(0,function()local o=world.snapshot().objects[5];assert(o.use==nil and o.set_position==nil)end)","world.read,world.use,timers"));tick(24009);assert(host.enabled("lever-denied"));
 pulse(24010);view=inventorybridge::capture();assert(view);const auto leverView=interactions::capture(lever.at());assert(leverView);change={};change.inventory=view->ticket;change.worldItem=leverView->ticket;change.op=InventoryOp::leverPosition;change.active=true;
 put(lever.physics()+0x5c8,uint8_t(1));assert(!inventorybridge::commit(change));put(lever.physics()+0x5c8,uint8_t(0));
 put(lever.physics()+0x5b8,int32_t(1));assert(!inventorybridge::commit(change));put(lever.physics()+0x5b8,int32_t(0));
 put(lever.physics()+0x528,0.0f);assert(!inventorybridge::commit(change));put(lever.physics()+0x528,1.0f);
 put(lever.physics()+0x430,int32_t(1));uintptr_t self=lever.physics();put(lever.physics()+0x438,(uintptr_t)&self);assert(!inventorybridge::commit(change)&&wakes==2);put(lever.physics()+0x430,int32_t(0));
 put(door.at()+0x184,uint32_t(0));put(door.physics()+0x604,uint8_t(0));put(door.physics()+0x603,uint8_t(0));
 assert(load("door-open","timers.after(0,function()"+doorUse+" end)","world.read,world.doors,timers"));tick(24011);assert(unlatches==1&&commands==1&&wakes==3&&*(int*)(door.physics()+0x528)==4&&carried.size()==3);
 assert(load("door-close","timers.after(0,function()world.snapshot().objects[1]:set_open(false)end)","world.read,world.doors,timers"));tick(24012);assert(commands==2&&wakes==4&&*(int*)(door.physics()+0x528)==8&&*(float*)(door.physics()+0x5d8)==1);
 assert(load("door-duplicate","timers.after(0,function()world.snapshot().objects[1]:set_open(false)end)","world.read,world.doors,timers"));tick(24013);assert(commands==2&&wakes==4);
 put(door.physics()+0x604,uint8_t(1));assert(load("door-locked","timers.after(0,function()assert(world.snapshot().objects[1].set_open==nil)end)","world.read,world.doors,timers"));tick(24014);assert(host.enabled("door-locked")&&commands==2);put(door.physics()+0x604,uint8_t(0));
 put(door.physics()+0x5f8,uint8_t(1));assert(load("door-sliding","timers.after(0,function()assert(world.snapshot().objects[1].set_open==nil)end)","world.read,world.doors,timers"));tick(24015);assert(host.enabled("door-sliding"));put(door.physics()+0x5f8,uint8_t(0));
 host.clear();host.setTime(25000);config[0].source="return {enabled=true,action='set_position',world_item_record=74,position=1}";
 const auto controlsExample=[&](){return host.load("example",source.data(),source.size(),"inventory.read,timers,events.lifecycle",8*1024*1024,config,"world.read,world.levers,world.doors");};
 assert(controlsExample());host.notifyPhase("session",5);tick(28501);assert(wakes==5&&host.enabled("example")&&*(int*)(lever.physics()+0x540)==1);
 config[0].source="return {enabled=true,action='set_open',world_item_record=70,open=true}";assert(controlsExample());host.notifyPhase("session",6);tick(32002);assert(wakes==6&&commands==3&&host.enabled("example"));host.clear();
 deviceTests();terminalTests();emitterTests();fireTests();flamesTests();std::puts("LUA_INTERACTIONS_PASSED");
}
