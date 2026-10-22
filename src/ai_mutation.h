#pragma once
#include "shared/hotload.h"
#include "character_snapshot.h"

namespace scripting {
struct AiChange {
 uint64_t target=0;hot::AiInput values;
 bool ai=true,hostility=false,concern=false;float hostilityValue=0,concernValue=0;
};
struct AiTarget {uint64_t ticket=0;uintptr_t world=0,player=0,character=0,ai=0,stats=0;uint32_t record=0;char name[17]{};};
template<class Mem>bool targetIdentity(const Mem& memory,uintptr_t base,const AiTarget& target){
 uintptr_t world=0,player=0,stats=0;uint32_t record=0;char name[17]{};
 if(!memory.get(base+npc::off::world,world)||world!=target.world||!memory.get(base+npc::off::controlled,player)||
    !player||player!=target.player||target.character==player||npc::motileAi(memory,base,target.character)!=target.ai||
    !memory.get(target.character+npc::off::stats,stats)||stats!=target.stats||!memory.get(stats+npc::off::recordId,record)||
    record!=target.record||!memory.get(stats+npc::off::name,*reinterpret_cast<char(*)[16]>(name))||std::memcmp(name,target.name,16))return false;
 uintptr_t list=0,array=0;int32_t count=0;
 if(!npc::readPointer(memory,world+npc::off::worldList,list)||!memory.get(list+npc::off::listCount,count)||count<=0||count>4096||
    !npc::readPointer(memory,list+npc::off::listArray,array))return false;
 for(int32_t i=0;i<count;++i){uintptr_t pointer=0;if(!memory.get(array+uintptr_t(i)*8,pointer))return false;if(pointer==target.character)return true;}
 return false;
}
template<class Mem>bool captureTarget(const Mem& memory,uintptr_t base,uintptr_t character,AiTarget& target){
 target={};target.character=character;
 if(!memory.get(base+npc::off::world,target.world)||!memory.get(base+npc::off::controlled,target.player)||
    !npc::readPointer(memory,character+npc::off::stats,target.stats)||!memory.get(target.stats+npc::off::recordId,target.record)||
    !memory.get(target.stats+npc::off::name,*reinterpret_cast<char(*)[16]>(target.name)))return false;
 target.ai=npc::motileAi(memory,base,character);
 return target.ai&&targetIdentity(memory,base,target);
}
template<class Mem>bool prepareAi(const Mem& memory,uintptr_t base,const AiTarget& target,const hot::AiInput& input,
                                const hot::AiConstants& constants,std::array<float,6>& values){
 const char* reason="";float blend=0;uint32_t flags=0;
 if(!hot::aiConstantsOk(constants)||!hot::aiInputOk(input,reason)||!targetIdentity(memory,base,target)||
    !memory.get(target.stats+0x90,blend)||!std::isfinite(blend)||!memory.get(target.character+npc::off::bodyFlags,flags))return false;
 values=hot::aiValues(hot::aiCompute(input,constants,blend,(flags&4)!=0));
 for(float value:values)if(!std::isfinite(value))return false;
 return true;
}
}
