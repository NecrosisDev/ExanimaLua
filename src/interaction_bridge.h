#pragma once
namespace interactions {
using Effect=void(*)(void*,uintptr_t);using Unlock=void(*)(uintptr_t);using Open=uintptr_t(*)(uint32_t);
using Trigger=void(*)(uintptr_t,uint32_t,uint32_t,uint32_t);using Find=uintptr_t(*)(uintptr_t,uint32_t);using Closed=char(*)(uint32_t);
static Effect effect=nullptr;static Unlock unlock=nullptr;static Open open=nullptr;static Trigger trigger=nullptr;static Find find=nullptr;static Closed isOpen=nullptr;
static bool consumePossible=false,keyPossible=false,openPossible=false,buttonPossible=false;
using Wake=void(*)(uintptr_t,int);using DoorCommand=void(*)(uintptr_t,uint32_t,uint32_t);
static Wake wake=nullptr;static Unlock unlatch=nullptr;static DoorCommand doorCommand=nullptr;
static bool leverPossible=false,doorPossible=false;
static bool slidingPossible=false,pairedPossible=false,lightLeverPossible=false;
struct LeverState {float start=0,travel=0,angle=0;int index=0;uint8_t overrideTarget=0;uint32_t flags=0;uintptr_t light=0;float lightValue=0;};
struct DoorLeaf {uintptr_t child=0,body=0,tetras=0;int count=0,index=0,mode=0;float speed=0;bool sliding=false,mechanical=false,closed=false;std::array<uint8_t,0x44> bytes{};std::array<uint8_t,0x68> tetra{};};
struct Target {
 scripting::InteractionView view;uintptr_t node=0,child=0,body=0,sector=0,world=0,scene=0,player=0;
 uint32_t item=0,state=0;std::vector<uint8_t> definition;LeverState lever;
 std::vector<DoorLeaf> doors;
 worlddevices::State device;
 worldemitters::State emitter;
};
static std::vector<Target> targets;static uint64_t nextTicket=0;
static bool available(scripting::InventoryOp op){
 const unsigned char effectPrefix[]={0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x48,0x8d,0x64,0x24,0xd0,0x48,0x89,0xce,0x48};
 const unsigned char unlockPrefix[]={0x53,0x48,0x8d,0x64,0x24,0xd0,0x48,0x89,0xcb,0xc7,0x44,0x24,0x28,0x00,0x00,0x00};
 const unsigned char openPrefix[]={0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x48,0x8d,0x64,0x24,0xe0,0x89,0xcf,0x4d,0x31};
 const unsigned char triggerPrefix[]={0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x41,0x56,0x48,0x8d,0x64,0x24,0xb8,0x48,0x89};
 const unsigned char findPrefix[]={0x53,0x57,0x56,0x31,0xc0,0x44,0x8b,0x99,0xc4,0x03,0x00,0x00,0x45,0x85,0xdb,0x0f};
 const unsigned char openCheckPrefix[]={0x30,0xc0,0xba,0xff,0xff,0xff,0xff,0x90,0x83,0xc2,0x01,0x41,0x89,0xd0,0x4c,0x8d};
 const unsigned char wakePrefix[]={0x53,0x57,0x56,0x41,0x54,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xcb,0x89,0xd6,0x80};
 const unsigned char leverReadPrefix[]={0x53,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0x48,0x8b,0x43,0x18,0x48,0x8b,0x80};
 const unsigned char leverTimerPrefix[]={0x53,0x48,0x8d,0x64,0x24,0xe0,0x30,0xdb,0x48,0x8b,0x41,0x18,0x8b,0x40,0x08,0x25};
 const unsigned char leverUpdatePrefix[]={0x53,0x56,0x48,0x8d,0x64,0x24,0x98,0x66,0x0f,0x7f,0x74,0x24,0x40,0x66,0x44,0x0f};
 const unsigned char commandPrefix[]={0x48,0x89,0xc8,0x89,0x90,0x28,0x05,0x00,0x00,0x44,0x89,0x80,0x2c,0x05,0x00,0x00};
 const unsigned char unlatchPrefix[]={0x53,0x48,0x8d,0x64,0x24,0xc0,0x48,0x89,0xcb,0xc6,0x83,0x04,0x06,0x00,0x00,0x00};
 const unsigned char doorUpdatePrefix[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0x80,0x66,0x0f,0x7f,0x74,0x24,0x50,0x66,0x44};
 if(op==scripting::InventoryOp::leverPosition)return gamebridge::operation(base+0x8ade0,wakePrefix,sizeof wakePrefix)&&gamebridge::operation(base+0x14b380,leverReadPrefix,sizeof leverReadPrefix)&&gamebridge::operation(base+0x14b410,leverTimerPrefix,sizeof leverTimerPrefix)&&gamebridge::operation(base+0x14b630,leverUpdatePrefix,sizeof leverUpdatePrefix);
 if(op==scripting::InventoryOp::doorOpen)return gamebridge::operation(base+0x8ade0,wakePrefix,sizeof wakePrefix)&&gamebridge::operation(base+0x14cb00,commandPrefix,sizeof commandPrefix)&&gamebridge::operation(base+0x14cc30,unlatchPrefix,sizeof unlatchPrefix)&&gamebridge::operation(base+0x14d680,doorUpdatePrefix,sizeof doorUpdatePrefix);
 if(op==scripting::InventoryOp::itemUse)return gamebridge::operation(base+0x11c310,effectPrefix,sizeof effectPrefix);
 if(op==scripting::InventoryOp::useItemOnWorld)return gamebridge::operation(base+0x14cb60,unlockPrefix,sizeof unlockPrefix);
 if(op==scripting::InventoryOp::worldPress)return gamebridge::operation(base+0x14a3e0,triggerPrefix,sizeof triggerPrefix)&&gamebridge::operation(base+0x101830,findPrefix,sizeof findPrefix);
 return gamebridge::operation(base+0x10e7c0,openPrefix,sizeof openPrefix)&&gamebridge::operation(base+0x1327f0,openCheckPrefix,sizeof openCheckPrefix);
}
static bool definition(uint32_t id,uint32_t tag,size_t minimum,std::vector<uint8_t>& out){
 const scripting::CharacterMemory m{copyWorldMemory};const auto at=inventorybridge::chunk(id,tag);uint32_t size=0,type=0;
 if(!at||!m.get(at,type)||type!=tag||!m.get(at+4,size)||size<minimum||size>1024*1024)return false;
 out.resize(size);return copyWorldMemory(at,out.data(),out.size());
}
enum class DeviceContract {sliding,paired,lightLever};
static bool availableDevice(DeviceContract kind){
 const unsigned char slidingPrefix[]={0x53,0x48,0x8d,0x64,0x24,0x90,0x66,0x0f,0x7f,0x74,0x24,0x50,0x66,0x44,0x0f,0x7f};
 const unsigned char doorConstructorPrefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0xa4,0x24,0x30,0xff,0xff,0xff,0x48,0x89,0x9d,0x58};
 const unsigned char pairStatePrefix[]={0x48,0x89,0xc8,0x88,0x90,0x01,0x06,0x00,0x00,0x80,0xb8,0x07,0x06,0x00,0x00,0x00};
 const unsigned char lightFinderPrefix[]={0x53,0x57,0x56,0x41,0x54,0x48,0x8d,0x64,0x24,0xd8,0x48,0x89,0xcb,0x8b,0x43,0x08};
 if(kind==DeviceContract::sliding)return gamebridge::operation(base+0x14ce50,slidingPrefix,sizeof slidingPrefix);
 if(kind==DeviceContract::paired)return gamebridge::operation(base+0x14bf20,doorConstructorPrefix,sizeof doorConstructorPrefix)&&gamebridge::operation(base+0x14cdf0,pairStatePrefix,sizeof pairStatePrefix);
 return gamebridge::operation(base+0x14b310,lightFinderPrefix,sizeof lightFinderPrefix);
}
static bool soundReady(){const scripting::CharacterMemory m{copyWorldMemory};uintptr_t audio=0;return m.get(base+0x4eec90,audio)&&audio&&readable(audio,0x18);}
static bool guiReady(){const scripting::CharacterMemory m{copyWorldMemory};uintptr_t gui=0,list=0;int count=0;
 return m.get(base+0x499790,gui)&&gui&&readable(gui,0xb8)&&m.get(gui+0xa8,count)&&count>=0&&count<=4096&&m.get(gui+0xb0,list)&&(!count||readable(list,size_t(count)*8));}
static bool worldIndex(uintptr_t world){
 const scripting::CharacterMemory m{copyWorldMemory};int last=-1;uintptr_t sectors=0;size_t total=0;
 if(!m.get(world+0x3c4,last)||last<0||last>=512||!m.get(world+0x3c8,sectors))return false;
 for(int i=0;i<=last;++i){uintptr_t sector=0,children=0;int childLast=-1;
  if(!m.get(sectors+size_t(i)*8,sector)||!sector||!m.get(sector+0x118,childLast)||childLast< -1||childLast>=8192||
   !m.get(sector+0x120,children)||(childLast>=0&&!readable(children,(size_t(childLast)+1)*8)))return false;
  total+=size_t(childLast+1);if(total>8192)return false;
  for(int j=0;j<=childLast;++j){uintptr_t child=0;if(!m.get(children+size_t(j)*8,child)||!child||!readable(child,0x184))return false;}
 }return true;
}
static bool childBody(const Target& t){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t child=0,list=0,body=0,parent=0,node=0;int last=-1;
 return m.get(t.node+0x118,last)&&last>=0&&last<128&&m.get(t.node+0x120,list)&&m.get(list,child)&&child==t.child&&
  m.get(child+0x128,parent)&&parent==t.node&&m.get(child+0x160,body)&&body==t.body&&body&&m.get(body+0x18,node)&&node==child;
}
static uintptr_t embeddedLight(const Target& t){
 const scripting::CharacterMemory m{copyWorldMemory};std::vector<uintptr_t> stack{t.node},seen;
 while(!stack.empty()&&seen.size()<256){const auto node=stack.back();stack.pop_back();if(std::find(seen.begin(),seen.end(),node)!=seen.end())return 0;seen.push_back(node);
  uint32_t flags=0;if(!m.get(node+8,flags))return 0;if((flags&0x5f)==5)return node;
  int last=-1;uintptr_t children=0;if(!m.get(node+0x118,last)||last< -1||last>=128||!m.get(node+0x120,children)||(last>=0&&!readable(children,(size_t(last)+1)*8)))return 0;
  for(int i=last;i>=0;--i){uintptr_t child=0,parent=0,sector=0;if(!m.get(children+size_t(i)*8,child)||!child||!m.get(child+0x128,parent)||parent!=node||!m.get(child+0x130,sector)||sector!=t.sector)return 0;stack.push_back(child);}
 }return 0;
}
static bool leverState(const Target& t,LeverState& s){
 const scripting::CharacterMemory m{copyWorldMemory};uint32_t record=0;uintptr_t special=0;uint8_t held=0;int remaining=0;
 if(!readable(t.body,0x5c9)||!m.get(t.body+0x570,record)||record!=t.item||!m.get(t.body+0x580,s.flags)||(s.flags&2)||
  !m.get(t.body+0x5c0,special)||!m.get(t.body+0x5c8,held)||held||!m.get(t.body+0x5b8,remaining)||remaining||
  !m.get(t.body+0x524,s.start)||!m.get(t.body+0x528,s.travel)||!m.get(t.body+0x530,s.angle)||!m.get(t.body+0x540,s.index)||!m.get(t.body+0x544,s.overrideTarget))return false;
 if(t.definition.size()>9&&t.definition[9]==6){if(!lightLeverPossible||!special||embeddedLight(t)!=special||!readable(special,0x2e0)||!m.get(special+0x2dc,s.lightValue)||!std::isfinite(s.lightValue)||std::abs(s.lightValue)>1)return false;s.light=special;}
 else if(special)return false;
 return std::isfinite(s.start)&&std::abs(s.start)<=6.284f&&std::isfinite(s.travel)&&s.travel>0.001f&&s.travel<=6.284f&&std::isfinite(s.angle)&&s.angle>=s.start-0.01f&&s.angle<=s.start+s.travel+0.01f&&s.index>=0&&s.index<=1&&s.overrideTarget<=1;
}
static int leverEndpoint(const LeverState& s){return s.overrideTarget?s.index:((s.angle-s.start)/s.travel>=0.5f?1:0);}
static bool doorType(uintptr_t body){
 return scripting::classMatches(body,base+0x31ab38,copyWorldMemory);
}
static bool doorLeaf(const Target& t,uintptr_t child,uintptr_t body,DoorLeaf& out){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t parent=0,node=0,sector=0;uint32_t record=0;
 if(!body||!readable(body,0x608)||!doorType(body)||!m.get(child+0x128,parent)||parent!=t.node||!m.get(child+0x130,sector)||sector!=t.sector||!m.get(body+0x18,node)||node!=child||
  !m.get(body+0x5e0,record)||record!=t.item||!copyWorldMemory(body+0x5c4,out.bytes.data(),out.bytes.size())||out.bytes[0x40]||out.bytes[0x39]||
  !m.get(body+0x528,out.mode)||(out.mode!=0&&out.mode!=1&&out.mode!=4&&out.mode!=8))return false;
 out.child=child;out.body=body;out.sliding=out.bytes[0x34]!=0;out.mechanical=out.bytes[0x36]!=0;out.closed=out.bytes[0x3a]!=0;
 // A hinge's squared tip speed; the game treats 0.015 or less as stopped.
 if(!out.sliding&&(!m.get(body+0x51c,out.speed)||!std::isfinite(out.speed)||out.speed<0))return false;
 if(out.sliding){
  if(!slidingPossible||!m.get(body+0x31c,out.count)||out.count<1||out.count>4096||!m.get(body+0x320,out.tetras)||!readable(out.tetras,size_t(out.count)*0x68)||
   !m.get(body+0x58c,out.index)||out.index<0||out.index>=out.count||!copyWorldMemory(out.tetras+size_t(out.index)*0x68,out.tetra.data(),out.tetra.size()))return false;
  float stiffness=0,travel=0;std::memcpy(&stiffness,out.tetra.data()+0x24,4);std::memcpy(&travel,out.tetra.data()+0x28,4);
  return std::isfinite(stiffness)&&stiffness>=0&&stiffness<=1000000&&std::isfinite(travel)&&travel>=0&&travel<=1.001f;
 }
 float travel=0,sign=0,closed=0,current=0,target=0;
 return m.get(body+0x520,sign)&&std::isfinite(sign)&&std::abs(sign)==1&&m.get(body+0x5c8,travel)&&std::isfinite(travel)&&std::abs(travel)>0.001f&&std::abs(travel)<=6.284f&&
  m.get(body+0x5c4,closed)&&std::isfinite(closed)&&std::abs(closed)<=12.568f&&m.get(body+0x5cc,current)&&std::isfinite(current)&&std::abs(current)<=12.568f&&
  m.get(body+0x5d8,target)&&std::isfinite(target)&&target>=0&&target<=1;
}
static bool doorPlan(const Target& t,std::vector<DoorLeaf>& leaves){
 if(t.view.locked||(t.state&1))return false;const scripting::CharacterMemory m{copyWorldMemory};int last=-1;uintptr_t list=0;
 if(!m.get(t.node+0x118,last)||last<0||last>=128||!m.get(t.node+0x120,list)||!readable(list,(size_t(last)+1)*8))return false;
 for(int i=0;i<=last;++i){uintptr_t child=0,body=0;if(!m.get(list+size_t(i)*8,child)||!child||!m.get(child+0x160,body))return false;
  if(body){DoorLeaf leaf;if(leaves.size()>=2||!doorLeaf(t,child,body,leaf))return false;leaves.push_back(leaf);}
 }
 if(leaves.empty()||leaves[0].child!=t.child||leaves[0].body!=t.body)return false;
 if(leaves.size()==1)return leaves[0].bytes[0x43]==0;
 if(!pairedPossible||leaves[0].body==leaves[1].body)return false;
 uintptr_t firstPair=0,secondPair=0;return leaves[0].bytes[0x43]&&leaves[1].bytes[0x43]&&m.get(leaves[0].body+0x5e8,firstPair)&&firstPair==leaves[1].body&&m.get(leaves[1].body+0x5e8,secondPair)&&secondPair==leaves[0].body;
}
static bool sameDoors(const Target& t){
 std::vector<DoorLeaf> current;if(!doorPlan(t,current)||current.size()!=t.doors.size())return false;
 for(size_t i=0;i<current.size();++i){const auto& a=current[i];const auto& b=t.doors[i];if(a.child!=b.child||a.body!=b.body||a.bytes!=b.bytes||a.mode!=b.mode||a.tetras!=b.tetras||a.count!=b.count||a.index!=b.index||a.tetra!=b.tetra)return false;}
 return true;
}
// Wake recursively visits native body links. Bound and cycle-check that graph
// before entering it, without replacing the engine's wake bookkeeping.
static bool wakeReady(uintptr_t body,std::vector<uintptr_t>& path,size_t& budget){
 if(!body||!budget--||path.size()>=32||std::find(path.begin(),path.end(),body)!=path.end()||!readable(body,0x448))return false;
 const scripting::CharacterMemory m{copyWorldMemory};uint8_t proxy=0;uintptr_t root=0;if(!m.get(body+0x440,proxy))return false;
 path.push_back(body);bool ok=true;
 if(proxy){ok=m.get(body+0x420,root)&&wakeReady(root,path,budget);}
 else{int count=0;uintptr_t list=0,node=0;int last=-1;
  ok=m.get(body+0x430,count)&&count>=0&&count<=256&&m.get(body+0x438,list)&&(!count||readable(list,size_t(count)*8))&&m.get(body+0x18,node)&&node&&m.get(node+0x118,last)&&last>=-1&&last<128;
  // The native child wake branch dispatches virtual methods; defer compound
  // bodies until their child-class contract has its own review.
  if(last>=0)ok=false;
  for(int i=0;ok&&i<count;++i){uintptr_t other=0;ok=m.get(list+size_t(i)*8,other)&&wakeReady(other,path,budget);}
 }path.pop_back();return ok;
}
static bool wakeReady(uintptr_t body){std::vector<uintptr_t> path;size_t budget=256;int ticks=0;return wake&&scripting::CharacterMemory{copyWorldMemory}.get(base+0x2ae6d0,ticks)&&ticks>0&&ticks<=4096&&wakeReady(body,path,budget);}
static bool identity(const Target& t,const inventorybridge::Target& owner){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t sector=0;std::array<float,3> position{},at{};uint32_t flags=0,item=0,state=0;
 if(t.world!=owner.world||t.scene!=owner.scene||t.player!=owner.player||!worlditems::actor(owner,sector,position)||sector!=t.sector||
  !m.get(t.node+8,flags)||(flags&0x7000)!=0x4000||!(flags&0x40000)||(flags&0x82000000)||!m.get(t.node+0x180,item)||item!=t.item||
  !m.get(t.node+0x184,state)||state!=t.state||(state&0xc80100)||!m.get(t.node+0x130,sector)||sector!=t.sector||!m.get(t.node+0x64,at)||!worlditems::withinReach(position,at))return false;
 std::vector<uint8_t> current;const auto tag=t.view.kind==scripting::InteractionKind::door?0x40000:t.view.kind==scripting::InteractionKind::container?0x20000:0x10000;
 if(!definition(t.item,tag,t.definition.size(),current)||current!=t.definition)return false;
 if(t.view.kind==scripting::InteractionKind::emitter||t.view.kind==scripting::InteractionKind::fireSource||t.view.kind==scripting::InteractionKind::torch){worldemitters::State now;if(!worldemitters::same(t.emitter,now)||!worldemitters::settled(now))return false;}
 else if(t.view.kind==scripting::InteractionKind::teleporter||t.view.kind==scripting::InteractionKind::scanner){if(!worlddevices::same(t.device))return false;}
 else if(t.view.kind!=scripting::InteractionKind::container){
  if(!childBody(t))return false;
  if(t.view.kind==scripting::InteractionKind::door){
   uint8_t locked=0,unlocked=0,pair=0;if(!readable(t.body,0x608)||!m.get(t.body+0x604,locked)||!m.get(t.body+0x603,unlocked)||!m.get(t.body+0x607,pair)||((locked&&!unlocked)!=t.view.locked))return false;
   if(pair){uintptr_t other=0,otherNode=0,parent=0,otherSector=0;if(!m.get(t.body+0x5e8,other)||!other||!readable(other,0x608)||!m.get(other+0x18,otherNode)||!otherNode||
    !m.get(otherNode+0x128,parent)||!parent||!m.get(parent+0x130,otherSector)||otherSector!=t.sector)return false;}
   if(t.view.doorControl&&!sameDoors(t))return false;
  }else if(t.view.kind==scripting::InteractionKind::lever){
   LeverState s;if(!leverState(t,s)||s.start!=t.lever.start||s.travel!=t.lever.travel||s.angle!=t.lever.angle||s.index!=t.lever.index||s.overrideTarget!=t.lever.overrideTarget||s.flags!=t.lever.flags||s.light!=t.lever.light||s.lightValue!=t.lever.lightValue)return false;
  }else{uint32_t record=0;uint8_t pressed=0;if(!readable(t.body,0x5c0)||!m.get(t.body+0x4f0,record)||record!=t.item||!m.get(t.body+0x51c,pressed)||bool(pressed)!=t.view.pressed)return false;}
 }
 const auto world=scripting::readWorld(t.world,copyWorldMemory);for(const auto& row:world.objects)if(row.nativeNode==t.node)return true;return false;
}
static const scripting::InteractionView* capture(uintptr_t node){
 if(!worldPulse||!aiPulseReady||worlditems::published)return nullptr;
 for(const auto& t:targets)if(t.node==node)return &t.view;
 if(targets.size()>=8)return nullptr;
 const auto view=inventorybridge::capture();if(!view)return nullptr;const auto owner=inventorybridge::target;
 const scripting::CharacterMemory m{copyWorldMemory};Target t;t.node=node;t.world=owner.world;t.scene=owner.scene;t.player=owner.player;
 if(!m.get(node+0x180,t.item)||!t.item||!m.get(node+0x184,t.state)||!m.get(node+0x130,t.sector))return nullptr;
 std::vector<uint8_t> device;
 // The native dispatcher prioritizes door chunks over device chunks. Never
 // reclassify a door as a button/container when key support is unavailable.
 if(inventorybridge::chunk(t.item,0x40000)){
  if((!keyPossible&&!doorPossible)||!definition(t.item,0x40000,16,t.definition))return nullptr;
  t.view.kind=scripting::InteractionKind::door;uint32_t flags=0;std::memcpy(&flags,t.definition.data()+8,4);if(flags&8)return nullptr;std::memcpy(&t.view.requiredKey,t.definition.data()+12,4);
 }else if(worldemitters::torchPossible&&definition(t.item,0x10000,24,device)&&device[8]==8&&device[9]==12){
  t.view.kind=scripting::InteractionKind::torch;t.definition=std::move(device);
  if(!worldemitters::capture(node,t.world,t.scene,t.player,t.emitter,false,true))return nullptr;
  t.view.pressed=t.emitter.effect!=0;t.view.igniteControl=!t.view.pressed;t.view.extinguishControl=t.view.pressed&&worldemitters::extinguishPossible;
 }else if((worldemitters::firePossible||worldemitters::extinguishPossible)&&definition(t.item,0x10000,16,device)&&device[8]==6&&device[9]==12){
  t.view.kind=scripting::InteractionKind::fireSource;t.definition=std::move(device);
  if(!worldemitters::capture(node,t.world,t.scene,t.player,t.emitter,true)||!worldemitters::settled(t.emitter))return nullptr;
  t.view.pressed=t.emitter.effect!=0;t.view.igniteControl=!t.view.pressed&&worldemitters::firePossible;t.view.extinguishControl=t.view.pressed&&worldemitters::extinguishPossible;
 }else if(worldemitters::possible&&definition(t.item,0x10000,24,device)&&device[8]==6&&device[9]==13){
  t.view.kind=scripting::InteractionKind::emitter;t.definition=std::move(device);
  if(!worldemitters::capture(node,t.world,t.scene,t.player,t.emitter)||!worldemitters::settled(t.emitter))return nullptr;t.view.pressed=t.emitter.active;
 }else if(worlddevices::teleporterPossible&&definition(t.item,0x10000,56,device)&&device[8]==2&&device[9]==15){
  t.view.kind=scripting::InteractionKind::teleporter;t.definition=std::move(device);if(!worlddevices::capture(node,t.sector,t.player,false,t.device))return nullptr;
 }else if(worlddevices::scannerPossible&&definition(t.item,0x10000,16,device)&&device[8]==2&&device[9]==18){
  t.view.kind=scripting::InteractionKind::scanner;t.definition=std::move(device);if(!worlddevices::capture(node,t.sector,t.player,true,t.device))return nullptr;
 }else if(leverPossible&&definition(t.item,0x10000,24,device)&&device[8]==3){
  if(device[9]==6&&!lightLeverPossible)return nullptr;t.view.kind=scripting::InteractionKind::lever;t.definition=std::move(device);
 }else if(buttonPossible&&definition(t.item,0x10000,16,device)&&device[8]==5){
  uint16_t flags=0;std::memcpy(&flags,device.data()+10,2);if(flags&4)return nullptr;t.view.kind=scripting::InteractionKind::button;t.definition=std::move(device);
 }else if(openPossible&&!inventorybridge::chunk(t.item,0x10000)&&definition(t.item,0x20000,32,t.definition)){
  t.view.kind=scripting::InteractionKind::container;t.view.locked=(t.state&1)!=0;
 }else return nullptr;
 if(t.view.kind!=scripting::InteractionKind::container&&t.view.kind!=scripting::InteractionKind::teleporter&&t.view.kind!=scripting::InteractionKind::scanner&&t.view.kind!=scripting::InteractionKind::emitter&&t.view.kind!=scripting::InteractionKind::fireSource&&t.view.kind!=scripting::InteractionKind::torch){
  uintptr_t list=0;int last=-1;if(!m.get(node+0x118,last)||last<0||!m.get(node+0x120,list)||!m.get(list,t.child)||!m.get(t.child+0x160,t.body))return nullptr;
  uint8_t value=0;if(t.view.kind==scripting::InteractionKind::door){uint8_t unlocked=0;if(!m.get(t.body+0x604,value)||!m.get(t.body+0x603,unlocked))return nullptr;t.view.locked=value&&!unlocked;
   if(doorPossible&&doorPlan(t,t.doors)){t.view.doorControl=true;t.view.closed=true;t.view.doorLeaves=int(t.doors.size());t.view.doorKind=t.doors[0].sliding?1:0;
    for(const auto& leaf:t.doors){t.view.closed=t.view.closed&&leaf.closed;if(leaf.sliding!=t.doors[0].sliding)t.view.doorKind=2;
     // The game clears the command (+0x528) when a leaf reaches its target; motion can outlast it.
     if(leaf.mode==4||leaf.mode==8)t.view.doorCommand=leaf.mode;
     t.view.doorMoving=t.view.doorMoving||leaf.mode==4||leaf.mode==8||(!leaf.sliding&&leaf.speed>0.015f);}
   }
  }else if(t.view.kind==scripting::InteractionKind::lever){
   if(!leverState(t,t.lever))return nullptr;t.view.position=std::clamp((t.lever.angle-t.lever.start)/t.lever.travel,0.0f,1.0f);t.view.targetPosition=leverEndpoint(t.lever);t.view.lightControl=t.lever.light!=0;
  }else{if(!m.get(t.body+0x51c,value))return nullptr;t.view.pressed=value!=0;}
 }
 if(!identity(t,owner)||nextTicket>=uint64_t((std::numeric_limits<lua_Integer>::max)()))return nullptr;
 t.view.ticket=++nextTicket;targets.push_back(std::move(t));return &targets.back().view;
}
struct EffectContext {uintptr_t reserved[3]{},view=0;uint32_t item=0,pad=0;uintptr_t source=0,node=0;uint32_t icon=0;int row=0;};
static_assert(sizeof(EffectContext)==0x40&&offsetof(EffectContext,item)==0x20&&offsetof(EffectContext,source)==0x28,"native use context");
static bool effectCall(uintptr_t player,uintptr_t view,uint32_t item){
 __try{EffectContext context;context.view=view;context.item=item;effect(&context,player);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool unlockCall(uintptr_t body){__try{unlock(body);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static int openCall(uint32_t item,uintptr_t& window){__try{if(isOpen(item))return 0;window=open(item);return 1;}__except(EXCEPTION_EXECUTE_HANDLER){return -1;}}
static bool findCall(uintptr_t world,uint32_t item,uintptr_t& node){__try{node=find(world,item);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool triggerCall(uintptr_t world,uint32_t item,bool state){__try{trigger(world,item,0,state?1:0);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool leverCall(uintptr_t body,int endpoint){
 __try{*(int*)(body+0x540)=endpoint;*(uint8_t*)(body+0x544)=1;wake(body,*(int*)(base+0x2ae6d0));return *(int*)(body+0x540)==endpoint&&*(uint8_t*)(body+0x544)==1;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool doorRequested(const DoorLeaf& leaf,bool opening){return opening?(leaf.mode==4||(leaf.sliding&&!leaf.mechanical&&!leaf.closed)):(leaf.closed||leaf.mode==8);}
static bool doorCall(const DoorLeaf& leaf,bool opening){
 __try{
  const auto body=leaf.body;const uint32_t duration=opening?(leaf.sliding?(leaf.mechanical?300u:80u):100u):(leaf.bytes[0x38]?100u:300u);
  if(opening&&leaf.closed)unlatch(body);
  // A slider's native latch release initializes its constraint and command.
  // Hinge targets are angular; never apply them to sliding constraints.
  if(!leaf.sliding){*(float*)(body+0x5d8)=opening?0.0f:1.0f;doorCommand(body,opening?4:8,duration);}
  else if(!opening||!leaf.closed)doorCommand(body,opening?4:8,duration);
  wake(body,*(int*)(base+0x2ae6d0));
  return *(uint32_t*)(body+0x528)==(opening?4u:8u)&&*(uint32_t*)(body+0x52c)==duration&&
   (leaf.sliding||*(float*)(body+0x5d8)==(opening?0.0f:1.0f))&&(!opening||!*(uint8_t*)(body+0x5fe));
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool consume(const inventorybridge::Target& owner,const scripting::InventoryView& current,size_t index){
 using namespace inventorybridge;using namespace scripting;
 if(!consumePossible||!effect||index>=current.carried.size())return false;
 const auto row=current.carried[index];if(!row.item||(row.state&0xc80100)||carrieditems::referenced(current,row.item))return false;
 std::vector<uint8_t> device;if((row.state&0xff0000)||!definition(row.item,0x10000,16,device)||device[8]!=1||(device[9]!=8&&device[9]!=14))return false;
 const CharacterMemory m{copyWorldMemory};uintptr_t master=0,behavior=0,view=0;uint32_t flags=0;float before=0;
 if(!m.get(owner.player+0x580,master)||!master||!m.get(master+0x12a4,flags)||!(flags&2)||!m.get(owner.player+0xd40,behavior)||!behavior||
  !m.get(behavior+fp::game::behaviour::view,view)||!view||!m.get(owner.player+0xdc4,before)||!std::isfinite(before)||std::abs(before)>1000000||(device[9]==14&&!soundReady()))return false;
 worlditems::published=true;if(!effectCall(owner.player,view,row.item))return fault();
 if(device[9]==8){float after=0;if(!m.get(owner.player+0xdc4,after)||!std::isfinite(after)||after<=before)return fault();}
 else{if(!m.get(owner.player+0xdf8,flags)||!(flags&0x800000))return fault();}
 if(!compact(owner.stats,index)||!finish(owner.stats,owner.player,row.item,false))return fault();
 auto expected=current.carried;expected.erase(expected.begin()+index);InventoryView after;
 if(!rows(owner.stats,after)||after.carried!=expected||after.worn!=current.worn||after.weapons!=current.weapons)return fault();return true;
}
static bool commit(const inventorybridge::Target& owner,const scripting::InventoryView& current,const scripting::InventoryChange& change){
 using namespace scripting;using namespace inventorybridge;
 if(unsigned(change.op)<unsigned(InventoryOp::itemUse)||change.op==InventoryOp::spawnItem)return carrieditems::commit(owner,current,change);
 if(worlditems::published)return false;uintptr_t sector=0;std::array<float,3> position{};if(!worlditems::actor(owner,sector,position))return false;
 bool applied=false;const CharacterMemory m{copyWorldMemory};
 if(change.op==InventoryOp::itemUse){applied=consume(owner,current,change.row);}
 else{
  const Target* found=nullptr;for(const auto& t:targets)if(t.view.ticket==change.worldItem){found=&t;break;}if(!found)return false;const Target chosen=*found;if(!identity(chosen,owner))return false;
  if(change.op==InventoryOp::fireExtinguish||change.op==InventoryOp::torchIgnite){
   const bool stopping=change.op==InventoryOp::fireExtinguish;
   if(stopping?!chosen.view.extinguishControl:(chosen.view.kind!=InteractionKind::torch||!chosen.view.igniteControl))return false;
   worlditems::published=true;if(!worldemitters::submit(chosen.emitter,!stopping))return worldemitters::failed.load()?fault():false;applied=true;
  }else if(change.op==InventoryOp::fireUse){
   if(chosen.view.kind!=InteractionKind::fireSource)return false;
   worlditems::published=true;if(!worldemitters::submit(chosen.emitter,true))return worldemitters::failed.load()?fault():false;applied=true;
  }else if(change.op==InventoryOp::emitterSet||change.op==InventoryOp::emitterToggle){
   if(chosen.view.kind!=InteractionKind::emitter)return false;
   const bool active=change.op==InventoryOp::emitterToggle?!chosen.emitter.active:change.active;
   worlditems::published=true;if(!worldemitters::submit(chosen.emitter,active))return worldemitters::failed.load()?fault():false;applied=true;
  }else if(change.op==InventoryOp::teleporterUse||change.op==InventoryOp::scannerUse){
   const bool scan=change.op==InventoryOp::scannerUse;if((scan&&(!worlddevices::scannerPossible||!worlddevices::scanner||chosen.view.kind!=InteractionKind::scanner))||
    (!scan&&(!worlddevices::teleporterPossible||!worlddevices::teleporter||chosen.view.kind!=InteractionKind::teleporter)))return false;
   worlditems::published=true;const int result=worlddevices::call(chosen.device);if(result<0)return fault();uint32_t id=0,state=0;
   if(!result){if(!m.get(chosen.node+0x180,id)||id!=chosen.item||!m.get(chosen.node+0x184,state)||state!=chosen.state)return fault();return false;}
   if(!worlddevices::published(chosen.device))return fault();aiPulseReady=false;applied=true;
  }else if(change.op==InventoryOp::leverPosition||change.op==InventoryOp::leverToggle){
   if(!leverPossible||chosen.view.kind!=InteractionKind::lever||!wakeReady(chosen.body))return false;
   const int endpoint=change.op==InventoryOp::leverToggle?1-chosen.view.targetPosition:int(change.active);
   if(endpoint==chosen.view.targetPosition)return false;worlditems::published=true;if(!leverCall(chosen.body,endpoint))return fault();applied=true;
  }else if(change.op==InventoryOp::doorOpen){
   if(!doorPossible||!unlatch||!doorCommand||chosen.view.kind!=InteractionKind::door||!chosen.view.doorControl||chosen.doors.empty())return false;
   bool needed=false;for(const auto& leaf:chosen.doors){if(!wakeReady(leaf.body))return false;
    if(change.active&&leaf.sliding&&!leaf.mechanical&&!leaf.closed&&leaf.mode==8)return false;
    if(!doorRequested(leaf,change.active)){needed=true;if(change.active&&leaf.closed){uintptr_t audio=0;if(!m.get(base+0x4eebd0,audio)||!audio||!readable(audio,0x18))return false;}}
   }
   if(!needed)return false;
   worlditems::published=true;for(const auto& leaf:chosen.doors)if(!doorRequested(leaf,change.active)&&!doorCall(leaf,change.active))return fault();applied=true;
  }else if(change.op==InventoryOp::useItemOnWorld){
   if(!keyPossible||!unlock||chosen.view.kind!=InteractionKind::door||!chosen.view.locked||change.row>=current.carried.size()||!soundReady())return false;
   const auto row=current.carried[change.row];std::vector<uint8_t> device;
   if(!row.item||(row.state&0xc80100)||row.item!=chosen.view.requiredKey||carrieditems::referenced(current,row.item)||!definition(row.item,0x10000,10,device)||device[8]!=1||device[9]!=0)return false;
   worlditems::published=true;if(!unlockCall(chosen.body))return fault();uint8_t unlocked=0;uint32_t state=0;
   if(!m.get(chosen.body+0x603,unlocked)||!unlocked||!m.get(chosen.node+0x184,state)||(state&1))return fault();applied=true;
  }else if(chosen.view.kind==InteractionKind::container){
   if(change.op!=InventoryOp::worldUse||!openPossible||!open||!isOpen||chosen.view.locked||!guiReady())return false;
   worlditems::published=true;uintptr_t window=0;const int opened=openCall(chosen.item,window);if(opened<0)return fault();if(!opened||!window)return false;uint32_t id=0;
   if(!m.get(window+0x15c,id)||id!=chosen.item)return fault();applied=true;
  }else if(chosen.view.kind==InteractionKind::button){
   if(!buttonPossible||!find||!trigger||!worldIndex(owner.world)||(change.op!=InventoryOp::worldUse&&change.op!=InventoryOp::worldPress))return false;
   uintptr_t node=0;if(!findCall(owner.world,chosen.item,node))return fault();if(node!=chosen.node)return false;const bool active=change.op==InventoryOp::worldUse||change.active;
   if(active==chosen.view.pressed)return false;worlditems::published=true;if(!triggerCall(owner.world,chosen.item,active))return fault();uint8_t pressed=0;
   if(!m.get(chosen.body+0x51c,pressed)||bool(pressed)!=active)return fault();applied=true;
  }else return false;
  InventoryView after;if(!rows(owner.stats,after)||after.carried!=current.carried||after.worn!=current.worn||after.weapons!=current.weapons)return fault();
 }
 if(applied){target={};targets.clear();worldCaptured=false;worldCopy={};}return applied;
}
}
