#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>
#include "shared/game_structs.h"

namespace scripting {
struct WorldObject {std::string kind,name;uint32_t item=0;uintptr_t nativeNode=0;std::array<float,3> position{};};
struct WorldSnapshot {bool ready=false,truncated=false;std::vector<WorldObject> objects;};
using WorldRead=bool(*)(uintptr_t,void*,size_t);
// Native 0x00bd70/0x00c340 ancestry, with a bound for copied runtime readers.
inline bool classMatches(uintptr_t object,uintptr_t wanted,WorldRead read){
 uintptr_t type=0;std::array<uintptr_t,32> seen{};size_t count=0;if(!read||!read(object,&type,8))return false;
 while(type&&count<seen.size()){if(type==wanted)return true;if(std::find(seen.begin(),seen.begin()+count,type)!=seen.begin()+count)return false;seen[count++]=type;
  uintptr_t parents=0;if(!read(type+0x10,&parents,8)||!parents||!read(parents,&type,8))return false;
 }return false;
}
// The editor's level-tree contract; copied values have no native object lifetime.
inline WorldSnapshot readWorld(uintptr_t root,WorldRead read){
 WorldSnapshot out;if(!root||!read)return out;
 std::vector<uintptr_t> stack{root};std::unordered_set<uintptr_t> seen;
 size_t visits=0;out.ready=true;
 while(!stack.empty()){
  if(++visits>8192){out.truncated=true;break;}
  const auto node=stack.back();stack.pop_back();
  if(!node||!seen.insert(node).second){out.truncated=true;continue;}
  unsigned char bytes[0x190]{};
  if(!read(node,bytes,sizeof bytes)){if(node==root)out.ready=false;out.truncated=true;continue;}
  uint32_t flags;std::memcpy(&flags,bytes+8,4);
  if(node!=root){
   WorldObject object;object.nativeNode=node;
   if((flags&0x7000)==0x4000){object.kind="item";std::memcpy(&object.item,bytes+0x180,4);object.item&=0x7fffffffu;}
   else if(flags&0x2000){object.kind="object";const char* name=(const char*)bytes+fp::game::sceneNode::name;
    size_t size=0;while(size<16&&name[size])++size;object.name.assign(name,size);}
   else if((flags&0xfff)==5)object.kind="light";
   if(!object.kind.empty()){
    // Level saved pose, not a dynamic body's rendered position or global coordinates.
    std::memcpy(object.position.data(),bytes+0x70+0x24,12);
    bool finite=true;for(float value:object.position)finite=finite&&std::isfinite(value);
    if(finite){if(out.objects.size()==512){out.truncated=true;break;}out.objects.push_back(std::move(object));}
    else out.truncated=true;
   }
  }
  if(node==root||(flags&0xfff)==0x11){
   int32_t last;uintptr_t children;
   std::memcpy(&last,bytes+fp::game::sceneNode::lastChild,4);
   std::memcpy(&children,bytes+fp::game::sceneNode::children,8);
   if(last==-1)continue;
   if(last<0||last>=8192||stack.size()+size_t(last)+1>8192){out.truncated=true;continue;}
   std::vector<uintptr_t> list(size_t(last)+1);
   if(!read(children,list.data(),list.size()*sizeof(uintptr_t))){out.truncated=true;continue;}
   for(auto i=list.rbegin();i!=list.rend();++i)if(*i)stack.push_back(*i);
  }
 }
 return out;
}
// world.nearby: placed objects (scene flag 0x2000) whose model name is one of `names` (case-insensitive) and whose
// live translation (node +0x64, the frame the player's body centre +0xab4 and the pickup reach check use) lies
// within `radius` of `centre`. Walks the same tree as readWorld without its 512-row cap, up to 32768 nodes, and
// reads only the fields it needs. At most 64 rows, nearest first.
struct NearbyObject {std::string name;float distance=0;std::array<float,3> position{};};
struct NearbyResult {bool ready=false,truncated=false;std::vector<NearbyObject> objects;};
inline bool sameName(const char* a,size_t n,const std::string& b){
 if(n!=b.size())return false;for(size_t i=0;i<n;++i)if(std::tolower((unsigned char)a[i])!=std::tolower((unsigned char)b[i]))return false;return true;
}
inline NearbyResult readNearby(uintptr_t root,WorldRead read,const std::array<float,3>& centre,float radius,const std::vector<std::string>& names){
 NearbyResult out;if(!root||!read)return out;
 std::vector<uintptr_t> stack{root};std::unordered_set<uintptr_t> seen;size_t visits=0;out.ready=true;
 while(!stack.empty()){
  if(++visits>32768){out.truncated=true;break;}
  const auto node=stack.back();stack.pop_back();
  if(!node||!seen.insert(node).second){out.truncated=true;continue;}
  uint32_t flags=0;if(!read(node+8,&flags,4)){if(node==root)out.ready=false;out.truncated=true;continue;}
  if(node!=root&&(flags&0x2000)&&(flags&0x7000)!=0x4000){
   char name[16]{};std::array<float,3> at{};
   if(read(node+fp::game::sceneNode::name,name,16)&&read(node+0x64,at.data(),12)){
    size_t n=0;while(n<16&&name[n])++n;
    bool wanted=false;for(const auto& w:names)wanted=wanted||sameName(name,n,w);
    if(wanted&&std::isfinite(at[0])&&std::isfinite(at[1])&&std::isfinite(at[2])){
     const float dx=at[0]-centre[0],dy=at[1]-centre[1],dz=at[2]-centre[2],d=std::sqrt(dx*dx+dy*dy+dz*dz);
     if(d<=radius)out.objects.push_back({std::string(name,n),d,at});
    }
   }else out.truncated=true;
  }
  if(node==root||(flags&0xfff)==0x11){
   int32_t last=0;uintptr_t children=0;
   if(!read(node+fp::game::sceneNode::lastChild,&last,4)||!read(node+fp::game::sceneNode::children,&children,8)){out.truncated=true;continue;}
   if(last==-1)continue;
   if(last<0||last>=32768||stack.size()+size_t(last)+1>32768){out.truncated=true;continue;}
   std::vector<uintptr_t> list(size_t(last)+1);
   if(!read(children,list.data(),list.size()*sizeof(uintptr_t))){out.truncated=true;continue;}
   for(auto i=list.rbegin();i!=list.rend();++i)if(*i)stack.push_back(*i);
  }
 }
 std::sort(out.objects.begin(),out.objects.end(),[](const NearbyObject& a,const NearbyObject& b){return a.distance<b.distance;});
 if(out.objects.size()>64){out.objects.resize(64);out.truncated=true;}
 return out;
}
}
