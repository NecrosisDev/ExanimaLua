#pragma once
// Inventory exports for other mods' native code (ExCamera's VR inventory), contract version 1: inventory_list,
// inventory_drop_row, inventory_pickup_node, inventory_request_state. The runtime stays the only owner of every
// transfer: a request runs the same inventorybridge/worlditems commit as row:drop() and object:pickup(), on the
// session pulse, under the same eligibility and fault gates. The callers' side copies a published table and queues
// requests; it never reads or writes game memory. docs/reviews/inventory-exports-review.md
namespace invexports {
struct ItemV1 {uint64_t ticket;uint32_t row,item,state;int16_t x,y;uint32_t flags;char name[32];};
static_assert(sizeof(ItemV1)==64,"inventory export row layout");
struct Request {uint32_t id=0;bool pickup=false;uint64_t ticket=0;uint32_t row=0;std::array<float,3> position{};uintptr_t node=0;uint64_t queued=0;};
struct Result {uint32_t id=0;int state=0;uintptr_t node=0;};
// The published table (seqlock: the reader takes no lock and allocates nothing).
static ItemV1 table[256];static volatile LONG count=-1;static volatile LONG sequence=0;
static std::vector<scripting::InventoryRow> publishedRows;static uint64_t ticket=0;
// Requests and their results, under one small lock (callers on any thread).
static SRWLOCK lock=SRWLOCK_INIT;static std::vector<Request> pending;static Result results[32];static uint32_t nextId=0;
static std::atomic<uint64_t> lastUse{0};
static void publish(const scripting::InventoryView* view){
 InterlockedIncrement(&sequence);MemoryBarrier();
 if(!view)count=-1;
 else{
  if(view->carried!=publishedRows){publishedRows=view->carried;++ticket;}
  LONG n=0;
  for(size_t i=0;i<view->carried.size()&&n<256;++i){
   const auto& r=view->carried[i];auto& out=table[n++];out={};out.ticket=ticket;out.row=uint32_t(i);out.item=r.item;out.state=r.state;out.x=r.x;out.y=r.y;
   bool droppable=worlditems::dropPossible&&worlditems::ordinary(r.item,r.state);
   for(const auto& worn:view->worn)droppable=droppable&&worn.item!=r.item;
   for(size_t w=0;w<view->weapons.size();w+=2)droppable=droppable&&view->weapons[w]!=r.item;
   out.flags=(droppable?1u:0u)|(inventorybridge::chunk(r.item,0x20000)?2u:0u);
  }
  count=n;
 }
 MemoryBarrier();InterlockedIncrement(&sequence);
}
static void settle(const Request& r,int state,uintptr_t node){
 AcquireSRWLockExclusive(&lock);results[r.id%32]={r.id,state,node};ReleaseSRWLockExclusive(&lock);
 host.report("runtime",state==1?"info":"warn",state==1?(r.pickup?"Inventory export: pickup applied":"Inventory export: drop applied"):(r.pickup?"Inventory export: pickup refused":"Inventory export: drop refused"));
}
// The session pulse (game thread, after the addons' own operations): refresh the table while a caller has used it in
// the last five seconds, then run at most one request. A request waits while the player is busy (a UI, combat, a
// drag) or another operation ran this pulse, and is refused after two seconds of waiting.
static void pulse(bool session,uint64_t now){
 if(!session||!gamebridge::active||!inventorybridge::possible){if(count!=-1)publish(nullptr);return;}
 Request r;bool have=false;
 AcquireSRWLockExclusive(&lock);if(!pending.empty()){r=pending.front();have=true;}ReleaseSRWLockExclusive(&lock);
 if(!have&&now>lastUse.load()+5000){if(count!=-1)publish(nullptr);return;}
 const auto view=inventorybridge::capture();publish(view);
 if(!have)return;
 if(!view||worlditems::published){
  if(now>r.queued+2000){AcquireSRWLockExclusive(&lock);if(!pending.empty()&&pending.front().id==r.id)pending.erase(pending.begin());ReleaseSRWLockExclusive(&lock);settle(r,-1,0);}
  return;
 }
 AcquireSRWLockExclusive(&lock);if(!pending.empty()&&pending.front().id==r.id)pending.erase(pending.begin());ReleaseSRWLockExclusive(&lock);
 scripting::InventoryChange change;change.inventory=view->ticket;
 if(r.pickup){
  if(!worlditems::pickupPossible){settle(r,-1,0);return;}
  const auto loose=worlditems::capture(r.node);if(!loose){settle(r,-1,0);return;}
  change.op=scripting::InventoryOp::pickup;change.worldItem=loose;
  const auto refreshed=inventorybridge::capture();if(!refreshed){settle(r,-1,0);return;}change.inventory=refreshed->ticket;
 }else{
  if(!worlditems::dropPossible||r.ticket!=ticket||view->carried!=publishedRows||r.row>=view->carried.size()){settle(r,-1,0);return;}
  change.op=scripting::InventoryOp::drop;change.row=r.row;change.position=r.position;
 }
 worlditems::dropped=0;
 const bool applied=inventorybridge::commit(change);
 settle(r,applied?1:-1,applied&&!r.pickup?worlditems::dropped:0);
}
static void reset(){
 AcquireSRWLockExclusive(&lock);pending.clear();ReleaseSRWLockExclusive(&lock);
 if(count!=-1)publish(nullptr);
}
static uint32_t queue(Request r){
 if(!runtimeEnabled.load()||!gamebridge::active)return 0;
 AcquireSRWLockExclusive(&lock);uint32_t id=0;
 if(pending.size()<4){id=++nextId;if(!id)id=++nextId;r.id=id;r.queued=GetTickCount64();pending.push_back(r);results[id%32]={id,0,0};}
 ReleaseSRWLockExclusive(&lock);lastUse=GetTickCount64();return id;
}
static int list(ItemV1* out,int capacity){
 lastUse=GetTickCount64();if(!out||capacity<0)return -1;
 for(int attempt=0;attempt<8;++attempt){
  const LONG before=sequence;if(before&1){YieldProcessor();continue;}MemoryBarrier();
  const LONG seen=count;if(seen<0){MemoryBarrier();if(sequence==before)return -1;continue;}
  const int n=seen<capacity?int(seen):capacity;std::memcpy(out,table,sizeof(ItemV1)*size_t(n));
  MemoryBarrier();if(sequence==before)return n;
 }
 return -1;
}
static uint32_t drop(uint64_t t,uint32_t row,const float* position){
 if(!position)return 0;Request r;r.ticket=t;r.row=row;
 for(int i=0;i<3;++i){if(!std::isfinite(position[i])||std::abs(position[i])>10000000.f)return 0;r.position[size_t(i)]=position[i];}
 const LONG before=sequence;if(before&1)return 0;MemoryBarrier();const LONG seen=count;
 if(seen<0||row>=uint32_t(seen)||table[row].ticket!=t||!(table[row].flags&1u)||sequence!=before)return 0;
 return queue(r);
}
static uint32_t pickup(uintptr_t node){if(!node)return 0;Request r;r.pickup=true;r.node=node;return queue(r);}
static int state(uint32_t id,uintptr_t* node){
 if(node)*node=0;if(!id)return -1;
 AcquireSRWLockShared(&lock);const auto r=results[id%32];ReleaseSRWLockShared(&lock);
 if(r.id!=id)return -1;if(node)*node=r.node;return r.state;
}
}
