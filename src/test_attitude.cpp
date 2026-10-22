#include "plugin.cpp"
#include <cstdio>
#include "test_assert.h"
template<class T>static void put(uintptr_t at,T value){std::memcpy((void*)at,&value,sizeof value);}
template<class T>static T get(uintptr_t at){T value{};std::memcpy(&value,(void*)at,sizeof value);return value;}
static int concernCalls=0;static bool faultConcern=false;
static void concernFixture(uintptr_t entry,float value){
 ++concernCalls;const float before=get<float>(entry+0x1c);auto flags=get<uint32_t>(entry+0x50);
 if(before<1.2f&&value>=1.2f)flags|=4;else if(before>=1.2f&&before<4&&value>=4)flags|=8;
 put(entry+0x50,flags);put(entry+0x1c,value);
 if(faultConcern)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
}
static bool load(const char* id,const std::string& source,const std::string& caps="characters.awareness,characters.attitude.write,timers"){
 return host.load(id,source.data(),source.size(),caps);
}
static void tick(uint64_t now){worldPulse=true;beginAiPulse("session",now);invalidateCharacterCopies();host.setTime(now);host.runTimers();worldPulse=false;}
int main(){
 using namespace scripting;
 const auto allocation=VirtualAlloc(nullptr,0x600000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);assert(allocation);base=(uintptr_t)allocation;
 std::vector<unsigned char> world(0x1c30),player(0x1400),character(0x1400),ai(0xa40),stats(0x140),list(0x118),behavior(0x20);
 const auto w=(uintptr_t)world.data(),p=(uintptr_t)player.data(),c=(uintptr_t)character.data(),a=(uintptr_t)ai.data(),s=(uintptr_t)stats.data(),l=(uintptr_t)list.data(),b=(uintptr_t)behavior.data();
 uintptr_t rows[]={c,p};put(base+npc::off::world,w);put(base+npc::off::controlled,p);put(base+0x49b300,w);
 std::memcpy(world.data()+0x140,"exanima01",9);put(w+npc::off::worldList,l);put(l+npc::off::listCount,int32_t(2));put(l+npc::off::listArray,(uintptr_t)rows);
 put(c+npc::off::ai,a);put(c+npc::off::stats,s);put(a,base+npc::off::motileAiVmt);put(a+npc::off::character,c);
 std::memcpy(stats.data()+npc::off::name,"Guard",5);put(s+npc::off::recordId,uint32_t(17));
 put(a+npc::off::memoryCount,int32_t(1));const auto e=a+npc::off::memory;
 put(e+npc::off::entryOther,b);put(b+npc::off::behaviourKind,uint32_t(1));put(b+npc::off::behaviourCharacter,p);
 const unsigned char prefix[]={0x48,0x89,0xc8,0x0f,0x28,0xc1,0xf3,0x0f,0x10,0x48,0x1c,0x0f,0x2f,0x0d};
 std::memcpy((void*)(base+0x26f160),prefix,sizeof prefix);put(base+0x30a300,15.f);put(base+0x2eefb0,1.2f);put(base+0x2eefa0,4.f);
 assert(attitudeContract());put(base+0x30a300,14.f);assert(!attitudeContract());put(base+0x30a300,15.f);
 charactersPossible=attitudeWritePossible=true;concernSetter=concernFixture;gamebridge::backupDone=true;
 host.capabilities(awarenessRead|attitudeWrite|timerEvents);host.setAwarenessProvider(captureAwareness);host.setAiCapture(captureAiTarget);host.setAiCommit(commitAi);
 beginAiPulse("session",0);
 assert(load("raise","timers.after(0,function()local c=characters.awareness().characters[1];assert(c.set_ai==nil);c:set_attitude{hostility=3.5,concern=2}end)"));tick(3001);
 assert(get<float>(e+0x24)==3.5f&&get<float>(e+0x1c)==2&&get<uint32_t>(e+0x50)==4&&concernCalls==1);
 assert(load("cross","timers.after(0,function()characters.awareness().characters[1]:set_attitude{concern=5}end)"));tick(3002);
 assert(get<float>(e+0x24)==3.5f&&get<float>(e+0x1c)==5&&get<uint32_t>(e+0x50)==12&&concernCalls==2);
 assert(load("merge","timers.after(0,function()local c=characters.awareness().characters[1];c:set_attitude{hostility=1};c:set_attitude{concern=0};error('discard')end)"));tick(3003);assert(get<float>(e+0x24)==3.5f&&get<float>(e+0x1c)==5);
 assert(load("range","timers.after(0,function()characters.awareness().characters[1]:set_attitude{hostility=21}end)"));tick(3004);assert(!host.enabled("range")&&get<float>(e+0x24)==3.5f);
 assert(load("denied","timers.after(0,function()assert(characters.awareness().characters[1].set_attitude==nil)end)","characters.awareness,timers"));tick(3005);assert(host.enabled("denied"));
 assert(load("stale","timers.after(0,function()local c=characters.awareness().characters[1];timers.after(0,function()c:set_attitude{hostility=0}end)end)"));tick(3006);tick(3007);assert(!host.enabled("stale")&&get<float>(e+0x24)==3.5f);
 worldPulse=true;beginAiPulse("session",3008);const auto ticket=captureAiTarget(c);assert(ticket);AiChange change;change.target=ticket;change.ai=false;change.hostility=change.concern=true;change.hostilityValue=1;change.concernValue=0;
 put(a+npc::off::memoryCount,int32_t(0));assert(!commitAi({change})&&get<float>(e+0x24)==3.5f);put(a+npc::off::memoryCount,int32_t(1));
 put(b+npc::off::behaviourCharacter,c);assert(!commitAi({change}));put(b+npc::off::behaviourCharacter,p);
 put(s+npc::off::recordId,uint32_t(18));assert(!commitAi({change}));put(s+npc::off::recordId,uint32_t(17));
 const hot::AiConstants constants{0.01,1,.5,2,3,.125,1.0/3};
 put(base+0x30a050,constants.hundredth);put(base+0x30a068,constants.one);put(base+0x30a038,constants.half);put(base+0x30a070,constants.two);
 put(base+0x30a1a0,constants.three);put(base+0x30a328,constants.eighth);put(base+0x30a490,constants.third);aiWritePossible=true;
 change.ai=true;change.values={80,0,0};
 faultConcern=true;assert(!commitAi({change})&&aiWriteBroken);assert(get<float>(e+0x24)==3.5f&&get<float>(e+0x1c)==5&&get<uint32_t>(e+0x50)==12&&get<float>(a+npc::off::skill)==0);
 faultConcern=false;assert(!commitAi({change}));aiWriteBroken=false;worldPulse=false;
 host.capabilities(awarenessRead|attitudeWrite|aiWrite|timerEvents);
 assert(load("combined","timers.after(0,function()local c=characters.awareness().characters[1];c:set_ai{skill=75,attribute2=10,attribute3=20};c:set_attitude{hostility=2};c:set_attitude{concern=1}end)","characters.awareness,characters.ai.write,characters.attitude.write,timers"));tick(3009);
 assert(get<float>(a+npc::off::skill)==0.75f&&get<float>(e+0x24)==2&&get<float>(e+0x1c)==1);
 host.setTime(10000);host.capabilities(awarenessRead|attitudeWrite|timerEvents|lifecycleEvents);
 std::ifstream example("encounter-truce.lua",std::ios::binary);assert(example);const std::string source{std::istreambuf_iterator<char>(example),std::istreambuf_iterator<char>()};
 assert(load("truce",source,"characters.awareness,characters.attitude.write,timers,events.lifecycle"));host.notifyPhase("session",1);tick(13501);
 assert(get<float>(e+0x24)==0&&get<float>(e+0x1c)==0&&host.enabled("truce"));
 VirtualFree(allocation,0,MEM_RELEASE);std::puts("LUA_ATTITUDE_PASSED");
}
