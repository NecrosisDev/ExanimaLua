#pragma once
static bool worldPossible=false,worldPulse=false,worldCaptured=false;
static scripting::WorldSnapshot worldCopy;
static bool copyWorldMemory(uintptr_t address,void* target,size_t size){
 __try{if(!readable(address,size))return false;std::memcpy(target,(void*)address,size);return true;}
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
