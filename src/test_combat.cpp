#include "plugin.cpp"
#include <cstdio>
#include "test_assert.h"
static int applies=0;
static unsigned char applyFixture(uintptr_t character,float,float,char canFall){++applies;return character==0x2000&&canFall?1:0;}
static void impactFixture(uintptr_t attacker){assert(attacker==0x1000&&combat::striking.attacker==0x1000);combat::applied(0x2000,0.5f,0.5f,0);}
static uint32_t clockSeconds=100;
static int64_t timeGetFixture(){return clockSeconds;}
static void timeSetFixture(uint32_t seconds){clockSeconds=seconds;}
static bool load(const char* id,const std::string& source,const char* caps){return host.load(id,source.data(),source.size(),caps);}
int main(){
 using namespace scripting;
 combat::original=applyFixture;combat::installed=true;host.capabilities(combatEvents|timerEvents);
 CharacterSnapshot snapshot;snapshot.ready=true;Character player,guard;
 player.nativeCharacter=0x1000;player.player=player.playerKnown=true;player.name="You";
 guard.nativeCharacter=0x2000;guard.name="Guard";guard.record=72;snapshot.characters={player,guard};
 // Nobody listens yet: the detour returns the game's result and queues nothing.
 assert(combat::applied(0x2000,1.f,2.f,1)==1&&applies==1);combat::deliver(host,&snapshot);assert(!combat::listening);
 assert(!load("denied","events.on('character.hit',function()end)","timers"));
 assert(load("listen",R"(local n=0
events.on('character.hit',function(e)
 n=n+1
 if n==1 then assert(e.record_id==72 and e.name=='Guard' and not e.is_player and e.health_damage==2 and e.balance_damage==1 and e.downed)
 elseif n==2 then assert(e.is_player and not e.downed and e.health_damage==0.5)
 else error('a third hit reached Lua') end
end))","events.combat"));
 combat::deliver(host,&snapshot);assert(combat::listening);
 assert(combat::applied(0x2000,1.f,2.f,1)==1);assert(combat::applied(0x1000,0.25f,0.5f,1)==0);assert(combat::applied(0x9999,1.f,1.f,0)==0);
 combat::deliver(host,&snapshot);assert(host.enabled("listen"));
 for(int i=0;i<70;++i)combat::applied(0x9999,1.f,1.f,0);combat::deliver(host,&snapshot);assert(host.enabled("listen"));
 combat::applied(0x2000,1.f,1.f,0);combat::deliver(host,(const scripting::CharacterSnapshot*)nullptr);combat::deliver(host,&snapshot);assert(host.enabled("listen"));
 combat::applied(0x2000,1.f,1.f,0);combat::deliver(host,&snapshot);assert(!host.enabled("listen"));
 combat::deliver(host,&snapshot);assert(!combat::listening);
 // With the impact detour, a hit inside a weapon impact names its attacker; any other hit is an impact.
 combat::impactInstalled=true;combat::originalImpact=impactFixture;
 assert(load("attacker",R"(local n=0
events.on('character.hit',function(e)
 n=n+1
 if n==1 then assert(e.source=='weapon' and e.attacker.name=='You' and e.attacker.is_player and e.name=='Guard')
 elseif n==2 then assert(e.source=='impact' and e.attacker==nil)
 else error('a third hit reached Lua') end
end))","events.combat"));
 combat::deliver(host,&snapshot);combat::impacted(0x1000);combat::applied(0x2000,1.f,1.f,0);combat::deliver(host,&snapshot);assert(host.enabled("attacker"));
 combat::applied(0x2000,1.f,1.f,0);combat::deliver(host,&snapshot);assert(!host.enabled("attacker"));
 std::puts("LUA_COMBAT_EVENTS_PASSED");
 // World time goes through the game's own getter and setter, inside a session pulse only.
 worldtime::get=timeGetFixture;worldtime::set=timeSetFixture;host.setTimeApi(worldtime::read,worldtime::write);host.capabilities(worldRead|worldTime|timerEvents);gamebridge::backupDone=true;
 assert(!load("early","world.set_time(5)","world.read,world.time,timers"));
 assert(load("clock","timers.after(0,function()assert(world.time()==100);assert(world.set_time(5000));assert(world.time()==5000)end)","world.read,world.time,timers"));
 assert(load("reader","timers.after(0,function()world.set_time(1)end)","world.read,timers"));
 assert(load("range","timers.after(0,function()world.set_time(-1)end)","world.read,world.time,timers"));
 worldPulse=true;host.setTime(30000);host.runTimers();worldPulse=false; assert(host.enabled("clock")&&clockSeconds==5000&&!host.enabled("reader")&&!host.enabled("range"));
 assert(load("idle","timers.after(0,function()assert(world.time()==nil and world.set_time(7)==false)end)","world.read,world.time,timers"));
 host.setTime(30001);host.runTimers();assert(host.enabled("idle")&&clockSeconds==5000);
 std::puts("LUA_WORLD_TIME_PASSED");
 // The shipped self-test addon: silent when disabled, and read-only and error-free when enabled.
 std::ifstream file("self-test.lua",std::ios::binary);assert(file);const std::string selfTest{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
 host.capabilities(worldRead|worldTime|timerEvents|lifecycleEvents|combatEvents);combat::installed=true;
 assert(host.load("self-test",selfTest.data(),selfTest.size(),"events.lifecycle,timers",8*1024*1024,{{"config","return {enabled=false}"}},"events.combat,world.read")&&!host.handlesCombat());
 assert(host.load("self-test",selfTest.data(),selfTest.size(),"events.lifecycle,timers",8*1024*1024,{{"config","return {enabled=true,every=1}"}},"events.combat,world.read")&&host.handlesCombat());
 worldPulse=true;host.notifyPhase("session",1);combat::deliver(host,&snapshot);combat::impacted(0x1000);combat::deliver(host,&snapshot);
 host.setTime(40000);host.runTimers();worldPulse=false;assert(host.enabled("self-test")&&clockSeconds==5000);
 std::puts("LUA_SELF_TEST_ADDON_PASSED");
}
