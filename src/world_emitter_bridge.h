#pragma once
namespace worldemitters {
using Signal=void(*)(uintptr_t,uint32_t);
using Queue=void(*)(uintptr_t,char(*)(void*),void*);
using Fire=uintptr_t(*)(uintptr_t,int);
using Torch=uintptr_t(*)(uintptr_t,uintptr_t);using Remove=void(*)(uintptr_t,uintptr_t,int);
using Publish=void(*)(uintptr_t,uintptr_t*,int);using Pose=void(*)(uintptr_t);
static Signal signal=nullptr;
static Fire fire=nullptr;
static Torch torch=nullptr;static Remove remove=nullptr;
static Publish publish=nullptr;static Pose pose=nullptr;
static Queue enqueue=nullptr;
static bool possible=false,firePossible=false,extinguishPossible=false,torchPossible=false;
static std::atomic<bool> pending{false},failed{false},cancelled{false};
struct TreeNode {uintptr_t node=0,parent=0,sector=0,body=0,list=0,mesh=0,parts=0,owner=0;uint32_t flags=0,control=0;int children=-1,meshes=-1,groups=0;std::array<float,12> pose{};};
inline bool operator==(const TreeNode& a,const TreeNode& b){return a.node==b.node&&a.parent==b.parent&&a.sector==b.sector&&a.body==b.body&&a.list==b.list&&a.mesh==b.mesh&&a.parts==b.parts&&a.flags==b.flags&&a.children==b.children&&a.meshes==b.meshes&&a.owner==b.owner&&a.control==b.control&&a.groups==b.groups&&a.pose==b.pose;}
struct State {
 uintptr_t world=0,scene=0,player=0,node=0,sector=0,effect=0,body=0,controller=0;
 uint32_t item=0,state=0,size=0,flags=0,sceneNodes=0;bool active=false,isFire=false,isTorch=false;
 std::array<TreeNode,32> tree{};size_t treeSize=0;
 std::array<uint8_t,256> definition{};
 std::array<float,3> position{},direction{};
};
struct Request {State before;bool active=false;};
// One pinned slot outlives addon reload; the native queue does not free payloads.
static Request request;
static bool queueAvailable(){
 const unsigned char drainPrefix[]={0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x48,0x8d,0x64,0x24,0xd0,0x48,0x89,0xcb,0x48};
 const unsigned char queuePrefix[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0x48,0x89,0xd6,0x4c,0x89};
 return gamebridge::operation(base+0x1a890,queuePrefix,sizeof queuePrefix)&&gamebridge::operation(base+0x1ac10,drainPrefix,sizeof drainPrefix);
}
static bool available(){
 const unsigned char signalPrefix[]={0x53,0x48,0x8d,0x64,0x24,0xd0,0x48,0x89,0xcb,0x44,0x8b,0x83,0x18,0x01,0x00,0x00};
 const unsigned char createPrefix[]={0x53,0x57,0x56,0x41,0x54,0x48,0x8d,0x64,0x24,0xb8,0x66,0x0f,0x7f,0x74,0x24,0x20};
 return queueAvailable()&&gamebridge::operation(base+0x149bf0,signalPrefix,sizeof signalPrefix)&&gamebridge::operation(base+0x1a7160,createPrefix,sizeof createPrefix);
}
static bool fireInfrastructureAvailable(){
 const unsigned char factoryPrefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0xa4,0x24,0xc0,0xfe,0xff,0xff,0x48,0x89,0x4d,0xf8};
 const unsigned char bodyPrefix[]={0x53,0x57,0x56,0x41,0x54,0x48,0x8d,0x64,0x24,0xb8,0x66,0x0f,0x7f,0x74,0x24,0x30};
 const unsigned char publishPrefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0xa0,0x48,0x89,0x5d,0xc8,0x48,0x89,0x75};
 return queueAvailable()&&gamebridge::operation(base+0xf2a70,factoryPrefix,sizeof factoryPrefix)&&
  gamebridge::operation(base+0xf1920,bodyPrefix,sizeof bodyPrefix)&&gamebridge::operation(base+0x5a420,publishPrefix,sizeof publishPrefix);
}
static bool availableFire(){
 const unsigned char firePrefix[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xd0,0x48,0x89,0xcb,0x48,0x8b,0x83,0x30,0x01};
 return fireInfrastructureAvailable()&&gamebridge::operation(base+0x1494d0,firePrefix,sizeof firePrefix);
}
static bool availableFlame(bool lighting){
 const unsigned char torchPrefix[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xb0,0x66,0x0f,0x7f,0x74,0x24,0x40,0x48,0x89};
 const unsigned char removePrefix[]={0x53,0x56,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xd3,0x44,0x89,0xc6,0x44,0x8b,0x81};
 const unsigned char finderPrefix[]={0x53,0x57,0x56,0x41,0x54,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xcb,0x31,0xf6,0x8b};
 const unsigned char replacePrefix[]={0x53,0x57,0x56,0x41,0x54,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xcb,0x49,0x89,0xd4};
 const unsigned char fadePrefix[]={0x8b,0x41,0x08,0x83,0xe0,0x5f,0x83,0xf8,0x09,0x75,0x3d,0x83,0xb9,0xc8,0x01,0x00};
 if(!queueAvailable()||!gamebridge::operation(base+0x5bed0,removePrefix,sizeof removePrefix))return false;
 return !lighting||(fireInfrastructureAvailable()&&gamebridge::operation(base+0x149740,torchPrefix,sizeof torchPrefix)&&gamebridge::operation(base+0x1496d0,finderPrefix,sizeof finderPrefix)&&
  gamebridge::operation(base+0xa1730,replacePrefix,sizeof replacePrefix)&&gamebridge::operation(base+0x1495e0,fadePrefix,sizeof fadePrefix));
}
static bool removalReady(uintptr_t world,uint32_t& slot){const scripting::CharacterMemory m{copyWorldMemory};uint32_t read=0;
 return m.get(world+0x3d0,read)&&m.get(world+0x3d4,slot)&&read<256&&slot<256&&((slot+1)%256)!=read&&readable(world+0x3d8+size_t(slot)*0x18,0x18);}
static bool idle(){if(cancelled.exchange(false))host.report("runtime","warn","Queued emitter request cancelled: native ownership changed before execution");if(failed.load()){if(!inventorybridge::broken)inventorybridge::fault();return false;}return !pending.load();}
static bool queueIdle(){
 const scripting::CharacterMemory m{copyWorldMemory};uint32_t read=0,write=0;
 return m.get(base+0x499990,read)&&m.get(base+0x499994,write)&&read<128&&write<128&&read==write;
}
static bool unique(State& s){
 const scripting::CharacterMemory m{copyWorldMemory};int last=-1;uintptr_t list=0;size_t total=0,found=0;
 if(!m.get(s.world+0x3c4,last)||last<0||last>=512||!m.get(s.world+0x3c8,list))return false;
 for(int i=0;i<=last;++i){uintptr_t sector=0;if(!m.get(list+size_t(i)*8,sector)||!sector)return false;std::vector<uintptr_t> nodes;
  if(!worlddevices::children(sector,nodes,8192)||(total+=nodes.size())>8192)return false;
  for(const auto node:nodes){uint32_t id=0;if(!node||!m.get(node+0x180,id))return false;if(id==s.item){if(node!=s.node)return false;++found;}}
 }s.sceneNodes=uint32_t(total);return found==1;
}
struct FireLease {uintptr_t world=0,scene=0,sector=0,source=0,node=0;uint32_t item=0;};
static std::array<FireLease,64> fires{};
constexpr float maxFireSize=2.0f;
static bool tree(State& s,uintptr_t root,bool lighting){
 const scripting::CharacterMemory m{copyWorldMemory};std::vector<uintptr_t> stack{root};s.treeSize=0;size_t markers=0;
 while(!stack.empty()){
  const auto at=stack.back();stack.pop_back();if(!at||s.treeSize==s.tree.size())return false;
  for(size_t i=0;i<s.treeSize;++i)if(s.tree[i].node==at)return false;
  TreeNode row;row.node=at;
  if(!m.get(at+8,row.flags)||(row.flags&0x82000000)||!m.get(at+0x128,row.parent)||!m.get(at+0x130,row.sector)||row.sector!=s.sector||!m.get(at+0x160,row.body)||
   !m.get(at+0x118,row.children)||row.children< -1||row.children>=32||!m.get(at+0x120,row.list)||(row.children>=0&&!readable(row.list,size_t(row.children+1)*8)))return false;
  if(at!=root){bool parent=false;for(size_t i=0;i<s.treeSize;++i)parent|=s.tree[i].node==row.parent;if(!parent)return false;}
  if(row.body){uintptr_t owner=0;if(lighting||!scripting::classMatches(row.body,base+0x30e9e8,copyWorldMemory)||!m.get(row.body+0x18,owner)||owner!=at)return false;}
  if(lighting){
   if(!m.get(at+0x70,row.pose))return false;for(float value:row.pose)if(!std::isfinite(value))return false;
   if((row.flags&0xfff)==1){uint32_t slot=0;if(at==root||(row.flags&0x7000)||++markers>1||row.children!=-1||!m.get(at+0x138,row.owner)||row.owner!=at||!readable(row.owner,0x1c0)||
    !m.get(at+0x168,row.groups)||row.groups>0||!m.get(at+0xc,row.control)||(row.control&1)||!removalReady(s.world,slot))return false;}
   if((row.flags&0x5f)==9){if(!readable(at,0x248)||!m.get(at+0x1c8,row.meshes)||row.meshes< -1||row.meshes>=128||!m.get(at+0x1d0,row.parts)||
    (row.meshes>=0&&(!readable(row.parts,size_t(row.meshes+1)*8)||!m.get(row.parts,row.mesh)||!row.mesh||!readable(row.mesh,0xf8))))return false;}
  }
  s.tree[s.treeSize++]=row;
  for(int i=row.children;i>=0;--i){uintptr_t child=0;if(!m.get(row.list+size_t(i)*8,child)||!child)return false;stack.push_back(child);}
 }return true;
}
static bool fireNodes(const State& s,std::vector<uintptr_t>& nodes,std::vector<uintptr_t>& effects){
 const scripting::CharacterMemory m{copyWorldMemory};if(!worlddevices::children(s.sector,nodes,8192))return false;
 for(const auto node:nodes){uintptr_t parent=0,sector=0,body=0,owner=0;
  if(!node||!m.get(node+0x160,body))return false;if(!body||!scripting::classMatches(body,base+0x30e9e8,copyWorldMemory))continue;
  if(!m.get(node+0x128,parent)||parent!=s.sector||!m.get(node+0x130,sector)||sector!=s.sector||!m.get(body+0x18,owner)||owner!=node)return false;effects.push_back(node);
 }return true;
}
static bool fireEffect(State& s){
 std::vector<uintptr_t> nodes,effects;if(!fireNodes(s,nodes,effects))return false;bool free=false;
 for(auto& lease:fires){
  if(lease.node&&(lease.world!=s.world||lease.scene!=s.scene||(lease.sector==s.sector&&std::find(nodes.begin(),nodes.end(),lease.node)==nodes.end())))lease={};
  if(!lease.node){free=true;continue;}
  if(lease.source==s.node&&lease.item==s.item){if(lease.sector!=s.sector||std::find(effects.begin(),effects.end(),lease.node)==effects.end())return false;s.effect=lease.node;}
 }
 const scripting::CharacterMemory m{copyWorldMemory};
 for(const auto node:effects){std::array<float,3> position{};if(!m.get(node+0x64,position))return false;const float distance=worlddevices::distance(position,s.position);if(distance<0)return false;if(distance<1){if(s.effect&&s.effect!=node)return false;s.effect=node;}}
 if(s.effect==s.node)return false;
 if(s.effect){if(!m.get(s.effect+0x160,s.body)||!m.get(s.effect+8,s.flags)||!tree(s,s.effect,false))return false;}
 return s.effect||free;
}
static bool torchEffect(State& s){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t sourceBody=0;uint32_t sourceFlags=0;
 if(!m.get(s.node+0x160,sourceBody)||sourceBody||!m.get(s.node+8,sourceFlags)||(sourceFlags&0xfff)==1)return false;
 std::vector<uintptr_t> stack{s.node},seen;
 while(!stack.empty()){const auto node=stack.back();stack.pop_back();if(!node||seen.size()==32||std::find(seen.begin(),seen.end(),node)!=seen.end())return false;seen.push_back(node);
  uintptr_t body=0,parent=0,sector=0;if(!m.get(node+0x160,body)||!m.get(node+0x128,parent)||!m.get(node+0x130,sector)||sector!=s.sector)return false;
  if(node!=s.node&&std::find(seen.begin(),seen.end(),parent)==seen.end())return false;
  if(body&&scripting::classMatches(body,base+0x30e9e8,copyWorldMemory)){if(s.effect)return false;s.effect=node;s.body=body;continue;}
  std::vector<uintptr_t> children;if(!worlddevices::children(node,children,32))return false;for(auto i=children.rbegin();i!=children.rend();++i)stack.push_back(*i);
 }return s.effect?tree(s,s.effect,false):tree(s,s.node,true);
}
static bool effect(State& s){
 const scripting::CharacterMemory m{copyWorldMemory};std::vector<uintptr_t> nodes;if(!worlddevices::children(s.node,nodes))return false;
 for(const auto node:nodes){uintptr_t parent=0,sector=0,body=0;uint32_t type=0;
  if(!node||!m.get(node+0x128,parent)||parent!=s.node||!m.get(node+0x130,sector)||sector!=s.sector||!m.get(node+0x160,body))return false;
  if(!body)continue;if(!m.get(body+0xc,type))return false;if((type&0xff000000)!=0x21000000)continue;
  uintptr_t owner=0;if(s.effect||!scripting::classMatches(body,base+0x323da0,copyWorldMemory)||!m.get(body+0x18,owner)||owner!=node||
   !m.get(body+0x198,s.flags)||!m.get(body+0xa0,s.controller)||!s.controller||!readable(s.controller,0x38))return false;
  s.effect=node;s.body=body;
 }return true;
}
static bool parameters(const State& s){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t renderer=0,resources=0,audio=0;double scale=0,step=0;
 if(!m.get(s.scene+0x3b8,renderer)||!renderer||!readable(renderer,0x70)||!m.get(renderer+0x10,resources)||!resources||!readable(resources,0x18)||
  !m.get(base+0x4eec90,audio)||!audio||!readable(audio,0x18))return false;
 if(s.isTorch){uint16_t size=0;float low=0,high=0;std::memcpy(&size,s.definition.data()+0x10,2);
  if(!m.get(base+0x31b0b0,scale)||!std::isfinite(scale)||scale<=0||!m.get(base+0x31b0c0,low)||!m.get(base+0x31b0c8,high)||!std::isfinite(low)||!std::isfinite(high)||low<=0||high<low)return false;
  const float actual=float(double(size)*scale);return std::isfinite(actual)&&(std::min)((std::max)(actual,low),high)<=maxFireSize;
 }
 if(s.isFire){uint16_t size=0;float fallback=0;std::memcpy(&size,s.definition.data()+0xc,2);
  if(!m.get(base+0x31b0b0,scale)||!std::isfinite(scale)||scale<=0||!m.get(base+0x31b0b8,fallback)||!std::isfinite(fallback)||fallback<=0)return false;
  const float actual=size?float(double(size)*scale):fallback;return std::isfinite(actual)&&actual>0&&actual<=(std::min)(fallback,maxFireSize);
 }
 if(!m.get(base+0x3243f0,scale)||!m.get(base+0x324570,step)||!std::isfinite(scale)||!std::isfinite(step)||scale<=0||step<=0)return false;
 uint16_t radius=0,length=0;int16_t strands=0;std::memcpy(&radius,s.definition.data()+0x10,2);std::memcpy(&length,s.definition.data()+0x12,2);std::memcpy(&strands,s.definition.data()+0x14,2);
 const float scaled=float(double(radius)*scale);const double segments=double(float(length)/(scaled*step));
 if(!std::isfinite(scaled)||scaled<=0||!std::isfinite(segments)||segments<0||segments>4096||strands<0||strands>128)return false;
 const double count=segments<1.5?10:std::floor(segments+0.5);return (count+1)*(strands?strands:10)<=16384;
}
static bool capture(uintptr_t node,uintptr_t world,uintptr_t scene,uintptr_t player,State& s,bool isFire=false,bool isTorch=false){
 const scripting::CharacterMemory m{copyWorldMemory};s={};s.node=node;s.world=world;s.scene=scene;s.player=player;s.isFire=isFire;s.isTorch=isTorch;uintptr_t parent=0,owner=0;uint32_t flags=0;
 if(!m.get(node+8,flags)||(flags&0x7000)!=0x4000||!(flags&0x40000)||(flags&0x82000000)||!m.get(node+0x180,s.item)||(s.item&0xc0000000u)!=0x80000000u||
  !m.get(node+0x184,s.state)||(s.state&0xc80100)||!m.get(node+0x130,s.sector)||!m.get(node+0x128,parent)||parent!=s.sector||!m.get(s.sector+0x220,owner)||owner!=world||
  !m.get(node+0x64,s.position)||!m.get(node+0x58,s.direction)||!unique(s))return false;
 for(float value:s.position)if(!std::isfinite(value))return false;float direction=0;for(float value:s.direction){if(!std::isfinite(value))return false;direction+=value*value;}if(!std::isfinite(direction)||direction<0.001f||direction>4)return false;
 const auto data=inventorybridge::chunk(s.item,0x10000);uint32_t tag=0;
 if(!data||!m.get(data,tag)||tag!=0x10000||!m.get(data+4,s.size)||s.size<(isFire?16u:24u)||s.size>s.definition.size()||!copyWorldMemory(data,s.definition.data(),s.size)||s.definition[8]!=(isTorch?8:6)||s.definition[9]!=((isFire||isTorch)?12:13))return false;
 uint16_t bits=0;std::memcpy(&bits,s.definition.data()+0xc,2);s.active=(bits&1)!=0;
 return parameters(s)&&(isTorch?torchEffect(s):isFire?fireEffect(s):effect(s));
}
static bool same(const State& before,State& now){
 return capture(before.node,before.world,before.scene,before.player,now,before.isFire,before.isTorch)&&now.sector==before.sector&&now.item==before.item&&now.state==before.state&&now.size==before.size&&now.definition==before.definition&&
  now.position==before.position&&now.direction==before.direction&&now.effect==before.effect&&now.body==before.body&&now.controller==before.controller&&now.flags==before.flags&&now.treeSize==before.treeSize&&now.tree==before.tree;
}
static bool settled(const State& s){return (s.isFire||s.isTorch)?true:(!(s.flags&1)&&s.active==bool(s.effect));}
static bool context(const State& s){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t world=0,scene=0,player=0,node=0,sector=0;std::array<float,3> position{};uint32_t action=0;int request=0,motion=0;
 return m.get(base+npc::off::world,world)&&world==s.world&&m.get(base+0x49b300,scene)&&scene==s.scene&&m.get(base+npc::off::controlled,player)&&player==s.player&&
  m.get(player+0x18,node)&&node&&m.get(node+0x130,sector)&&sector==s.sector&&m.get(player+npc::off::root,position)&&worlditems::withinReach(position,s.position)&&
  m.get(player+npc::off::action,action)&&!action&&m.get(player+0xdc0,request)&&request!=0x1f&&m.get(player+npc::off::motion,motion)&&motion!=0x1f;
}
static void run(){
 try{
  const auto before=request.before;State now;
  // Stale scene, record or effect ownership cancels before any native write.
  if(!context(before)||!same(before,now)||!settled(now)){cancelled.store(true);return;}
  if((before.isFire||before.isTorch)&&!request.active){
   uint32_t slot=0;if(!before.effect||!removalReady(before.world,slot)){cancelled.store(true);return;}
   remove(before.world,before.effect,1);const scripting::CharacterMemory m{copyWorldMemory};uintptr_t node=0,sector=0,dead=0;uint32_t flags=0,write=0;int mode=0;
   const auto at=before.world+0x3d8+size_t(slot)*0x18;
   if(!m.get(at,node)||node!=before.effect||!m.get(at+8,sector)||sector!=before.sector||!m.get(at+16,mode)||mode!=1||!m.get(before.world+0x3d4,write)||write!=(slot+1)%256||
    !m.get(before.effect+8,flags)||!(flags&0x80000000)||(flags&0x40000)||!m.get(base+0x49b2e0,dead)||!m.get(before.effect+0x130,sector)||sector!=dead)failed.store(true);
   uint32_t item=0,state=0;std::array<uint8_t,256> definition{};const auto data=inventorybridge::chunk(before.item,0x10000);
   if(!m.get(before.node+0x180,item)||item!=before.item||!m.get(before.node+0x184,state)||state!=before.state||!copyWorldMemory(data,definition.data(),before.size)||definition!=before.definition)failed.store(true);
   return;
  }
  if(before.isTorch){
   const auto data=inventorybridge::chunk(before.item,0x10000);uintptr_t created=torch(before.node,data+8);const scripting::CharacterMemory m{copyWorldMemory};uintptr_t owner=0,sector=0,body=0,back=0;uint32_t flags=0;
   if(!created||!m.get(created+0x128,owner)||owner!=before.node||!m.get(created+0x130,sector)||sector!=before.sector||!m.get(created+0x160,body)||!scripting::classMatches(body,base+0x30e9e8,copyWorldMemory)||!m.get(body+0x18,back)||back!=created){failed.store(true);return;}
   State checked=before;if(!tree(checked,created,false)){failed.store(true);return;}pose(created);publish(before.world,&created,0);
   State after;if(!capture(before.node,before.world,before.scene,before.player,after,false,true)||after.effect!=created||after.item!=before.item||after.state!=before.state||after.definition!=before.definition||!m.get(created+8,flags)||(flags&0x49000)!=0x49000)failed.store(true);
   return;
  }
  if(before.isFire){
   std::vector<uintptr_t> prior,effects;if(!fireNodes(before,prior,effects)){cancelled.store(true);return;}
   uint16_t size=0;std::memcpy(&size,before.definition.data()+0xc,2);fire(before.node,size);
   std::vector<uintptr_t> after,created;if(!fireNodes(before,after,created)){failed.store(true);return;}uintptr_t published=0;
   for(const auto node:created)if(std::find(prior.begin(),prior.end(),node)==prior.end()){if(published){failed.store(true);return;}published=node;}
   State source;uint32_t flags=0;std::array<float,3> position{};
   if(!published||!scripting::CharacterMemory{copyWorldMemory}.get(published+8,flags)||(flags&0x49000)!=0x49000||
    !scripting::CharacterMemory{copyWorldMemory}.get(published+0x64,position)||worlddevices::distance(position,before.position)<0||worlddevices::distance(position,before.position)>=1||
    !capture(before.node,before.world,before.scene,before.player,source,true)||source.item!=before.item||source.state!=before.state||source.definition!=before.definition){failed.store(true);return;}
   for(auto& lease:fires)if(!lease.node){lease={before.world,before.scene,before.sector,before.node,published,before.item};return;}
   failed.store(true);return;
  }
  const auto data=inventorybridge::chunk(before.item,0x10000);uint16_t bits=0;std::memcpy(&bits,before.definition.data()+0xc,2);
  bits=uint16_t((bits&~1u)|(request.active?1u:0u));std::memcpy((void*)(data+0xc),&bits,2);
  signal(before.node,request.active?1u:0u);
  State after;if(!capture(before.node,before.world,before.scene,before.player,after)){failed.store(true);return;}
  auto expected=before.definition;std::memcpy(expected.data()+0xc,&bits,2);
  if(after.item!=before.item||after.state!=before.state||after.definition!=expected||after.active!=request.active||
   (request.active?(!after.effect||(after.flags&1)):(after.effect!=before.effect||after.body!=before.body||after.flags!=1)))failed.store(true);
  if(!request.active){int remaining=0;if(!scripting::CharacterMemory{copyWorldMemory}.get(after.controller+0x30,remaining)||remaining!=10)failed.store(true);}
 }catch(...){failed.store(true);}
}
static char queued(void* payload){
 __try{if(payload!=&request)failed.store(true);else run();}__except(EXCEPTION_EXECUTE_HANDLER){failed.store(true);}
 pending.store(false);return 0;
}
static bool submit(const State& s,bool active){
 if((s.isTorch?(!torchPossible||!torch||!publish||!pose):s.isFire?(active?(!firePossible||!fire):(!extinguishPossible||!remove)):(!possible||!signal))||!enqueue||!idle()||!queueIdle()||!settled(s))return false;
 if(s.isFire||s.isTorch){if(active?(s.effect||s.sceneNodes>=8192):!s.effect)return false;if(!active){uint32_t slot=0;if(!extinguishPossible||!remove||!removalReady(s.world,slot))return false;}}
 else if(s.active==active)return true;
 request={s,active};pending.store(true);
 __try{enqueue(base+0x499990,queued,&request);return true;}__except(EXCEPTION_EXECUTE_HANDLER){failed.store(true);return false;}
}
}
