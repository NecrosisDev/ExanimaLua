#pragma once
#include "world_snapshot.h"
#include "shared/npc_live.h"
#include "awareness_snapshot.h"

namespace scripting {
// Live skill slot at stats +0xc0 + 8*i (six slots): u16 rating, then six technique numbers (0 = empty).
struct Skill {unsigned index=0;uint16_t rating=0;std::array<uint8_t,6> techniques{};};
// A role the character holds (stats +0x128 count, entries from +0x130): id, charroles name, script variables.
struct Role {uint32_t id=0;std::string name;std::vector<int32_t> variables;};   // the first 32 variables
constexpr uintptr_t skillsAt=0xc0;constexpr size_t skillSlots=6,rolesListed=16;
struct Character {
 uintptr_t nativeCharacter=0;
 std::string name;uint32_t record=0;bool player=false,playerKnown=false,stateKnown=false,dead=false,down=false,poolKnown=false,healthPool=false;
 std::array<float,3> position{};
 float health=NAN,balance=NAN,maximum=NAN,stamina=NAN,staminaMax=NAN,focus=NAN,focusMax=NAN;
 Awareness awareness;
 bool skillsKnown=false,rolesKnown=false;std::vector<Skill> skills;std::vector<Role> roles;
 bool stageKnown=false;uint32_t lifeStage=0;
};
constexpr uintptr_t lifeStageAt=0x11a4;
struct CharacterSnapshot {bool ready=false,truncated=false;std::vector<Character> characters;};
struct CharacterMemory {
 WorldRead read=nullptr;mutable bool failed=false;
 template<class T>bool get(uintptr_t address,T& value)const{
  T copied{};if(!read||!read(address,&copied,sizeof copied)){failed=true;return false;}
  std::memcpy(&value,&copied,sizeof copied);return true;
 }
};
template<class Memory>void readDetails(const Memory& memory,uintptr_t base,uintptr_t pointer,Character& c){
 uintptr_t stats=0;if(!npc::readPointer(memory,pointer+npc::off::stats,stats))return;
 uint8_t slots[skillSlots*8]{};
 if(memory.get(stats+skillsAt,slots)){c.skillsKnown=true;
  for(size_t i=0;i<skillSlots;++i){Skill s;s.index=unsigned(i);std::memcpy(&s.rating,slots+i*8,2);std::memcpy(s.techniques.data(),slots+i*8+2,6);c.skills.push_back(s);}}
 int32_t count=0;uintptr_t entry=0;
 if(!memory.get(stats+npc::off::roleCount,count)||count<0||count>npc::rolesMax)return;
 if(count&&!npc::readPointer(memory,stats+npc::off::roleFirst,entry))return;
 for(int32_t i=0;i<count&&c.roles.size()<rolesListed;++i){
  Role role;uint8_t vars=0;if(!memory.get(entry,role.id)||!memory.get(entry+npc::off::roleVarCount,vars))return;
  for(uint8_t v=0;v<vars&&v<32;++v){int32_t value=0;if(!memory.get(entry+npc::off::roleVars+4*uintptr_t(v),value))return;role.variables.push_back(value);}
  char name[npc::off::nameLength+1]{};if(npc::roleName(memory,base,role.id,name))role.name=name;
  c.roles.push_back(std::move(role));entry+=npc::off::roleVars+4*uintptr_t(vars);
 }
 c.rolesKnown=true;
}
inline CharacterSnapshot readCharacters(uintptr_t base,WorldRead read,bool includeAwareness=false,bool includeDetails=false){
 CharacterSnapshot out;CharacterMemory memory{read};uintptr_t world=0,list=0;int32_t count=0;
 if(!npc::readPointer(memory,base+npc::off::world,world)||!npc::readPointer(memory,world+npc::off::worldList,list)||
    !memory.get(list+npc::off::listCount,count)||count<0||count>npc::off::listMax)return out;
 out.ready=true;if(!count)return out;
 uintptr_t characters[128]{};memory.failed=false;
 const unsigned found=npc::listCharacters(memory,base,characters,128);
 out.truncated=memory.failed||count>int32_t(found);
 memory.failed=false;const auto player=npc::controlledCharacter(memory,base);const bool playerKnown=!memory.failed&&player!=0;
 std::unordered_set<uintptr_t> seen;
 for(unsigned i=0;i<found;++i){
  const auto pointer=characters[i];if(!seen.insert(pointer).second){out.truncated=true;continue;}
  npc::Live body;
  if(!npc::readBody(memory,base,pointer,0,body,false)){out.truncated=true;continue;}
  Character character;character.nativeCharacter=pointer;character.playerKnown=playerKnown;character.player=player&&pointer==player;character.record=body.record;
  if(npc::printableName(body.name))character.name=body.name;
  std::copy(std::begin(body.pos),std::end(body.pos),character.position.begin());
  character.health=body.health;character.balance=body.balance;character.maximum=body.vigour;
  character.stamina=body.stamina;character.staminaMax=body.staminaMax;character.focus=body.focus;character.focusMax=body.focusMax;
  // Death stage +0x11a4 (u32): 0 alive, 1 collapsing, 2 down, 3 player only, 4 dead (NPCs reach 4 three seconds
  // after 2). The action word's 0x800 is the ragdoll state, so `action & 0xa00` also caught knocked-down living
  // NPCs; only the exact value 0xa00 means dead there, and it is the fallback when the stage is unreadable.
  uint32_t action=0,flags=0,stage=0;
  const bool actionKnown=memory.get(pointer+npc::off::action,action);
  character.stageKnown=memory.get(pointer+lifeStageAt,stage)&&stage<=4;character.lifeStage=stage;
  character.stateKnown=character.stageKnown||actionKnown;
  if(character.stageKnown){character.dead=stage==4;character.down=stage==1||stage==2||stage==3;}
  else{character.dead=action==npc::actionDeadBits;character.down=!character.dead&&(action&npc::actionDownBit)!=0;}
  character.poolKnown=memory.get(pointer+npc::off::bodyFlags,flags);character.healthPool=(flags&npc::noHealthPool)==0;
  if(!character.poolKnown||!character.healthPool)character.health=NAN;
  if(includeAwareness)character.awareness=readAwareness(memory,base,pointer,playerKnown?player:0);
  if(includeDetails)readDetails(memory,base,pointer,character);
  out.characters.push_back(std::move(character));
 }
 return out;
}
}
