#pragma once
#include "shared/game_layout.h"
#include "shared/npc_live.h"

namespace scripting {
inline bool characterListContract(const fp::ImageView& view){
 if(!view.data||view.textBegin>=view.textEnd||view.textEnd>view.size)return false;
 bool found=false;
 for(const auto& signature:fp::sig::signatures){
  if(signature.target!=fp::sig::world||!std::strstr(signature.pattern,"28 1C 00 00")||!std::strstr(signature.pattern,"F4 0D 00 00"))continue;
  fp::Pattern pattern;uint32_t at=0;if(!fp::parsePattern(signature.pattern,pattern))return false;
  const auto count=fp::findPattern(view,pattern,view.textBegin,view.textEnd,&at);
  if(count>1)return false;if(!count)continue;
  if(size_t(at)+signature.offset+4>view.size)return false;
  int32_t displacement=0;std::memcpy(&displacement,view.data+at+signature.offset,4);
  const int64_t target=int64_t(at)+signature.insnEnd+displacement+signature.addend;
  if(target!=int64_t(npc::off::world)||uint64_t(target)+8>view.size)return false;
  found=true;
 }
 return found;
}
}
