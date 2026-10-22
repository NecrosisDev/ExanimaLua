#pragma once
static bool aiWritePossible=false,aiWriteBroken=false,aiPulseReady=false;
static bool(*worldMutationReady)()=nullptr;
static bool mutationsReady(){return !worldMutationReady||worldMutationReady();}
static bool attitudeWritePossible=false;
static bool containerWritePossible=false;
static bool inventoryWritePossible=false;
using ConcernSetter=void(*)(uintptr_t,float);
static ConcernSetter concernSetter=nullptr;
static bool attitudeContract(){
 const unsigned char prefix[]={0x48,0x89,0xc8,0x0f,0x28,0xc1,0xf3,0x0f,0x10,0x48,0x1c,0x0f,0x2f,0x0d};
 float maximum=0,low=0,high=0;
 return gamebridge::operation(base+0x26f160,prefix,sizeof prefix)&&
  copyWorldMemory(base+0x30a300,&maximum,4)&&maximum==npc::concernLimit&&
  copyWorldMemory(base+0x2eefb0,&low,4)&&low==1.2f&&copyWorldMemory(base+0x2eefa0,&high,4)&&high==4.0f;
}
static bool callConcern(uintptr_t entry,float value){
 __try{if(!concernSetter)return false;concernSetter(entry,value);return true;}
 __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static uint64_t nextAiTicket=0,aiStableSince=0,aiPendingSince=0,nextAiBackup=0;
static uintptr_t aiWorld=0,aiPlayer=0,aiScene=0;
static char aiPending[17]{};
static std::vector<scripting::AiTarget> aiTargets;
static void invalidateCharacterCopies();
static bool readAiConstants(hot::AiConstants& k){
 return copyWorldMemory(base+0x30a050,&k.hundredth,8)&&copyWorldMemory(base+0x30a068,&k.one,4)&&
  copyWorldMemory(base+0x30a038,&k.half,4)&&copyWorldMemory(base+0x30a070,&k.two,4)&&
  copyWorldMemory(base+0x30a1a0,&k.three,4)&&copyWorldMemory(base+0x30a328,&k.eighth,4)&&
  copyWorldMemory(base+0x30a490,&k.third,8)&&hot::aiConstantsOk(k);
}
// Each pulse: boosts that lapsed, whose character left or died, whose addon is gone, or that outlived the session
// get their saved fields back, only while the same scene is loaded and the character is still that character.
static void tickBoosts(scripting::Host& owner,const scripting::CharacterSnapshot* roster,bool session,uint64_t now){
 if(boosts.empty())return;const scripting::CharacterMemory memory{copyWorldMemory};uintptr_t scene=0;copyWorldMemory(base+0x49b300,&scene,8);
 for(auto it=boosts.begin();it!=boosts.end();){
  bool alive=false;if(session&&roster&&roster->ready)for(const auto& c:roster->characters)if(c.nativeCharacter==it->target.character)alive=!(c.stateKnown&&c.dead);
  const bool keep=session&&alive&&now<it->until&&owner.enabled(it->owner);
  if(keep){++it;continue;}
  if(scene&&scene==it->scene&&scripting::targetIdentity(memory,base,it->target))
   for(size_t k=0;k<it->address.size();++k)if(writableAiFloat(it->address[k]))aiWriter(it->address[k],it->saved[k]);
  it=boosts.erase(it);
 }
}
static void beginAiPulse(const std::string& phase,uint64_t now){
 aiTargets.clear();aiPulseReady=false;
 if(phase!="session"||(!aiWritePossible&&!attitudeWritePossible&&!containerWritePossible&&!inventoryWritePossible)){aiWorld=aiPlayer=aiScene=0;return;}
 uintptr_t world=0,player=0,scene=0;char name[17]{},pending[17]{};
 if(!copyWorldMemory(base+npc::off::world,&world,8)||!copyWorldMemory(base+npc::off::controlled,&player,8)||
    !copyWorldMemory(base+0x49b300,&scene,8)||!world||!player||!scene||!readable(player,0x1300)||
    !copyWorldMemory(world+0x140,name,16)||!copyWorldMemory(base+0x2b7690,pending,16)){aiWorld=aiPlayer=aiScene=0;return;}
 if(world!=aiWorld||player!=aiPlayer||scene!=aiScene){aiWorld=world;aiPlayer=player;aiScene=scene;aiStableSince=now;}
 if(std::memcmp(pending,aiPending,16)){std::memcpy(aiPending,pending,17);aiPendingSince=now;}
 aiPulseReady=(aiWritePossible||attitudeWritePossible||containerWritePossible||inventoryWritePossible)&&!aiWriteBroken&&gamebridge::backupDone&&mutationsReady()&&
  hot::settled(true,now-aiStableSince,name,now-aiPendingSince<5000?pending:"");
}
static uint64_t captureAiTarget(uintptr_t character){
 if(!worldPulse||!aiPulseReady)return 0;
 for(const auto& previous:aiTargets)if(previous.character==character)return previous.ticket;
 if(aiTargets.size()>=256||nextAiTicket>=uint64_t((std::numeric_limits<lua_Integer>::max)()))return 0;
 const scripting::CharacterMemory memory{copyWorldMemory};
 scripting::AiTarget target;
 if(!scripting::captureTarget(memory,base,character,target))return 0;
 target.ticket=++nextAiTicket;aiTargets.push_back(target);return target.ticket;
}
static bool writableAiFloat(uintptr_t address){
 MEMORY_BASIC_INFORMATION info{};
 return readable(address,4)&&VirtualQuery((void*)address,&info,sizeof info)==sizeof info&&
  (info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
static bool writeAiFloat(uintptr_t address,const float& value){
 __try{
  if(!writableAiFloat(address))return false;
  std::memcpy((void*)address,&value,4);return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool(*aiWriter)(uintptr_t,const float&)=writeAiFloat;
// boost_ai: what a boost saved, to put back exactly (see script_host.h boostAi).
struct Boost {scripting::AiTarget target;uintptr_t scene=0;std::array<uintptr_t,6> address{};std::array<float,6> saved{};uint64_t until=0;std::string owner;};
static std::vector<Boost> boosts;
static Boost* boostOf(uintptr_t character){for(auto& b:boosts)if(b.target.character==character)return &b;return nullptr;}
static bool commitAi(const std::vector<scripting::AiChange>& changes){
 if(!worldPulse||!aiPulseReady||!mutationsReady()||aiWriteBroken||changes.empty()||changes.size()>8||!gamebridge::backupDone)return false;
 struct Scalar {uintptr_t address=0;float value=0,before=0;bool touched=false;};
 struct Prepared {
  scripting::AiTarget target;std::vector<Scalar> fields;
  uintptr_t entry=0;bool concern=false,concernTouched=false;float concernValue=0,concernBefore=0,flagsBefore=0;
 };
 std::vector<Prepared> prepared;const scripting::CharacterMemory memory{copyWorldMemory};
 for(const auto& change:changes){
  const scripting::AiTarget* target=nullptr;
  for(const auto& candidate:aiTargets)if(candidate.ticket==change.target){target=&candidate;break;}
  if(!target)return false;
  for(const auto& prior:prepared)if(prior.target.character==target->character)return false;
  // A boost already running on this character only moves its lapse time.
  if(change.boostSeconds>0&&change.ai&&!change.hostility&&!change.concern)if(auto* b=boostOf(target->character)){b->until=GetTickCount64()+uint64_t(change.boostSeconds*1000.0f);continue;}
  Prepared next;next.target=*target;
  if(!scripting::targetIdentity(memory,base,*target)||(!change.ai&&!change.hostility&&!change.concern))return false;
  if(change.ai){
   hot::AiConstants constants{};std::array<float,6> values{};
   if(!aiWritePossible||!readAiConstants(constants)||!scripting::prepareAi(memory,base,*target,change.values,constants,values))return false;
   for(size_t i=0;i<hot::aiOffsets.size();++i)next.fields.push_back({target->ai+hot::aiOffsets[i],values[i]});
  }
  if(change.hostility||change.concern){
   if(!attitudeWritePossible||!concernSetter||!std::isfinite(change.hostilityValue)||!std::isfinite(change.concernValue)||
      change.hostilityValue<0||change.hostilityValue>npc::hostilityLimit||change.concernValue<0||change.concernValue>npc::concernLimit)return false;
   next.entry=npc::playerEntry(memory,target->ai,target->player);if(!next.entry)return false;
   if(change.hostility)next.fields.push_back({next.entry+npc::off::entryHostility,change.hostilityValue});
   if(change.concern){next.concern=true;next.concernValue=change.concernValue;
    if(!memory.get(next.entry+npc::off::entryConcern,next.concernBefore)||!std::isfinite(next.concernBefore)||
       !memory.get(next.entry+0x50,next.flagsBefore)||!writableAiFloat(next.entry+npc::off::entryConcern)||!writableAiFloat(next.entry+0x50))return false;
   }
  }
  for(auto& field:next.fields)if(!memory.get(field.address,field.before)||!std::isfinite(field.before)||!writableAiFloat(field.address))return false;
  prepared.push_back(next);
 }
 // Recheck every identity and destination before the first write.
 for(const auto& entry:prepared){
  uintptr_t scene=0;if(!memory.get(base+0x49b300,scene)||scene!=aiScene||!scripting::targetIdentity(memory,base,entry.target))return false;
  if(entry.entry&&npc::playerEntry(memory,entry.target.ai,entry.target.player)!=entry.entry)return false;
  for(const auto& field:entry.fields){float current=0;
   if(!writableAiFloat(field.address)||!memory.get(field.address,current)||std::memcmp(&current,&field.before,4))return false;
  }
  if(entry.concern){float value=0,flags=0;
   if(!memory.get(entry.entry+npc::off::entryConcern,value)||std::memcmp(&value,&entry.concernBefore,4)||
      !memory.get(entry.entry+0x50,flags)||std::memcmp(&flags,&entry.flagsBefore,4))return false;
  }
 }
 auto fault=[&](){
  aiWriteBroken=true;aiPulseReady=false;
  for(auto row=prepared.rbegin();row!=prepared.rend();++row){
   if(row->concernTouched){aiWriter(row->entry+npc::off::entryConcern,row->concernBefore);aiWriter(row->entry+0x50,row->flagsBefore);}
   for(auto field=row->fields.rbegin();field!=row->fields.rend();++field)if(field->touched)aiWriter(field->address,field->before);
  }
  host.report("runtime","error","NPC write fault; further NPC writes disabled. Quit without saving and restore the backup if needed.");return false;
 };
 for(auto& row:prepared){
  for(auto& field:row.fields){field.touched=true;if(!aiWriter(field.address,field.value))return fault();}
  if(row.concern){row.concernTouched=true;if(!callConcern(row.entry,row.concernValue))return fault();}
 }
 // Boosts: the AI fields as they were before this write are what comes back; a plain set_ai ends a running boost.
 for(const auto& change:changes){
  const scripting::AiTarget* target=nullptr;for(const auto& candidate:aiTargets)if(candidate.ticket==change.target){target=&candidate;break;}
  if(!target||!change.ai)continue;
  const Prepared* row=nullptr;for(const auto& p:prepared)if(p.target.character==target->character)row=&p;
  if(!row||row->fields.size()<hot::aiOffsets.size())continue;
  if(change.boostSeconds>0){if(boostOf(target->character)||boosts.size()>=32)continue;
   Boost b;b.target=*target;b.scene=aiScene;b.until=GetTickCount64()+uint64_t(change.boostSeconds*1000.0f);b.owner=change.owner;
   for(size_t k=0;k<hot::aiOffsets.size();++k){b.address[k]=row->fields[k].address;b.saved[k]=row->fields[k].before;}
   boosts.push_back(std::move(b));}
  else boosts.erase(std::remove_if(boosts.begin(),boosts.end(),[&](const Boost& b){return b.target.character==target->character;}),boosts.end());
 }
 invalidateCharacterCopies();return true;
}
