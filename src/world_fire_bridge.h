#pragma once
// world.light_fire(name, {position, size}) / world.put_out_fire(name): named fires an addon lights on placed
// scenery (docs/reviews/world-fire-review.md). The fire is the game's own: 0x1494d0(node, size) builds it at a
// placed node's position through the fire factory 0xf2a70 (the path gear.use's ignite_target uses), and removal is
// the world's removal queue 0x5bed0(world, node, 1) (the path a fireplace's extinguish uses). Both run as jobs on
// the game's ring 0x499990, never inside a pulse. A fire is a free child of its sector whose body is class
// 0x30e9e8; the job finds the new one by comparing the sector's fire nodes before and after.
// Saves: a fire body is rebuilt at load (0xf2ed0), so a burning mod fire would burn for ever after a reload. The
// positions of lit mod fires are kept in the runtime's save entry ("#fires"); after a load that carries them, the
// fire nodes within 30 cm of each are removed.
// effects.attach_fire(name, target, {size}) / effects.remove_fire(name): the same factory's small fire, built with a
// zero offset, its save tag (body +0x158) cleared so no load rebuilds it, parented under the target's scene node
// (0x50170, as 0x149740 does), moved to the target's sector when different (0x5c030), posed (0x50eb0) and published
// (0x5a420); removed with 0x50a60(fire, 1) while it is still the target's child. Never saved, never a damage source.
// fire_nodes / world.fires: every 10 pulses the player's sector's fire nodes (tagged 0x3DD0E002) and the attached
// fires, nearest the player first, with their size (body +0x12c); published for other mods' threads (seqlock).
namespace worldfires {
struct Lit {std::string owner,name;uintptr_t world=0,sector=0,node=0;std::array<float,3> at{};};
struct Attached {std::string owner,name;uintptr_t world=0,target=0,node=0;};
enum Kind {light=0,putOut=1,attach=2,detach=3};
struct Order {std::string owner,name;int kind=light;std::array<float,3> at{};int size=100;float scale=1;uintptr_t world=0,sector=0,target=0;int hand=-1;};
static std::vector<Lit> lit;static std::vector<Attached> attached;static std::deque<Order> orders;static std::vector<std::array<float,3>> stale;
static std::atomic<bool> busy{false};static Order job;static uintptr_t jobResult=0;static bool jobDone=false;
static uintptr_t knownWorld=0;
using Factory=uintptr_t(*)(const float*,float,uintptr_t);using Attach=void(*)(uintptr_t,uintptr_t);using Unlink=void(*)(uintptr_t,int);
using Move=void(*)(uintptr_t,uintptr_t);using Pose=void(*)(uintptr_t);using Publish=void(*)(uintptr_t,uintptr_t*,int);
static Factory factory=nullptr;static Attach attachNode=nullptr;static Unlink unlink=nullptr;static Move move=nullptr;static Pose pose=nullptr;static Publish publish=nullptr;
static bool attachPossible=false;
static void availableAttach(){
 const unsigned char attachPrefix[]={0x53,0x56,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xcb,0x48,0x89,0xd6,0x48,0x8b,0x86};
 const unsigned char unlinkPrefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0x90,0x48,0x89,0x5d,0xc8,0x48,0x89,0x4d};
 const unsigned char movePrefix[]={0x53,0x57,0x56,0x41,0x54,0x48,0x8d,0xa4,0x24,0xc8,0xfe,0xff,0xff};
 attachPossible=worldemitters::fireInfrastructureAvailable()&&worldContract()&&gamebridge::operation(base+0x50170,attachPrefix,sizeof attachPrefix)&&
  gamebridge::operation(base+0x50a60,unlinkPrefix,sizeof unlinkPrefix)&&gamebridge::operation(base+0x5c030,movePrefix,sizeof movePrefix);
 if(attachPossible){factory=(Factory)(base+0xf2a70);attachNode=(Attach)(base+0x50170);unlink=(Unlink)(base+0x50a60);move=(Move)(base+0x5c030);
  pose=(Pose)(base+0x50eb0);publish=(Publish)(base+0x5a420);}
}
static bool possible(){return worldemitters::firePossible&&worldemitters::fire&&worldemitters::extinguishPossible&&worldemitters::remove&&gamebridge::backupDone;}
static bool sectors(uintptr_t world,std::vector<uintptr_t>& out){
 const scripting::CharacterMemory m{copyWorldMemory};int last=-1;uintptr_t list=0;out.clear();
 if(!m.get(world+0x3c4,last)||last<0||last>=512||!m.get(world+0x3c8,list))return false;
 for(int i=0;i<=last;++i){uintptr_t sector=0;if(!m.get(list+size_t(i)*8,sector)||!sector)return false;out.push_back(sector);}return true;
}
// The sector's fire nodes: free children whose body is the fire class and points back at them.
static bool fires(uintptr_t sector,std::vector<uintptr_t>& out){
 const scripting::CharacterMemory m{copyWorldMemory};std::vector<uintptr_t> nodes;out.clear();if(!worlddevices::children(sector,nodes,8192))return false;
 for(const auto node:nodes){uintptr_t body=0,parent=0,owner=0;if(!node||!m.get(node+0x160,body))return false;
  if(!body||!scripting::classMatches(body,base+0x30e9e8,copyWorldMemory))continue;
  if(m.get(node+0x128,parent)&&parent==sector&&m.get(body+0x18,owner)&&owner==node)out.push_back(node);}
 return true;
}
static float nearestFire(uintptr_t sector,const std::array<float,3>& at){
 const scripting::CharacterMemory m{copyWorldMemory};std::vector<uintptr_t> list;if(!fires(sector,list))return -1;float best=1e9f;
 for(const auto node:list){std::array<float,3> p{};if(!m.get(node+0x64,p))return -1;const float d=worlddevices::distance(p,at);if(d<0)return -1;best=(std::min)(best,d);}
 return best;
}
// The placed node nearest `at` (within 60 cm) in the player's sector: what the fire is built on.
static bool target(const std::array<float,3>& at,uintptr_t& world,uintptr_t& sector,uintptr_t& node){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t player=0,body=0,owner=0;
 if(!m.get(base+npc::off::world,world)||!world||!m.get(base+npc::off::controlled,player)||!player||!m.get(player+0x18,body)||!body||
  !m.get(body+0x130,sector)||!sector||!m.get(sector+0x220,owner)||owner!=world)return false;
 std::vector<uintptr_t> nodes;if(!worlddevices::children(sector,nodes,8192))return false;float best=60.f;node=0;
 for(const auto n:nodes){uintptr_t nb=0;uint32_t id=0;std::array<float,3> p{};
  if(!n||!m.get(n+0x180,id)||!id||!m.get(n+0x64,p))continue;
  if(m.get(n+0x160,nb)&&nb&&scripting::classMatches(nb,base+0x30e9e8,copyWorldMemory))continue;   // a fire itself
  const float d=worlddevices::distance(p,at);if(d>=0&&d<best){best=d;node=n;}}
 return node!=0;
}
static void remember(){
 std::string bytes;for(const auto& l:lit)bytes.append((const char*)l.at.data(),12);
 AcquireSRWLockExclusive(&savedata::storeLock);if(bytes.empty())savedata::store.erase("#fires");else savedata::store["#fires"]=bytes;ReleaseSRWLockExclusive(&savedata::storeLock);
}
// The job (game thread): light, remove one fire node through the removal queue, attach a flame, or unlink one.
static void work(){
 jobResult=0;const scripting::CharacterMemory m{copyWorldMemory};
 if(job.kind==light){
  std::vector<uintptr_t> before,after;
  if(fires(job.sector,before)&&nearestFire(job.sector,job.at)>=50.f){
   worldemitters::fire(job.target,job.size);
   if(fires(job.sector,after))for(const auto n:after)if(std::find(before.begin(),before.end(),n)==before.end()){if(jobResult){jobResult=0;break;}jobResult=n;}
  }
 }else if(job.kind==putOut){
  uint32_t slot=0;if(job.target&&worldemitters::removalReady(job.world,slot)){worldemitters::remove(job.world,job.target,1);jobResult=job.target;}
 }else if(job.kind==attach){
  uintptr_t sector=0,owner=0;if(!m.get(job.target+0x130,sector)||!sector||!m.get(sector+0x220,owner)||owner!=job.world||!readable(job.target,0x198))return;
  const float zero[3]{};uintptr_t fire=factory(zero,job.scale,job.world);uintptr_t body=0;
  if(!fire||!readable(fire,0x198)||!m.get(fire+0x160,body)||!body||!readable(body,0x160))return;
  *(uint32_t*)(body+0x158)=0;*(uint32_t*)(fire+8)|=0x9000;   // untagged: no load rebuilds it
  attachNode(job.target,fire);uintptr_t at=0;if(m.get(fire+0x130,at)&&at!=sector)move(fire,sector);
  pose(fire);uintptr_t published=fire;publish(job.world,&published,0);jobResult=fire;
 }else if(job.kind==detach){
  uintptr_t parent=0;if(job.target&&m.get(job.target+0x128,parent)&&parent==job.sector){unlink(job.target,1);jobResult=job.target;}
 }
}
static unsigned char run(void*){
 __try{work();}__except(EXCEPTION_EXECUTE_HANDLER){jobResult=0;}
 jobDone=true;busy=false;return 0;
}
static bool owned(const std::string& owner,const std::string& name){for(const auto& l:lit)if(l.owner==owner&&l.name==name)return true;return false;}
static size_t count(const std::string& owner){size_t n=0;for(const auto& l:lit)n+=l.owner==owner;return n;}
static size_t countAttached(const std::string& owner){size_t n=0;for(const auto& l:attached)n+=l.owner==owner;return n;}
// The published list: other mods' threads read it (fire_nodes), Lua reads `listed` on this thread (world.fires).
struct Entry {uintptr_t node=0;float size=0;};
static Entry table[64];static volatile LONG tableCount=0,tableSequence=0;
static std::vector<scripting::FireInfo> listed;static unsigned scanTick=0;
static void scan(uintptr_t world){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t player=0,body=0,sector=0;std::array<float,3> me{};
 struct Found {uintptr_t node;float size,distance;std::array<float,3> at;bool mod;};std::vector<Found> found;
 if(m.get(base+npc::off::controlled,player)&&player&&m.get(player+0x18,body)&&body&&m.get(body+0x130,sector)&&sector&&m.get(player+npc::off::root,me)){
  std::vector<uintptr_t> list;if(fires(sector,list))for(const auto n:list){uintptr_t b=0;uint32_t tag=0;float size=0;std::array<float,3> p{};
   if(!m.get(n+0x160,b)||!m.get(b+0x158,tag)||tag!=0x3DD0E002||!m.get(b+0x12c,size)||!m.get(n+0x64,p))continue;
   bool mod=false;for(const auto& l:lit)mod=mod||l.node==n;found.push_back({n,size,worlddevices::distance(p,me),p,mod});}
  for(const auto& a:attached){uintptr_t b=0;float size=0;std::array<float,3> p{};uintptr_t parent=0;
   if(a.world!=world||!m.get(a.node+0x128,parent)||parent!=a.target||!m.get(a.node+0x160,b)||!b||!m.get(b+0x12c,size)||!m.get(a.target+0x64,p))continue;
   found.push_back({a.node,size,worlddevices::distance(p,me),p,true});}
 }
 std::sort(found.begin(),found.end(),[](const Found& x,const Found& y){return x.distance<y.distance;});
 listed.clear();Entry next[64];LONG n=0;
 for(const auto& f:found){if(n<64){next[n]={f.node,f.size};++n;}scripting::FireInfo i;i.at=f.at;i.size=f.size;i.mod=f.mod;i.distance=f.distance;listed.push_back(i);}
 InterlockedIncrement(&tableSequence);MemoryBarrier();std::memcpy(table,next,sizeof(Entry)*size_t(n));tableCount=n;MemoryBarrier();InterlockedIncrement(&tableSequence);
}
static bool readFires(std::vector<scripting::FireInfo>& out){out=listed;return true;}
static int exportNodes(uintptr_t* nodes,float* sizes,int capacity){
 if(!nodes||!sizes||capacity<=0)return 0;
 for(int attempt=0;attempt<8;++attempt){
  const LONG before=tableSequence;if(before&1){YieldProcessor();continue;}MemoryBarrier();
  const LONG seen=tableCount;const int n=(std::min)(int(seen),capacity);
  for(int i=0;i<n;++i){nodes[i]=table[i].node;sizes[i]=table[i].size;}
  MemoryBarrier();if(tableSequence==before)return n;
 }
 return 0;
}
// The session pulse (game thread side of the presentation pulse): collect, settle the last job, start the next.
static void pulse(scripting::Host& host,bool session){
 std::vector<scripting::FireOrder> wanted;host.collectFires(wanted);
 uintptr_t world=0;const scripting::CharacterMemory m{copyWorldMemory};
 if(!session||!m.get(base+npc::off::world,world)||!world){
  if(!busy.load())orders.clear();knownWorld=0;
  if(tableCount){InterlockedIncrement(&tableSequence);MemoryBarrier();tableCount=0;MemoryBarrier();InterlockedIncrement(&tableSequence);}listed.clear();return;}
 if(world!=knownWorld){   // a level change or a load: the old fires went with their scene
  knownWorld=world;lit.clear();attached.clear();orders.clear();
  std::string saved;AcquireSRWLockShared(&savedata::storeLock);const auto f=savedata::store.find("#fires");if(f!=savedata::store.end())saved=f->second;ReleaseSRWLockShared(&savedata::storeLock);
  stale.clear();for(size_t i=0;i+12<=saved.size()&&stale.size()<256;i+=12){std::array<float,3> p{};std::memcpy(p.data(),saved.data()+i,12);stale.push_back(p);}
  remember();
 }
 if(++scanTick%10==0)scan(world);
 if(!possible())return;
 for(const auto& w:wanted){Order o;o.owner=w.owner;o.name=w.name;o.kind=w.kind;o.at=w.at;o.size=int(std::lround(w.size*100.f));o.scale=w.size;o.target=uintptr_t(w.handle);o.hand=w.hand;
  if(orders.size()<64)orders.push_back(o);}
 if(jobDone&&!busy.load()){jobDone=false;
  if(job.kind==light&&jobResult){Lit l;l.owner=job.owner;l.name=job.name;l.world=job.world;l.sector=job.sector;l.node=jobResult;l.at=job.at;lit.push_back(l);remember();}
  else if(job.kind==light)host.report(job.owner.c_str(),"warn",("light_fire '"+job.name+"' refused: no placed object there, a fire already burns within 50 cm, or the game did not build it").c_str());
  else if(job.kind==attach&&jobResult){Attached a;a.owner=job.owner;a.name=job.name;a.world=job.world;a.target=job.target;a.node=jobResult;attached.push_back(a);}
  else if(job.kind==attach)host.report(job.owner.c_str(),"warn",("attach_fire '"+job.name+"' refused: the target left, or the game did not build the fire").c_str());
 }
 if(busy.load())return;
 // A save's leftover mod fires go first, one removal per job.
 while(!stale.empty()){const auto at=stale.back();stale.pop_back();std::vector<uintptr_t> all;if(!sectors(world,all))break;
  for(const auto sector:all){std::vector<uintptr_t> list;if(!fires(sector,list))continue;
   for(const auto n:list){std::array<float,3> p{};if(m.get(n+0x64,p)&&worlddevices::distance(p,at)>=0&&worlddevices::distance(p,at)<30.f){
    job={};job.kind=putOut;job.world=world;job.target=n;busy=true;if(!nativecalls::enqueue(run)){busy=false;}return;}}}
 }
 while(!orders.empty()){
  Order o=orders.front();orders.pop_front();
  if(o.kind==light){
   if(owned(o.owner,o.name)||count(o.owner)>=24)continue;
   if(!target(o.at,o.world,o.sector,o.target)){host.report(o.owner.c_str(),"warn",("light_fire '"+o.name+"': no placed object within 60 cm").c_str());continue;}
   if(nearestFire(o.sector,o.at)<50.f)continue;
   std::array<float,3> p{};if(!m.get(o.target+0x64,p))continue;o.at=p;
  }else if(o.kind==putOut){
   auto it=std::find_if(lit.begin(),lit.end(),[&](const Lit& l){return l.owner==o.owner&&l.name==o.name;});if(it==lit.end())continue;
   std::vector<uintptr_t> list;const bool present=it->world==world&&fires(it->sector,list)&&std::find(list.begin(),list.end(),it->node)!=list.end();
   o.world=it->world;o.target=present?it->node:0;lit.erase(it);remember();if(!present)continue;
  }else if(o.kind==attach){
   if(!attachPossible)continue;bool taken=false;for(const auto& a:attached)taken=taken||(a.owner==o.owner&&a.name==o.name);if(taken||countAttached(o.owner)>=16)continue;
   // The target: the player (handle 0) or a live character of this pulse's roster; its scene node (+0x18).
   uintptr_t character=o.target;if(!character&&!m.get(base+npc::off::controlled,character))continue;
   const auto roster=captureCharacters();bool alive=false;
   if(roster&&roster->ready)for(const auto& c:roster->characters)if(c.nativeCharacter==character)alive=!(c.stateKnown&&c.dead);
   uintptr_t node=0;if(!alive)continue;
   if(o.hand>=0){uint32_t record=0;bool lit=false;if(!scripting::heldItem(m,base,character,o.hand,node,record,lit)||!node){host.report(o.owner.c_str(),"warn",("attach_fire '"+o.name+"': nothing in that hand").c_str());continue;}}
   else if(!m.get(character+0x18,node)||!node)continue;
   o.target=node;o.world=world;
  }else{
   auto it=std::find_if(attached.begin(),attached.end(),[&](const Attached& a){return a.owner==o.owner&&a.name==o.name;});if(it==attached.end())continue;
   o.world=it->world;o.sector=it->target;o.target=it->world==world?it->node:0;attached.erase(it);if(!o.target)continue;
  }
  job=o;busy=true;if(!nativecalls::enqueue(run))busy=false;return;
 }
}
}
