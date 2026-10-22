#include "plugin.cpp"
#include <cstdio>
#include "test_assert.h"
// Runtime 0.24.0 additions, against the real bridges and a fake image: version and capability reporting, load
// errors, items.stats, world.level, zone.enter, game.message, LuaLog/LuaEvent numbers, and hit weapon/part.
static std::vector<std::string> logged;
static void capture(void*,const char* id,const char* level,const char* message){logged.push_back(std::string(id)+"|"+level+"|"+message);}
static bool saw(const std::string& part){for(const auto& line:logged)if(line.find(part)!=std::string::npos)return true;return false;}
static size_t count(const std::string& part){size_t n=0;for(const auto& line:logged)n+=line.find(part)!=std::string::npos;return n;}
static bool load(const char* id,const std::string& source,const char* caps,const char* optional=""){return host.load(id,source.data(),source.size(),caps,8*1024*1024,{},optional);}
static void tick(uint64_t now){worldPulse=true;host.setTime(now);host.runTimers();worldPulse=false;}
template<class T>static void put(uint8_t* at,T value){std::memcpy(at,&value,sizeof value);}
// Item records: a chunk is {tag, size including this header, payload}.
static std::map<std::pair<uint32_t,uint32_t>,std::vector<uint8_t>> chunks;
static std::vector<uint8_t>& chunk(uint32_t id,uint32_t tag,size_t payload){auto& c=chunks[{id,tag}];c.assign(8+payload,0);put(c.data(),tag);put(c.data()+4,uint32_t(8+payload));return c;}
static uintptr_t lookupFixture(uint32_t id,uint32_t tag){const auto found=chunks.find({id,tag});return found==chunks.end()?0:(uintptr_t)found->second.data();}
static uint32_t previousLevel=0;static std::string previousText;
static uint32_t previousSink(uint32_t level,const char*,const char* text){previousLevel=level;previousText=uintptr_t(text)>0x10000?text:"";return level+1;}
static uintptr_t zoneCharacter=0;static uint32_t zoneRuns=0;
static void zoneFixture(uintptr_t character,uintptr_t){zoneCharacter=character;++zoneRuns;}
static unsigned targetCalls=0;
static void targetListener(void*,uintptr_t,uint32_t,const void*){++targetCalls;}
static void removerListener(void*,uintptr_t,uint32_t,const void*){remove_zone_listener(targetListener,nullptr);}
static int32_t numberFixture(uintptr_t,int index){return index==1?42:0;}
static unsigned char applyFixture(uintptr_t,float,float,char){return 0;}
static void impactFixture(uintptr_t){combat::applied(0x2000,0.5f,0.5f,0);}
int main(){
 using namespace scripting;setvbuf(stdout,nullptr,_IONBF,0);host.setLogger(capture);
 // A fake image: base points at a writable block large enough for the globals these bridges read.
 auto image=(uint8_t*)VirtualAlloc(nullptr,0x600000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(image);base=(uintptr_t)image;

 // Version and capabilities.
 host.capabilities(timerEvents|itemStats|worldRead);
 assert(load("version","assert(api.version==1 and api.revision==28 and api.runtime=='0.28.0' and api.addon=='version')\n"
  "local c=api.capabilities() assert(#c==2 and c[1]=='items.stats' and c[2]=='timers')","timers,items.stats"));
 assert(!load("typo","","timers,item.stats")&&host.error().find("did you mean 'items.stats'")!=std::string::npos);
 assert(!load("absent","","world.spawn")&&host.error().find("missing capability: world.spawn (")!=std::string::npos);
 assert(load("optional","","timers","world.raed")&&saw("optional|warn|unknown capability in optional: 'world.raed' (did you mean 'world.read'?)"));
 assert(!load("event","events.on('zone.entr',function()end)","timers")&&host.error().find("unknown event 'zone.entr'")!=std::string::npos);
 assert(!load("needs","events.on('zone.enter',function()end)","timers")&&host.error().find("needs the events.zone capability")!=std::string::npos);
 // A callback error names where it failed and how to recover.
 assert(load("broken","timers.after(0,function()error('boom')end)","timers"));tick(1000);
 assert(!host.enabled("broken")&&saw("broken|error|timer callback failed: broken:1: boom")&&saw("write its ID to reload.request"));
 std::puts("LUA_API_VERSION_PASSED");

 // items.stats through the record chunk lookup.
 auto& weapon=chunk(558,weaponTag,0x60);uint8_t* w=weapon.data()+8;put(w,uint32_t(0x11));
 const uint8_t values[]={150,60,137,100,1,100,180,100,0,50,0,0,0,0,0,0,128,3};std::memcpy(w+0x30,values,sizeof values);
 w[0x0c]=200;w[0x0d]=30;w[0x0e]=5;chunk(558,0x100000,0x30);
 auto& apparel=chunk(900,apparelTag,0x224);uint8_t* a=apparel.data()+8;put(a+0x150+4*8,uint32_t(85));put(a+0x150+4*9,uint32_t(40));
 a[0x1a0]=100;a[0x1a1]=70;a[0x1a2]=140;a[0x1a3]=35;a[0x1a5]=4;a[0x1a6]=3;a[0x1b0]=90;a[0x1b1]=255;a[0x1b2]=1;chunk(900,0x100000,0x30);
 chunk(901,weaponTag,0x10);chunk(901,0x100000,0x30);   // too short to be a weapon chunk
 itemstats::lookup=lookupFixture;host.setStatsRead(itemstats::read);
 assert(!load("early","items.stats(558)","items.stats"));
 assert(load("stats",R"(timers.after(0,function()
 local function near(a,b) return math.abs(a-b)<1e-9 end
 local s=items.stats(558) local w=s.weapon
 assert(s.item_id==558 and s.armour==nil and w.class==1 and w.polearm and w.sound==3)
 assert(near(w.heft,1.5) and near(w.balance,0.6) and near(w.effective_heft,1.04) and near(w.thrust,0.5))
 assert(near(w.parry_weight,0.5625) and near(w.power_multiplier,1) and near(w.recoil,0.94) and near(w.recoil_two_handed,0.748))
 assert(w.grip.kind==1 and near(w.grip.edge_start,0.5) and near(w.grip.edge_end,0.9) and near(w.grip.cut,0.5) and w.grip.crush==0)
 assert(near(w.grip.point,0.5*0.5^0.7) and w.alternate_grip==nil and #w.raw==18 and w.raw[1]==150)
 assert(s.condition.quality==200 and s.condition.wear==30 and s.condition.dirt==5)
 local c=items.stats(900).condition assert(c.quality==90 and c.wear==255 and c.dirt==1)
 local r=items.stats(900).armour
 assert(r.coverage.chest==85 and r.coverage.head==40 and r.coverage.pelvis==nil and near(r.impact,0.5) and near(r.cut,0.5))
 assert(near(r.crush,1) and near(r.pierce,0.25) and r.encumbrance==4 and r.layer==3 and r.vision_limit==0)
 assert(items.stats(901).weapon==nil and items.stats(12345)==nil and items.stats(0xC0000001)==nil)
 assert(not pcall(items.stats,0) and not pcall(items.stats,1.5))
end))","items.stats,timers"));
 tick(2000);assert(host.enabled("stats"));
 assert(load("menu","timers.after(0,function()assert(items.stats(558)==nil)end)","items.stats,timers"));host.setTime(2500);host.runTimers();assert(host.enabled("menu"));   // outside a session pulse
 assert(load("quota","timers.after(0,function()for i=1,33 do items.stats(558) end end)","items.stats,timers"));tick(3000);assert(!host.enabled("quota"));
 std::puts("LUA_ITEM_STATS_PASSED");

 // world.level from the world name, mode, level id and flow state globals.
 static uint8_t level[0x200]{};put(image+0x4f0bd0,(uintptr_t)level);std::memcpy(level+0x140,"exanima02",9);
 put(image+0x2b7540,int32_t(4));put(image+0x2e8210,int32_t(2));put(image+0x4eff70,int32_t(3));put(image+0x4f0830,uintptr_t(0x1234));
 host.setLevelRead(levelinfo::read);
 assert(load("level","timers.after(0,function()local l=world.level() assert(l.name=='exanima02' and l.mode=='story' and l.id==2 and l.loaded)end)","world.read,timers"));tick(4000);assert(host.enabled("level"));
 put(image+0x2b7540,int32_t(8));put(image+0x4eff70,int32_t(1));
 assert(load("arena","timers.after(0,function()local l=world.level() assert(l.mode=='arena' and not l.loaded)end)","world.read,timers"));tick(5000);assert(host.enabled("arena"));
 level[0x145]=7;assert(load("garbled","timers.after(0,function()assert(world.level()==nil)end)","world.read,timers"));tick(6000);assert(host.enabled("garbled"));
 level[0x145]='m';LevelInfo outside;assert(!levelinfo::read(outside));   // only inside a session pulse
 std::puts("LUA_WORLD_LEVEL_PASSED");

 // zone.enter: the detour copies {character, ZoneID} and always runs the game's zone script.
 CharacterSnapshot snapshot;snapshot.ready=true;Character player,guard;
 player.nativeCharacter=0x1000;player.player=player.playerKnown=true;player.name="You";player.record=1;
 guard.nativeCharacter=0x2000;guard.name="Guard";guard.record=72;snapshot.characters={player,guard};
 zones::original=zoneFixture;zones::installed=true;host.capabilities(zoneEvents|timerEvents);
 static uint8_t payload[0x40]{};put(payload+0xc,uint32_t(5));
 zones::entered(0x1000,(uintptr_t)payload);assert(zoneRuns==1&&zoneCharacter==0x1000);zones::deliver(host,&snapshot);assert(!zones::listening);
 assert(load("zones",R"(local n=0
events.on('zone.enter',function(e)
 n=n+1
 if n==1 then assert(e.zone_id==5 and e.is_player and e.name=='You' and e.record_id==1)
 elseif n==2 then assert(e.zone_id==6 and not e.is_player and e.name=='Guard')
 else error('a third zone entry reached Lua') end
end))","events.zone"));
 zones::deliver(host,&snapshot);assert(zones::listening);
 zones::entered(0x1000,(uintptr_t)payload);put(payload+0xc,uint32_t(6));zones::entered(0x2000,(uintptr_t)payload);zones::entered(0x9999,(uintptr_t)payload);
 assert(zoneRuns==4);zones::deliver(host,&snapshot);assert(host.enabled("zones"));
 zones::entered(0x2000,(uintptr_t)payload);zones::deliver(host,&snapshot);assert(!host.enabled("zones"));
 // Native listeners share the detour: called before the zone script; a faulting one is dropped.
 struct Seen {uintptr_t character=0;uint32_t zone=0;unsigned calls=0;} seen;
 const zones::Listener listener=[](void* context,uintptr_t character,uint32_t zone,const void*){auto s=(Seen*)context;s->character=character;s->zone=zone;++s->calls;};
 const zones::Listener faulting=[](void*,uintptr_t,uint32_t,const void*){*(volatile int*)nullptr=1;};
 assert(add_zone_listener(listener,&seen)&&add_zone_listener(listener,&seen)&&add_zone_listener(faulting,nullptr)&&zones::nativeCount==2);
 put(payload+0xc,uint32_t(9));zones::entered(0x2000,(uintptr_t)payload);
 assert(seen.calls==1&&seen.character==0x2000&&seen.zone==9&&zones::nativeCount==1&&zoneRuns==6);
 assert(remove_zone_listener(listener,&seen)&&!remove_zone_listener(listener,&seen));zones::entered(0x2000,(uintptr_t)payload);assert(seen.calls==1);
 // A listener that removes another during the same call: the removed one is not called from the stale copy.
 assert(add_zone_listener(removerListener,nullptr)&&add_zone_listener(targetListener,nullptr));zones::entered(0x2000,(uintptr_t)payload);
 assert(targetCalls==0&&zones::nativeCount==1&&remove_zone_listener(removerListener,nullptr));
 std::puts("LUA_ZONE_EVENTS_PASSED");

 // game.message: the slot is chained only from the game's logger, and the previous function still runs.
 const unsigned char logger[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0xa0,0x48,0x89,0x5d,0xc8,0x89,0xcb};
 std::memcpy(image+0x25d80,logger,sizeof logger);DWORD old=0;assert(VirtualProtect(image+0x25000,0x1000,PAGE_EXECUTE_READ,&old));
 put(image+0x2ab6b0,uintptr_t(0x777));assert(!gamemessages::install());   // someone else's function: left alone
 put(image+0x2ab6b0,base+0x25d80);assert(gamemessages::install()&&*(uintptr_t*)(image+0x2ab6b0)==(uintptr_t)&gamemessages::sink);
 gamemessages::previous=previousSink;host.capabilities(gameMessages);
 assert(load("messages","local n=0 events.on('game.message',function(m)n=n+1 assert(m.caption=='Script Error' and m.text=='Function \"ChangeRole\" does not exist.' and m.level==4)end)","events.messages"));
 for(int i=0;i<4;++i)assert(gamemessages::sink(4,"Script Error","Function \"ChangeRole\"\ndoes not exist.")==5&&previousLevel==4);
 assert(previousText=="Function \"ChangeRole\"\ndoes not exist.");
 gamemessages::deliver(host);assert(host.enabled("messages")&&count("game|error|Script Error: Function \"ChangeRole\" does not exist.")==3&&saw("(repeats are no longer logged)"));
 gamemessages::sink(2,nullptr,(const char*)0x10);gamemessages::deliver(host);assert(!host.enabled("messages"));   // a bad pointer copies as empty
 std::puts("LUA_GAME_MESSAGES_PASSED");

 // Script commands: LuaEvent evaluates its value with the game's reader; LuaLog writes a line; refusals are logged.
 struct Vm {std::vector<uint8_t> bytes=std::vector<uint8_t>(0x200);std::vector<std::string> texts;
  uintptr_t call(std::initializer_list<const char*> arguments){
   texts.assign(arguments.begin(),arguments.end());const int n=int(texts.size());std::memcpy(bytes.data()+0x50,&n,4);
   for(size_t i=0;i<texts.size();++i){const char* t=texts[i].data();const uint32_t size=uint32_t(texts[i].size());std::memcpy(bytes.data()+0x58+i*0x10,&t,8);std::memcpy(bytes.data()+0x60+i*0x10,&size,4);}
   return (uintptr_t)bytes.data();}
 } vm;
 scriptcommands::registered=true;runtimeEnabled=true;scriptcommands::number=numberFixture;host.capabilities(scriptEvents|scriptFlags|timerEvents);host.setFlagApi(scriptcommands::getFlag,scriptcommands::setFlag);
 assert(load("numbers","events.on('script.event',function(e)assert(e.name=='zone_entered' and e.value=='ZoneID' and e.number==42)end)","events.script"));
 scriptcommands::deliver(host,true);
 assert(scriptcommands::luaEvent(vm.call({"zone_entered","ZoneID"}))==1&&scriptcommands::luaLog(vm.call({"reached","branch","3"}))==1);
 for(int i=0;i<4;++i)assert(scriptcommands::luaEvent(vm.call({"bad name"}))==0);
 assert(scriptcommands::luaFlag(vm.call({"never_set"}))==0);
 scriptcommands::deliver(host,true);assert(host.enabled("numbers")&&saw("script|info|reached branch 3"));
 assert(count("script|warn|LuaEvent(bad name): the first argument must be a name")==3&&saw("script|info|LuaFlag(never_set): read before any addon set it"));
 assert(scriptcommands::luaFlag(vm.call({"never_set"}))==0);scriptcommands::deliver(host,true);assert(count("LuaFlag(never_set)")==1);   // hints once
 scriptcommands::number=nullptr;
 assert(load("plain","events.on('script.event',function(e)assert(e.number==nil and e.value=='7')end)","events.script"));
 scriptcommands::deliver(host,true);scriptcommands::luaEvent(vm.call({"zone_entered","7"}));scriptcommands::deliver(host,true);assert(host.enabled("plain"));
 std::puts("LUA_SCRIPT_COMMANDS_024_PASSED");

 // character.hit names the delivering weapon's item record and the struck body part.
 static uint8_t attacker[0xd00]{},blade[0x500]{},root[0x500]{},node[0x200]{},part[0x500]{};
 put(attacker+0xc38,(uintptr_t)blade);put(blade+0x428,(uintptr_t)root);put(root+0x428,(uintptr_t)root);put(root+0x18,(uintptr_t)node);put(node+0x180,uint32_t(0x80000123u));
 put(attacker+0xc20,(uintptr_t)part);put(part+0x468,int32_t(8));
 combat::original=applyFixture;combat::originalImpact=impactFixture;combat::installed=combat::impactInstalled=true;host.capabilities(combatEvents);
 Character fighter;fighter.nativeCharacter=(uintptr_t)attacker;fighter.name="Fighter";snapshot.characters.push_back(fighter);
 assert(load("hits","events.on('character.hit',function(h)assert(h.source=='weapon' and h.attacker.name=='Fighter' and h.weapon_id==0x80000123 and h.part==9 and h.part_name=='chest')end)","events.combat"));
 combat::deliver(host,&snapshot);combat::impacted((uintptr_t)attacker);combat::deliver(host,&snapshot);assert(host.enabled("hits"));
 put(root+0x428,(uintptr_t)blade);put(blade+0x428,(uintptr_t)root);put(part+0x468,int32_t(-1));   // a parent loop and a non-part body: both unknown
 assert(load("unknown","events.on('character.hit',function(h)assert(h.weapon_id==nil and h.part==nil)end)","events.combat"));
 combat::deliver(host,&snapshot);combat::impacted((uintptr_t)attacker);combat::deliver(host,&snapshot);assert(host.enabled("unknown"));
 std::puts("LUA_HIT_DETAILS_PASSED");
}
