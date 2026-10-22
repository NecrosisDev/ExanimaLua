#include "script_host.h"
#include <cstdio>
#include <map>
#include "test_assert.h"
static std::map<uintptr_t,std::vector<unsigned char>> memory;
static bool readMemory(uintptr_t address,void* out,size_t size){
 for(const auto& row:memory)if(address>=row.first&&address-row.first<=row.second.size()&&size<=row.second.size()-(address-row.first)){
  std::memcpy(out,row.second.data()+address-row.first,size);return true;
 }return false;
}
template<class T>static void put(uintptr_t address,size_t offset,T value){std::memcpy(memory[address].data()+offset,&value,sizeof value);}
static scripting::WorldSnapshot snapshot;
static const scripting::WorldSnapshot* provider(){return &snapshot;}
int main(){
 using namespace scripting;
 memory[0x10000].resize(0x190);memory[0x20000].resize(0x190);memory[0x30000].resize(16);
 put(0x10000,0x118,int32_t(1));put(0x10000,0x120,uintptr_t(0x30000));
 put(0x30000,0,uintptr_t(0x20000));put(0x30000,8,uintptr_t(0x10000));
 put(0x20000,8,uint32_t(0x4000));put(0x20000,0x180,uint32_t(0x80000042));put(0x20000,0x94,float(12));
 snapshot=readWorld(0x10000,readMemory);
 assert(snapshot.ready&&snapshot.truncated&&snapshot.objects.size()==1&&snapshot.objects[0].item==0x42&&snapshot.objects[0].position[0]==12);
 put(0x10000,0x118,int32_t(0));snapshot=readWorld(0x10000,readMemory);assert(snapshot.ready&&!snapshot.truncated);
 Host host(worldRead);host.setWorldProvider(provider);
 const std::string source="local s=world.snapshot();assert(s.space=='level_saved' and #s.objects==1 and s.objects[1].item_id==66 and s.objects[1].position[1]==12);s.objects[1].position[1]=999;assert(world.snapshot().objects[1].position[1]==12)";
 assert(host.load("world",source.data(),source.size(),"world.read"));
 assert(!host.load("denied","world.snapshot()",16,""));
 const std::string quota="for i=1,5 do world.snapshot() end";assert(!host.load("quota",quota.data(),quota.size(),"world.read"));
 snapshot.ready=false;const std::string absent="assert(world.snapshot()==nil)";assert(host.load("absent",absent.data(),absent.size(),"world.read"));
 put(0x10000,0x118,int32_t(9000));snapshot=readWorld(0x10000,readMemory);assert(snapshot.truncated);
 assert(!readWorld(0x99999,readMemory).ready);
 put(0x10000,0x118,int32_t(0));put(0x20000,0x94,(std::numeric_limits<float>::quiet_NaN)());snapshot=readWorld(0x10000,readMemory);assert(snapshot.truncated&&snapshot.objects.empty());
 memory.clear();memory[0x10000].resize(0x190);memory[0x30000].resize(513*8);
 put(0x10000,0x118,int32_t(512));put(0x10000,0x120,uintptr_t(0x30000));
 for(size_t i=0;i<513;++i){const uintptr_t address=0x100000+i*0x1000;memory[address].resize(0x190);put(address,8,uint32_t(0x2000));
  std::memcpy(memory[address].data()+0x140,"sixteenbyte_name",16);put(0x30000,i*8,address);}
 snapshot=readWorld(0x10000,readMemory);assert(snapshot.ready&&snapshot.truncated&&snapshot.objects.size()==512&&snapshot.objects[0].name.size()==16);
 std::puts("LUA_WORLD_PASSED");
}
