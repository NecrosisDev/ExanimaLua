#pragma once
// zone.enter: the game's zone-script runner 0xaa4d0(character, zone payload) is detoured. The trigger test 0xaa540
// calls it once each time a character's position crosses into a type-0x10 zone marker.
// The payload holds the zone's ZoneID at +0xc. The detour copies {character, ZoneID}, then runs the
// original, so the zone's own script still runs first-hand and unchanged; Lua sees the entry at the next session
// pulse after the character is matched against that pulse's snapshot. The runner uses the game's one global script
// context, so the detour never calls back into the game or Lua. The player-only zone test 0xaa7c0 (flags & 0xf00 ==
// 0x200) calls the same runner, so those zones arrive here too; whether that path re-fires while the player stays
// inside is not traced, which the 32-per-pulse queue bounds.
// This detour is the one owner of 0xaa4d0 for every mod: native mods register a listener through the exported
// add_zone_listener instead of hooking it again. Listeners run synchronously on the game's thread inside the
// detour, before the zone's script, with the zone payload read-only; they must return quickly and must not run
// game scripts (the script context is global and not re-entrant). A listener fault is caught and the listener
// dropped.
namespace zones {
using Run=void(*)(uintptr_t,uintptr_t);
using Listener=void(*)(void* context,uintptr_t character,uint32_t zone,const void* payload);
struct Entry {uintptr_t character=0;uint32_t zone=0;};
struct Native {Listener call=nullptr;void* context=nullptr;};
static Run original=nullptr;static bool created=false,installed=false;
static std::atomic<bool> listening{false};
static SRWLOCK lock=SRWLOCK_INIT,listenerLock=SRWLOCK_INIT;
static std::array<Entry,32> queue;static size_t queued=0,lost=0;
static std::array<Native,8> natives;static std::atomic<size_t> nativeCount{0};
static bool addListener(Listener call,void* context){
 if(!call)return false;AcquireSRWLockExclusive(&listenerLock);bool ok=false;const size_t n=nativeCount.load();
 for(size_t i=0;i<n;++i)if(natives[i].call==call&&natives[i].context==context)ok=true;
 if(!ok&&n<natives.size()){natives[n]={call,context};nativeCount=n+1;ok=true;}
 ReleaseSRWLockExclusive(&listenerLock);return ok;
}
// Calls in progress, so that a removal returns only after the listener can no longer be running (unless the
// removal comes from inside a listener on the same thread, which cannot wait for itself).
static std::atomic<int> inFlight{0};static thread_local bool calling=false;
static bool dropListener(Listener call,void* context){
 AcquireSRWLockExclusive(&listenerLock);bool found=false;size_t n=nativeCount.load();
 for(size_t i=0;i<n;++i)if(natives[i].call==call&&natives[i].context==context){natives[i]=natives[n-1];natives[n-1]={};nativeCount=--n;found=true;break;}
 ReleaseSRWLockExclusive(&listenerLock);return found;
}
static bool removeListener(Listener call,void* context){
 const bool found=dropListener(call,context);
 if(found&&!calling)while(inFlight.load())Sleep(0);
 return found;
}
static bool callNative(const Native& n,uintptr_t character,uint32_t zone,uintptr_t payload){__try{n.call(n.context,character,zone,(const void*)payload);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
// Checked before each call, so a listener another listener removed during this same detour call is skipped.
static bool listed(const Native& n){
 AcquireSRWLockShared(&listenerLock);bool found=false;const size_t count=nativeCount.load();
 for(size_t i=0;i<count;++i)if(natives[i].call==n.call&&natives[i].context==n.context)found=true;
 ReleaseSRWLockShared(&listenerLock);return found;
}
static void entered(uintptr_t character,uintptr_t payload){
 if(nativeCount.load(std::memory_order_relaxed)){
  uint32_t zone=0;
  if(copyWorldMemory(payload+0xc,&zone,4)){
   // The count is raised before the list is copied, so a removal that misses this copy still waits for it.
   ++inFlight;AcquireSRWLockShared(&listenerLock);std::array<Native,8> copy=natives;const size_t n=nativeCount.load();ReleaseSRWLockShared(&listenerLock);
   const bool outer=calling;calling=true;
   for(size_t i=0;i<n;++i)if(listed(copy[i])&&!callNative(copy[i],character,zone,payload))dropListener(copy[i].call,copy[i].context);
   calling=outer;--inFlight;
  }
 }
 if(listening.load(std::memory_order_relaxed)){
  uint32_t zone=0;
  if(copyWorldMemory(payload+0xc,&zone,4)){AcquireSRWLockExclusive(&lock);if(queued<queue.size())queue[queued++]={character,zone};else ++lost;ReleaseSRWLockExclusive(&lock);}
 }
 original(character,payload);
}
static void clear(){AcquireSRWLockExclusive(&lock);queued=lost=0;ReleaseSRWLockExclusive(&lock);}
// The first 14 bytes are four instructions without a RIP-relative operand; the 5-byte detour stays inside them.
static bool install(){
 if(installed)return true;
 const unsigned char prefix[]={0x53,0x48,0x8d,0x64,0x24,0xd0,0x48,0x89,0x4c,0x24,0x20,0x48,0x89,0xd3};
 if(!created){if(!gamebridge::operation(base+0xaa4d0,prefix,sizeof prefix)||MH_CreateHook((void*)(base+0xaa4d0),(void*)entered,(void**)&original)!=MH_OK)return false;created=true;}
 const auto status=MH_EnableHook((void*)(base+0xaa4d0));installed=status==MH_OK||status==MH_ERROR_ENABLED;return installed;
}
static void deliver(scripting::Host& owner,const scripting::CharacterSnapshot* snapshot){
 listening=installed&&owner.handlesZones();
 std::vector<Entry> entries;size_t missed=0;
 AcquireSRWLockExclusive(&lock);entries.assign(queue.begin(),queue.begin()+ptrdiff_t(queued));missed=lost;queued=lost=0;ReleaseSRWLockExclusive(&lock);
 if(!listening||!snapshot||!snapshot->ready)return;
 static uint64_t droppedTotal=0;
 if(missed){const auto before=droppedTotal;droppedTotal+=missed;if(!before||before/1000!=droppedTotal/1000)
  owner.report("runtime","warn",("Zone events dropped (more than 32 in one pulse): "+std::to_string(droppedTotal)+" so far").c_str());}
 std::vector<scripting::ZoneEntry> found;
 for(const auto& e:entries)for(const auto& c:snapshot->characters)if(e.character&&c.nativeCharacter==e.character){
  found.push_back({e.zone,c.record,c.name,c.player&&c.playerKnown});break;}
 if(!found.empty())owner.notifyZones(found);
}
}
