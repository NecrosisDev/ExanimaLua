#pragma once
#include <map>
#include <unordered_set>
#include "shared/npc_live.h"

namespace scripting {
struct Awareness {
 bool available=false;
 std::map<std::string,bool> flags;
 std::map<std::string,float> values,towardPlayer;
 std::map<std::string,std::string> labels;
};
template<class Mem>struct TrackedMemory {
 const Mem& memory;mutable std::unordered_set<uintptr_t> successful;
 template<class T>bool get(uintptr_t address,T& value)const{
  if(!memory.get(address,value))return false;successful.insert(address);return true;
 }
 bool known(uintptr_t address)const{return successful.count(address)!=0;}
};
template<class Mem>Awareness readAwareness(const Mem& memory,uintptr_t base,uintptr_t character,uintptr_t player){
 Awareness out;TrackedMemory<Mem> tracked{memory,{}};
 if(character==player)return out;
 const uintptr_t ai=npc::motileAi(tracked,base,character);if(!ai)return out;
 npc::Live live;npc::readBrain(tracked,base,character,player,live);if(!live.hasBrain)return out;
 out.available=true;
 auto flag=[&](const char* key,uintptr_t offset,bool value){if(tracked.known(ai+offset))out.flags[key]=value;};
 flag("asleep",npc::off::awake,live.asleep);flag("wants_attack",npc::off::wantsAttack,live.wantsAttack);
 flag("casting",npc::off::casting,live.casting);flag("returning",npc::off::moveMode,live.returning);
 flag("opposite_side",npc::off::oppositeSide,live.oppositeSide);
 if(tracked.known(ai+npc::off::attackKind)){
  switch(live.attack){
   case npc::Attack::swing:out.labels["attack"]="swing";break;case npc::Attack::overhead:out.labels["attack"]="overhead";break;
   case npc::Attack::thrust:out.labels["attack"]="thrust";break;default:out.labels["attack"]="other";break;
  }
 }
 auto metric=[&](const char* key,float value){if(std::isfinite(value))out.values[key]=value;};
 metric("skill",live.skill);metric("stress",live.stress);metric("shock",live.shock);metric("courage",live.courage);metric("alarm",live.alarm);
 if(std::isfinite(live.fear()))out.values["fear"]=live.fear();
 int32_t level=0;if(tracked.get(ai+npc::off::level,level)&&level>=0&&level<=4){
  constexpr const char* levels[]={"none","wary","provoked","fight","flee"};out.labels["engagement"]=levels[level];
 }
 if(tracked.known(ai+npc::off::mode)){
  switch(live.mode){
   case npc::Mode::wander:out.labels["mode"]="wander";break;case npc::Mode::stand:out.labels["mode"]="stand";break;
   case npc::Mode::dormant:out.labels["mode"]="dormant";break;case npc::Mode::follow:out.labels["mode"]="follow";break;
   case npc::Mode::lieDown:out.labels["mode"]="lie_down";break;case npc::Mode::shortWander:out.labels["mode"]="short_wander";break;
   default:break;
  }
 }
 if(tracked.known(ai+npc::off::handler)){
  switch(live.handler){
   case npc::Handler::idle:out.labels["handler"]="idle";break;case npc::Handler::engage:out.labels["handler"]="engage";break;
   case npc::Handler::flee:out.labels["handler"]="flee";break;case npc::Handler::fetch:out.labels["handler"]="fetch";break;
   case npc::Handler::dormant:out.labels["handler"]="dormant";break;case npc::Handler::wander:out.labels["handler"]="wander";break;
   case npc::Handler::follow:out.labels["handler"]="follow";break;case npc::Handler::lieDown:out.labels["handler"]="lie_down";break;
   case npc::Handler::shortWander:out.labels["handler"]="short_wander";break;case npc::Handler::playerBrain:out.labels["handler"]="player_brain";break;case npc::Handler::other:out.labels["handler"]="other";break;
   default:break;
  }
 }
 if(player&&tracked.known(character+npc::off::opponent))out.flags["fights_player"]=live.fightsPlayer;
 const auto entry=npc::playerEntry(tracked,ai,player);
 if(entry){
  auto toward=[&](const char* key,uintptr_t offset,float value){if(tracked.known(entry+offset)&&std::isfinite(value))out.towardPlayer[key]=value;};
  toward("trust",npc::off::entryTrust,live.you.trust);toward("hesitation",npc::off::entryHesitation,live.you.hesitation);
  toward("interest",npc::off::entryInterest,live.you.interest);toward("concern",npc::off::entryConcern,live.you.concern);
  toward("hostility",npc::off::entryHostility,live.you.hostility);toward("danger",npc::off::entryDanger,live.you.danger);
  toward("closeness",npc::off::entryCloseness,live.you.closeness);
 }
 return out;
}
}
