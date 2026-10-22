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
  const uintptr_t victim=*(const uintptr_t*)(rbp-0x68);if(!victim)return;
  if(site==0){combat::seenKinds.victim=victim;combat::seenKinds.cut=*(const float*)(rbp-0x78);combat::seenKinds.crush=*(const float*)(rbp-0x80);combat::seenKinds.fresh=true;}
  Entry s;if(!find(victim,s))return;
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
static std::string why;   // why install refused, for the log
static bool install(){
 if(installed)return true;if(refused)return false;
 static const uintptr_t sites[2]={0x15b0fc,0x15b2be};
 static const unsigned char expect[2][9]={{0xf3,0x0f,0x10,0x45,0x90,0xf3,0x0f,0x59,0x05},{0xf3,0x0f,0x10,0x45,0x84,0xf3,0x0f,0x59,0x05}};
 const unsigned char entry[]={0x55,0x48,0x89,0xe5};   // 0x159a60 still the game's own
 // The game's own entry, or the runtime's own combat-events detour there (combat_bridge.h checked these bytes).
 if(!combat::impactCreated&&!gamebridge::operation(base+0x159a60,entry,sizeof entry)){refused=true;why="the entry 0x159a60 is not the game's";return false;}
 for(int i=0;i<2;++i)if(!gamebridge::operation(base+sites[i],expect[i],sizeof expect[i])){refused=true;why=i?"the bytes at 0x15b2be differ":"the bytes at 0x15b0fc differ";return false;}
 auto* memory=(unsigned char*)VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!memory){refused=true;why="no stub memory";return false;}
 for(int i=0;i<2;++i){
  stubs[i]=memory+i*256;
  {const auto s=MH_CreateHook((void*)(base+sites[i]),stubs[i],&trampolines[i]);if(s!=MH_OK){refused=true;why=std::string("MH_CreateHook at a site returned ")+std::to_string(int(s));return false;}}
  buildStub((unsigned char*)stubs[i],i,trampolines[i]);
 }
 DWORD old=0;if(!VirtualProtect(memory,4096,PAGE_EXECUTE_READ,&old)){refused=true;why="stub protection failed";return false;}FlushInstructionCache(GetCurrentProcess(),memory,4096);
 for(int i=0;i<2;++i){const auto s=MH_EnableHook((void*)(base+sites[i]));if(s!=MH_OK&&s!=MH_ERROR_ENABLED){for(int k=0;k<2;++k)MH_DisableHook((void*)(base+sites[k]));refused=true;why=std::string("MH_EnableHook returned ")+std::to_string(int(s));return false;}}
 installed=true;return true;
}
// Each pulse: resolve the addons' targets against this pulse's roster and publish them.
static std::vector<scripting::DamageScale> wanted;static bool published=false;
static void refresh(scripting::Host& owner,bool session){
 owner.collectDamageScales(wanted);const scripting::CharacterSnapshot* roster=(session&&!wanted.empty())?captureCharacters():nullptr;
 // A reader of character_hits also gets cut/crush kinds, which these hooks see.
 if((!wanted.empty()||combat::hitsRead.load())&&!installed&&!refused&&!install())owner.report("runtime","warn",("characters.set_damage_scale unavailable: "+why).c_str());
 Entry next[64];LONG n=0;
 if(installed&&session&&roster&&roster->ready)for(const auto& w:wanted)for(const auto& c:roster->characters){
  if(n>=64)break;if(!c.nativeCharacter||(c.stateKnown&&c.dead))continue;
  if(!scripting::targets(w,c))continue;
  bool merged=false;for(LONG i=0;i<n;++i)if(next[i].character==c.nativeCharacter){next[i].cut*=w.cut;next[i].pierce*=w.pierce;next[i].crush*=w.crush;next[i].impact*=w.impact;merged=true;}
  if(!merged){next[n]={c.nativeCharacter,w.cut,w.pierce,w.crush,w.impact};++n;}
 }
 if(!n&&!published)return;
 InterlockedIncrement(&sequence);MemoryBarrier();std::memcpy(table,next,sizeof(Entry)*size_t(n));count=n;MemoryBarrier();InterlockedIncrement(&sequence);published=n>0;
}
}
// characters.set_hit_sound("player", material): while held, the player's wound sound (character +0xec0, normally
// WndFls) and every body part's impact sound (part +0x470; parts at character +0x928, count +0x920) are the game's
// material sound object 0x4f0940[material] (6 = HitGlm, the golem's stone sound). Equipment refresh ECA40 rewrites
// +0x470, so each pulse puts it back, remembering the game's latest value; release, a phase change or unload writes
// the remembered values back where ours is still in place (docs/reviews/hit-sound-review.md).
static bool hitSoundPossible=false;   // set at enable: the material sound table and the player global readable
namespace hitsound {
struct Held {uintptr_t character=0,sound=0,woundSaved=0;bool wound=false;int parts=0;std::array<uintptr_t,64> part{},partSaved{};};
static Held held;static bool active=false;
static bool writable(uintptr_t a){MEMORY_BASIC_INFORMATION m{};return readable(a,8)&&VirtualQuery((void*)a,&m,sizeof m)==sizeof m&&(m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));}
static bool put(uintptr_t a,uintptr_t v){__try{if(!writable(a))return false;*(uintptr_t*)a=v;return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static uintptr_t get(uintptr_t a){uintptr_t v=0;return copyWorldMemory(a,&v,8)?v:0;}
static void restore(){
 if(!active)return;active=false;
 // Only into the same live player: after a level change the old character may be gone.
 if(held.character&&get(base+0x4f0830)==held.character){
  if(held.wound&&get(held.character+0xec0)==held.sound)put(held.character+0xec0,held.woundSaved);
  for(int i=0;i<held.parts;++i)if(held.part[size_t(i)]&&get(held.part[size_t(i)]+0x470)==held.sound)put(held.part[size_t(i)]+0x470,held.partSaved[size_t(i)]);
 }
 held=Held{};
}
static void pulse(int material,bool session){
 if(material<0||!session){restore();return;}
 const uintptr_t player=get(base+0x4f0830);const uintptr_t sound=readable(base+0x4f0940+size_t(material)*8,8)?get(base+0x4f0940+size_t(material)*8):0;
 if(!player||!sound){restore();return;}
 if(active&&(held.character!=player||held.sound!=sound))restore();
 if(!active){held=Held{};held.character=player;held.sound=sound;active=true;}
 const uintptr_t wound=get(player+0xec0);
 if(wound&&wound!=sound){held.woundSaved=wound;held.wound=put(player+0xec0,sound)||held.wound;}
 int32_t count=0;if(!copyWorldMemory(player+0x920,&count,4)||count<0||count>64)return;
 for(int32_t i=0;i<count;++i){const uintptr_t part=get(player+0x928+size_t(i)*8);if(!part)continue;
  int slot=-1;for(int k=0;k<held.parts;++k)if(held.part[size_t(k)]==part)slot=k;
  if(slot<0){if(held.parts>=64)continue;slot=held.parts++;held.part[size_t(slot)]=part;held.partSaved[size_t(slot)]=0;}
  const uintptr_t current=get(part+0x470);
  if(current!=sound){held.partSaved[size_t(slot)]=current;put(part+0x470,sound);}
 }
}
}
// characters.set_limb_strength: a per-limb muscle factor. The joint solver 0x15da80(part, pass) weights each joint
// row by row+0x3c * part+0x594 * sqrt(stamina) * character+0x58c; +0x594 is rewritten by several native writers, so a
// lasting write would not hold. The detour multiplies +0x594 for the call only and puts the native value back
// afterwards if nothing else wrote it meanwhile (docs/reviews/limb-strength-review.md). The table is a seqlock copy
// built at the session pulse; the solver may run on the physics worker, so it takes no lock.
namespace limbs {
struct Entry {uintptr_t part=0;float factor=1;};
static Entry table[128];static volatile LONG count=0;static volatile LONG sequence=0;static volatile bool published=false;
using Solve=uint64_t(*)(uintptr_t,uint64_t);static Solve original=nullptr;static bool created=false,installed=false,refused=false;
static float factorOf(uintptr_t part){
 for(int attempt=0;attempt<4;++attempt){
  const LONG before=sequence;if(before&1){YieldProcessor();continue;}MemoryBarrier();
  const LONG seen=count;const LONG total=seen<128?seen:128;float f=1;
  for(LONG i=0;i<total;++i)if(table[i].part==part){f=table[i].factor;break;}
  MemoryBarrier();if(sequence==before)return f;
 }
 return 1;
}
static bool scale(uintptr_t part,float factor,float& before,float& written){
 __try{float* p=(float*)(part+0x594);before=*p;if(!std::isfinite(before))return false;written=before*factor;*p=written;return true;}
 __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static void unscale(uintptr_t part,float before,float written){
 __try{float* p=(float*)(part+0x594);if(std::memcmp(p,&written,4)==0)*p=before;}__except(EXCEPTION_EXECUTE_HANDLER){}
}
static uint64_t solve(uintptr_t part,uint64_t pass){
 const float factor=published?factorOf(part):1.f;
 float before=0,written=0;const bool scaled=factor<1.f&&scale(part,factor,before,written);
 const uint64_t result=original(part,pass);
 if(scaled)unscale(part,before,written);
 return result;
}
static bool install(){
 if(installed)return true;if(refused)return false;
 const unsigned char entry[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0xa4,0x24,0xa0,0xfe,0xff,0xff,0x48,0x89,0x9d,0xd0,0xfe,0xff,0xff};
 if(!created){if(!titlerows::untouched(base+0x15da80,entry,sizeof entry)||MH_CreateHook((void*)(base+0x15da80),(void*)solve,(void**)&original)!=MH_OK){refused=true;return false;}created=true;}
 const auto s=MH_EnableHook((void*)(base+0x15da80));installed=s==MH_OK||s==MH_ERROR_ENABLED;if(!installed)refused=true;return installed;
}
static std::vector<scripting::LimbStrength> wanted;
static void refresh(scripting::Host& owner,bool session){
 owner.collectLimbs(wanted);
 if(!wanted.empty()&&!installed&&!refused&&!install())owner.report("runtime","warn","characters.set_limb_strength unavailable: the joint solver did not match or is hooked by another mod");
 Entry next[128];LONG n=0;
 const scripting::CharacterSnapshot* roster=(session&&installed&&!wanted.empty())?captureCharacters():nullptr;
 if(roster&&roster->ready)for(const auto& w:wanted)for(const auto& c:roster->characters){
  if(!c.nativeCharacter||(c.stateKnown&&c.dead))continue;if(!scripting::targets(w,c))continue;
  int32_t parts=0;if(!copyWorldMemory(c.nativeCharacter+0x920,&parts,4)||parts<0||parts>64)continue;
  for(int32_t i=0;i<parts&&n<128;++i){uintptr_t part=0;int32_t index=-1;
   if(!copyWorldMemory(c.nativeCharacter+0x928+size_t(i)*8,&part,8)||!part||!copyWorldMemory(part+0x468,&index,4)||index+1!=w.part)continue;
   bool merged=false;for(LONG k=0;k<n;++k)if(next[k].part==part){next[k].factor=(std::min)(next[k].factor,w.factor);merged=true;}
   if(!merged){next[n].part=part;next[n].factor=w.factor;++n;}}
 }
 if(!n&&!published)return;
 InterlockedIncrement(&sequence);MemoryBarrier();std::memcpy(table,next,sizeof(Entry)*size_t(n));count=n;MemoryBarrier();InterlockedIncrement(&sequence);published=n>0;
}
}
// characters.set_strength / set_anim_rate (target, factor): a held multiple of one of a character's own floats,
// written at the session pulse on the game thread (docs/reviews/body-scales-review.md):
//   +0x998  motor strength (1.25 x body size, written only by setup 0x173230; read every tick to drive the arms)
//   +0xdcc  base animation rate (template +0x5c; knockdown 0x156b20 lowers it and ramps it back; 0x169200 derives the
//           per-tick +0xdc8 from it, which is never written here)
// Targets: the player, or every live character with a record id. Each pulse: a value that is not ours (the game wrote
// it) becomes the new base before ours is put back. When a target is released, dies, or the addon unloads, the base is
// written back where ours is still in place; a character that left the roster (level change) is forgotten unwritten.
namespace bodyscale {
struct Held {uintptr_t character=0;int field=0;float base=0,written=0,factor=1;};
static std::vector<Held> held;static constexpr uintptr_t offsets[2]={0x998,0xdcc};
static bool readFloat(uintptr_t a,float& v){return copyWorldMemory(a,&v,4)&&std::isfinite(v);}
static bool writeFloat(uintptr_t a,float v){__try{if(!hitsound::writable(a))return false;*(float*)a=v;return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static std::vector<scripting::BodyScale> wanted;
static void pulse(scripting::Host& owner,bool enabled,bool session){
 if(enabled)owner.collectBody(wanted);else wanted.clear();
 if(wanted.empty()&&held.empty())return;
 const scripting::CharacterSnapshot* roster=session?captureCharacters():nullptr;const bool ready=roster&&roster->ready;
 struct Want {uintptr_t character;int field;float factor;};std::vector<Want> desired;
 if(ready)for(const auto& w:wanted)for(const auto& c:roster->characters){
  if(!c.nativeCharacter||(c.stateKnown&&c.dead))continue;if(!scripting::targets(w,c))continue;
  bool merged=false;for(auto& d:desired)if(d.character==c.nativeCharacter&&d.field==w.field){d.factor*=w.factor;merged=true;}
  if(!merged)desired.push_back({c.nativeCharacter,w.field,w.factor});
 }
 // Released, dead or gone: put the base back where ours is still there (only for a character still in the roster).
 for(auto it=held.begin();it!=held.end();){
  bool still=false;for(const auto& d:desired)still=still||(d.character==it->character&&d.field==it->field);
  if(still){++it;continue;}
  bool present=false;if(ready)for(const auto& c:roster->characters)present=present||c.nativeCharacter==it->character;
  float now=0;if(present&&readFloat(it->character+offsets[it->field],now)&&std::memcmp(&now,&it->written,4)==0)writeFloat(it->character+offsets[it->field],it->base);
  it=held.erase(it);
 }
 for(const auto& d:desired){
  float now=0;if(!readFloat(d.character+offsets[d.field],now))continue;
  Held* h=nullptr;for(auto& x:held)if(x.character==d.character&&x.field==d.field)h=&x;
  if(!h){if(held.size()>=128)continue;held.push_back({d.character,d.field,now,now,1});h=&held.back();}
  else if(std::memcmp(&now,&h->written,4)!=0){
   // The game wrote it. A knockdown's recovery adds 0.01 a tick to the animation rate (0x156b20): a small rise is that
   // ramp and moves the base by the same step; anything else is the game's new base.
   if(d.field==1&&now>h->written&&now-h->written<0.3f)h->base+=now-h->written;else h->base=now;}
  h->factor=d.factor;const float want=h->base*d.factor;
  if(std::memcmp(&now,&want,4)==0){h->written=want;continue;}
  if(writeFloat(d.character+offsets[d.field],want))h->written=want;
 }
}
}
