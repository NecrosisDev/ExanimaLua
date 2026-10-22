#include "plugin.cpp"
#include <map>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include "test_assert.h"
static constexpr char modelName[16]="keyring_a";
static void loadKeyring(){host.capabilities(scripting::itemApiCapabilities);std::ifstream file("keyring.lua");assert(file);std::stringstream source;source<<file.rdbuf();auto text=source.str();assert(host.load("keyring",text.data(),text.size(),"events.item_use,items.read,containers.read,containers.open,doors.unlock"));}
static std::map<std::pair<uint32_t,uint32_t>,std::vector<unsigned char>> records;
static unsigned unlocks=0,opened=0,uses=0,inventoryUses=0;
static uintptr_t focusAt=0,objectAt=0,doorAt=0;static int kind=4;static bool hitOk=true;
static uintptr_t lookup(uint32_t id,uint32_t tag){const auto it=records.find({id,tag});return it==records.end()?0:(uintptr_t)it->second.data();}
static uintptr_t openStorage(uint32_t id){assert(id==1424);++opened;return 99;}
static uintptr_t focus(uintptr_t){return focusAt;}
static bool pick(uintptr_t,float,float,void* result){assert(*(uint32_t*)((unsigned char*)result+0x34)==1);*(uintptr_t*)result=objectAt;return hitOk;}
static uintptr_t object(uintptr_t p){return p;}
static int interaction(uintptr_t){return kind;}
static void unlock(uintptr_t p){assert(p==doorAt);++unlocks;}
static uintptr_t stockUse(uintptr_t,uintptr_t){++uses;return 77;}
static void stockInventoryUse(uintptr_t,uintptr_t){++inventoryUses;}
static void screenToLocal(uintptr_t,int*,int*){}
static int hitIndex=1;
static int inventoryHit(uintptr_t,int,int,void*,void*){return hitIndex;}
template<class T> static void put(std::vector<unsigned char>& b,size_t off,T value){std::memcpy(b.data()+off,&value,sizeof value);}
static std::vector<unsigned char>& record(uint32_t id,uint32_t tag,size_t size){
 auto& b=records[{id,tag}];b.resize(size);put(b,0,tag);put(b,4,uint32_t(size));return b;
}
int main(){
 loadKeyring();
 std::vector<unsigned char> image(0x500000),control(0x40),mouse(0x40),obj(0x188),node(0x168),door(0x608),gui(0x200),form(0x200);
 base=(uintptr_t)image.data();objectAt=(uintptr_t)obj.data();doorAt=(uintptr_t)door.data();focusAt=(uintptr_t)mouse.data();
 assert(gamebridge::atMenu());
 put(image,0x4f0830,uintptr_t(1));assert(!gamebridge::atMenu());
 put(image,0x4f0830,uintptr_t(0));
 put(image,0x49a960,focusAt);put(control,0x18,focusAt);put(control,0x20,uint32_t(1424));
 uintptr_t nodeAt=(uintptr_t)node.data();put(obj,0x120,(uintptr_t)&nodeAt);put(node,0x160,doorAt);
 put(obj,0x180,uint32_t(500));door[0x604]=1;
 auto& look=record(1424,gamebridge::lookTag,48);std::memcpy(look.data()+8,modelName,16);
 auto& container=record(1424,gamebridge::containerTag,68);put(container,28,uint32_t(3));
 put(container,32,uint32_t(11));put(container,44,uint32_t(90));put(container,56,uint32_t(120));
 auto& key=record(90,gamebridge::deviceTag,68);key[8]=1;
 auto& locked=record(500,0x40000,28);put(locked,12,uint32_t(90));
 gamebridge::api={lookup,openStorage,focus,pick,object,interaction,unlock,screenToLocal};
 gamebridge::originalUse=stockUse;gamebridge::originalInventoryUse=stockInventoryUse;
 gamebridge::backupDone=true;gamebridge::active=true;
 const auto savedControl=control,savedContainer=container;
 scripting::Event validation;validation.item=1424;
 assert(gamebridge::doorStillValid((uintptr_t)control.data(),objectAt,nodeAt,doorAt,validation,500));
 put(obj,0x180,uint32_t(501));assert(!gamebridge::doorStillValid((uintptr_t)control.data(),objectAt,nodeAt,doorAt,validation,500));put(obj,0x180,uint32_t(500));
 put(node,0x160,uintptr_t(0));assert(!gamebridge::doorStillValid((uintptr_t)control.data(),objectAt,nodeAt,doorAt,validation,500));put(node,0x160,doorAt);
 gamebridge::use((uintptr_t)control.data(),0);assert(unlocks==1&&uses==0&&control==savedControl&&container==savedContainer);
 put(locked,12,uint32_t(999));gamebridge::use((uintptr_t)control.data(),0);assert(unlocks==1);
 put(locked,12,uint32_t(90));key[9]=8;gamebridge::use((uintptr_t)control.data(),0);assert(unlocks==1);key[9]=0;
 put(container,48,gamebridge::removedFlags);gamebridge::use((uintptr_t)control.data(),0);assert(unlocks==1);put(container,48,uint32_t(0));
 uintptr_t formAt=(uintptr_t)form.data();put(image,0x499790,(uintptr_t)gui.data());put(gui,0xa8,int(1));put(gui,0xb0,(uintptr_t)&formAt);
 put(form,0xb8,uint32_t(0xce1c2013));put(form,0x15c,uint32_t(1424));
 gamebridge::use((uintptr_t)control.data(),0);assert(unlocks==1);
 put(gui,0xa8,int(0));gamebridge::use((uintptr_t)control.data(),0);assert(unlocks==2);
 kind=3;gamebridge::use((uintptr_t)control.data(),0);assert(unlocks==2);kind=4;
 door[0x604]=0;gamebridge::use((uintptr_t)control.data(),0);assert(unlocks==2);door[0x604]=1;
 focusAt=0;gamebridge::use((uintptr_t)control.data(),0);assert(unlocks==2);focusAt=(uintptr_t)mouse.data();
 hitOk=false;gamebridge::use((uintptr_t)control.data(),0);assert(unlocks==2);hitOk=true;
 put(container,28,uint32_t(100000));gamebridge::use((uintptr_t)control.data(),0);assert(unlocks==2);put(container,28,uint32_t(3));
 // Two runtime instances with the same model must search their own contents.
 auto& otherLook=record(0x80000022,gamebridge::lookTag,48);
 std::memcpy(otherLook.data()+8,modelName,16);
 auto& otherContents=record(0x80000022,gamebridge::containerTag,44);
 put(otherContents,28,uint32_t(1));put(otherContents,32,uint32_t(11));
 put(control,0x20,uint32_t(0x80000022));
 unsigned failedUses=uses;
 assert(gamebridge::use((uintptr_t)control.data(),0)==77&&uses==failedUses+1&&unlocks==2);
 // A committed drag into this ring is visible on the next use; removal is too.
 put(otherContents,32,uint32_t(90));
 gamebridge::use((uintptr_t)control.data(),0);assert(unlocks==3);
 put(otherContents,28,uint32_t(0));
 gamebridge::use((uintptr_t)control.data(),0);assert(unlocks==3);
 // A ring containing another ring does not inherit the nested ring's keys.
 put(otherContents,28,uint32_t(1));put(otherContents,32,uint32_t(1424));
 gamebridge::use((uintptr_t)control.data(),0);assert(unlocks==3);
 put(locked,12,uint32_t(0x80000022));failedUses=uses;
 gamebridge::use((uintptr_t)control.data(),0);assert(unlocks==3&&uses==failedUses);
 put(locked,12,uint32_t(90));put(control,0x20,uint32_t(1424));
 std::vector<unsigned char> inventory(0x168),vtable(0x128),row(24);
 put(inventory,0,(uintptr_t)vtable.data());put(vtable,0x120,(uintptr_t)inventoryHit);
 put(inventory,0x158,int(1));put(inventory,0x160,(uintptr_t)row.data());put(row,8,uint32_t(1424));
 gamebridge::inventoryUse((uintptr_t)control.data(),(uintptr_t)inventory.data());assert(opened==1&&inventoryUses==0);
 put(row,8,uint32_t(11));gamebridge::inventoryUse((uintptr_t)control.data(),(uintptr_t)inventory.data());assert(opened==1);
 put(row,8,uint32_t(1424));hitIndex=0;gamebridge::inventoryUse((uintptr_t)control.data(),(uintptr_t)inventory.data());assert(opened==1);hitIndex=1;
 const unsigned stockBefore=uses;
 put(control,0x20,uint32_t(11));assert(gamebridge::use((uintptr_t)control.data(),0)==77&&uses==stockBefore+1);
 gamebridge::inventoryUse((uintptr_t)control.data(),(uintptr_t)inventory.data());assert(inventoryUses==1);
 put(control,0x20,uint32_t(1424));assert(disable());assert(gamebridge::use((uintptr_t)control.data(),0)==77&&uses==stockBefore+2);
 gamebridge::inventoryUse((uintptr_t)control.data(),(uintptr_t)inventory.data());assert(inventoryUses==2);gamebridge::active=true;loadKeyring();
 gamebridge::backupDone=false;gamebridge::backupFailed=true;
 gamebridge::inventoryUse((uintptr_t)control.data(),(uintptr_t)inventory.data());assert(opened==1);gamebridge::use((uintptr_t)control.data(),0);assert(unlocks==3);
 std::puts("LUA_BRIDGE_PASSED: simulated native calls; unlock, wrong-key forwarding, separate rings, committed add/remove, menu guard, stale UI, identity, disabled, backup failure, no mutation");
}
