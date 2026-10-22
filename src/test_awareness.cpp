#include "script_host.h"
#include <map>
#include <cstdio>
#include "test_assert.h"
static std::map<uintptr_t,std::vector<unsigned char>> heap;
static uintptr_t denied=0;static unsigned reads=0;
struct Memory {
 template<class T>bool get(uintptr_t address,T& value)const{
  ++reads;if(address==denied)return false;
  for(const auto& block:heap)if(address>=block.first&&address-block.first<=block.second.size()&&sizeof value<=block.second.size()-(address-block.first)){
   std::memcpy(&value,block.second.data()+address-block.first,sizeof value);return true;
  }return false;
 }
};
template<class T>static void put(uintptr_t block,size_t offset,T value){std::memcpy(heap[block].data()+offset,&value,sizeof value);}
static scripting::CharacterSnapshot copy;
static const scripting::CharacterSnapshot* provider(){return &copy;}
static void load(scripting::Host& host,const std::string& code,bool expected=true,const std::string& caps="characters.awareness"){
 const bool result=host.load("test",code.data(),code.size(),caps);if(result!=expected)std::fprintf(stderr,"%s\n",host.error().c_str());assert(result==expected);
}
int main(){
 using namespace scripting;constexpr uintptr_t image=0x100000,character=0x200000,player=0x300000,ai=0x400000,behavior=0x500000;
 heap[character].resize(0x1400);heap[ai].resize(0xa40);heap[behavior].resize(0x20);
 put(character,npc::off::ai,ai);put(ai,0,image+npc::off::motileAiVmt);put(ai,npc::off::character,character);
 put(ai,npc::off::level,int32_t(3));put(ai,npc::off::mode,int32_t(4));put(ai,npc::off::handler,image+npc::off::handlerEngage);
 put(ai,npc::off::stress,2.5f);put(ai,npc::off::shock,1.0f);put(ai,npc::off::courage,0.25f);put(ai,npc::off::wantsAttack,uint8_t(1));
 put(ai,npc::off::attackKind,uint32_t(0x10));put(ai,npc::off::oppositeSide,int32_t(2));
 put(character,npc::off::opponent,player);put(ai,npc::off::memoryCount,int32_t(1));
 put(behavior,npc::off::behaviourKind,uint32_t(1));put(behavior,npc::off::behaviourCharacter,player);
 put(ai,npc::off::memory+npc::off::entryOther,behavior);put(ai,npc::off::memory+npc::off::entryTrust,-62.5f);
 put(ai,npc::off::memory+npc::off::entryHesitation,0.5f);
 const Memory memory;auto brain=readAwareness(memory,image,character,player);
 assert(brain.available&&brain.labels["engagement"]=="fight"&&brain.labels["mode"]=="follow"&&brain.labels["handler"]=="engage");
 assert(brain.values["fear"]==3.25f&&brain.flags["fights_player"]&&brain.towardPlayer["trust"]==-62.5f);
 assert(brain.labels["attack"]=="thrust"&&brain.flags["opposite_side"]);
 Character row;row.awareness=brain;copy.ready=true;copy.characters.push_back(row);
 Host host(awarenessRead|charactersRead);host.setAwarenessProvider(provider);host.setCharacterProvider(provider);
 load(host,"local s=characters.awareness();local a=s.characters[1].awareness;assert(a.engagement=='fight' and a.mode=='follow' and a.attack=='thrust' and a.opposite_side and a.fear==3.25 and a.toward_player.trust==-62.5 and a.wants_attack and a.fights_player);a.toward_player.trust=99;assert(characters.awareness().characters[1].awareness.toward_player.trust==-62.5)");
 load(host,"for i=1,2 do characters.snapshot();characters.awareness() end;characters.snapshot()",false,"characters.read,characters.awareness");
 load(host,"characters.awareness()",false,"");load(host,"characters.awareness()",false,"characters.read");
 load(host,"assert(characters.snapshot().characters[1].awareness==nil)",true,"characters.read");
 load(host,"for i=1,5 do characters.awareness() end",false);
 denied=ai+npc::off::awake;brain=readAwareness(memory,image,character,player);assert(!brain.flags.count("asleep"));
 denied=ai+npc::off::entryTrust+npc::off::memory;brain=readAwareness(memory,image,character,player);assert(!brain.towardPlayer.count("trust"));
 denied=0;put(ai,npc::off::level,int32_t(99));put(ai,npc::off::mode,int32_t(99));put(ai,npc::off::stress,(std::numeric_limits<float>::quiet_NaN)());
 brain=readAwareness(memory,image,character,player);assert(!brain.labels.count("engagement")&&!brain.labels.count("mode")&&!brain.values.count("fear")&&!brain.values.count("stress"));
 put(ai,npc::off::handler,image+npc::off::handlerFollow);assert(readAwareness(memory,image,character,player).labels["handler"]=="follow");
 put(ai,npc::off::handler,image+npc::off::handlerShortWander);assert(readAwareness(memory,image,character,player).labels["handler"]=="short_wander");
 put(ai,npc::off::memoryCount,int32_t(99));brain=readAwareness(memory,image,character,player);assert(brain.towardPlayer.empty());
 brain=readAwareness(memory,image,character,0);assert(brain.towardPlayer.empty()&&!brain.flags.count("fights_player"));
 put(ai,npc::off::character,player);assert(!readAwareness(memory,image,character,player).available);
 put(ai,npc::off::character,character);put(ai,0,image+0x123);assert(!readAwareness(memory,image,character,player).available);
 reads=0;assert(!readAwareness(memory,image,player,player).available&&reads==0);
 copy.ready=false;load(host,"assert(characters.awareness()==nil)");
 std::puts("LUA_AWARENESS_PASSED");
}
