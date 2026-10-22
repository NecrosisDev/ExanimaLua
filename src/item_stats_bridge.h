#pragma once
#include "item_stats.h"
// items.stats(id): copies an item's weapon and apparel chunks through the game's record chunk lookup 0x1041b0
// (the same reviewed lookup the inventory and container paths call on this thread) and parses the copy. Nothing
// is written; the lookup resolves database, instanced and level-local IDs itself.
namespace itemstats {
using Lookup=uintptr_t(*)(uint32_t,uint32_t);
static Lookup lookup=nullptr;
static bool available(){
 const unsigned char chunkPrefix[]={0x53,0x56,0x48,0x8d,0x64,0x24,0xc8,0x89,0xd3,0x48,0x8d,0x54};
 return gamebridge::operation(base+0x1041b0,chunkPrefix,sizeof chunkPrefix)&&readable(base+0x4ef920,8);
}
static uintptr_t find(uint32_t id,uint32_t tag){__try{return lookup?lookup(id,tag):0;}__except(EXCEPTION_EXECUTE_HANDLER){return 0;}}
// The payload copy, or false when the chunk is absent, mis-tagged or shorter than `bytes`.
static bool payload(uint32_t id,uint32_t tag,size_t bytes,std::vector<uint8_t>& out){
 const auto at=find(id,tag);uint32_t header[2]{};
 if(!at||!copyWorldMemory(at,header,8)||header[0]!=tag||header[1]<8+bytes||header[1]>64*1024)return false;
 out.resize(bytes);return copyWorldMemory(at+8,out.data(),bytes);
}
// `gameThread`: called from a game click handler during play (gear.use) rather than a Lua pulse.
static bool readAt(uint32_t id,scripting::ItemStats& stats,bool gameThread){
 // Session pulses only, like the other world reads: level-local and instanced records change while a level loads.
 if((!worldPulse&&!gameThread)||!lookup||!id||(id&0xc0000000u)==0xc0000000u)return false;
 std::vector<uint8_t> bytes;stats={};
 if(payload(id,scripting::weaponTag,scripting::weaponBytes,bytes)){stats.weapon=true;scripting::parseWeapon(bytes.data(),stats.w);stats.condition=scripting::conditionAt(bytes.data()+0x0c);}
 if(payload(id,scripting::apparelTag,scripting::apparelBytes,bytes)){stats.armour=true;scripting::parseArmour(bytes.data(),stats.a);if(!stats.condition.known)stats.condition=scripting::conditionAt(bytes.data()+0x1b0);}
 return find(id,0x100000)!=0;   // a record without a look chunk is not an item
}
static bool read(uint32_t id,scripting::ItemStats& stats){return readAt(id,stats,false);}
}
