#include "plugin.cpp"
#include <cassert>
#include <cstdio>
#include "test_assert.h"
static int attempts=0,reloads=0,fired=0,surveyed=0,characterSurveys=0,awarenessSurveys=0;
static void capture(void*,const char*,const char*,const char* message){if(!std::strcmp(message,"timer pulse"))++fired;if(!std::strcmp(message,"world pulse"))++surveyed;if(std::strstr(message,"Character census:"))++characterSurveys;if(std::strstr(message,"NPC awareness:"))++awarenessSurveys;}
static uintptr_t forwarded(uintptr_t,uintptr_t){return 42;}
static bool activateFixture(){++attempts;if(attempts<2)return false;gamebridge::active=true;return true;}
static bool reloadFixture(const char*,const char*){++reloads;return true;}
int main(){
 runtimeEnabled=true;frameReady=true;nativePossible=true;itemActivator=activateFixture;host.setLogger(capture);
 session.configure(scripting::lifecycleEvents,true);reloadCallback=reloadFixture;reloadRuntime="fixture";
 assert(session.submit({"addon","events.on('item.use','test',function() return true end)","events.item_use,items.read","",{}},true));
 pumpPresentation(1,2,"menu",0,0);assert(attempts==0);
 pumpPresentation(1,1,"menu",0,0);pumpPresentation(1,1,"menu",0,499);assert(attempts==0);
 pumpPresentation(1,1,"menu",0,500);assert(attempts==1&&!host.handles("test"));
 assert(!worldPulse&&captureWorld()==nullptr);
 pumpPresentation(1,1,"menu",0,5499);assert(attempts==1);
 pumpPresentation(1,1,"menu",0,5500);assert(attempts==2&&host.handles("test"));
 gamebridge::originalUse=forwarded;assert(gamebridge::useBody(0,0)==42);
 assert(session.submit({"timer","timers.after(0,function()log.info('timer pulse')end)","timers","",{}},true));
 pumpPresentation(1,1,"menu",0,5550);assert(fired==0);
 pumpPresentation(2,1,"menu",0,5560);assert(fired==0);
 pumpPresentation(1,1,"menu",0,5560);assert(fired==1);
 assert(request_script_reload("all"));pumpPresentation(1,1,"session",123,5600);assert(reloads==0);
 pumpPresentation(1,1,"menu",0,6000);pumpPresentation(1,1,"menu",0,6500);assert(reloads==1);
 assert(disable());assert(!request_script_reload("all")&&!host.handles("test"));
 runtimeEnabled=true;installed=true;nativePossible=true;itemActivator=activateItems;
 session.configure(scripting::lifecycleEvents,true);
 assert(session.submit({"again","events.on('item.use','again',function() return true end)","events.item_use,items.read","",{}},true));
 pumpPresentation(1,1,"menu",0,7000);pumpPresentation(1,1,"menu",0,7500);
 assert(gamebridge::active&&host.handles("again"));assert(disable());
 std::vector<unsigned char> image(0x4f0bd8);unsigned char root[0x190]{},item[0x190]{};
 const uintptr_t itemPointer=(uintptr_t)item,rootPointer=(uintptr_t)root;
 const uintptr_t children=(uintptr_t)&itemPointer;const uint32_t flags=0x4000,id=99;
 std::memcpy(root+0x120,&children,8);std::memcpy(item+8,&flags,4);std::memcpy(item+0x180,&id,4);
 base=(uintptr_t)image.data();std::memcpy(image.data()+0x4f0bd0,&rootPointer,8);
 runtimeEnabled=true;nativePossible=false;worldPossible=true;host.setWorldProvider(captureWorld);
 session.configure(scripting::worldRead|scripting::lifecycleEvents|scripting::timerEvents,false);
 const std::string survey="events.on('runtime.phase',function(e) assert(world.snapshot()==nil);if e.phase=='session' then timers.after(0,function()local s=world.snapshot();assert(s and #s.objects==1 and s.objects[1].item_id==99);log.info('world pulse')end)end end)";
 assert(session.submit({"survey",survey,"world.read,events.lifecycle,timers","",{}},true));
 pumpPresentation(1,1,"menu",0,8000);pumpPresentation(1,1,"menu",0,8500);
 pumpPresentation(1,1,"session",123,8600);assert(surveyed==0);
 pumpPresentation(2,1,"session",123,8601);assert(surveyed==0);
 pumpPresentation(1,1,"session",123,8601);assert(surveyed==1&&!worldPulse&&captureWorld()==nullptr);
 assert(disable());
 std::vector<unsigned char> characterWorld(npc::off::worldList+8),character(0x1400),characterList(0x118);
 const uintptr_t characterWorldPointer=(uintptr_t)characterWorld.data(),characterPointer=(uintptr_t)character.data(),listPointer=(uintptr_t)characterList.data(),arrayPointer=(uintptr_t)&characterPointer;
 const int32_t count=1;
 std::memcpy(image.data()+npc::off::world,&characterWorldPointer,8);std::memcpy(image.data()+npc::off::controlled,&characterPointer,8);
 std::memcpy(characterWorld.data()+npc::off::worldList,&listPointer,8);std::memcpy(characterList.data()+npc::off::listCount,&count,4);std::memcpy(characterList.data()+npc::off::listArray,&arrayPointer,8);
 runtimeEnabled=true;charactersPossible=true;host.setCharacterProvider(captureCharacters);
 session.configure(scripting::charactersRead|scripting::lifecycleEvents|scripting::timerEvents,false);
 std::ifstream source("character-observer.lua",std::ios::binary);assert(source);const std::string observer{std::istreambuf_iterator<char>(source),std::istreambuf_iterator<char>()};
 assert(session.submit({"observer",observer,"characters.read,events.lifecycle,timers","",{}},true));
 pumpPresentation(1,1,"menu",0,9000);pumpPresentation(1,1,"menu",0,9500);
 pumpPresentation(1,1,"session",characterPointer,9600);
 pumpPresentation(2,1,"session",characterPointer,11600);assert(characterSurveys==0);
 pumpPresentation(1,1,"session",characterPointer,11600);assert(characterSurveys==1&&charactersCopy.characters.size()==1&&charactersCopy.characters[0].player);
 assert(!worldPulse&&captureCharacters()==nullptr);assert(disable());
 runtimeEnabled=true;host.setAwarenessProvider(captureAwareness);
 session.configure(scripting::awarenessRead|scripting::lifecycleEvents|scripting::timerEvents,false);
 std::ifstream awarenessSource("awareness-observer.lua",std::ios::binary);assert(awarenessSource);
 const std::string awarenessObserver{std::istreambuf_iterator<char>(awarenessSource),std::istreambuf_iterator<char>()};
 assert(session.submit({"awareness",awarenessObserver,"characters.awareness,events.lifecycle,timers","",{}},true));
 pumpPresentation(1,1,"menu",0,12000);pumpPresentation(1,1,"menu",0,12500);
 pumpPresentation(1,1,"session",characterPointer,12600);pumpPresentation(2,1,"session",characterPointer,14600);assert(awarenessSurveys==0);
 pumpPresentation(1,1,"session",characterPointer,14600);assert(awarenessSurveys==1&&!worldPulse&&captureAwareness()==nullptr);
 assert(disable());base=0;
 std::puts("LUA_PRESENTATION_PASSED: production pulse thread gating, settled-menu retry/backoff, pending activation, queued menu-only reload and disable");
}
