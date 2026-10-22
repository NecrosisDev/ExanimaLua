#pragma once
// item.id_changed: an item's ID changed. A level-local item (0x8...) gets a world-database ID (0x4...) once when it
// leaves its level with the player: 102DF0(id) returns the new ID (and recurses into container contents), called for
// every carried, worn and held item. An addon that remembers an item by ID follows it here. The
// detour calls the original first and only queues (old, new) when they differ; Lua hears them at the next pulse. It
// is made the first time an addon listens, only on the game's own entry bytes.
namespace itemids {
using Convert=uint32_t(*)(uint32_t);
static Convert original=nullptr;static bool created=false,installed=false,refused=false;
static std::atomic<bool> listening{false};
static SRWLOCK lock=SRWLOCK_INIT;static std::array<std::pair<uint32_t,uint32_t>,256> queue{};static size_t queued=0,lost=0;
static uint32_t converted(uint32_t id){
 const uint32_t result=original(id);
 if(result!=id&&result&&listening.load(std::memory_order_relaxed)){
  AcquireSRWLockExclusive(&lock);if(queued<queue.size())queue[queued++]={id,result};else ++lost;ReleaseSRWLockExclusive(&lock);}
 return result;
}
static const unsigned char prefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0xa4,0x24,0x60,0xff,0xff,0xff,0x48,0x89,0x5d,0x80};
static bool install(){
 if(installed)return true;if(refused)return false;
 if(!created){if(!gamebridge::operation(base+0x102df0,prefix,sizeof prefix)||MH_CreateHook((void*)(base+0x102df0),(void*)converted,(void**)&original)!=MH_OK){refused=true;return false;}created=true;}
 const auto s=MH_EnableHook((void*)(base+0x102df0));installed=s==MH_OK||s==MH_ERROR_ENABLED;if(!installed)refused=true;return installed;
}
static void deliver(scripting::Host& owner){
 const bool wanted=owner.handlesIdChanges();
 if(wanted&&!installed&&!refused&&gamebridge::active&&!install())owner.report("runtime","warn","item.id_changed unavailable: the ID conversion did not match or could not be hooked");
 listening=installed&&wanted;
 std::vector<scripting::IdChange> changes;size_t missed=0;
 AcquireSRWLockExclusive(&lock);for(size_t i=0;i<queued;++i)changes.push_back({queue[i].first,queue[i].second});missed=lost;queued=lost=0;ReleaseSRWLockExclusive(&lock);
 if(!listening)return;if(missed)owner.report("runtime","warn","item.id_changed: more than 256 changes in one pulse; some were dropped");
 if(!changes.empty())owner.notifyIdChanges(changes);
}
}
