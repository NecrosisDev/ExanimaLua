#pragma once
// Light-school casts, addon point lights and focus hold-back. Static reading of Exanima.exe 0.9.5.2g; nothing here
// has run in the game.
//
// power.cast: the cast dispatcher 0x1d8340(slot) reads the caster's prepared slot (stats +0x198, ten u32 power ids;
// slot -1 = the selected one, bit 0x100000) and switches on the domain byte; domain 5 (Light) has a spell-book page
// (pwr_light.pwr) but no case, so it plays ThaumFail01. The detour answers only a Light-school slot while an enabled
// addon listens to power.cast: it queues the id and returns 1 (cast taken). Everything else, including every
// shipped power, goes to the original unchanged. The detour reads memory only.
//
// lights: one point light per addon request, built the way the game builds its own player light (0x120750: a
// TLightSource from 0x51880, kind 3, flags 0x1000/0x140100, published with 0x5a420 into the world the player
// control uses, applied with 0x51eb0) and moved the way 0x121430 moves it (head body +0x18 node, +0x20 position,
// ceiling probe 0x121150 straight up, pose propagation 0x50eb0). Removal uses 0x5bed0(world, node, 1) like 0x121650.
// A light belongs to one level (see Live below): when the level goes, the light is forgotten, never touched, and
// created again in the next level while the addon still asks for it.
//
// focus: the game holds focus back for continuous powers by adding to +0xd84, which 0x17a6b0 moves off the ceiling
// +0xd78 a little each tick; ending a power subtracts the same amount (0x17ab40), which gives the ceiling back.
// The hold here does the same for the controlled character. A level change or load starts from zero because
// character setup sets the ceiling again (0x173230); possession copies ceiling and reserve between bodies and keeps
// the hold. Holding never projects the ceiling below 0.05 and releasing never projects it above the full 1.0
// (0x31be70). If the runtime is disabled while something is held, the hold and the lights stay until the level ends.
//
// Native writes run only inside a job on the game's own work queue 0x1a890 (the queue the runtime's emitter and
// fire operations use), one job at a time from one pinned payload.
namespace powers {
using Cast=uint64_t(*)(uint32_t);
using Create=uintptr_t(*)(uintptr_t,int);using Apply=void(*)(uintptr_t);using Publish=void(*)(uintptr_t,uintptr_t*,int);
using Remove=void(*)(uintptr_t,uintptr_t,int);using Probe=char(*)(float*,const float*,float,float,uintptr_t,float*);
using Queue=void(*)(uintptr_t,char(*)(void*),void*);
static Cast original=nullptr;static bool created=false,installed=false;
static Create create=nullptr;static Apply apply=nullptr,pose=nullptr;static Publish publish=nullptr;static Remove remove=nullptr;static Probe probe=nullptr;
static Queue enqueue=nullptr;
static bool castPossible=false,lightsPossible=false,focusPossible=false;
static std::atomic<bool> listening{false},pending{false};
static std::atomic<unsigned> faults{0},refused{0};
static SRWLOCK queueLock=SRWLOCK_INIT;static std::array<uint32_t,16> casts{};static size_t castCount=0,castLost=0;
constexpr uintptr_t lightClass=0x2fcf40;constexpr size_t lightSize=0x2e8;constexpr size_t maxLights=6;
constexpr float fullCeiling=1.0f,lowestCeiling=0.05f;

// --- power.cast -------------------------------------------------------------------------------------------------
// Mirrors 0x1d8340's own slot choice: 0xdf8 bit 0x200000 (possession ending) and the blocked flag 0x1228 bit 0x10
// stay with the original, bit 0x400000 casts as the possessed body (0x4f0840).
static bool lightSlot(uint32_t slot,uint32_t& power,uintptr_t& who,uint32_t& raw){
 uintptr_t character=0,caster=0,stats=0;uint32_t flags=0,blocked=0;std::array<uint32_t,10> slots{};
 if(!copyWorldMemory(base+0x4f0830,&character,8)||!character||!copyWorldMemory(character+0xdf8,&flags,4)||(flags&0x200000))return false;
 caster=character;
 if(flags&0x400000){uintptr_t possessed=0;if(copyWorldMemory(base+0x4f0840,&possessed,8)&&possessed)caster=possessed;}
 if(!copyWorldMemory(caster+0x10a0,&stats,8)||!stats||!copyWorldMemory(stats+0x198,slots.data(),sizeof slots))return false;
 int index=int(slot);
 if(slot==0xffffffffu){index=-1;for(int i=0;i<10;++i)if(slots[size_t(i)]&0x100000){index=i;break;}}
 if(index<0||index>9)return false;
 const uint32_t id=slots[size_t(index)];
 if(!(id&0xff)||(id&0xff00)!=0x500||!copyWorldMemory(caster+0x1228,&blocked,4)||(blocked&0x10))return false;
 power=id&0x3ffff;who=character;raw=id;return true;
}
// A taken Light cast also records itself the way 0x1d8340 does after a cast, so Iteration (+0xc40 & 0x800000) sees
// it across schools: controlled +0xdb0 = id & 0xe0fffff for an instant power (mode bits 16..17 clear), else 0.
static uint64_t cast(uint32_t slot){
 if(listening.load(std::memory_order_relaxed)){
  uint32_t power=0,raw=0;uintptr_t character=0;
  if(lightSlot(slot,power,character,raw)){
   AcquireSRWLockExclusive(&queueLock);if(castCount<casts.size())casts[castCount++]=power;else ++castLost;ReleaseSRWLockExclusive(&queueLock);
   if(readable(character+0xdb0,4))*(uint32_t*)(character+0xdb0)=(raw&0x30000)?0u:(raw&0xe0fffffu);
   return 1;
  }
 }
 return original(slot);
}
// Continuity bindings 0x1d3a80(character): the controlled character's count, less what addons hold (floored at 0).
// Leaf function; the first ten bytes are two instructions with no RIP-relative operand. Calls the original first.
using Bindings=int(*)(uintptr_t);static Bindings originalBindings=nullptr;static bool bindingsCreated=false,bindingsInstalled=false;
static std::atomic<int> heldBindings{0};
static int bindings(uintptr_t character){
 const int total=originalBindings(character);const int held=heldBindings.load(std::memory_order_relaxed);
 if(held<=0)return total;uintptr_t player=0;if(!copyWorldMemory(base+0x4f0830,&player,8)||player!=character)return total;
 return (std::max)(0,total-held);
}
static const unsigned char bindingsPrefix[]={0x45,0x31,0xc9,0x48,0x8b,0x81,0xa0,0x10,0x00,0x00,0x4c,0x8d,0x80,0xe8,0x00,0x00};
static bool installBindings(){
 if(bindingsInstalled)return true;if(!castPossible)return false;
 if(!bindingsCreated){if(!gamebridge::operation(base+0x1d3a80,bindingsPrefix,sizeof bindingsPrefix)||MH_CreateHook((void*)(base+0x1d3a80),(void*)bindings,(void**)&originalBindings)!=MH_OK)return false;bindingsCreated=true;}
 const auto status=MH_EnableHook((void*)(base+0x1d3a80));bindingsInstalled=status==MH_OK||status==MH_ERROR_ENABLED;return bindingsInstalled;
}
// The first 12 bytes are five instructions with no RIP-relative operand; the 5-byte detour stays inside them.
static const unsigned char castPrefix[]={0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x48,0x8d,0x64,0x24,0xd0,0x89,0xcb,0x31,0xf6};
static bool install(){
 if(installed)return true;if(!castPossible)return false;
 if(!created){if(!gamebridge::operation(base+0x1d8340,castPrefix,sizeof castPrefix)||MH_CreateHook((void*)(base+0x1d8340),(void*)cast,(void**)&original)!=MH_OK)return false;created=true;}
 const auto status=MH_EnableHook((void*)(base+0x1d8340));installed=status==MH_OK||status==MH_ERROR_ENABLED;return installed;
}
static void deliver(scripting::Host& owner){
 listening=installed&&owner.handlesCasts();
 // The bindings detour is made the first time an addon holds one, and only on the game's own entry bytes.
 const int held=owner.desiredBindings();
 if(held>0&&!bindingsInstalled&&!installBindings()){static bool told=false;if(!told){told=true;owner.report("runtime","warn","powers.hold_bindings unavailable: the bindings function did not match or could not be hooked");}}
 heldBindings=bindingsInstalled?held:0;
 std::vector<scripting::PowerCast> found;size_t missed=0;
 AcquireSRWLockExclusive(&queueLock);for(size_t i=0;i<castCount;++i)found.push_back({casts[i]});missed=castLost;castCount=castLost=0;ReleaseSRWLockExclusive(&queueLock);
 if(!listening)return;
 if(missed)owner.report("runtime","warn","Power casts dropped: more than 16 in one pulse");
 if(!found.empty())owner.notifyCasts(found);
}
static void clear(){AcquireSRWLockExclusive(&queueLock);castCount=castLost=0;ReleaseSRWLockExclusive(&queueLock);}
// powers.caster(): copies of what the game's cost rules read on the controlled character. Bindings mirror
// 0x1d3a80: Concentration rating stats+0xe8 / 5000 techniques, technique bytes from stats+0xea scanned from the
// last; the first of 2..4 (Continuity I..III) gives that value, minus one unless +0x1228 bit 0x100 (an extra
// binding from equipment). Used bindings count running powers (+0xda8, +0xda4) whose id has the continuous bit
// 0x20000, as the Force use functions do. Technique flags at +0xc40: 0x800000 Iteration, 0x100000 Sustain,
// 0x400000 Abstraction, 0x200000 Clarity. Reads only.
static bool readCaster(scripting::CasterState& c){
 if(!castPossible)return false;
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t player=0,stats=0,list=0;uint32_t flags=0,extra=0;uint16_t rating=0;int count=0;
 if(!m.get(base+0x4f0830,player)||!player||!m.get(player+0xd74,c.focus)||!m.get(player+0xd78,c.ceiling)||!m.get(player+0xd84,c.reserve)||
  !m.get(player+0xc40,flags)||!m.get(player+0x1228,extra)||!m.get(player+0x10a0,stats)||!stats||!m.get(stats+0xe8,rating)||
  !m.get(player+0xda4,count)||count<0||count>64||(count&&(!m.get(player+0xda8,list)||!list)))return false;
 c.bindings=0;
 for(int i=int(rating/5000)-1;i>=0;--i){uint8_t technique=0;if(!m.get(stats+0xea+uintptr_t(i),technique))return false;
  if(technique>1&&technique<5){c.bindings=(extra&0x100)?technique:technique-1;break;}}
 c.bindingsUsed=0;
 for(int i=0;i<count;++i){uintptr_t power=0;uint32_t id=0;if(!m.get(list+uintptr_t(i)*8,power)||!power||!m.get(power+0x10,id))return false;if(id&0x20000)++c.bindingsUsed;}
 uint32_t last=0;if(m.get(player+0xdb0,last))c.lastPower=last&0x3ffff;
 c.iteration=(flags&0x800000)!=0;c.sustain=(flags&0x100000)!=0;c.abstraction=(flags&0x400000)!=0;c.clarity=(flags&0x200000)!=0;
 return std::isfinite(c.focus)&&std::isfinite(c.ceiling)&&std::isfinite(c.reserve);
}

// --- lights and focus -------------------------------------------------------------------------------------------
struct Want {char key[160]{};scripting::LightSpec spec;};
// The world object at 0x4f0bd0 is created once and outlives every level (its only writer, 0x181250, runs while it
// is null), so it cannot tell levels apart. A light belongs to one "generation": the pulse starts a new one each time
// the runtime leaves its session phase (title menu or loading, when the level is torn down), and the light also
// remembers the level name (world +0x140) and the owner 0x5a420 gave it (node +0x130, set by 0x55960). Any mismatch
// means the level that held the node may be gone: the node is forgotten, never read further or written.
struct Live {char key[160]{};char level[16]{};uintptr_t node=0,owner=0,glow=0;uint32_t generation=0;std::array<float,3> at{};float clock=0;scripting::LightSpec spec;};
// Glow (lights.set{glow}): a particle emitter built the way the exit portal's is (CFEA0):
// CFD20(scene, spec, "_PrtlPrt01"), name +0x140, attach
// to the light node with 50170 so it follows it, publish with 5A420; removed with 50A60(node, 1). The material is
// borrowed (white, never recoloured with 6F870); colour comes from the spec's own ramp (+0x44 bit 0x200, +0x60, read
// live by pointer). The spec is the portal's 0xA0 bytes (2B3FD0) with size, spread, life, count and budget made
// small, and +0x80 (sound) 0. Spec and ramp live in this slot's static storage, which outlives the emitter; it is not
// saved with the level (no +0x158 tag), so it is made again after a load like the light.
struct GlowSpec {alignas(16) uint8_t spec[0xa0]{};alignas(16) uint8_t ramp[4+9*4]{};};
static std::array<GlowSpec,maxLights> glowSpecs{};
using Emitter=uintptr_t(*)(uintptr_t,const void*,const char*);using Attach=void(*)(uintptr_t,uintptr_t);using Unlink=void(*)(uintptr_t,int);
static Emitter emitter=nullptr;static Attach attach=nullptr;static Unlink unlink=nullptr;static bool glowPossible=false;
struct Material {int64_t references=-1,length=10;char text[16]="_PrtlPrt01";};   // an FPC constant string
static const Material glowMaterial{};
static void glowName(char out[16],size_t slot){std::memset(out,0,16);std::memcpy(out,"+LuaGlow",8);out[8]=char('0'+slot);}
static void fillRamp(GlowSpec& g,const std::array<float,3>& c){
 auto byte=[](float v){return uint8_t(std::lround(std::clamp(v,0.f,1.f)*255.f));};
 g.ramp[0]=6;uint8_t* keys=g.ramp+4;std::memset(keys,0,9*4);
 keys[4]=byte(c[0]*0.5f);keys[5]=byte(c[1]*0.5f);keys[6]=byte(c[2]*0.5f);keys[7]=0xff;
 for(int k=2;k<9;++k){keys[k*4]=byte(c[0]);keys[k*4+1]=byte(c[1]);keys[k*4+2]=byte(c[2]);keys[k*4+3]=0xff;}
}
static bool fillSpec(GlowSpec& g,const scripting::LightSpec& s){
 if(!copyWorldMemory(base+0x2b3fd0,g.spec,sizeof g.spec))return false;
 const int32_t count=30,budget=48;const float life=0.45f,spread=4.0f;
 std::memcpy(g.spec+0x0c,&count,4);std::memcpy(g.spec+0x10,&life,4);std::memcpy(g.spec+0x14,&s.glowSize,4);std::memcpy(g.spec+0x18,&spread,4);
 std::memcpy(g.spec+0x40,&budget,4);const void* ramp=g.ramp;std::memcpy(g.spec+0x60,&ramp,8);std::memset(g.spec+0x80,0,8);
 fillRamp(g,s.glowColor);return true;
}
static bool ownGlow(uintptr_t node,size_t slot){char name[16]{},expected[16]{};uint32_t flags=0;glowName(expected,slot);
 return node&&copyWorldMemory(node+0x140,name,16)&&!std::memcmp(name,expected,16)&&copyWorldMemory(node+8,&flags,4)&&!(flags&0x80000000u);}
static void dropGlow(Live& l,size_t slot){if(l.glow&&ownGlow(l.glow,slot))unlink(l.glow,1);l.glow=0;}
static void makeGlow(Live& l,size_t slot,uintptr_t world){
 auto& g=glowSpecs[slot];if(!fillSpec(g,l.spec))return;
 const uintptr_t node=emitter(world,g.spec,glowMaterial.text);if(!node||!readable(node,0x170)){++faults;return;}
 glowName((char*)(node+0x140),slot);attach(l.node,node);uintptr_t published=node;publish(world,&published,0);l.glow=node;
}
struct Job {
 // Written by the pulse only while no job is pending; read by the job.
 std::array<Want,maxLights> wants{};size_t wanted=0;float focus=0;bool session=false;uint32_t generation=1;
 // Owned by the job; the pulse reads them only while no job is pending.
 std::array<Live,maxLights> live{};float focusApplied=0;uint32_t focusGeneration=1;uint64_t last=0;bool broken=false;
};
static Job job;
static bool finite3(const std::array<float,3>& v){return std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);}
// Pure helpers, exercised by test_powers.cpp.
// The point the light floats toward: the head plus offsets along the body's facing (x/z of +0xa08), its right and
// world up, with a slow sway. right = (forward.z, -forward.x) is inferred from the y-up layout (PROVISIONAL(2026-10-21,
// handedness of +0xa08 not observed in game, ratify-by: 2026-11-04)).
static std::array<float,3> target(const std::array<float,3>& head,const std::array<float,3>& facing,const scripting::LightSpec& s,float seconds){
 float fx=facing[0],fz=facing[2];const float n=std::sqrt(fx*fx+fz*fz);
 if(!(n>0.001f)||!std::isfinite(n)){fx=0;fz=1;}else{fx/=n;fz/=n;}
 const float rx=fz,rz=-fx;const float sway=s.bob>0?s.bob*std::sin(seconds*2.0943951f):0.0f;
 return {head[0]+fx*s.forward+rx*s.right,head[1]+s.up+sway,head[2]+fz*s.forward+rz*s.right};
}
// Exponential approach: after `smoothing` seconds about 63% of a gap is closed; 0 snaps.
static std::array<float,3> ease(const std::array<float,3>& from,const std::array<float,3>& to,float smoothing,float dt){
 if(!(smoothing>0)||!(dt>0))return to;const float k=1.0f-std::exp(-dt/smoothing);
 return {from[0]+(to[0]-from[0])*k,from[1]+(to[1]-from[1])*k,from[2]+(to[2]-from[2])*k};
}
// How much more to hold (positive) or give back (negative) so that `applied` approaches `desired`, keeping the
// projected ceiling (ceiling minus what is still moving off it) inside [lowestCeiling, fullCeiling].
static float focusStep(float desired,float applied,float ceiling,float reserve){
 if(!std::isfinite(desired)||!std::isfinite(applied)||!std::isfinite(ceiling)||!std::isfinite(reserve))return 0;
 const float projected=ceiling-reserve;float delta=desired-applied;
 if(delta>0)delta=(std::min)(delta,(std::max)(0.0f,projected-lowestCeiling));
 else if(delta<0)delta=(std::max)(delta,(std::min)(0.0f,projected-fullCeiling));
 return std::fabs(delta)<0.00001f?0.0f:delta;
}
static bool sameKey(const char* a,const char* b){return !std::strncmp(a,b,160);}
static void nodeName(char out[16],size_t slot){std::memset(out,0,16);std::memcpy(out,"+LuaLight",9);out[9]=char('0'+slot);}
// Our node, untouched by anyone else: the light class, our name, the owner it was published under, not queued for
// deletion (bit 31). Only asked about nodes of the current generation and level (see Live).
static bool ours(uintptr_t node,size_t slot,uintptr_t owner){
 char name[16]{},expected[16]{};uint32_t flags=0;uintptr_t current=0;nodeName(expected,slot);
 return node&&readable(node,lightSize)&&scripting::classMatches(node,base+lightClass,copyWorldMemory)&&copyWorldMemory(node+0x140,name,16)&&!std::memcmp(name,expected,16)&&
  copyWorldMemory(node+8,&flags,4)&&!(flags&0x80000000u)&&copyWorldMemory(node+0x130,&current,8)&&current&&current==owner;
}
// After 0x51eb0 the game's own light also marks +0x268 bit 8 (0x121430).
static void refresh(uintptr_t node){apply(node);*(uint32_t*)(node+0x268)|=8;}
static void style(uintptr_t node,const scripting::LightSpec& s){
 for(size_t i=0;i<3;++i)*(float*)(node+0x210+4*i)=s.color[i];
 *(float*)(node+0x224)=s.range;*(float*)(node+0x21c)=s.intensity;
}
// The game's own light takes the head node's world position (+0x20) into +0x94 unchanged (0x121430); so does this.
static void place(uintptr_t node,const std::array<float,3>& at){
 for(size_t i=0;i<3;++i)*(float*)(node+0x94+4*i)=at[i];
 pose(node);
}
static bool current(const Live& l,uint32_t generation,const char level[16]){return l.generation==generation&&!std::memcmp(l.level,level,16);}
// Removes a light we own. Returns false (and keeps it, to retry next pulse) only while the removal ring is full.
static bool drop(Live& l,uintptr_t world,size_t slot,uint32_t generation,const char level[16]){
 if(!l.node)return true;
 if(!current(l,generation,level)){l=Live{};return true;}            // its level is gone; never touch it
 if(!ours(l.node,slot,l.owner)){++faults;l=Live{};return true;}       // someone else changed it; leave it alone
 uint32_t queueSlot=0;if(!worldemitters::removalReady(world,queueSlot)){++refused;return false;}
 if(glowPossible)dropGlow(l,slot);
 remove(world,l.node,1);l=Live{};return true;
}
static void run(){
 const scripting::CharacterMemory m{copyWorldMemory};
 uintptr_t world=0,control=0,controlWorld=0,player=0;int state=0,loading=1;
 if(!m.get(base+0x4f0bd0,world)||!world||!m.get(base+0x4eff70,state)||!m.get(base+0x499860,loading))return;
 char level[16]{};if(!m.get(world+0x140,level))return;
 // Lights from an earlier generation or another level belong to a level that may be gone: forget them unread.
 for(auto& l:job.live)if(l.node&&!current(l,job.generation,level))l=Live{};
 // A new generation starts with the ceiling the game set up again (0x173230); possession, which copies the ceiling
 // and reserve between bodies (0x1d30d0, 0x1e13d0), stays in the same generation and keeps what is held.
 if(job.focusGeneration!=job.generation){job.focusGeneration=job.generation;job.focusApplied=0;}
 const bool play=world&&state==3&&loading<=0&&m.get(base+0x4efd70,control)&&control&&m.get(control+0x60,controlWorld)&&controlWorld&&
  m.get(controlWorld+0x1f0,controlWorld)&&controlWorld==world&&m.get(base+0x4f0830,player)&&player;
 if(!play||!job.session)return;
 uintptr_t head=0,headNode=0;std::array<float,3> at{},facing{};
 if(!m.get(player+0x11e0,head)||!head||!m.get(head+0x18,headNode)||!headNode||!m.get(headNode+0x20,at)||!finite3(at)||!m.get(player+0xa08,facing)||!finite3(facing))return;
 const uint64_t now=GetTickCount64();const float dt=job.last?(std::min)(float(now-job.last)/1000.0f,0.25f):0.0f;job.last=now;
 if(lightsPossible){
  // Lights no longer wanted go first, so their slots are free for new ones.
  for(size_t i=0;i<maxLights;++i){auto& l=job.live[i];if(!l.node)continue;bool still=false;
   for(size_t w=0;w<job.wanted;++w)still|=sameKey(l.key,job.wants[w].key);if(!still)drop(l,world,i,job.generation,level);}
  for(size_t w=0;w<job.wanted;++w){const auto& want=job.wants[w];size_t slot=maxLights;
   for(size_t i=0;i<maxLights;++i)if(job.live[i].node&&sameKey(job.live[i].key,want.key))slot=i;
   if(slot==maxLights)for(size_t i=0;i<maxLights;++i)if(!job.live[i].node){slot=i;break;}
   if(slot==maxLights)break;
   auto& l=job.live[slot];
   if(l.node&&!ours(l.node,slot,l.owner)){l=Live{};++faults;continue;}
   if(!l.node&&job.broken)continue;
   // A placed light stands where its addon put it; a follow light floats near the head.
   auto goal=want.spec.placed?want.spec.position:target(at,facing,want.spec,l.clock+=dt);
   // Like the game's own player light: never above the ceiling over the head (10 cm margin).
   if(!want.spec.placed&&goal[1]-at[1]>1.0f){static const float up[3]={0,1,0};std::array<float,3> from=at;
    if(probe(from.data(),up,goal[1]-at[1],10.0f,world,from.data())&&std::isfinite(from[1]))goal[1]=(std::min)(goal[1],from[1]);}
   if(!l.node){
    uint32_t freeSlot=0;if(!worldemitters::removalReady(world,freeSlot)){++refused;continue;}  // keep room to remove it later
    const uintptr_t node=create(base+lightClass,1);if(!node||!readable(node,lightSize)){++faults;continue;}
    *(uint32_t*)(node+0x260)=3;*(uint32_t*)(node+8)|=0x1000;*(uint32_t*)(node+0xc)|=0x140100;*(uint8_t*)(node+0x2e1)=1;*(uint8_t*)(node+0x2e0)=1;
    style(node,want.spec);*(float*)(node+0x2c8)=0.4f;*(float*)(node+0x220)=0.4f;*(float*)(node+0x22c)=4.0f;
    nodeName((char*)(node+0x140),slot);
    for(size_t i=0;i<3;++i)*(float*)(node+0x94+4*i)=goal[i];
    uintptr_t published=node;publish(world,&published,0);
    *(float*)(node+0x220)=0.01f;apply(node);   // fade in, as 0x120750 does
    uintptr_t owner=0;
    if(!m.get(node+0x130,owner)||!owner||!ours(node,slot,owner)){
     // Published but not as expected: stop creating lights for this run rather than pile up nodes we cannot
     // account for. The level owns the node; it goes with the level.
     job.broken=true;++faults;continue;
    }
    std::memcpy(l.key,want.key,sizeof l.key);std::memcpy(l.level,level,16);l.node=node;l.owner=owner;l.generation=job.generation;l.at=goal;l.spec=want.spec;
   }else{
    if(!scripting::sameLight(l.spec,want.spec)){
     const bool glowChanged=l.spec.glow!=want.spec.glow||l.spec.glowSize!=want.spec.glowSize;
     if(l.glow&&!want.spec.glow&&glowPossible)dropGlow(l,slot);
     else if(l.glow&&glowChanged&&glowPossible){dropGlow(l,slot);}   // remade below with the new size
     else if(l.glow&&l.spec.glowColor!=want.spec.glowColor)fillRamp(glowSpecs[slot],want.spec.glowColor);   // read live: recolours in place
     style(l.node,want.spec);l.spec=want.spec;refresh(l.node);}
    float size=*(float*)(l.node+0x220);const float full=*(float*)(l.node+0x2c8);
    if(std::isfinite(size)&&std::isfinite(full)&&size<full){size=(std::min)(full,size+dt*0.5f);*(float*)(l.node+0x220)=size;refresh(l.node);}
    l.at=want.spec.placed?goal:ease(l.at,goal,want.spec.smoothing,dt);
   }
   place(l.node,l.at);
   if(glowPossible&&l.spec.glow&&!l.glow)makeGlow(l,slot,world);
  }
 }
 if(focusPossible){
  float ceiling=0,reserve=0;
  if(m.get(player+0xd78,ceiling)&&m.get(player+0xd84,reserve)){
   const float delta=focusStep(job.focus,job.focusApplied,ceiling,reserve);
   if(delta!=0){*(float*)(player+0xd84)=reserve+delta;job.focusApplied+=delta;}
  }
 }
}
static char queued(void* payload){
 __try{if(payload==&job)run();else ++faults;}__except(EXCEPTION_EXECUTE_HANDLER){++faults;}
 pending.store(false);return 0;
}
static void submit(){__try{enqueue(base+0x499990,queued,&job);}__except(EXCEPTION_EXECUTE_HANDLER){pending.store(false);++faults;}}
// Each pulse: copy what the addons ask for into the pinned job and queue it, unless one is still waiting.
static uint32_t generation=1;static bool inSession=false;static unsigned reportedFaults=0,reportedRefused=0;
static void step(scripting::Host& owner,const std::string& phase){
 // Leaving the session phase (title menu, loading) tears the level down: everything placed so far is the old
 // generation's. Counted on every pulse, even while a job is pending.
 const bool playing=phase=="session";if(inSession&&!playing)++generation;inSession=playing;
 if(faults!=reportedFaults){reportedFaults=faults;owner.report("runtime","warn","Lights: a light node was not as expected and was left to the game (see lua-powers-lights-review.md)");}
 if(refused!=reportedRefused){reportedRefused=refused;owner.report("runtime","info","Lights: the game's removal queue was full; retrying next pulse");}
 if((!lightsPossible&&!focusPossible)||!enqueue||pending.load())return;
 if(job.generation!=generation){job.generation=generation;job.broken=false;}
 const auto lights=owner.desiredLights();const float focus=owner.desiredFocus();
 bool holding=job.focusApplied!=0;for(const auto& l:job.live)holding|=l.node!=0;
 if(lights.empty()&&focus==0&&!holding)return;
 if(!worldemitters::queueIdle())return;
 job.wanted=0;
 for(const auto& light:lights){if(job.wanted==maxLights)break;auto& w=job.wants[job.wanted++];
  const std::string key=light.addon+"/"+light.light.name;std::memset(w.key,0,sizeof w.key);std::memcpy(w.key,key.data(),(std::min)(key.size(),sizeof w.key-1));w.spec=light.light;}
 job.focus=focus;job.session=playing;
 pending.store(true);submit();
}
static const unsigned char createPrefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0xc0,0x48,0x89,0x4d,0xf0,0x48,0x89,0x55};
static const unsigned char applyPrefix[]={0x53,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xc8,0xf3,0x0f,0x10,0x80,0x20,0x02,0x00};
static const unsigned char probePrefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0xa4,0x24,0xe0,0xfe,0xff,0xff,0x48,0x89,0x9d,0x00};
static const unsigned char removePrefix[]={0x53,0x56,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xd3,0x44,0x89,0xc6,0x44,0x8b,0x81};
static const unsigned char publishPrefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0xa0,0x48,0x89,0x5d,0xc8,0x48,0x89,0x75};
static bool globals(){for(uintptr_t rva:{uintptr_t(0x4f0bd0),uintptr_t(0x4efd70),uintptr_t(0x4f0830),uintptr_t(0x4f0840),uintptr_t(0x4eff70),uintptr_t(0x499860)})if(!readable(base+rva,8))return false;return true;}
static bool available(){
 castPossible=globals()&&gamebridge::operation(base+0x1d8340,castPrefix,sizeof castPrefix);
 const bool queue=globals()&&worldemitters::queueAvailable();
 focusPossible=queue;
 // 0x50eb0 is the world contract's propagator; its prefix is checked by worldContract().
 lightsPossible=queue&&readable(base+lightClass,0x70)&&gamebridge::operation(base+0x51880,createPrefix,sizeof createPrefix)&&gamebridge::operation(base+0x51eb0,applyPrefix,sizeof applyPrefix)&&
  gamebridge::operation(base+0x121150,probePrefix,sizeof probePrefix)&&gamebridge::operation(base+0x5bed0,removePrefix,sizeof removePrefix)&&gamebridge::operation(base+0x5a420,publishPrefix,sizeof publishPrefix);
 if(lightsPossible){create=(Create)(base+0x51880);apply=(Apply)(base+0x51eb0);pose=(Apply)(base+0x50eb0);publish=(Publish)(base+0x5a420);remove=(Remove)(base+0x5bed0);probe=(Probe)(base+0x121150);}
 {const unsigned char emitterPrefix[]={0x53,0x57,0x56,0x41,0x54,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xcb,0x48,0x89,0xd6};
  const unsigned char attachPrefix[]={0x53,0x56,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xcb,0x48,0x89,0xd6,0x48,0x8b,0x86};
  const unsigned char unlinkPrefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0x90,0x48,0x89,0x5d,0xc8,0x48,0x89,0x4d};
  glowPossible=lightsPossible&&gamebridge::operation(base+0xcfd20,emitterPrefix,sizeof emitterPrefix)&&gamebridge::operation(base+0x50170,attachPrefix,sizeof attachPrefix)&&
   gamebridge::operation(base+0x50a60,unlinkPrefix,sizeof unlinkPrefix)&&readable(base+0x2b3fd0,0xa0);
  if(glowPossible){emitter=(Emitter)(base+0xcfd20);attach=(Attach)(base+0x50170);unlink=(Unlink)(base+0x50a60);}}
 if(queue)enqueue=(Queue)(base+0x1a890);
 return castPossible||lightsPossible||focusPossible;
}
}
