#pragma once
#include "character_contract.h"
static bool charactersPossible=false,charactersCaptured=false;
static scripting::CharacterSnapshot charactersCopy;
static bool awarenessCaptured=false;
static scripting::CharacterSnapshot awarenessCopy;
static bool detailsCaptured=false;
static scripting::CharacterSnapshot detailsCopy;
static void invalidateCharacterCopies(){charactersCaptured=false;awarenessCaptured=false;detailsCaptured=false;}
static bool charactersContract(){
 IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};
 if(!copyWorldMemory(base,&dos,sizeof dos)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||dos.e_lfanew>0x100000)return false;
 if(!copyWorldMemory(base+dos.e_lfanew,&nt,sizeof nt)||nt.Signature!=IMAGE_NT_SIGNATURE||
    nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||
    nt.OptionalHeader.SizeOfImage<npc::off::world+8||nt.OptionalHeader.SizeOfImage>64*1024*1024||
    !nt.FileHeader.NumberOfSections||nt.FileHeader.NumberOfSections>96)return false;
 const auto sectionTable=uintptr_t(dos.e_lfanew)+offsetof(IMAGE_NT_HEADERS64,OptionalHeader)+nt.FileHeader.SizeOfOptionalHeader;
 if(sectionTable+size_t(nt.FileHeader.NumberOfSections)*sizeof(IMAGE_SECTION_HEADER)>nt.OptionalHeader.SizeOfImage)return false;
 for(unsigned i=0;i<nt.FileHeader.NumberOfSections;++i){
  IMAGE_SECTION_HEADER section{};if(!copyWorldMemory(base+sectionTable+i*sizeof section,&section,sizeof section))return false;
  if(std::memcmp(section.Name,".text\0\0\0",8))continue;
  const size_t begin=section.VirtualAddress,size=section.Misc.VirtualSize;
  if(!size||size>16*1024*1024||begin>nt.OptionalHeader.SizeOfImage||size>nt.OptionalHeader.SizeOfImage-begin)return false;
  std::vector<uint8_t> image(nt.OptionalHeader.SizeOfImage);
  for(size_t offset=0;offset<size;){
   const size_t chunk=(std::min)(size-offset,size_t(4096-((base+begin+offset)&4095)));
   if(!copyWorldMemory(base+begin+offset,image.data()+begin+offset,chunk))return false;offset+=chunk;
  }
  return scripting::characterListContract({image.data(),image.size(),base,uint32_t(begin),uint32_t(begin+size)});
 }
 return false;
}
// The character scan's own cost, for the frame-cost line.
static double characterScanMs=0;static uint64_t characterScans=0;
static const scripting::CharacterSnapshot* captureCharacters(){
 if(!charactersPossible||!worldPulse)return nullptr;
 if(!charactersCaptured){charactersCaptured=true;LARGE_INTEGER a,b,f;QueryPerformanceCounter(&a);charactersCopy=scripting::readCharacters(base,copyWorldMemory);
  QueryPerformanceCounter(&b);QueryPerformanceFrequency(&f);characterScanMs+=double(b.QuadPart-a.QuadPart)*1000.0/double(f.QuadPart);++characterScans;}
 return &charactersCopy;
}
static const scripting::CharacterSnapshot* captureAwareness(){
 if(!charactersPossible||!worldPulse)return nullptr;
 if(!awarenessCaptured){awarenessCaptured=true;awarenessCopy=scripting::readCharacters(base,copyWorldMemory,true);}
 return &awarenessCopy;
}
// Skills and roles: a third lazy copy, so addons that only count bodies do not pay for the role lookups.
static const scripting::CharacterSnapshot* captureDetails(){
 if(!charactersPossible||!worldPulse)return nullptr;
 if(!detailsCaptured){detailsCaptured=true;detailsCopy=scripting::readCharacters(base,copyWorldMemory,false,true);}
 return &detailsCopy;
}
