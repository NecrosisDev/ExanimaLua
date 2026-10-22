#include "script_host.h"
#include "character_contract.h"
#include <map>
#include <cstdio>
#include "test_assert.h"

static std::map<uintptr_t,std::vector<unsigned char>> memory;
static std::vector<std::pair<uintptr_t,size_t>> denied;
static unsigned labelReads=0,brainReads=0;
static constexpr uintptr_t image=0x100000,world=0x800000,list=0x900000,rows=0xa00000,player=0xb00000,other=0xc00000,stats=0xd00000;
static bool readMemory(uintptr_t address,void* out,size_t size){
 if(address==other+npc::off::ai)++brainReads;
 if(address==other+npc::off::headPart||address==stats+npc::off::roleCount)++labelReads;
 for(const auto& block:denied)if(address<block.first+block.second&&block.first<address+size){std::memset(out,0x42,size);return false;}
 for(const auto& block:memory)if(address>=block.first&&address-block.first<=block.second.size()&&size<=block.second.size()-(address-block.first)){
  std::memcpy(out,block.second.data()+address-block.first,size);return true;
 }return false;
}
template<class T>static void put(uintptr_t block,size_t offset,T value){std::memcpy(memory[block].data()+offset,&value,sizeof value);}
static scripting::CharacterSnapshot snapshot,details;
static const scripting::CharacterSnapshot* provider(){return &snapshot;}
static const scripting::CharacterSnapshot* detailsProvider(){return &details;}
static void checkLua(scripting::Host& host,const std::string& id,const std::string& code,bool expected=true,const std::string& caps="characters.read"){
 const bool loaded=host.load(id,code.data(),code.size(),caps);
 if(loaded!=expected)std::fprintf(stderr,"%s: %s\n",id.c_str(),host.error().c_str());assert(loaded==expected);
}
static void testContract(){
 const fp::sig::Signature* source=nullptr;
 for(const auto& signature:fp::sig::signatures)if(signature.target==fp::sig::world&&std::strstr(signature.pattern,"28 1C 00 00")){source=&signature;break;}
 assert(source);fp::Pattern pattern;assert(fp::parsePattern(source->pattern,pattern));
 std::vector<uint8_t> bytes(npc::off::world+8);
 auto install=[&](uint32_t at,uint32_t target){std::memcpy(bytes.data()+at,pattern.bytes.data(),pattern.bytes.size());
  const int32_t displacement=int32_t(target-at-source->insnEnd);std::memcpy(bytes.data()+at+source->offset,&displacement,4);};
 install(0x1000,uint32_t(npc::off::world));fp::ImageView view{bytes.data(),bytes.size(),0,0x1000,0x1200};
 assert(scripting::characterListContract(view));install(0x1000,0x100);assert(!scripting::characterListContract(view));
 install(0x1000,uint32_t(npc::off::world));install(0x1100,uint32_t(npc::off::world));assert(!scripting::characterListContract(view));
 view.textBegin=view.textEnd+1;assert(!scripting::characterListContract(view));
}
int main(){
 using namespace scripting;testContract();
 memory[image].resize(npc::off::world+8);memory[world].resize(npc::off::worldList+8);memory[list].resize(0x118);memory[rows].resize(4096*8);
 memory[player].resize(0x1400);memory[other].resize(0x1400);memory[stats].resize(0x140);
 put(image,npc::off::world,world);put(image,npc::off::controlled,player);put(world,npc::off::worldList,list);
 put(list,npc::off::listCount,int32_t(2));put(list,npc::off::listArray,rows);put(rows,0,player);put(rows,8,other);
 put(other,npc::off::stats,stats);std::memcpy(memory[stats].data()+npc::off::name,"Guard",5);put(stats,npc::off::recordId,uint32_t(72));
 put(other,npc::off::health,0.8f);put(other,npc::off::balance,0.4f);put(other,npc::off::vigour,1.0f);
 put(other,npc::off::stamina,0.7f);put(other,npc::off::staminaMax,1.0f);put(other,npc::off::focus,0.6f);put(other,npc::off::focusCeiling,1.0f);
 put(other,npc::off::root,300.0f);snapshot=readCharacters(image,readMemory);
 assert(snapshot.ready&&!snapshot.truncated&&snapshot.characters.size()==2&&snapshot.characters[0].player&&snapshot.characters[1].name=="Guard"&&labelReads==0&&brainReads==0);
 // The canonical default still reads labels for the editor; runtime opts out.
 npc::Live body;CharacterMemory accessor{readMemory};assert(npc::readBody(accessor,image,other,0,body));assert(labelReads==2);
 Host host(charactersRead);host.setCharacterProvider(provider);
 checkLua(host,"copy","local s=characters.snapshot();assert(s.space=='scene_local_cm' and #s.characters==2);local c=s.characters[2];assert(c.name=='Guard' and c.record_id==72 and not c.is_player and c.has_health_pool and c.health>0.79 and c.health_max==1 and c.position[1]==300 and not c.dead);c.position[1]=999;assert(characters.snapshot().characters[2].position[1]==300)");
 // Details: the six live skill slots and the held roles with their script variables, only when asked for.
 static constexpr uintptr_t roles=0xe00000;memory[roles].resize(0x40);
 put(stats,0xc0,uint16_t(25000));memory[stats][0xc2]=1;memory[stats][0xc3]=2;memory[stats][0xc4]=4;put(stats,0xc0+8*3,uint16_t(7000));memory[stats][0xc0+8*3+2]=6;
 put(stats,npc::off::roleCount,int32_t(2));put(stats,npc::off::roleFirst,roles);
 put(roles,0,uint32_t(7));memory[roles][0x0a]=2;put(roles,0x0c,int32_t(5));put(roles,0x10,int32_t(-1));put(roles,0x14,uint32_t(19));memory[roles][0x14+0x0a]=0;
 details=readCharacters(image,readMemory,false,true);host.setDetailsProvider(detailsProvider);
 checkLua(host,"details",R"(local c=characters.snapshot({details=true}).characters[2]
local s=c.skills assert(s.close_combat.rating==25000 and s.close_combat.active_techniques==5 and #s.close_combat.techniques==3 and s.close_combat.techniques[3]==4)
assert(s.shields.rating==7000 and s.shields.active_techniques==1 and s.shields.techniques[1]==6 and s.insight.rating==0 and s.insight.index==4)
assert(#c.roles==2 and c.roles[1].id==7 and c.roles[1].name==nil and c.roles[1].variables[1]==5 and c.roles[1].variables[2]==-1 and c.roles[2].id==19 and #c.roles[2].variables==0)
assert(characters.snapshot().characters[2].skills==nil and characters:snapshot().characters[2].skills==nil and characters.snapshot(5))
assert(not pcall(characters.awareness,{details=true}))
)");
 checkLua(host,"denied","characters.snapshot()",false,"");
 checkLua(host,"quota","for i=1,5 do characters.snapshot() end",false);
 denied={{other+npc::off::action,4},{other+scripting::lifeStageAt,4},{other+npc::off::focus,4},{image+npc::off::controlled,8}};
 put(other,npc::off::bodyFlags,uint32_t(npc::noHealthPool));snapshot=readCharacters(image,readMemory);
 checkLua(host,"unknown","local c=characters.snapshot().characters[2];assert(c.dead==nil and c.down==nil and c.focus==nil and c.is_player==nil and not c.has_health_pool and c.health==nil and c.health_max==nil and c.balance_max==1)");
 denied={{other+npc::off::bodyFlags,4}};snapshot=readCharacters(image,readMemory);
 checkLua(host,"unknown_pool","local c=characters.snapshot().characters[2];assert(c.has_health_pool==nil and c.health==nil and c.health_max==nil and c.balance_max==1)");
 // Death stage: knocked down (2) is down and alive even with the ragdoll bit set; 4 is dead; unreadable falls back to action == 0xa00.
 denied.clear();put(other,npc::off::action,uint32_t(0x800|npc::actionDownBit));put(other,scripting::lifeStageAt,uint32_t(2));snapshot=readCharacters(image,readMemory);
 assert(!snapshot.characters[1].dead&&snapshot.characters[1].down&&snapshot.characters[1].lifeStage==2);
 put(other,scripting::lifeStageAt,uint32_t(4));snapshot=readCharacters(image,readMemory);assert(snapshot.characters[1].dead&&!snapshot.characters[1].down);
 checkLua(host,"stage","local c=characters.snapshot().characters[2];assert(c.dead and c.life_stage==4)");
 denied={{other+scripting::lifeStageAt,4}};put(other,npc::off::action,uint32_t(npc::actionDeadBits));snapshot=readCharacters(image,readMemory);assert(snapshot.characters[1].dead&&!snapshot.characters[1].stageKnown);
 put(other,npc::off::action,uint32_t(0x800|npc::actionDownBit|0x10));snapshot=readCharacters(image,readMemory);assert(!snapshot.characters[1].dead&&snapshot.characters[1].down);denied.clear();put(other,scripting::lifeStageAt,uint32_t(0));
 put(rows,8,player);snapshot=readCharacters(image,readMemory);assert(snapshot.truncated&&snapshot.characters.size()==1);put(rows,8,other);
 put(other,npc::off::root,(std::numeric_limits<float>::quiet_NaN)());snapshot=readCharacters(image,readMemory);assert(snapshot.truncated&&snapshot.characters.size()==1);put(other,npc::off::root,300.0f);
 for(unsigned i=0;i<129;++i){const uintptr_t address=0x1000000+uintptr_t(i)*0x2000;memory[address].resize(0x1400);put(rows,size_t(i)*8,address);}
 put(list,npc::off::listCount,int32_t(129));snapshot=readCharacters(image,readMemory);assert(snapshot.ready&&snapshot.truncated&&snapshot.characters.size()==128);
 put(list,npc::off::listCount,int32_t(4097));snapshot=readCharacters(image,readMemory);assert(!snapshot.ready);checkLua(host,"unavailable","assert(characters.snapshot()==nil)");
 put(list,npc::off::listCount,int32_t(0));put(list,npc::off::listArray,uintptr_t(0));snapshot=readCharacters(image,readMemory);assert(snapshot.ready&&!snapshot.truncated&&snapshot.characters.empty());
 memory.clear();assert(!readCharacters(image,readMemory).ready);
 std::puts("LUA_CHARACTERS_PASSED");
}
