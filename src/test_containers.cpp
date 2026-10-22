#include "plugin.cpp"
#include <cstdio>
#include "test_assert.h"
template<class T>static void put(uintptr_t at,T value){std::memcpy((void*)at,&value,sizeof value);}
static std::vector<uint8_t> bytes;static uintptr_t database=0,table=0;static bool chestOpen=false,faultReplace=false;static int writes=0;
static void sync(){put(database+0x18,(uintptr_t)bytes.data());put(database+0x20,uint32_t(bytes.size()));put(table+12,uint32_t(bytes.size()));}
static int findFixture(uintptr_t db,int id){return db==database&&id==7?0:-1;}
static char openFixture(int){return chestOpen?1:0;}
static uintptr_t chunkFixture(uint32_t item,uint32_t tag){return item>=41&&item<=43&&tag==0x100000?0x10000:0;}
static void replaceFixture(uintptr_t db,unsigned index,void* data){
 assert(db==database&&index==0);++writes;if(faultReplace)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
 struct Buffer {const uint8_t* data;uint32_t size,pad;};const auto& buffer=*(Buffer*)data;
 bytes.assign(buffer.data,buffer.data+buffer.size);sync();
}
static bool load(const char* id,const std::string& source,const std::string& caps="world.read,containers.edit,timers"){
 return host.load(id,source.data(),source.size(),caps);
}
static void tick(uint64_t now){
 worldPulse=true;beginAiPulse("session",now);containerbridge::targets.clear();worldCaptured=false;worldCopy={};host.setTime(now);host.runTimers();worldPulse=false;
}
int main(){
 using namespace scripting;
 std::vector<uint8_t> image(0x600000),world(0x1c30),player(0x1400),node(0x190),db(0x40);uint32_t slots[4]={7,0,0,0};
 base=(uintptr_t)image.data();const auto w=(uintptr_t)world.data(),p=(uintptr_t)player.data(),n=(uintptr_t)node.data();database=(uintptr_t)db.data();table=(uintptr_t)slots;
 put(base+npc::off::world,w);put(base+npc::off::controlled,p);put(base+0x49b300,w);std::memcpy(world.data()+0x140,"exanima01",9);
 const uintptr_t child=n;put(w+0x118,int32_t(0));put(w+0x120,(uintptr_t)&child);put(n+8,uint32_t(0x4000));put(n+0x180,uint32_t(0x80000007));
 put(base+0x4ef930,database);put(database+8,int32_t(1));put(database+0x10,table);
 bytes.resize(88);put((uintptr_t)bytes.data(),uint32_t(0x100000));put((uintptr_t)bytes.data()+4,uint32_t(36));
 put((uintptr_t)bytes.data()+28,uint32_t(123));put((uintptr_t)bytes.data()+32,uint32_t(456));
 put((uintptr_t)bytes.data()+36,uint32_t(0x20000));put((uintptr_t)bytes.data()+40,uint32_t(44));put((uintptr_t)bytes.data()+64,uint32_t(1));put((uintptr_t)bytes.data()+68,uint32_t(41));
 put((uintptr_t)bytes.data()+80,uint32_t(0x123));put((uintptr_t)bytes.data()+84,uint32_t(8));sync();const auto original=bytes;
 ContainerView view;assert(containerView(bytes,view)&&view.entries.size()==1&&view.entries[0].item==41);
 auto malformed=bytes;malformed.pop_back();assert(!containerView(malformed,view));
 containerbridge::find=findFixture;containerbridge::replace=replaceFixture;containerbridge::open=openFixture;containerbridge::chunk=chunkFixture;
 worldPossible=containerWritePossible=true;gamebridge::backupDone=true;host.capabilities(worldRead|containerEdit|timerEvents);host.setWorldProvider(captureWorld);
 host.setContainerApi(containerbridge::capture,containerbridge::read,containerbridge::commit);beginAiPulse("session",0);
 assert(load("rollback","timers.after(0,function()local c=world.snapshot().objects[1];c:add_item(42);error('discard')end)"));tick(3001);assert(bytes==original&&writes==0);
 assert(load("add","timers.after(0,function()local c=world.snapshot().objects[1];local r=c:contents();assert(#r==1 and r[1].item_id==41);r[1].item_id=999;assert(c:contents()[1].item_id==41);c:add_item(42)end)"));tick(3002);
 assert(writes==1&&containerView(bytes,view)&&view.entries.size()==2&&view.entries.back().item==42);
 assert(std::memcmp(bytes.data()+28,original.data()+28,8)==0);
 assert(load("remove","timers.after(0,function()world.snapshot().objects[1]:remove_item(42)end)"));tick(3003);assert(bytes==original&&writes==2);
 assert(load("stale","timers.after(0,function()local c=world.snapshot().objects[1];timers.after(0,function()c:add_item(42)end)end)"));tick(3004);tick(3005);assert(bytes==original&&!host.enabled("stale"));
 assert(load("denied","timers.after(0,function()assert(world.snapshot().objects[1].add_item==nil)end)","world.read,timers"));tick(3006);assert(host.enabled("denied"));
 chestOpen=true;assert(load("open","timers.after(0,function()assert(world.snapshot().objects[1].add_item==nil)end)"));tick(3007);assert(host.enabled("open"));chestOpen=false;
 put(base+0x499860,int32_t(1));assert(load("gui","timers.after(0,function()assert(world.snapshot().objects[1].add_item==nil)end)"));tick(3008);assert(host.enabled("gui"));put(base+0x499860,int32_t(0));
 assert(load("invalid","timers.after(0,function()world.snapshot().objects[1]:add_item(999)end)"));tick(3009);assert(bytes==original&&writes==2);
 worldPulse=true;beginAiPulse("session",3010);containerbridge::targets.clear();const auto ticket=containerbridge::capture(n);assert(ticket);
 const std::vector<ContainerChange> change={{ticket,42,true}};
 bytes[29]^=1;sync();assert(!containerbridge::commit(change)&&writes==2);bytes=original;sync();
 put(n+0x180,uint32_t(7));assert(!containerbridge::commit(change));put(n+0x180,uint32_t(0x80000007));
 chestOpen=true;assert(!containerbridge::commit(change));chestOpen=false;
 assert(!containerbridge::commit({{ticket,43,false}})&&writes==2);
 gamebridge::backupDone=false;assert(!containerbridge::commit(change));gamebridge::backupDone=true;worldPulse=false;
 host.setTime(10000);host.capabilities(worldRead|containerEdit|timerEvents|lifecycleEvents);
 std::ifstream example("reward-chest.lua",std::ios::binary);assert(example);const std::string source{std::istreambuf_iterator<char>(example),std::istreambuf_iterator<char>()};
 const std::vector<Module> modules={{"config","return {enabled=true,container_record=7,item_id=42}"}};
 assert(host.load("reward",source.data(),source.size(),"world.read,containers.edit,timers,events.lifecycle",8*1024*1024,modules));host.notifyPhase("session",1);tick(13501);
 assert(containerView(bytes,view)&&view.entries.size()==2&&writes==3);
 worldPulse=true;beginAiPulse("session",13502);containerbridge::targets.clear();const auto faultTicket=containerbridge::capture(n);assert(faultTicket);
 faultReplace=true;assert(!containerbridge::commit({{faultTicket,43,true}})&&containerbridge::broken&&aiWriteBroken);assert(!containerbridge::commit({{faultTicket,43,true}}));
 std::puts("LUA_CONTAINERS_PASSED");
}
