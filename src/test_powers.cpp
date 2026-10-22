#include "plugin.cpp"
#include <cstdio>
#include "test_assert.h"
// Owned-process fixtures: a committed block stands in for the image, small blocks for the player, world and lights.
static uintptr_t page(size_t size){return (uintptr_t)VirtualAlloc(nullptr,size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);}
template<class T>static void put(uintptr_t at,T value){std::memcpy((void*)at,&value,sizeof value);}
template<class T>static T get(uintptr_t at){T value{};std::memcpy(&value,(void*)at,sizeof value);return value;}
static int originals=0,creates=0,publishes=0,applies=0,poses=0,removes=0,probes=0;
static uintptr_t registry=0,sentinel=0,lastNode=0;static bool publishBroken=false;
static uint64_t originalCast(uint32_t){++originals;return 7;}
static uintptr_t createFixture(uintptr_t type,int){++creates;const auto node=page(0x1000);put(node,type);put(node+0x128,sentinel);return lastNode=node;}
// 0x5a420 -> 0x55960 records the owner at +0x130; +0x128 keeps the "+World" sentinel.
static void publishFixture(uintptr_t,uintptr_t* node,int){++publishes;if(!publishBroken)put(*node+0x130,registry);}
static void applyFixture(uintptr_t){++applies;}
static void poseFixture(uintptr_t){++poses;}
static void removeFixture(uintptr_t,uintptr_t node,int mode){assert(mode==1);++removes;put(node+8,get<uint32_t>(node+8)|0x80000000u);}
// A ceiling 50 cm above the head.
static char probeFixture(float* from,const float* up,float distance,float margin,uintptr_t,float* out){
 ++probes;assert(up[1]==1&&margin==10&&distance>50);out[1]=from[1]+50;return 1;
}
static void enqueueFixture(uintptr_t,char(*callback)(void*),void* payload){callback(payload);}
static bool load(const char* id,const std::string& source,const char* caps){return host.load(id,source.data(),source.size(),caps);}
static bool about(float a,float b){return std::fabs(a-b)<0.001f;}
int main(){
 using namespace scripting;
 // Pure helpers first.
 LightSpec s;s.forward=-40;s.right=30;s.up=60;
 auto t=powers::target({100,150,200},{0,0,1},s,0);assert(about(t[0],130)&&about(t[1],210)&&about(t[2],160));
 t=powers::target({0,0,0},{0,0,0},s,0);assert(about(t[0],30)&&about(t[2],-40));   // no facing: +z
 s.bob=10;t=powers::target({0,0,0},{0,0,1},s,0.75f);assert(about(t[1],70));
 auto e=powers::ease({0,0,0},{10,0,0},0,0.1f);assert(about(e[0],10));
 e=powers::ease({0,0,0},{10,0,0},1,1);assert(about(e[0],6.3212f));
 assert(about(powers::focusStep(0.15f,0,1,0),0.15f));
 assert(about(powers::focusStep(0.5f,0,0.3f,0),0.25f));            // never below 0.05 of ceiling
 assert(about(powers::focusStep(0,0.15f,0.85f,0),-0.15f));
 assert(about(powers::focusStep(0,0.15f,1,0),0));                   // ceiling already reset: nothing to give back
 assert(about(powers::focusStep(0,0.15f,0.95f,0),-0.05f));
 assert(powers::focusStep(std::numeric_limits<float>::quiet_NaN(),0,1,0)==0);

 // Fixture image and objects.
 base=page(0x600000);const auto player=page(0x2000),stats=page(0x400),world=page(0x2000),control=page(0x100),holder=page(0x400),head=page(0x100),headNode=page(0x100);
 registry=page(0x100);sentinel=page(0x100);put(sentinel+0x64,std::array<float,3>{10,0,0});   // never subtracted
 std::memcpy((void*)(world+0x140),"exanima01\0\0\0\0\0\0",16);
 put(base+0x4f0830,player);put(base+0x4f0bd0,world);put(base+0x4eff70,int(3));put(base+0x499860,int(0));put(base+0x4efd70,control);
 put(control+0x60,holder);put(holder+0x1f0,world);put(player+0x10a0,stats);put(player+0x11e0,head);put(head+0x18,headNode);
 put(headNode+0x20,std::array<float,3>{100,150,200});put(player+0xa08,std::array<float,3>{0,0,1});put(player+0xd78,1.0f);put(player+0xd84,0.0f);
 put(stats+0x198+4*2,uint32_t(0x20501|0x100000));put(stats+0x198+4*3,uint32_t(0x201));
 powers::original=originalCast;powers::installed=true;powers::lightsPossible=powers::focusPossible=true;
 powers::create=createFixture;powers::publish=publishFixture;powers::apply=applyFixture;powers::pose=poseFixture;powers::remove=removeFixture;powers::probe=probeFixture;powers::enqueue=enqueueFixture;
 host.capabilities(powerCast|focusHold|worldLights|timerEvents);

 // Nobody listens: every slot goes to the game.
 assert(powers::cast(2)==7&&originals==1);powers::deliver(host);assert(!powers::listening);
 assert(!load("denied","events.on('power.cast',function()end)","world.lights"));
 assert(!load("early","lights.set('orb',{})","world.lights"));
 assert(!load("early-focus","powers.hold_focus(0.1)","powers.focus"));
 assert(load("orb",R"(local on=false
events.on('power.cast',function(e)
 assert(e.power==0x20501 and e.school=='light' and e.mode=='continuous' and e.number==1)
 on=not on
 if on then
  assert(lights.set('orb',{color={0.8,0.9,1},range=2000,intensity=2,follow={forward=-40,right=30,up=60,smoothing=0}}))
  assert(powers.hold_focus(0.15) and powers.held_focus()==0.15 and #lights.list()==1)
 else
  assert(lights.remove('orb') and not lights.remove('orb'))
  powers.hold_focus(0)
 end
end))","powers.cast,powers.focus,world.lights"));
 assert(load("bad",R"(events.on('power.cast',function() lights.set('bad name!',{}) end))","powers.cast,world.lights"));
 powers::deliver(host);assert(powers::listening);

 // Shipped powers, the blocked flag and a possession ending stay with the game.
 assert(powers::cast(3)==7&&originals==2);
 put(player+0x1228,uint32_t(0x10));assert(powers::cast(2)==7&&originals==3);put(player+0x1228,uint32_t(0));
 put(player+0xdf8,uint32_t(0x200000));assert(powers::cast(2)==7&&originals==4);put(player+0xdf8,uint32_t(0));
 assert(powers::cast(9)==7&&originals==5);   // empty slot

 // The selected Light slot (-1) is taken and reaches Lua; the bad addon is disabled, the orb addon is not.
 assert(powers::cast(0xffffffffu)==1&&originals==5);powers::deliver(host);
 assert(host.enabled("orb")&&!host.enabled("bad"));
 auto lights=host.desiredLights();assert(lights.size()==1&&lights[0].addon=="orb"&&lights[0].light.range==2000&&about(host.desiredFocus(),0.15f));

 // Outside a session pulse nothing is touched.
 powers::step(host,"menu");assert(creates==0&&about(get<float>(player+0xd84),0));
 // First pulse: one light, clamped under the ceiling, at the head's world position plus offsets (nothing subtracted,
 // as the game's own light); focus held.
 powers::step(host,"session");
 assert(creates==1&&publishes==1&&probes==1&&poses==1&&!powers::pending);
 const auto node=lastNode;char name[16]{};std::memcpy(name,(void*)(node+0x140),16);assert(!std::strcmp(name,"+LuaLight0"));
 assert(get<uint32_t>(node+0x260)==3&&about(get<float>(node+0x224),2000)&&about(get<float>(node+0x210),0.8f));
 auto local=get<std::array<float,3>>(node+0x94);assert(about(local[0],130)&&about(local[1],200)&&about(local[2],160));
 assert(about(get<float>(player+0xd84),0.15f));
 // Later pulses move the same light, mark it as the game does, and do not hold focus twice.
 put(headNode+0x20,std::array<float,3>{200,150,200});powers::step(host,"session");
 assert(creates==1&&poses==2&&about(get<std::array<float,3>>(node+0x94)[0],230)&&(get<uint32_t>(node+0x268)&8)&&about(get<float>(player+0xd84),0.15f));

 // A node someone else renamed is forgotten, never written.
 std::memcpy((void*)(node+0x140),"+SomethingElse\0",16);const auto beforePoses=poses;powers::step(host,"session");
 assert(powers::faults==1&&creates==1&&poses==beforePoses&&removes==0);
 powers::step(host,"session");assert(creates==2&&poses==beforePoses+1);   // a fresh light replaces it
 const auto second=lastNode;

 // The level ends (loading pulse): the light belongs to it, so it is forgotten without a removal or write. The world
 // object itself stays the same, as in the game. Setup gives the next level a full ceiling; the hold starts again.
 powers::step(host,"loading");assert(removes==0&&creates==2);
 put(player+0xd78,1.0f);put(player+0xd84,0.0f);
 const auto beforeLevel=poses;powers::step(host,"session");
 assert(removes==0&&creates==3&&lastNode!=second&&poses==beforeLevel+1&&about(get<float>(player+0xd84),0.15f));
 // A different level name without a loading pulse: also forgotten unread. The hold is not taken twice.
 std::memcpy((void*)(world+0x140),"exanima02\0\0\0\0\0\0",16);powers::step(host,"session");
 assert(removes==0&&creates==4&&about(get<float>(player+0xd84),0.15f));
 // Possession switches the controlled body; the game copies ceiling and reserve over, and the hold is kept, not re-added.
 const auto body=page(0x2000);std::memcpy((void*)body,(void*)player,0x2000);put(base+0x4f0830,body);powers::step(host,"session");
 assert(about(get<float>(body+0xd84),0.15f));
 std::memcpy((void*)player,(void*)body,0x2000);put(base+0x4f0830,player);
 put(player+0xd84,0.0f);put(player+0xd78,0.85f);

 // Casting again: focus is given back at once; while the game's removal ring is full the light is kept and retried.
 put(world+0x3d0,uint32_t(1));put(world+0x3d4,uint32_t(0));
 powers::cast(2);powers::deliver(host);assert(host.desiredLights().empty()&&host.desiredFocus()==0);
 powers::step(host,"session");assert(removes==0&&powers::refused>=1&&about(get<float>(player+0xd84),-0.15f));
 put(world+0x3d0,uint32_t(0));powers::step(host,"session");assert(removes==1);
 // Nothing left: no more jobs.
 const auto quiet=poses;powers::step(host,"session");assert(poses==quiet&&removes==1);

 // A publish that does not record the owner: no light is tracked, and no more are made until the next level.
 publishBroken=true;powers::cast(2);powers::deliver(host);powers::step(host,"session");
 const auto brokenCreates=creates;assert(powers::faults==2);
 powers::step(host,"session");assert(creates==brokenCreates);
 publishBroken=false;powers::step(host,"loading");powers::step(host,"session");assert(creates==brokenCreates+1);

 // powers.caster(): bindings from Continuity II (technique byte 3 -> 2), plus one from equipment flag 0x100;
 // one running continuous power uses a binding; Abstraction and Clarity flags.
 powers::castPossible=true;host.setCasterRead(powers::readCaster);
 const auto running=page(0x100),list=page(0x100);put(running+0x10,uint32_t(0x20208));put(list,running);
 put(stats+0xe8,uint16_t(10000));put(stats+0xea,uint8_t(1));put(stats+0xeb,uint8_t(3));
 put(player+0xda4,int(1));put(player+0xda8,list);put(player+0xc40,uint32_t(0x400000|0x200000));put(player+0xd74,0.6f);
 assert(load("reader",R"(events.on('power.cast',function()
 local c=powers.caster()
 assert(c.bindings==2 and c.bindings_used==1 and c.techniques.abstraction and c.techniques.clarity and not c.techniques.iteration and not c.techniques.sustain)
 assert(math.abs(c.focus-0.6)<1e-6 and math.abs(c.focus_ceiling-0.85)<1e-6)
end))","powers.cast"));
 powers::cast(2);powers::deliver(host);assert(host.enabled("reader"));
 put(player+0x1228,uint32_t(0x100));scripting::CasterState c;assert(powers::readCaster(c)&&c.bindings==3);put(player+0x1228,uint32_t(0));
 put(stats+0xe8,uint16_t(4999));assert(powers::readCaster(c)&&c.bindings==0);
 assert(host.unload("reader"));
 // A failing handler leaves no light and no hold behind.
 assert(load("failing",R"(events.on('power.cast',function() lights.set('x',{}) powers.hold_focus(0.2) error('boom') end))","powers.cast,powers.focus,world.lights"));
 // (This cast turns the orb addon's light on again; the failing addon's own light and hold are not counted.)
 powers::cast(2);powers::deliver(host);assert(!host.enabled("failing")&&host.desiredLights().size()==1&&about(host.desiredFocus(),0.15f));
 std::puts("LUA_POWERS_PASSED");
}
