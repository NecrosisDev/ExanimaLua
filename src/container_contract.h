#pragma once
#include "shared/hotload.h"
namespace scripting {
struct ContainerEntry {uint32_t item=0,state=0;};
struct ContainerView {std::vector<ContainerEntry> entries;};
struct ContainerChange {uint64_t target=0;uint32_t item=0;bool add=false;};
inline bool containerView(const std::vector<uint8_t>& record,ContainerView& out){
 out.entries.clear();size_t at=0;bool found=false;
 while(at<record.size()){
  if(record.size()-at<8)return false;uint32_t tag=0,size=0;
  std::memcpy(&tag,record.data()+at,4);std::memcpy(&size,record.data()+at+4,4);
  if(size<8||size>record.size()-at)return false;
  if(tag==0x20000){
   if(found||size<32)return false;found=true;uint32_t count=0;std::memcpy(&count,record.data()+at+28,4);
   if(count>4096||size_t(count)*12>size-32)return false;
   for(uint32_t i=0;i<count;++i){ContainerEntry row;
    std::memcpy(&row.item,record.data()+at+32+size_t(i)*12,4);std::memcpy(&row.state,record.data()+at+36+size_t(i)*12,4);out.entries.push_back(row);
   }
  }at+=size;
 }return found;
}
}
