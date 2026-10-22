#pragma once
#include "container_contract.h"
namespace scripting {
struct InventoryRow {uint32_t item=0,state=0;int16_t x=0,y=0;};
inline bool operator==(const InventoryRow& a,const InventoryRow& b){return a.item==b.item&&a.state==b.state&&a.x==b.x&&a.y==b.y;}
struct InventoryView {uint64_t ticket=0;std::vector<InventoryRow> carried,worn;std::array<uint32_t,8> weapons{};std::array<float,3> position{};};
enum class InventoryOp {take,store,equip,unequip,switchWeapons,pickup,drop,equipWeapon,bagTake,bagStore,unequipWeapon,equipShield,bagTransfer,itemUse,useItemOnWorld,worldUse,worldPress,leverPosition,leverToggle,doorOpen,teleporterUse,scannerUse,emitterSet,emitterToggle,fireUse,fireExtinguish,torchIgnite,spawnItem};
struct InventoryChange {InventoryOp op=InventoryOp::take;uint64_t inventory=0,container=0;size_t row=0;uint64_t worldItem=0;std::array<float,3> position{};size_t bag=0;unsigned hand=0;size_t destinationBag=0;bool active=false;uint32_t item=0;};
enum class InteractionKind {door,container,button,lever,teleporter,scanner,emitter,fireSource,torch};
struct InteractionView {uint64_t ticket=0;InteractionKind kind=InteractionKind::door;uint32_t requiredKey=0;bool igniteControl=false,extinguishControl=false,locked=false,pressed=false,doorControl=false,closed=false,lightControl=false;float position=0;int targetPosition=0,doorKind=0,doorLeaves=0,doorCommand=0;bool doorMoving=false;};
inline bool containerRows(const std::vector<uint8_t>& bytes,std::vector<InventoryRow>& rows,size_t& offset){
 ContainerView view;if(!containerView(bytes,view))return false;
 for(size_t at=0;at<bytes.size();){uint32_t tag=0,size=0;std::memcpy(&tag,bytes.data()+at,4);std::memcpy(&size,bytes.data()+at+4,4);
  if(tag==0x20000){offset=at;rows.resize(view.entries.size());if(!rows.empty())std::memcpy(rows.data(),bytes.data()+at+32,rows.size()*12);return true;}at+=size;
 }return false;
}
inline bool changeContainerRow(std::vector<uint8_t>& bytes,size_t index,const InventoryRow* append){
 std::vector<InventoryRow> rows;size_t at=0;if(!containerRows(bytes,rows,at))return false;
 if(append){if(rows.size()>=4096)return false;rows.push_back(*append);}else{if(index>=rows.size())return false;rows.erase(rows.begin()+index);}
 uint32_t size=0;std::memcpy(&size,bytes.data()+at+4,4);
 // Preserve trailing chunk fields as well as unrelated chunks.
 const auto end=at+32+(append?(rows.size()-1):(rows.size()+1))*12;
 if(append)bytes.insert(bytes.begin()+end,12,0);else bytes.erase(bytes.begin()+at+32+index*12,bytes.begin()+at+44+index*12);
 size=append?size+12:size-12;const auto count=uint32_t(rows.size());std::memcpy(bytes.data()+at+4,&size,4);std::memcpy(bytes.data()+at+28,&count,4);
 if(!rows.empty())std::memcpy(bytes.data()+at+32,rows.data(),rows.size()*12);return true;
}
static_assert(sizeof(InventoryRow)==12,"native carried row layout");
}
