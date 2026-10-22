#include "plugin.cpp"
#include <cstdio>
#include "test_assert.h"
static void put(uintptr_t at,const void* value,size_t size){std::memcpy((void*)at,value,size);}
template<class T>static void put(uintptr_t at,T value){put(at,&value,sizeof value);}
static bool load(const char* id,const std::string& code,const std::string& caps="characters.read,characters.ai.write,timers"){
 return host.load(id,code.data(),code.size(),caps);
}
static int writeCalls=0;
static bool failThirdWrite(uintptr_t address,const float& value){if(++writeCalls==3)return false;return writeAiFloat(address,value);}
static void tick(uint64_t now){
 worldPulse=true;beginAiPulse("session",now);invalidateCharacterCopies();host.setTime(now);host.runTimers();worldPulse=false;
}
int main(){
 using namespace scripting;
 std::vector<unsigned char> image(0x600000),world(0x1c30),player(0x1400),character(0x1400),ai(0xa40),stats(0x140),list(0x118);
 base=(uintptr_t)image.data();const auto w=(uintptr_t)world.data(),p=(uintptr_t)player.data(),c=(uintptr_t)character.data(),a=(uintptr_t)ai.data(),s=(uintptr_t)stats.data(),l=(uintptr_t)list.data();
 uintptr_t rows[]={c,p};put(base+npc::off::world,w);put(base+npc::off::controlled,p);put(base+0x49b300,w);
 std::memcpy(world.data()+0x140,"exanima01",9);put(w+npc::off::worldList,l);put(l+npc::off::listCount,int32_t(2));put(l+npc::off::listArray,(uintptr_t)rows);
 put(c+npc::off::ai,a);put(c+npc::off::stats,s);put(a,base+npc::off::motileAiVmt);put(a+npc::off::character,c);
 std::memcpy(stats.data()+npc::off::name,"Guard",5);put(s+npc::off::recordId,uint32_t(17));
 const hot::AiConstants constants{0.01,1,.5,2,3,.125,1.0/3};
 put(base+0x30a050,constants.hundredth);put(base+0x30a068,constants.one);put(base+0x30a038,constants.half);put(base+0x30a070,constants.two);
 put(base+0x30a1a0,constants.three);put(base+0x30a328,constants.eighth);put(base+0x30a490,constants.third);
 host.capabilities(charactersRead|aiWrite|timerEvents);host.setCharacterProvider(captureCharacters);host.setAiCommit(commitAi);host.setAiCapture(captureAiTarget);
 charactersPossible=aiWritePossible=true;gamebridge::backupDone=true;beginAiPulse("session",0);
 auto skill=[&](){float value=0;std::memcpy(&value,(void*)(a+npc::off::skill),4);return value;};
 const std::string set="local c=characters.snapshot().characters[1];assert(c.set_ai);c:set_ai{skill=80,attribute2=40,attribute3=-20}";
 assert(load("success","timers.after(0,function()"+set+"end)"));tick(3001);assert(skill()==0.8f);
 const auto expected=hot::aiValues(hot::aiCompute({80,40,-20},constants,0,false));
 for(size_t i=0;i<expected.size();++i){float value=0;std::memcpy(&value,(void*)(a+hot::aiOffsets[i]),4);assert(value==expected[i]);}
 assert(load("rollback","timers.after(0,function()local c=characters.snapshot().characters[1];c:set_ai{skill=10,attribute2=0,attribute3=0};error('rollback')end)"));tick(3002);assert(skill()==0.8f&&!host.enabled("rollback"));
 assert(load("stale","timers.after(0,function()local c=characters.snapshot().characters[1];timers.after(0,function()c:set_ai{skill=1,attribute2=0,attribute3=0}end)end)"));tick(3003);tick(3004);assert(skill()==0.8f&&!host.enabled("stale"));
 assert(load("denied","timers.after(0,function()assert(characters.snapshot().characters[1].set_ai==nil)end)","characters.read,timers"));tick(3005);assert(host.enabled("denied"));
 assert(load("invalid","timers.after(0,function()characters.snapshot().characters[1]:set_ai{skill=101,attribute2=0,attribute3=0}end)"));tick(3006);assert(skill()==0.8f&&!host.enabled("invalid"));
 assert(load("duplicate","timers.after(0,function()local c=characters.snapshot().characters[1];c:set_ai{skill=10,attribute2=0,attribute3=0};c:set_ai{skill=20,attribute2=0,attribute3=0}end)"));tick(3007);assert(skill()==0.8f&&!host.enabled("duplicate"));
 worldPulse=true;beginAiPulse("session",3007);invalidateCharacterCopies();const auto snapshot=captureCharacters();assert(snapshot&&aiTargets.empty());
 const auto ticket=captureAiTarget(snapshot->characters[0].nativeCharacter);assert(ticket);
 const std::vector<AiChange> change={{ticket,{10,0,0}}};
 put(s+npc::off::recordId,uint32_t(18));assert(!commitAi(change)&&skill()==0.8f);put(s+npc::off::recordId,uint32_t(17));
 rows[0]=p;assert(!commitAi(change)&&skill()==0.8f);rows[0]=c;
 put(a+npc::off::character,p);assert(!commitAi(change)&&skill()==0.8f);put(a+npc::off::character,c);
 gamebridge::backupDone=false;assert(!commitAi(change));gamebridge::backupDone=true;
 put(base+0x30a068,2.0f);assert(!commitAi(change));put(base+0x30a068,1.0f);
 assert(!commitAi({change[0],{999999,{1,0,0}}})&&skill()==0.8f);
 assert(commitAi(change)&&skill()==0.1f);
 const auto beforeFailure=hot::aiValues(hot::aiCompute({10,0,0},constants,0,false));
 aiWriter=failThirdWrite;assert(!commitAi({{ticket,{90,70,50}}})&&aiWriteBroken);aiWriter=writeAiFloat;
 for(size_t i=0;i<beforeFailure.size();++i){float value=0;std::memcpy(&value,(void*)(a+hot::aiOffsets[i]),4);assert(value==beforeFailure[i]);}
 assert(!commitAi(change));aiWriteBroken=false;worldPulse=false;assert(!commitAi(change));
 gamebridge::backupDone=false;beginAiPulse("session",4000);assert(!aiPulseReady);gamebridge::backupDone=true;
 put(base+0x49b300,w+8);beginAiPulse("session",4001);assert(!aiPulseReady);put(base+0x49b300,w);beginAiPulse("session",4002);assert(!aiPulseReady);
 std::memcpy((void*)(base+0x2b7690),"exanima02",9);beginAiPulse("session",7002);assert(!aiPulseReady);
 beginAiPulse("menu",8000);assert(!aiPulseReady&&aiTargets.empty());
 std::memset((void*)(base+0x2b7690),0,16);beginAiPulse("session",10000);host.setTime(10000);
 std::ifstream example("practice-ai.lua",std::ios::binary);assert(example);const std::string source{std::istreambuf_iterator<char>(example),std::istreambuf_iterator<char>()};
 host.capabilities(charactersRead|aiWrite|timerEvents|lifecycleEvents);assert(load("practice",source,"characters.read,characters.ai.write,timers,events.lifecycle"));
 host.notifyPhase("session",1);tick(13501);assert(skill()==0.4f);
 std::puts("LUA_AI_WRITE_PASSED");
}
