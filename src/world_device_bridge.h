#pragma once
namespace worlddevices {
using Use=char(*)(uintptr_t);
static Use teleporter=nullptr,scanner=nullptr;
static bool teleporterPossible=false,scannerPossible=false,teleporterLease=false;
static uintptr_t scannerOwner=0;
struct State {uintptr_t node=0,sector=0,player=0,socket=0,crystal=0,playerRoot=0;uint32_t crystalId=0,crystalState=0;bool scan=false;std::array<float,3> socketAt{},crystalAt{};};
static bool available(bool scan){
 const unsigned char teleporterPrefix[]={0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x48,0x8d,0x64,0x24,0xd0,0x48,0x89,0xcb,0x40};
 const unsigned char scannerPrefix[]={0x53,0x56,0x48,0x8d,0x64,0x24,0xc8,0x48,0x89,0xcb,0x40,0x30,0xf6,0x48,0x83,0x3d};
 const unsigned char crystalPrefix[]={0x53,0x57,0x56,0x41,0x54,0x48,0x8d,0x64,0x24,0xc8,0x48,0x89,0xcb,0x48,0x8b,0x02};
 const unsigned char scanEffectPrefix[]={0x53,0x57,0x56,0x41,0x54,0x41,0x55,0x48,0x8d,0x64,0x24,0xc0,0x48,0x89,0xcb,0x41};
 const unsigned char destinationsPrefix[]={0x53,0x48,0x8d,0x64,0x24,0xd0,0x48,0x89,0xcb,0x48,0x63,0x83,0x64,0x01,0x00,0x00};
 if(!gamebridge::operation(base+0x274940,crystalPrefix,sizeof crystalPrefix))return false;
 if(scan)return gamebridge::operation(base+0x276420,scannerPrefix,sizeof scannerPrefix)&&gamebridge::operation(base+0x276250,scanEffectPrefix,sizeof scanEffectPrefix);
 return gamebridge::operation(base+0x2749c0,teleporterPrefix,sizeof teleporterPrefix)&&gamebridge::operation(base+0x273cb0,destinationsPrefix,sizeof destinationsPrefix);
}
static bool children(uintptr_t node,std::vector<uintptr_t>& out,int limit=128){
 const scripting::CharacterMemory m{copyWorldMemory};int last=-1;uintptr_t list=0;
 if(!m.get(node+0x118,last)||last< -1||last>=limit||!m.get(node+0x120,list))return false;out.resize(size_t(last+1));
 return out.empty()||copyWorldMemory(list,out.data(),out.size()*8);
}
static float distance(const std::array<float,3>& a,const std::array<float,3>& b){std::array<float,3> d{};for(size_t i=0;i<3;++i){if(!std::isfinite(a[i])||!std::isfinite(b[i]))return -1;d[i]=a[i]-b[i];}const float sum=d[0]*d[0]+d[1]*d[1]+d[2]*d[2];return std::isfinite(sum)?sum:-1;}
// Mirrors 0x029030's active-window lookup without entering an unbounded list.
static int portalWindow(uintptr_t& window){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t gui=0,owner=0,list=0;int count=0;uint32_t wanted=0;
 if(!m.get(base+0x2efaa0,wanted)||!wanted||!m.get(base+0x49a930,gui)||!gui||!m.get(gui+0x128,owner)||!owner||!m.get(owner+0xa8,count)||count<0||count>4096||
  !m.get(owner+0xb0,list)||(count&&!readable(list,size_t(count)*8)))return -1;
 for(int i=0;i<count;++i){uintptr_t child=0;uint32_t type=0;uint8_t active=0;if(!m.get(list+size_t(i)*8,child)||!child||!m.get(child+0xb8,type)||!m.get(child+0x79,active))return -1;if(type==wanted&&active){window=child;return 1;}}
 window=0;return 0;
}
static int scanEffect(uintptr_t player){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t root=0;if(!m.get(player+0x18,root)||!root)return -1;std::vector<uintptr_t> list;if(!children(root,list))return -1;
 for(const auto child:list){uintptr_t parent=0;if(!child||!m.get(child+0x128,parent)||parent!=root)return -1;if(scripting::classMatches(child,base+0x34c710,copyWorldMemory))return 1;}return 0;
}
static bool idle(){
 if(teleporterPossible){uintptr_t window=0;const int status=portalWindow(window);if(status==1)return false;if(teleporterLease){if(status<0)return false;teleporterLease=false;}}
 if(scannerOwner){uintptr_t player=0;if(!scripting::CharacterMemory{copyWorldMemory}.get(base+npc::off::controlled,player))return false;
  if(player==scannerOwner){if(scanEffect(player)!=0)return false;}scannerOwner=0;
 }return true;
}
static bool capture(uintptr_t node,uintptr_t sector,uintptr_t player,bool scan,State& out){
 const scripting::CharacterMemory m{copyWorldMemory};uint32_t busy=1;float lift=0,reach=0,proximity=0;std::array<float,3> center{},anchor{};
 if(!idle()||!m.get(base+0x4f0840,busy)||busy||!m.get(base+0x34ccc8,lift)||!std::isfinite(lift)||std::abs(lift)>200||!m.get(base+0x34ccd0,reach)||!std::isfinite(reach)||reach<=0||reach>1000000||
  !m.get(base+0x34cac0,proximity)||!std::isfinite(proximity)||proximity<=0||proximity>10000||!m.get(node+0x64,center)||!m.get(player+0xadc,anchor))return false;
 center[1]+=lift;const auto range=distance(center,anchor);if(range<0||range>reach)return false;
 out={};out.node=node;out.sector=sector;out.player=player;out.scan=scan;
 std::vector<uintptr_t> list;if(!children(node,list))return false;
 for(const auto child:list){char name=0;uintptr_t parent=0,atSector=0;if(!child||!m.get(child+0x140,name)||!m.get(child+0x128,parent)||parent!=node||!m.get(child+0x130,atSector)||atSector!=sector)return false;if(name=='['){out.socket=child;break;}}
 if(!out.socket||!m.get(out.socket+0x64,out.socketAt)||!children(sector,list,8192))return false;
 for(const auto child:list){std::array<float,3> at{};if(!child||!m.get(child+0x64,at))return false;const double d=distance(at,out.socketAt);if(d<0)return false;if(d<proximity){out.crystal=child;out.crystalAt=at;break;}}
 uint32_t flags=0;uintptr_t crystalSector=0,parent=0;
 if(!out.crystal||!m.get(out.crystal+8,flags)||(flags&0x7000)!=0x4000||!(flags&0x40000)||(flags&0x82000000)||!m.get(out.crystal+0x130,crystalSector)||crystalSector!=sector||
  !m.get(out.crystal+0x128,parent)||parent!=sector||!m.get(out.crystal+0x180,out.crystalId)||!m.get(out.crystal+0x184,out.crystalState))return false;
 if(scan){uintptr_t audio=0;if(out.crystalId!=500||scanEffect(player)!=0||!m.get(player+0x18,out.playerRoot)||!m.get(base+0x4eec90,audio)||!audio||!readable(audio,0x18))return false;}
 else{uintptr_t window=0;if((out.crystalId!=0x1af&&out.crystalId!=0x58f)||((out.crystalState>>16)&255)>=0x55||portalWindow(window)!=0)return false;}
 return true;
}
static bool same(const State& before){State now;if(!capture(before.node,before.sector,before.player,before.scan,now))return false;
 return now.socket==before.socket&&now.crystal==before.crystal&&now.crystalId==before.crystalId&&now.crystalState==before.crystalState&&now.socketAt==before.socketAt&&now.crystalAt==before.crystalAt&&now.playerRoot==before.playerRoot;
}
static int call(const State& s){__try{return (s.scan?scanner:teleporter)(s.node)?1:0;}__except(EXCEPTION_EXECUTE_HANDLER){return -1;}}
static bool published(const State& s){
 if(s.scan){uint32_t armed=0;if(scanEffect(s.player)!=1||!scripting::CharacterMemory{copyWorldMemory}.get(base+0x4f0c60,armed)||armed!=1)return false;scannerOwner=s.player;return true;}
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t window=0,source=0,crystal=0,data=0;uint32_t id=0;
 if(portalWindow(window)!=1||!m.get(window+0x180,source)||source!=s.node||!m.get(window+0x188,crystal)||crystal!=s.crystal||!m.get(window+0x1a0,data)||!m.get(s.node+0x180,id))return false;
 const auto definition=inventorybridge::chunk(id,0x10000);uint32_t size=0;uint8_t kind=0,subtype=0;
 if(!definition||data!=definition+8||!m.get(definition+4,size)||size<56||size>1024*1024||!readable(definition,size)||!m.get(definition+8,kind)||kind!=2||!m.get(definition+9,subtype)||subtype!=15)return false;
 teleporterLease=true;return true;
}
}
