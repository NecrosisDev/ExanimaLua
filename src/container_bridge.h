#pragma once
namespace containerbridge {
using Find=int(*)(uintptr_t,int);using Replace=void(*)(uintptr_t,unsigned,void*);
using Open=char(*)(int);using Chunk=uintptr_t(*)(uint32_t,uint32_t);
static Find find=nullptr;static Replace replace=nullptr;static Open open=nullptr;static Chunk chunk=nullptr;
static bool broken=false;static uint64_t nextTicket=0;
struct Record {uintptr_t db=0;unsigned slot=0;std::vector<uint8_t> bytes;};
struct Target {uint64_t ticket=0;uintptr_t node=0,world=0,scene=0,player=0;uint32_t record=0;scripting::ContainerView view;Record source;};
static std::vector<Target> targets;
static bool available(){
 const unsigned char findPrefix[]={0x53,0x57,0x56,0xb8,0xff,0xff,0xff,0xff};
 const unsigned char replacePrefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0xb0,0x48,0x89,0x5d,0xd0};
 const unsigned char openPrefix[]={0x30,0xc0,0xba,0xff,0xff,0xff,0xff,0x90,0x83,0xc2,0x01};
 const unsigned char chunkPrefix[]={0x53,0x56,0x48,0x8d,0x64,0x24,0xc8,0x89,0xd3,0x48,0x8d,0x54};
 return gamebridge::operation(base+0x104660,findPrefix,sizeof findPrefix)&&gamebridge::operation(base+0x1048e0,replacePrefix,sizeof replacePrefix)&&
  gamebridge::operation(base+0x1327f0,openPrefix,sizeof openPrefix)&&gamebridge::operation(base+0x1041b0,chunkPrefix,sizeof chunkPrefix)&&readable(base+0x4ef930,8);
}
static bool closed(uint32_t record){
 __try{int count=-1;return copyWorldMemory(base+0x499860,&count,4)&&count==0&&open&&!open(int(record));}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static int slot(uintptr_t db,uint32_t id){
 __try{return find?find(db,int(id)):-1;}__except(EXCEPTION_EXECUTE_HANDLER){return -1;}
}
static bool itemValid(uint32_t item){
 __try{return item&&item<0x40000000u&&chunk&&chunk(item,0x100000)!=0;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool publish(uintptr_t db,unsigned slot,void* buffer){
 __try{if(!replace)return false;replace(db,slot,buffer);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool record(uint32_t id,Record& out,bool carried=false){
 const scripting::CharacterMemory memory{copyWorldMemory};uintptr_t db=0,table=0,body=0;int32_t count=0;uint32_t bodySize=0;
 const auto kind=id&0xc0000000u;
 if(!id||kind==0xc0000000u||(!carried&&kind!=0x80000000u))return false;
 const auto database=kind==0x80000000u?0x4ef930:kind==0x40000000u?0x4ef940:0x4ef920;
 if(!memory.get(base+database,db)||!db||!memory.get(db+8,count)||count<=0||count>200000||
    !memory.get(db+0x10,table)||!table||!readable(table,size_t(count)*16)||!memory.get(db+0x18,body)||!body||!memory.get(db+0x20,bodySize))return false;
 const int index=slot(db,id&0x3fffffffu);if(index<0||index>=count)return false;
 uint32_t row[4]{};if(!memory.get(table+uintptr_t(index)*16,row)||row[0]!=(id&0x3fffffffu)||row[3]<8||row[3]>1024*1024||body+row[2]<body||row[2]>bodySize||row[3]>bodySize-row[2])return false;
 out.bytes.resize(row[3]);if(!copyWorldMemory(body+row[2],out.bytes.data(),out.bytes.size()))return false;
 out.db=db;out.slot=unsigned(index);return true;
}
static bool identity(const Target& target){
 const scripting::CharacterMemory memory{copyWorldMemory};uintptr_t world=0,scene=0,player=0;uint32_t flags=0,id=0;
 if(!memory.get(base+npc::off::world,world)||world!=target.world||!memory.get(base+0x49b300,scene)||scene!=target.scene||
    !memory.get(base+npc::off::controlled,player)||player!=target.player||!memory.get(target.node+8,flags)||
    (flags&0x7000)!=0x4000||!memory.get(target.node+0x180,id)||id!=target.record||!closed(id))return false;
 const auto snapshot=scripting::readWorld(world,copyWorldMemory);
 for(const auto& object:snapshot.objects)if(object.nativeNode==target.node)return true;
 return false;
}
static uint64_t capture(uintptr_t node){
 if(!worldPulse||!aiPulseReady||!containerWritePossible||broken)return 0;
 for(const auto& target:targets)if(target.node==node)return target.ticket;
 if(targets.size()>=8)return 0;
 const scripting::CharacterMemory memory{copyWorldMemory};Target target;target.node=node;
 uint32_t flags=0;
 if(!memory.get(node+8,flags)||(flags&0x7000)!=0x4000||!memory.get(node+0x180,target.record)||
    (target.record&0xc0000000u)!=0x80000000u||!memory.get(base+npc::off::world,target.world)||
    !memory.get(base+0x49b300,target.scene)||!memory.get(base+npc::off::controlled,target.player)||!identity(target)||
    !record(target.record,target.source)||!scripting::containerView(target.source.bytes,target.view))return 0;
 if(nextTicket>=uint64_t((std::numeric_limits<lua_Integer>::max)()))return 0;
 target.ticket=++nextTicket;targets.push_back(std::move(target));return targets.back().ticket;
}
static const scripting::ContainerView* read(uint64_t ticket){
 if(!worldPulse||!aiPulseReady||broken)return nullptr;
 for(const auto& target:targets)if(target.ticket==ticket)return &target.view;
 return nullptr;
}
static bool commit(const std::vector<scripting::ContainerChange>& changes){
 if(!worldPulse||!aiPulseReady||!mutationsReady()||!containerWritePossible||broken||!gamebridge::backupDone||changes.empty()||changes.size()>16)return false;
 const Target* target=nullptr;for(const auto& value:targets)if(value.ticket==changes.front().target){target=&value;break;}
 if(!target||!identity(*target))return false;
 Record current;if(!record(target->record,current)||current.db!=target->source.db||current.bytes!=target->source.bytes)return false;
 auto next=current.bytes;
 for(const auto& change:changes){
  if(change.target!=target->ticket||!itemValid(change.item))return false;
  scripting::ContainerView view;if(!scripting::containerView(next,view)||(change.add&&view.entries.size()>=4096)||!hot::chestEdit(next,change.item,change.add))return false;
 }
 scripting::ContainerView checked;if(!scripting::containerView(next,checked)||next.size()>1024*1024)return false;
 Record latest;if(!identity(*target)||!record(target->record,latest)||latest.db!=current.db||latest.bytes!=current.bytes)return false;
 struct Buffer {const void* data;uint32_t size,pad;} buffer{next.data(),uint32_t(next.size()),0};
 if(publish(latest.db,latest.slot,&buffer)){
  Record after;if(record(target->record,after)&&after.db==latest.db&&after.bytes==next){targets.clear();return true;}
 }
 broken=true;aiWriteBroken=true;aiPulseReady=false;host.report("runtime","error","Container publication fault or verification failure; further world/NPC writes disabled. Quit without saving and use the backup if needed.");return false;
}
}
