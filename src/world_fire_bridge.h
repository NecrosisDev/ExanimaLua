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
namespace worldfires {
struct Lit {std::string owner,name;uintptr_t world=0,sector=0,node=0;std::array<float,3> at{};};
struct Order {std::string owner,name;bool light=false;std::array<float,3> at{};int size=100;uintptr_t world=0,sector=0,target=0;};
static std::vector<Lit> lit;static std::deque<Order> orders;static std::vector<std::array<float,3>> stale;
static std::atomic<bool> busy{false};static Order job;static uintptr_t jobResult=0;static bool jobDone=false;
static uintptr_t knownWorld=0;
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
// The job (game thread): light, or remove one fire node through the removal queue.
static void work(){
 jobResult=0;
 if(job.light){
  std::vector<uintptr_t> before,after;
  if(fires(job.sector,before)&&nearestFire(job.sector,job.at)>=50.f){
   worldemitters::fire(job.target,job.size);
   if(fires(job.sector,after))for(const auto n:after)if(std::find(before.begin(),before.end(),n)==before.end()){if(jobResult){jobResult=0;break;}jobResult=n;}
  }
 }else{
  uint32_t slot=0;if(job.target&&worldemitters::removalReady(job.world,slot)){worldemitters::remove(job.world,job.target,1);jobResult=job.target;}
 }
}
static unsigned char run(void*){
 __try{work();}__except(EXCEPTION_EXECUTE_HANDLER){jobResult=0;}
 jobDone=true;busy=false;return 0;
}
static bool owned(const std::string& owner,const std::string& name){for(const auto& l:lit)if(l.owner==owner&&l.name==name)return true;return false;}
static size_t count(const std::string& owner){size_t n=0;for(const auto& l:lit)n+=l.owner==owner;return n;}
// The session pulse (game thread side of the presentation pulse): collect, settle the last job, start the next.
static void pulse(scripting::Host& host,bool session){
 std::vector<scripting::FireOrder> wanted;host.collectFires(wanted);
 uintptr_t world=0;const scripting::CharacterMemory m{copyWorldMemory};
 if(!session||!m.get(base+npc::off::world,world)||!world){if(!busy.load())orders.clear();knownWorld=0;return;}
 if(world!=knownWorld){   // a level change or a load: the old fires went with their scene
  knownWorld=world;lit.clear();orders.clear();
  std::string saved;AcquireSRWLockShared(&savedata::storeLock);const auto f=savedata::store.find("#fires");if(f!=savedata::store.end())saved=f->second;ReleaseSRWLockShared(&savedata::storeLock);
  stale.clear();for(size_t i=0;i+12<=saved.size()&&stale.size()<256;i+=12){std::array<float,3> p{};std::memcpy(p.data(),saved.data()+i,12);stale.push_back(p);}
  remember();
 }
 if(!possible())return;
 for(const auto& w:wanted){Order o;o.owner=w.owner;o.name=w.name;o.light=w.light;o.at=w.at;o.size=int(std::lround(w.size*100.f));if(orders.size()<64)orders.push_back(o);}
 if(jobDone&&!busy.load()){jobDone=false;
  if(job.light&&jobResult){Lit l;l.owner=job.owner;l.name=job.name;l.world=job.world;l.sector=job.sector;l.node=jobResult;l.at=job.at;lit.push_back(l);remember();}
  else if(job.light)host.report(job.owner.c_str(),"warn",("light_fire '"+job.name+"' refused: no placed object there, a fire already burns within 50 cm, or the game did not build it").c_str());
 }
 if(busy.load())return;
 // A save's leftover mod fires go first, one removal per job.
 while(!stale.empty()){const auto at=stale.back();stale.pop_back();std::vector<uintptr_t> all;if(!sectors(world,all))break;
  for(const auto sector:all){std::vector<uintptr_t> list;if(!fires(sector,list))continue;
   for(const auto n:list){std::array<float,3> p{};if(m.get(n+0x64,p)&&worlddevices::distance(p,at)>=0&&worlddevices::distance(p,at)<30.f){
    job={};job.light=false;job.world=world;job.target=n;busy=true;if(!nativecalls::enqueue(run)){busy=false;}return;}}}
 }
 while(!orders.empty()){
  Order o=orders.front();orders.pop_front();
  if(o.light){
   if(owned(o.owner,o.name)||count(o.owner)>=24)continue;
   if(!target(o.at,o.world,o.sector,o.target)){host.report(o.owner.c_str(),"warn",("light_fire '"+o.name+"': no placed object within 60 cm").c_str());continue;}
   if(nearestFire(o.sector,o.at)<50.f)continue;
   std::array<float,3> p{};if(!m.get(o.target+0x64,p))continue;o.at=p;
  }else{
   auto it=std::find_if(lit.begin(),lit.end(),[&](const Lit& l){return l.owner==o.owner&&l.name==o.name;});if(it==lit.end())continue;
   std::vector<uintptr_t> list;const bool present=it->world==world&&fires(it->sector,list)&&std::find(list.begin(),list.end(),it->node)!=list.end();
   o.world=it->world;o.target=present?it->node:0;lit.erase(it);remember();if(!present)continue;
  }
  job=o;busy=true;if(!nativecalls::enqueue(run))busy=false;return;
 }
}
}
