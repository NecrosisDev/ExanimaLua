#pragma once
// characters.set_damage_scale: per-kind multipliers on the damage a character takes, applied inside the game's own
// damage computation 0x159a60 where the kinds are still separate (before armour). Static reading of Exanima.exe
// 0.9.5.2g (docs/reviews/damage-scale-review.md):
//   victim [rbp-0x68]; impact energy [rbp-0x70], cut [rbp-0x78], crush [rbp-0x80], point/pierce [rbp-0x7c].
//   0x15b0fc  movss xmm0,[rbp-0x70]  the join after the armour layer loop (both paths), before the reduction
//   0x15b2be  movss xmm0,[rbp-0x7c]  the thrust branch's join, after it set point from the weapon
// Each site gets a small stub that saves the volatile registers and flags, calls `onKinds(rbp, site)`, restores them
// and continues in MinHook's trampoline. The multipliers are a published copy (resolved at the pulse from record ids
// or the player), so the damage path takes no lock and reads only its own frame and that copy.
namespace damagescale {
struct Entry {uintptr_t character=0;float cut=1,pierce=1,crush=1,impact=1;};
static Entry table[64];static volatile LONG count=0;static volatile LONG sequence=0;
static bool installed=false,refused=false;static void* stubs[2]{};static void* trampolines[2]{};
static std::atomic<uint64_t> scaled{0},faults{0};
static bool find(uintptr_t character,Entry& out){
 for(int attempt=0;attempt<4;++attempt){
  const LONG before=sequence;if(before&1){YieldProcessor();continue;}MemoryBarrier();
  const LONG seen=count;const LONG total=seen<64?seen:64;bool hit=false;
  for(LONG i=0;i<total;++i)if(table[i].character==character){out=table[i];hit=true;break;}
  MemoryBarrier();if(sequence==before)return hit;
 }
 return false;
}
static void __cdecl onKinds(uintptr_t rbp,int site){
 __try{
  const uintptr_t victim=*(const uintptr_t*)(rbp-0x68);if(!victim)return;Entry s;if(!find(victim,s))return;
  if(site==0){*(float*)(rbp-0x70)*=s.impact;*(float*)(rbp-0x78)*=s.cut;*(float*)(rbp-0x80)*=s.crush;*(float*)(rbp-0x7c)*=s.pierce;}
  else *(float*)(rbp-0x7c)*=s.pierce;
  ++scaled;
 }__except(EXCEPTION_EXECUTE_HANDLER){++faults;}
}
// pushfq; push rax rcx rdx r8 r9 r10 r11; mov rax,rsp; and rsp,-16; sub rsp,0x90; mov [rsp+0x80],rax;
// movdqu [rsp+0x20..0x70],xmm0..5; mov rcx,rbp; mov edx,site; mov rax,onKinds; call rax; movdqu xmm0..5 back;
// mov rsp,[rsp+0x80]; pop r11 r10 r9 r8 rdx rcx rax; popfq; jmp [rip] -> trampoline.
static size_t buildStub(unsigned char* p,int site,void* trampoline){
 size_t n=0;auto put=[&](std::initializer_list<unsigned char> bytes){for(auto b:bytes)p[n++]=b;};
 auto put64=[&](uint64_t v){std::memcpy(p+n,&v,8);n+=8;};
 put({0x9c,0x50,0x51,0x52,0x41,0x50,0x41,0x51,0x41,0x52,0x41,0x53});
 put({0x48,0x89,0xe0,0x48,0x83,0xe4,0xf0,0x48,0x81,0xec,0x90,0x00,0x00,0x00,0x48,0x89,0x84,0x24,0x80,0x00,0x00,0x00});
 const unsigned char reg[6]={0x44,0x4c,0x54,0x5c,0x64,0x6c};
 for(int i=0;i<6;++i)put({0xf3,0x0f,0x7f,reg[i],0x24,(unsigned char)(0x20+0x10*i)});
 put({0x48,0x89,0xe9,0xba});const uint32_t s=uint32_t(site);std::memcpy(p+n,&s,4);n+=4;
 put({0x48,0xb8});put64(uint64_t(&onKinds));put({0xff,0xd0});
 for(int i=0;i<6;++i)put({0xf3,0x0f,0x6f,reg[i],0x24,(unsigned char)(0x20+0x10*i)});
 put({0x48,0x8b,0xa4,0x24,0x80,0x00,0x00,0x00});
 put({0x41,0x5b,0x41,0x5a,0x41,0x59,0x41,0x58,0x5a,0x59,0x58,0x9d});
 put({0xff,0x25,0x00,0x00,0x00,0x00});put64(uint64_t(trampoline));
 return n;
}
static bool install(){
 if(installed)return true;if(refused)return false;
 static const uintptr_t sites[2]={0x15b0fc,0x15b2be};
 static const unsigned char expect[2][9]={{0xf3,0x0f,0x10,0x45,0x90,0xf3,0x0f,0x59,0x05},{0xf3,0x0f,0x10,0x45,0x84,0xf3,0x0f,0x59,0x05}};
 const unsigned char entry[]={0x55,0x48,0x89,0xe5};   // 0x159a60 still the game's own
 if(!gamebridge::operation(base+0x159a60,entry,sizeof entry)){refused=true;return false;}
 for(int i=0;i<2;++i)if(!gamebridge::operation(base+sites[i],expect[i],sizeof expect[i])){refused=true;return false;}
 auto* memory=(unsigned char*)VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!memory){refused=true;return false;}
 for(int i=0;i<2;++i){
  stubs[i]=memory+i*256;
  if(MH_CreateHook((void*)(base+sites[i]),stubs[i],&trampolines[i])!=MH_OK){refused=true;return false;}
  buildStub((unsigned char*)stubs[i],i,trampolines[i]);
 }
 DWORD old=0;if(!VirtualProtect(memory,4096,PAGE_EXECUTE_READ,&old)){refused=true;return false;}FlushInstructionCache(GetCurrentProcess(),memory,4096);
 for(int i=0;i<2;++i){const auto s=MH_EnableHook((void*)(base+sites[i]));if(s!=MH_OK&&s!=MH_ERROR_ENABLED){for(int k=0;k<2;++k)MH_DisableHook((void*)(base+sites[k]));refused=true;return false;}}
 installed=true;return true;
}
// Each pulse: resolve the addons' targets against this pulse's roster and publish them.
static std::vector<scripting::DamageScale> wanted;static bool published=false;
static void refresh(scripting::Host& owner,bool session){
 owner.collectDamageScales(wanted);const scripting::CharacterSnapshot* roster=(session&&!wanted.empty())?captureCharacters():nullptr;
 if(!wanted.empty()&&!installed&&!refused&&!install())owner.report("runtime","warn","characters.set_damage_scale unavailable: the damage computation did not match or could not be hooked");
 Entry next[64];LONG n=0;
 if(installed&&session&&roster&&roster->ready)for(const auto& w:wanted)for(const auto& c:roster->characters){
  if(n>=64)break;if(!c.nativeCharacter||(c.stateKnown&&c.dead))continue;
  if(w.player?!(c.playerKnown&&c.player):(c.record!=w.record))continue;
  bool merged=false;for(LONG i=0;i<n;++i)if(next[i].character==c.nativeCharacter){next[i].cut*=w.cut;next[i].pierce*=w.pierce;next[i].crush*=w.crush;next[i].impact*=w.impact;merged=true;}
  if(!merged){next[n]={c.nativeCharacter,w.cut,w.pierce,w.crush,w.impact};++n;}
 }
 if(!n&&!published)return;
 InterlockedIncrement(&sequence);MemoryBarrier();std::memcpy(table,next,sizeof(Entry)*size_t(n));count=n;MemoryBarrier();InterlockedIncrement(&sequence);published=n>0;
}
}
