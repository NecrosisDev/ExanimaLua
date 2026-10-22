#pragma once
// world.level(): where the session is, from globals the level code keeps (static reading).
// The world object 0x4f0bd0 holds the current level's 16-byte name at +0x140 (written by
// 0x181450/0x180920), int32 0x2b7540 is the game mode (4 story, 8 arena, 2 practice, 0x1010 versus) and int32
// 0x2e8210 the current level's id; 0x4eff70 is the game-flow state. Reads only; undecoded values stay unknown.
namespace levelinfo {
constexpr uintptr_t worldGlobal=0x4f0bd0,nameAt=0x140,modeGlobal=0x2b7540,idGlobal=0x2e8210,flowGlobal=0x4eff70;
static bool read(scripting::LevelInfo& out){
 out={};if(!worldPulse)return false;uintptr_t world=0;char name[17]{};
 if(!copyWorldMemory(base+worldGlobal,&world,8)||!world||!copyWorldMemory(world+nameAt,name,16))return false;
 size_t n=0;while(n<16&&name[n])++n;if(!n)return false;
 for(size_t i=0;i<n;++i)if(name[i]<0x20||name[i]>0x7e)return false;
 for(size_t i=n;i<16;++i)if(name[i])return false;   // a decoded name is NUL padded
 out.name.assign(name,n);
 int32_t mode=0,id=0,flow=0;uintptr_t player=0;
 if(copyWorldMemory(base+modeGlobal,&mode,4))out.mode=mode==4?"story":mode==8?"arena":mode==2?"practice":mode==0x1010?"versus":"";
 if(copyWorldMemory(base+idGlobal,&id,4)&&id>=0&&id<0x10000){out.id=uint32_t(id);out.idKnown=true;}
 // No native "level ready" flag exists; flow state 3 with a controlled character is the game's own playing state.
 out.loaded=copyWorldMemory(base+flowGlobal,&flow,4)&&flow==3&&copyWorldMemory(base+0x4f0830,&player,8)&&player;
 return true;
}
}
