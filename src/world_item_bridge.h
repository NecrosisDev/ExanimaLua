#pragma once
namespace worlditems {
using Switch=void(*)(uintptr_t);using Convert=uint32_t(*)(uint32_t,uintptr_t);
using Create=uintptr_t(*)(uint32_t,uint32_t,uintptr_t,uintptr_t);using Initialize=void(*)(uintptr_t);
using Register=void(*)(uintptr_t,uintptr_t*,int);using Release=void(*)(uintptr_t,const void*);using Remove=void(*)(uintptr_t,unsigned);
static Switch requestSwitch=nullptr;static Convert convert=nullptr;static Create create=nullptr;static Initialize initialize=nullptr;
static Register registerNode=nullptr;static Release release=nullptr;static Remove remove=nullptr;
static bool switchPossible=false,pickupPossible=false,dropPossible=false,published=false;
static uintptr_t dropped=0;   // the last drop's new scene node (the inventory exports' drop result)
struct Target {uint64_t ticket=0;uintptr_t node=0,sector=0,body=0,world=0,player=0,parent=0;uint32_t item=0,state=0;};
static std::vector<Target> targets;static uint64_t nextTicket=0;
static void beginPulse(){targets.clear();published=false;}
static bool available(scripting::InventoryOp op){
 const unsigned char switchPrefix[]={0x53,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0x8b,0x83,0xf4,0x0d,0x00,0x00,0x25,0x02,0x02,0x01,0x00};
 const unsigned char removePrefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0x90,0x48,0x89,0x5d,0xc8,0x48,0x89,0x4d,0xf0,0x89,0x55,0xf8};
 const unsigned char convertPrefix[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xc0,0x89,0xcb,0x48,0x89,0xd6,0x89,0xdf};
 const unsigned char createPrefix[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xe0,0x89,0xc8,0x4c,0x89,0xc3,0x31,0xf6};
 const unsigned char initializePrefix[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xe0,0x48,0x89,0xcb,0x48,0x8b,0x83,0x28,0x01,0x00,0x00};
 const unsigned char registerPrefix[]={0x55,0x48,0x89,0xe5,0x48,0x8d,0x64,0x24,0xa0,0x48,0x89,0x5d,0xc8,0x48,0x89,0x75,0xd0};
 const unsigned char releasePrefix[]={0x53,0x57,0x56,0x48,0x8d,0x64,0x24,0xa0,0x48,0x89,0xcb,0x48,0x89,0xd6,0x48,0x85,0xdb};
 if(op==scripting::InventoryOp::switchWeapons)return gamebridge::operation(base+0x136e50,switchPrefix,sizeof switchPrefix);
 if(op==scripting::InventoryOp::pickup)return gamebridge::operation(base+0x50a60,removePrefix,sizeof removePrefix);
 return op==scripting::InventoryOp::drop&&gamebridge::operation(base+0x102700,convertPrefix,sizeof convertPrefix)&&gamebridge::operation(base+0x11cee0,createPrefix,sizeof createPrefix)&&
  gamebridge::operation(base+0x11d0e0,initializePrefix,sizeof initializePrefix)&&gamebridge::operation(base+0x5a420,registerPrefix,sizeof registerPrefix)&&gamebridge::operation(base+0x117000,releasePrefix,sizeof releasePrefix);
}
static bool actor(const inventorybridge::Target& owner,uintptr_t& sector,std::array<float,3>& position){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t node=0,world=0;uint32_t action=0;int request=0,motion=0;
 if(!inventorybridge::identity(owner)||!m.get(owner.player+npc::off::action,action)||action||!m.get(owner.player+0xdc0,request)||request==0x1f||
  !m.get(owner.player+npc::off::motion,motion)||motion==0x1f||!m.get(owner.player+0x18,node)||!node||!m.get(node+0x130,sector)||!sector||
  !m.get(sector+0x220,world)||world!=owner.world||!m.get(owner.player+npc::off::root,position))return false;
 for(float value:position)if(!std::isfinite(value))return false;return true;
}
static bool withinReach(const std::array<float,3>& a,const std::array<float,3>& b){double d=0;for(size_t i=0;i<3;++i){if(!std::isfinite(a[i])||!std::isfinite(b[i]))return false;const double v=double(a[i])-b[i];d+=v*v;}return d<=200.0*200.0;}
static bool ordinary(uint32_t item,uint32_t state){
 uint32_t volume=0;return item&&(item&0xc0000000u)!=0xc0000000u&&!(state&0xc80100)&&inventorybridge::volume(item,volume)&&
  !inventorybridge::chunk(item,0x40000)&&!inventorybridge::chunk(item,0x10000)&&!inventorybridge::chunk(item,0x20000)&&!inventorybridge::chunk(item,0x8000ac07);
}
static bool queueReady(uintptr_t world){
 const scripting::CharacterMemory m{copyWorldMemory};uint32_t read=0,write=0;
 return m.get(world+0x3d0,read)&&m.get(world+0x3d4,write)&&read<256&&write<256&&((write+256-read)%256)<200;
}
static bool treeReady(uintptr_t node){
 const scripting::CharacterMemory m{copyWorldMemory};std::vector<uintptr_t> stack{node};std::unordered_set<uintptr_t> seen;
 while(!stack.empty()){
  const auto at=stack.back();stack.pop_back();if(!at||!seen.insert(at).second||seen.size()>512||!readable(at,0x198))return false;
  int last=-1;uintptr_t children=0,body=0;if(!m.get(at+0x118,last)||last< -1||last>=128||!m.get(at+0x120,children)||!m.get(at+0x160,body))return false;
  if(body&&!readable(body,0x440))return false;
  for(int i=0;i<=last;++i){uintptr_t child=0;if(!m.get(children+size_t(i)*8,child))return false;stack.push_back(child);}
 }return true;
}
static bool parentContains(const Target& t,bool& found){
 const scripting::CharacterMemory m{copyWorldMemory};int last=-1;uintptr_t children=0;found=false;
 if(!t.parent||!m.get(t.parent+0x118,last)||last< -1||last>=8192||!m.get(t.parent+0x120,children))return false;
 for(int i=0;i<=last;++i){uintptr_t child=0;if(!m.get(children+size_t(i)*8,child))return false;if(child==t.node){if(found)return false;found=true;}}
 return true;
}
static bool identity(const Target& t,const inventorybridge::Target& owner){
 const scripting::CharacterMemory m{copyWorldMemory};uintptr_t sector=0,body=0,entry=0,master=0,parent=0;std::array<float,3> position{},itemPosition{};uint32_t flags=0,item=0,state=0,type=0,kind=0;uintptr_t holder=0;
 if(t.world!=owner.world||t.player!=owner.player||!actor(owner,sector,position)||sector!=t.sector||!m.get(t.node+8,flags)||(flags&0x7000)!=0x4000||
  !(flags&0x40000)||(flags&0x82000000)||!m.get(t.node+0x180,item)||item!=t.item||!m.get(t.node+0x184,state)||state!=t.state||!ordinary(item,state)||
  !m.get(t.node+0x194,kind)||kind==0x40000||kind==0x10000||kind==0x8000ac07||!m.get(t.node+0x130,sector)||sector!=t.sector||
  !m.get(t.node+0x160,body)||body!=t.body||!body||!m.get(body+0xc,type)||(type&0x10000000)==0||(type&0xff000000)==0x1a000000||!m.get(body+0x420,holder)||holder||
  !m.get(t.node+0x128,parent)||parent!=t.parent||!m.get(t.node+0x138,master)||!master||!readable(master,0x1c0)||!m.get(base+0x4f07c0,entry)||entry==t.node||
  !m.get(t.node+0x64,itemPosition)||!withinReach(position,itemPosition)||!queueReady(t.world)||!treeReady(t.node))return false;
 bool found=false;if(!parentContains(t,found)||!found)return false;
 const auto snapshot=scripting::readWorld(t.world,copyWorldMemory);for(const auto& row:snapshot.objects)if(row.nativeNode==t.node)return true;return false;
}
static uint64_t capture(uintptr_t node){
 if(!pickupPossible||published||!worldPulse||!aiPulseReady)return 0;
 const auto view=inventorybridge::capture();if(!view)return 0;const auto& owner=inventorybridge::target;
 for(const auto& t:targets)if(t.node==node)return t.ticket;
 if(targets.size()>=8||nextTicket>=uint64_t((std::numeric_limits<lua_Integer>::max)()))return 0;
 const scripting::CharacterMemory m{copyWorldMemory};Target t;t.node=node;t.world=owner.world;t.player=owner.player;
 if(!m.get(node+0x128,t.parent)||!m.get(node+0x130,t.sector)||!m.get(node+0x160,t.body)||!m.get(node+0x180,t.item)||!m.get(node+0x184,t.state)||!identity(t,owner))return 0;
 t.ticket=++nextTicket;targets.push_back(t);return t.ticket;
}
static bool switchCall(uintptr_t player){__try{if(!requestSwitch)return false;requestSwitch(player);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool removeCall(uintptr_t node){__try{if(!remove)return false;remove(node,0);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool createCall(uintptr_t sector,const scripting::InventoryRow& row,uintptr_t& node,uint32_t& actual){
 __try{uintptr_t db=*(uintptr_t*)(base+0x4ef930);actual=convert(row.item,db);node=actual&&(actual&0xc0000000u)!=0xc0000000u?create(actual,row.state,sector,0):0;return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool publishDrop(const inventorybridge::Target& owner,uintptr_t node,const std::array<float,3>& position){
 __try{std::memcpy((void*)(node+0x94),position.data(),12);initialize(node);registerNode(owner.world,&node,0);if(!(*(uint32_t*)(node+8)&0x40000))return false;release(node,nullptr);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool commit(const inventorybridge::Target& owner,const scripting::InventoryView& current,const scripting::InventoryChange& change){
 if(published||current.weapons!=owner.view.weapons)return false;
 uintptr_t sector=0;std::array<float,3> position{};if(!actor(owner,sector,position))return false;
 const scripting::CharacterMemory m{copyWorldMemory};
 if(change.op==scripting::InventoryOp::switchWeapons){
  if(!switchPossible||!requestSwitch||(current.weapons[0]==current.weapons[4]&&current.weapons[2]==current.weapons[6]))return false;
  published=true;if(!switchCall(owner.player))return inventorybridge::fault();int request=0;
  if(!m.get(owner.player+0xdc0,request)||request!=0x1f)return inventorybridge::fault();
  host.report("runtime","info","Native weapon switch requested; completion remains with the game action controller");inventorybridge::target={};return true;
 }
 auto expected=current.carried;uint32_t item=0;const Target* loose=nullptr;
 if(change.op==scripting::InventoryOp::pickup){
  if(!pickupPossible||!remove||expected.size()>=4096)return false;
  for(const auto& t:targets)if(t.ticket==change.worldItem){loose=&t;break;}
  if(!loose||!identity(*loose,owner))return false;
  int maximum=0;if(!m.get(base+0x2b7290,maximum)||maximum<1||!inventorybridge::fits(expected,loose->item,400,unsigned(maximum)))return false;
  const auto chosen=*loose;published=true;
  // Keep the record ID/state owned by carried inventory before native node destruction.
  if(!inventorybridge::add(owner.stats,{chosen.item,chosen.state})||!removeCall(chosen.node))return inventorybridge::fault();
  bool stillPresent=false;if(!parentContains(chosen,stillPresent)||stillPresent)return inventorybridge::fault();
  item=chosen.item;expected.push_back({chosen.item,chosen.state,40,4});
 }else if(change.op==scripting::InventoryOp::drop){
  if(!dropPossible||!convert||!create||!initialize||!registerNode||!release||change.row>=expected.size()||!withinReach(position,change.position)||!queueReady(owner.world))return false;
  const auto row=expected[change.row];if(!ordinary(row.item,row.state))return false;
  for(const auto& worn:current.worn)if(worn.item==row.item)return false;
  for(size_t i=0;i<current.weapons.size();i+=2)if(current.weapons[i]==row.item)return false;
  uintptr_t library=0,db=0;if(!m.get(owner.world+0x1db8,library)||!library||!readable(library,0x18)||!m.get(base+0x4ef930,db)||!db)return false;
  published=true;uintptr_t node=0;uint32_t actual=0;
  if(!createCall(sector,row,node,actual))return inventorybridge::fault();if(!node)return false;
  uint32_t id=0,state=0,type=0;uintptr_t body=0;
  if(!m.get(node+0x180,id)||id!=actual||!m.get(node+0x184,state)||state!=row.state||!m.get(node+0x160,body)||!body||!m.get(body+0xc,type)||
   !(type&0x10000000)||(type&0xff000000)==0x1a000000||!treeReady(node)||!publishDrop(owner,node,change.position)||!inventorybridge::compact(owner.stats,change.row))return inventorybridge::fault();
  dropped=node;item=row.item;expected.erase(expected.begin()+change.row);
 }else if(change.op==scripting::InventoryOp::spawnItem){
  // A database ID only: a level-local ID names one mutable record, which a second node would share.
  const scripting::InventoryRow row{change.item,0};
  if(!dropPossible||!convert||!create||!initialize||!registerNode||!release||(row.item&0xc0000000u)||!ordinary(row.item,0)||!withinReach(position,change.position)||!queueReady(owner.world))return false;
  uintptr_t library=0,db=0;if(!m.get(owner.world+0x1db8,library)||!library||!readable(library,0x18)||!m.get(base+0x4ef930,db)||!db)return false;
  published=true;uintptr_t node=0;uint32_t actual=0;
  if(!createCall(sector,row,node,actual))return inventorybridge::fault();if(!node)return false;
  uint32_t id=0,state=0,type=0;uintptr_t body=0;
  if(!m.get(node+0x180,id)||id!=actual||!m.get(node+0x184,state)||state||!m.get(node+0x160,body)||!body||!m.get(body+0xc,type)||
   !(type&0x10000000)||(type&0xff000000)==0x1a000000||!treeReady(node)||!publishDrop(owner,node,change.position))return inventorybridge::fault();
  scripting::InventoryView after;
  if(!inventorybridge::rows(owner.stats,after)||after.carried!=current.carried||after.worn!=current.worn||after.weapons!=current.weapons)return inventorybridge::fault();
  inventorybridge::target={};targets.clear();worldCaptured=false;worldCopy={};return true;
 }else return false;
 scripting::InventoryView after;
 if(!inventorybridge::rows(owner.stats,after)||after.carried!=expected||after.worn!=current.worn||after.weapons!=current.weapons||!inventorybridge::finish(owner.stats,owner.player,item,false))return inventorybridge::fault();
 inventorybridge::target={};targets.clear();worldCaptured=false;worldCopy={};return true;
}
}
