#pragma once
// Check only the operations this adapter uses. No game version or file hash gate.
// Entry prefixes identify known calling/layout contracts; supported detours may chain.
namespace gamebridge {
inline bool executable(uintptr_t p){
 MEMORY_BASIC_INFORMATION m{};
 return p>=0x10000&&VirtualQuery((void*)p,&m,sizeof m)==sizeof m&&m.State==MEM_COMMIT&&
  !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&
  (m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
inline uintptr_t jumpTarget(uintptr_t p){
 if(!readable(p,16))return 0;
 const auto b=(const unsigned char*)p;
 if(b[0]==0xe9){int32_t d;std::memcpy(&d,b+1,4);return p+5+d;}
 if(b[0]==0xeb)return p+2+int8_t(b[1]);
 if(b[0]==0xff&&b[1]==0x25){
  int32_t d;std::memcpy(&d,b+2,4);const auto slot=p+6+d;
  if(!readable(slot,8))return 0;
  uintptr_t target;std::memcpy(&target,(void*)slot,8);return target;
 }
 if(b[0]==0x48&&b[1]==0xb8&&b[10]==0xff&&b[11]==0xe0){
  uintptr_t target;std::memcpy(&target,b+2,8);return target;
 }
 return 0;
}
inline bool operation(uintptr_t p,const unsigned char* prefix,size_t size){
 if(!executable(p)||!readable(p,size))return false;
 if(!std::memcmp((void*)p,prefix,size))return true;
 // Do not require untouched entry bytes when another mod owns an entry detour.
 // A compatible detour must retain the original function's calling contract.
 uintptr_t seen[8]{};unsigned count=0;
 while(count<8){
  seen[count++]=p;
  const auto target=jumpTarget(p);
  if(!target||!executable(target))return false;
  for(unsigned i=0;i<count;++i)if(seen[i]==target)return false;
  if(!jumpTarget(target))return true;
  p=target;
 }
 return false;
}
inline bool runtimeAvailable(uintptr_t image){
 if(!readable(image,sizeof(IMAGE_DOS_HEADER)))return false;
 const auto dos=(const IMAGE_DOS_HEADER*)image;
 if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>0x100000)return false;
 const auto at=image+dos->e_lfanew;
 if(!readable(at,sizeof(IMAGE_NT_HEADERS64)))return false;
 const auto nt=(const IMAGE_NT_HEADERS64*)at;
 if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return false;
 struct Operation{unsigned rva;unsigned char prefix[12];};
 constexpr Operation needed[]={
  {0x11c9a0,{0x55,0x48,0x89,0xe5,0x48,0x8d,0xa4,0x24,0x70,0xff,0xff,0xff}},
  {0x11cb70,{0x53,0x57,0x56,0x41,0x54,0x48,0x8d,0x64,0x24,0x98,0x48,0x89}},
  {0x1041b0,{0x53,0x56,0x48,0x8d,0x64,0x24,0xc8,0x89,0xd3,0x48,0x8d,0x54}},
  {0x10e7c0,{0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x48,0x8d,0x64,0x24,0xe0}},
  {0x2f540,{0x48,0x8d,0x64,0x24,0xc8,0x48,0x89,0xc8,0x48,0xc7,0x44,0x24}},
  {0x8f720,{0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48}},
  {0x4fcb0,{0x48,0x89,0xc8,0xeb,0x0a,0x0f,0x1f,0x00,0x48,0x8b,0x80,0x28}},
  {0x118f80,{0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xc0,0x48,0x89,0xcb,0x31}},
  {0x14cb60,{0x53,0x48,0x8d,0x64,0x24,0xd0,0x48,0x89,0xcb,0xc7,0x44,0x24}},
  {0x2cd90,{0x44,0x8b,0x49,0x20,0x44,0x29,0x0a,0x44,0x8b,0x49,0x24,0x45}}
 };
 for(const auto& op:needed){
  if(op.rva+sizeof op.prefix>nt->OptionalHeader.SizeOfImage||!operation(image+op.rva,op.prefix,sizeof op.prefix))return false;
 }
 for(unsigned rva:{0x499790u,0x49a960u,0x4f0830u})
  if(rva+8>nt->OptionalHeader.SizeOfImage||!readable(image+rva,8))return false;
 return true;
}
inline bool lifecycleAvailable(uintptr_t image){
 if(!readable(image,sizeof(IMAGE_DOS_HEADER)))return false;
 const auto dos=(const IMAGE_DOS_HEADER*)image;
 if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>0x100000)return false;
 const auto at=image+dos->e_lfanew;if(!readable(at,sizeof(IMAGE_NT_HEADERS64)))return false;
 const auto nt=(const IMAGE_NT_HEADERS64*)at;
 if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return false;
 // Native menu draw contract; no menu strings, rows or controls are changed.
 const unsigned char prefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0xa4,0x24,0x50,0xff,0xff,0xff};
 if(0x128f30+sizeof prefix>nt->OptionalHeader.SizeOfImage||!operation(image+0x128f30,prefix,sizeof prefix))return false;
 // Void native presentation wrapper; leaves OpenGL hook ownership to render mods.
 const unsigned char presentPrefix[]={0x48,0x8d,0x64,0x24,0xd8,0x48,0x83,0x3d,0x93,0x6a,0x47,0x00,0x00};
 if(0x21e60+sizeof presentPrefix>nt->OptionalHeader.SizeOfImage||!operation(image+0x21e60,presentPrefix,sizeof presentPrefix))return false;
 for(unsigned slot:{0x499610u,0x4f0170u,0x4f0830u})if(slot+8>nt->OptionalHeader.SizeOfImage||!readable(image+slot,8))return false;
 return true;
}

}

