#pragma once
static bool worldPossible=false,worldPulse=false,worldCaptured=false;
static scripting::WorldSnapshot worldCopy;
// Guarded copies are the runtime's hot path (every scan of characters and scene nodes): the committed-region check
// (VirtualQuery, a system call) is cached per thread for one pulse. The copy itself stays under SEH, so a region the
// game released since its check fails the copy instead of faulting.
namespace readcache {
struct Region {uintptr_t lo=0,hi=0;};
static thread_local Region slots[32];static thread_local unsigned seen=0,nextSlot=0;
static std::atomic<unsigned> generation{1};
static void newPulse(){generation.fetch_add(1,std::memory_order_relaxed);}
static bool check(uintptr_t p,size_t n){
 if(p<0x10000||p+n<p)return false;
 const unsigned g=generation.load(std::memory_order_relaxed);
 if(seen!=g){for(auto& s:slots)s={};seen=g;}
 for(const auto& s:slots)if(p>=s.lo&&p+n<=s.hi&&s.hi)return true;
 MEMORY_BASIC_INFORMATION m{};
 if(VirtualQuery((void*)p,&m,sizeof m)!=sizeof m||m.State!=MEM_COMMIT||(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
 const uintptr_t lo=(uintptr_t)m.BaseAddress,hi=lo+m.RegionSize;slots[nextSlot++%32]={lo,hi};return p+n<=hi;
}
}
static bool copyWorldMemory(uintptr_t address,void* target,size_t size){
 __try{if(!readcache::check(address,size))return false;std::memcpy(target,(void*)address,size);return true;}
 __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool worldContract(){
 // Reviewed editor pose propagator identifies the scene-node layout contract.
 const unsigned char prefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0x90,0x48,0x89,0x5d,0xc8,0x48,0x89,0x7d,0xd0,0x48,0x89,0x75,0xd8,0x48,0x89,0x4d,0xf8};
 return gamebridge::operation(base+0x50eb0,prefix,sizeof prefix)&&readable(base+0x4f0bd0,8);
}
// world.nearby, around the controlled character's body centre (+0xab4), in the live frame pickup reach uses.
static bool readNearby(float radius,const std::vector<std::string>& names,scripting::NearbyResult& out){
 out={};if(!worldPossible||!worldPulse)return false;uintptr_t root=0,player=0;std::array<float,3> centre{};
 if(!copyWorldMemory(base+0x4f0bd0,&root,8)||!root||!copyWorldMemory(base+0x4f0830,&player,8)||!player||!copyWorldMemory(player+0xab4,centre.data(),12))return false;
 for(float v:centre)if(!std::isfinite(v))return false;
 out=scripting::readNearby(root,copyWorldMemory,centre,radius,names);return out.ready;
}
static const scripting::WorldSnapshot* captureWorld(){
 if(!worldPossible||!worldPulse)return nullptr;
 if(!worldCaptured){
  worldCaptured=true;uintptr_t root=0;
  if(copyWorldMemory(base+0x4f0bd0,&root,8))worldCopy=scripting::readWorld(root,copyWorldMemory);
 }
 return &worldCopy;
}
