#pragma once
namespace inventorybridge {
using Append=void(*)(uintptr_t,uint32_t,uint32_t);using Refresh=void(*)(uintptr_t);using Rebuild=void(*)(uintptr_t,uintptr_t);
using Fit=int(*)(uintptr_t,uintptr_t,void*);using RemoveWorn=void(*)(uintptr_t,uint32_t);
using Recount=void(*)(uintptr_t,uint32_t,int);
static Append append=nullptr,wear=nullptr;static Refresh refresh=nullptr,notify=nullptr;static Rebuild rebuild=nullptr;static Fit fit=nullptr;static RemoveWorn unwear=nullptr;
static Recount recount=nullptr;
static bool possible=false,equipmentPossible=false,broken=false;
struct Target {scripting::InventoryView view;uintptr_t world=0,scene=0,player=0,stats=0;};
using ExtraCommit=bool(*)(const Target&,const scripting::InventoryView&,const scripting::InventoryChange&);
static ExtraCommit extraCommit=nullptr;
using Ready=bool(*)();static Ready extraReady=nullptr;
static Target target;static uint64_t nextTicket=0;
static bool available(bool equipment){
 const unsigned char appendPrefix[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xd0,0x48,0x89,0xcb,0x89,0xd6,0x44,0x89,0xc7,0x83,0x03,0x01};
 const unsigned char notifyPrefix[]={0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x48,0x8d,0x64,0x24,0xe0,0x40,0x30,0xff};
 const unsigned char chunkPrefix[]={0x53,0x56,0x48,0x8d,0x64,0x24,0xc8,0x89,0xd3,0x48,0x8d,0x54};
 const unsigned char recountPrefix[]={0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0x89,0xd6,0x44,0x89,0xc7};
 if(!gamebridge::operation(base+0x1041b0,chunkPrefix,sizeof chunkPrefix)||!gamebridge::operation(base+0x113a20,appendPrefix,sizeof appendPrefix)||!gamebridge::operation(base+0x137b10,notifyPrefix,sizeof notifyPrefix)||!gamebridge::operation(base+0x111b70,recountPrefix,sizeof recountPrefix))return false;
 if(!equipment)return true;
 const unsigned char wearPrefix[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xd0,0x48,0x89,0xcb,0x89,0xd6,0x44,0x89,0xc7,0x48,0x63,0x83,0x84};
 const unsigned char unwearPrefix[]={0x53,0x57,0x56,0x8b,0x81,0x84,0x00,0x00,0x00,0x83,0xe8,0x01,0x85,0xc0};
 const unsigned char fitPrefix[]={0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x64,0x24,0xb0,0x48,0x89,0x4c,0x24,0x48};
 const unsigned char refreshPrefix[]={0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0x8b,0x43,0x54};
 const unsigned char rebuildPrefix[]={0x53,0x48,0x8d,0x64,0x24,0xd0,0x48,0x89,0xcb,0x48,0x8d,0x4c,0x24,0x20,0xba,0x20};
 return gamebridge::operation(base+0x116480,wearPrefix,sizeof wearPrefix)&&gamebridge::operation(base+0x116500,unwearPrefix,sizeof unwearPrefix)&&
  gamebridge::operation(base+0x115e90,fitPrefix,sizeof fitPrefix)&&gamebridge::operation(base+0x112540,refreshPrefix,sizeof refreshPrefix)&&gamebridge::operation(base+0xf1420,rebuildPrefix,sizeof rebuildPrefix);
}
static bool rows(uintptr_t stats,scripting::InventoryView& view){
 const scripting::CharacterMemory memory{copyWorldMemory};int32_t count=0;uintptr_t data=0;
 if(!memory.get(stats+0xf0,count)||count<0||count>4096||!memory.get(stats+0xf8,data))return false;
 view.carried.resize(size_t(count));if(count&&!copyWorldMemory(data,view.carried.data(),size_t(count)*12))return false;
 if(!memory.get(stats+0x84,count)||count<0||count>128||!memory.get(stats+0x88,data))return false;
 view.worn.clear();for(int i=0;i<count;++i){uint32_t row[2]{};if(!memory.get(data+size_t(i)*8,row))return false;view.worn.push_back({row[0],row[1]});}
 return memory.get(stats+0x64,view.weapons);
}
static bool loadoutsReady(uintptr_t stats){
 const scripting::CharacterMemory memory{copyWorldMemory};int32_t count=0,nestedCount=0;uintptr_t data=0,nested=0,pending=0;
 if(!memory.get(stats+0x100,count)||count<0||count>128||!memory.get(stats+0x108,data)||(count&&!readable(data,size_t(count)*0x30)))return false;
 for(int i=0;i<count;++i)if(!memory.get(data+size_t(i)*0x30+0x24,nestedCount)||nestedCount<0||nestedCount>128||
  !memory.get(data+size_t(i)*0x30+0x28,nested)||(nestedCount&&!readable(nested,size_t(nestedCount)*8)))return false;
 if(!memory.get(stats+0x118,pending))return false;
 if(pending&&(!memory.get(pending,nestedCount)||nestedCount<0||nestedCount>4096||!memory.get(pending+8,nested)||(nestedCount&&!readable(nested,size_t(nestedCount)*12))))return false;
 return true;
}
static bool quiet(uintptr_t player){
 if(extraReady&&!extraReady())return false;
 const scripting::CharacterMemory memory{copyWorldMemory};int32_t windows=-1,dragFlags=0,readJob=0,writeJob=0,readDone=0,writeDone=0;uintptr_t drag=0;uint8_t combat=1;uint32_t action=~0u;
 uintptr_t behavior=0,view=0,ui=0,manager=0,capture=0;int format=0;
 int request=0,motion=0;if(!memory.get(player+0xdc0,request)||request==0x1f||!memory.get(player+npc::off::motion,motion)||motion==0x1f)return false;
 if(!memory.get(player+fp::game::character::behaviour,behavior)||!behavior||!memory.get(behavior+fp::game::behaviour::view,view)||!view||
    !memory.get(base+0x49aa60,ui)||(ui&&ui!=view)||!memory.get(base+0x49a960,manager)||!manager||
    !memory.get(manager+fp::game::gui::capture,capture)||(capture&&capture!=view)||!memory.get(manager+fp::game::gui::dragFormat,format)||format)return false;
 return memory.get(base+0x499860,windows)&&windows==0&&memory.get(player+0x12fe,combat)&&!combat&&memory.get(player+0xdf4,action)&&!(action&(0x40204|0xa00))&&
  memory.get(base+0x4efdd0,drag)&&drag&&memory.get(drag+8,dragFlags)&&!(dragFlags&0x96000)&&
  memory.get(base+0x5b3a60,readJob)&&memory.get(base+0x5b3a64,writeJob)&&readJob==writeJob&&
  memory.get(base+0x5b3a80,readDone)&&memory.get(base+0x5b3a84,writeDone)&&readDone==writeDone;
}
static bool identity(const Target& t){
 const scripting::CharacterMemory memory{copyWorldMemory};uintptr_t world=0,scene=0,player=0,stats=0;
 return worldPulse&&aiPulseReady&&mutationsReady()&&!aiWriteBroken&&!broken&&gamebridge::backupDone&&memory.get(base+npc::off::world,world)&&world==t.world&&
  memory.get(base+0x49b300,scene)&&scene==t.scene&&memory.get(base+npc::off::controlled,player)&&player==t.player&&player&&
  memory.get(player+0x10a0,stats)&&stats==t.stats&&stats&&quiet(player)&&loadoutsReady(stats);
}
static const scripting::InventoryView* capture(){
 if(!possible||!worldPulse||!aiPulseReady||broken)return nullptr;
 if(target.view.ticket&&identity(target)){scripting::InventoryView current;if(rows(target.stats,current)&&current.carried==target.view.carried&&current.worn==target.view.worn&&current.weapons==target.view.weapons)return &target.view;}
 target={};
 const scripting::CharacterMemory memory{copyWorldMemory};
 if(!memory.get(base+npc::off::world,target.world)||!memory.get(base+0x49b300,target.scene)||!memory.get(base+npc::off::controlled,target.player)||
    !memory.get(target.player+0x10a0,target.stats)||!identity(target)||!rows(target.stats,target.view)||nextTicket>=uint64_t((std::numeric_limits<lua_Integer>::max)()))return nullptr;
 if(!memory.get(target.player+npc::off::root,target.view.position))return nullptr;
 for(float value:target.view.position)if(!std::isfinite(value))return nullptr;
 target.view.ticket=++nextTicket;return &target.view;
}
static bool compact(uintptr_t stats,size_t index){
 __try{int count=*(int*)(stats+0xf0);auto data=*(scripting::InventoryRow**)(stats+0xf8);
  if(count<=0||count>4096||index>=size_t(count)||!readable((uintptr_t)data,size_t(count)*12))return false;
  std::memmove(data+index,data+index+1,(size_t(count)-index-1)*12);*(int*)(stats+0xf0)=count-1;return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool add(uintptr_t stats,const scripting::InventoryRow& row){
 __try{if(!append)return false;append(stats+0xf0,row.item,row.state);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool finish(uintptr_t stats,uintptr_t player,uint32_t item,bool equipment){
 __try{if(!notify||!recount)return false;recount(stats,item,0);notify(stats);if(equipment){if(!refresh||!rebuild)return false;refresh(stats);rebuild(player,stats);}return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool fault(){broken=true;aiWriteBroken=true;aiPulseReady=false;host.report("runtime","error","Inventory operation fault or unexpected result; mutations disabled. Quit without saving and use the backup if needed.");return false;}
static uintptr_t chunk(uint32_t item,uint32_t tag){
 __try{return containerbridge::chunk?containerbridge::chunk(item,tag):0;}__except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
static bool volume(uint32_t item,uint32_t& out){
 const auto look=chunk(item,0x100000);uint32_t size=0;uint8_t value=0;
 if(!look||!copyWorldMemory(look+4,&size,4)||size<0x25||!copyWorldMemory(look+0x24,&value,1))return false;out=value;return true;
}
static bool fits(const std::vector<scripting::InventoryRow>& destination,uint32_t item,unsigned capacity,unsigned maximum){
 uint32_t amount=0;if(!volume(item,amount)||(maximum&&amount>maximum))return false;uint64_t used=0;
 for(const auto& row:destination)if(!(row.state&0xc80100)){uint32_t size=0;if(!volume(row.item,size))return false;used+=size;}
 return !capacity||used+amount<=capacity;
}
static void displaced(uintptr_t stats,uint32_t item,uint32_t state){append(stats+0xf0,item,state);}
static int changeEquipment(const Target& selected,const scripting::InventoryRow& item,bool equipping,uintptr_t apparel,size_t index){
 __try{
  if(equipping){
   struct Callback{void(*call)(uintptr_t,uint32_t,uint32_t);uintptr_t context;} callback{displaced,selected.stats};
   if(fit(selected.stats,apparel,&callback)<0)return 0;
   wear(selected.stats,item.item,item.state);
   if(!compact(selected.stats,index))return -1;
  }else{unwear(selected.stats,item.item);if(!add(selected.stats,item))return -1;}
  return 1;
 }__except(EXCEPTION_EXECUTE_HANDLER){return -1;}
}
static std::map<std::pair<uint32_t,uint32_t>,int> ownership(const scripting::InventoryView& view){
 std::map<std::pair<uint32_t,uint32_t>,int> counts;for(const auto& row:view.carried)++counts[{row.item,row.state}];for(const auto& row:view.worn)++counts[{row.item,row.state}];return counts;
}
static bool commitEquipment(const Target& selected,const scripting::InventoryView& current,const scripting::InventoryChange& change){
 if(!equipmentPossible||!fit||!wear||!unwear||!refresh||!rebuild||!append||!containerbridge::chunk)return false;
 const bool equipping=change.op==scripting::InventoryOp::equip;const auto& source=equipping?current.carried:current.worn;
 if(change.row>=source.size()||current.carried.size()+current.worn.size()+1>4096||current.worn.size()>=128)return false;
 const auto item=source[change.row];if(!item.item||(item.state&0xc80100))return false;
 size_t copies=0;for(const auto& row:current.carried)copies+=row.item==item.item;for(const auto& row:current.worn)copies+=row.item==item.item;if(copies!=1)return false;
 uintptr_t apparel=chunk(item.item,0x1000);
 if(!apparel||!readable(apparel,0x224))return false;
 uint32_t size=0;if(!copyWorldMemory(apparel+4,&size,4)||size<0x224||!identity(selected))return false;
 const int result=changeEquipment(selected,item,equipping,apparel,change.row);if(result<0)return fault();if(!result)return false;
 scripting::InventoryView after;if(!rows(selected.stats,after))return fault();
 size_t carried=0,worn=0;for(const auto& row:after.carried)carried+=row.item==item.item;for(const auto& row:after.worn)worn+=row.item==item.item&&row.state==item.state;
 if(carried!=(equipping?0u:1u)||worn!=(equipping?1u:0u)||ownership(after)!=ownership(current)||!finish(selected.stats,selected.player,item.item,true))return fault();
 target={};return true;
}
static bool commit(const scripting::InventoryChange& change){
 if(!possible||unsigned(change.op)>unsigned(scripting::InventoryOp::spawnItem)||!change.inventory||change.inventory!=target.view.ticket||!identity(target))return false;
 const Target selected=target;scripting::InventoryView current;if(!rows(selected.stats,current)||current.carried!=selected.view.carried||current.worn!=selected.view.worn||current.weapons!=selected.view.weapons)return false;
 if(unsigned(change.op)>=unsigned(scripting::InventoryOp::switchWeapons))return extraCommit&&extraCommit(selected,current,change);
 const bool equipment=change.op==scripting::InventoryOp::equip||change.op==scripting::InventoryOp::unequip;
 if(equipment)return commitEquipment(selected,current,change);
 const containerbridge::Target* chest=nullptr;for(const auto& c:containerbridge::targets)if(c.ticket==change.container){chest=&c;break;}
 if(!chest||!containerbridge::identity(*chest))return false;
 const auto destination=*chest;containerbridge::Record record;
 if(!containerbridge::record(destination.record,record)||record.db!=destination.source.db||record.bytes!=destination.source.bytes)return false;
 std::vector<scripting::InventoryRow> chestRows;size_t offset=0;if(!scripting::containerRows(record.bytes,chestRows,offset))return false;
 const bool taking=change.op==scripting::InventoryOp::take;const auto& source=taking?chestRows:current.carried;
 if(change.row>=source.size()||(taking&&current.carried.size()>=4096))return false;
 const auto item=source[change.row];if(!item.item||(item.state&0xc80100)||!containerbridge::chunk)return false;
 // Equipped/quick-slot references are ownership, not ordinary carried loot.
 const scripting::CharacterMemory memory{copyWorldMemory};
 if(!taking){for(const auto& worn:current.worn)if(worn.item==item.item)return false;
  for(size_t at=0x64;at<=0x7c;at+=8){uint32_t id=0;if(!memory.get(selected.stats+at,id)||id==item.item)return false;}}
 if(item.item==destination.record||chunk(item.item,0x20000))return false;
 uint16_t capacity=0,maximum=0;std::memcpy(&capacity,record.bytes.data()+offset+0x10,2);std::memcpy(&maximum,record.bytes.data()+offset+0x12,2);
 int playerMaximum=0;if(!memory.get(base+0x2b7290,playerMaximum)||playerMaximum<1)return false;
 if(!fits(taking?current.carried:chestRows,item.item,taking?400:capacity,taking?unsigned(playerMaximum):maximum))return false;
 auto next=record.bytes;if(!scripting::changeContainerRow(next,change.row,taking?nullptr:&item)||next.size()>1024*1024)return false;
 containerbridge::Record latest;scripting::InventoryView last;
 if(!identity(selected)||!containerbridge::identity(destination)||!rows(selected.stats,last)||last.carried!=current.carried||last.worn!=current.worn||
  !containerbridge::record(destination.record,latest)||latest.db!=record.db||latest.bytes!=record.bytes)return false;
 struct Buffer{const void* data;uint32_t size,pad;} buffer{next.data(),uint32_t(next.size()),0};
 if(!containerbridge::publish(latest.db,latest.slot,&buffer))return fault();
 containerbridge::Record after;if(!containerbridge::record(destination.record,after)||after.bytes!=next)return fault();
 if(taking){if(!add(selected.stats,item))return fault();}else if(!compact(selected.stats,change.row))return fault();
 scripting::InventoryView result;auto expected=current.carried;
 if(taking){auto appended=item;appended.x=40;appended.y=4;expected.push_back(appended);}else expected.erase(expected.begin()+change.row);
 if(!rows(selected.stats,result)||result.carried!=expected||result.worn!=current.worn||!finish(selected.stats,selected.player,item.item,false))return fault();
 target={};containerbridge::targets.clear();return true;
}
}
