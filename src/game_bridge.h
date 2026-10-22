#pragma once
namespace gear {   // gear_bridge.h
static bool use(uintptr_t control,uint32_t item);static bool isGear(uint32_t item,std::string& kind);static bool carriedOnly(uint32_t item);
static bool condition(uint32_t item,scripting::Condition& out);static bool restore(uint32_t item,const std::string& kind,int wear,int dirt);
static void consumeLater(uint32_t item);}
namespace gamebridge {
using InventoryUse=void(*)(uintptr_t,uintptr_t);
using Use=uintptr_t(*)(uintptr_t,uintptr_t);
static InventoryUse originalInventoryUse=nullptr;
static Use originalUse=nullptr;
static std::atomic<bool> active{false};
static bool backupDone=false,backupFailed=false;
constexpr uint32_t containerTag=0x20000,deviceTag=0x10000,lookTag=0x100000,removedFlags=0xc80000;
static uint32_t word(const unsigned char* p){uint32_t v;std::memcpy(&v,p,4);return v;}
struct Api {
 uintptr_t(*getChunk)(uint32_t,uint32_t)=nullptr;
 uintptr_t(*openStorage)(uint32_t)=nullptr;
 uintptr_t(*focus)(uintptr_t)=nullptr;
 bool(*pick)(uintptr_t,float,float,void*)=nullptr;
 uintptr_t(*object)(uintptr_t)=nullptr;
 int(*interaction)(uintptr_t)=nullptr;
 void(*unlock)(uintptr_t)=nullptr;
 void(*screenToLocal)(uintptr_t,int*,int*)=nullptr;
};
static Api api;
static std::string reportedError;
static scripting::Result dispatch(const scripting::Event& event){
 const auto result=host.dispatch(event);
 if(host.error()!=reportedError){reportedError=host.error();OutputDebugStringA(reportedError.c_str());}
 return result;
}
static bool atMenu(){return readable(base+0x4f0830,8)&&*(uintptr_t*)(base+0x4f0830)==0;}
static const unsigned char* chunk(uint32_t id,uint32_t tag,size_t minimum){
 const auto p=api.getChunk(id,tag);if(!readable(p,8))return nullptr;
 const auto b=(const unsigned char*)p;const auto n=word(b+4);
 return word(b)==tag&&n>=minimum&&n<=1024*1024&&readable(p,n)?b:nullptr;
}
static std::string model(uint32_t id){
 const auto b=chunk(id,lookTag,24);if(!b)return {};
 const char* name=(const char*)b+8;size_t n=0;while(n<16&&name[n])++n;
 return std::string(name,n);
}
static bool saveBackup(){
 if(backupDone)return true;if(backupFailed)return false;
 wchar_t path[32768]{};const auto n=GetModuleFileNameW(module,path,DWORD(std::size(path)));
 if(!n||n>=std::size(path)){backupFailed=true;return false;}
 const auto slash=wcsrchr(path,L'\\');if(!slash){backupFailed=true;return false;}slash[1]=0;
 const bool own=saves::adoptGameFolder(base+0x4985d0);std::wstring made;unsigned copied=0,failed=0;
 backupDone=saves::backup(path,made,copied,failed);
 char where[600]{};WideCharToMultiByte(CP_UTF8,0,saves::folder().c_str(),-1,where,int(sizeof where),nullptr,nullptr);
 char line[800];snprintf(line,sizeof line,"Save backup %s from %s%s: %u file(s), %u problem(s)",backupDone?"made":"FAILED",where,
  own?"":" (default: the game's own save folder was not readable)",copied,failed);
 host.report("runtime",backupDone?"info":"error",line);backupFailed=!backupDone;
 return backupDone;
}
static bool storageClosed(uint32_t id){
 const auto gui=*(uintptr_t*)(base+0x499790);if(!gui)return true;
 if(!readable(gui,0xb8))return false;
 const int n=*(int*)(gui+0xa8);const auto list=*(uintptr_t*)(gui+0xb0);
 if(n<0||n>4096|| (n&&!readable(list,size_t(n)*8)))return false;
 for(int i=0;i<n;++i){const auto form=*(uintptr_t*)(list+size_t(i)*8);
  if(!readable(form,0x160))return false;
  if(*(uint32_t*)(form+0xb8)==0xce1c2013&&*(uint32_t*)(form+0x15c)==id)return false;
 }
 return true;
}
static bool containerSnapshot(scripting::Event& event){
 const auto b=chunk(event.item,containerTag,32);if(!b)return true;
 const auto n=word(b+28);if(n>(word(b+4)-32)/12)return false;
 event.container=true;event.storageOpen=!storageClosed(event.item);
 if(event.storageOpen)return true;
 for(uint32_t i=0;i<n;++i){const auto row=b+32+size_t(i)*12;const auto id=word(row);
  const auto device=chunk(id,deviceTag,10);
  const bool ordinary=device&&device[8]==1&&device[9]==0&&!chunk(id,containerTag,32);
  event.entries.push_back({id,ordinary,bool(word(row+4)&removedFlags)});
 }
 return true;
}
static bool doorStillValid(uintptr_t control,uintptr_t object,uintptr_t node,uintptr_t door,const scripting::Event& event,uint32_t record){
 if(!readable(control,0x24)||*(uint32_t*)(control+0x20)!=event.item||!readable(object,0x188)||*(uint32_t*)(object+0x180)!=record)return false;
 const auto nodes=*(uintptr_t*)(object+0x120);
 if(!readable(nodes,8)||*(uintptr_t*)nodes!=node||!readable(node,0x168)||*(uintptr_t*)(node+0x160)!=door||!readable(door,0x608))return false;
 return *(unsigned char*)(door+0x604)&&!*(unsigned char*)(door+0x603);
}
static void inventoryUseBody(uintptr_t control,uintptr_t inventory){
 std::lock_guard<std::recursive_mutex> lock(scriptMutex);
 if(runtimeEnabled&&!session.ownsThread(GetCurrentThreadId())){originalInventoryUse(control,inventory);return;}
 if(!readable(control,0x40))return;
 scripting::Event event;event.item=*(uint32_t*)(control+0x20);event.model=model(event.item);
 if(!host.handles(event.model)){originalInventoryUse(control,inventory);return;}
 if(!saveBackup()||!containerSnapshot(event)||!readable(inventory,0x168))return;
 const auto mouse=*(uintptr_t*)(base+0x49a960);if(!readable(mouse,0x20))return;
 int x=*(int*)(mouse+0x18),y=*(int*)(mouse+0x1c);api.screenToLocal(inventory,&x,&y);
 const auto table=*(uintptr_t*)inventory;if(!readable(table,0x128))return;
 alignas(16) unsigned char position[16]{};uint32_t flags=0;
 const auto hit=(int(*)(uintptr_t,int,int,void*,void*))(*(uintptr_t*)(table+0x120));
 const int index=hit(inventory,x,y,position,&flags),count=*(int*)(inventory+0x158);
 const auto rows=*(uintptr_t*)(inventory+0x160);
 if(index<1||index>count||count>65535||!readable(rows,size_t(count)*24))return;
 event.target="inventory";event.targetItem=*(uint32_t*)(rows+size_t(index-1)*24+8);
 if(gear::isGear(event.targetItem,event.targetGear)){event.targetCarried=gear::carriedOnly(event.targetItem);gear::condition(event.targetItem,event.targetCondition);}
 const auto result=dispatch(event);
 if(result.action==scripting::Action::openStorage)api.openStorage(event.item);
 if(result.handled&&result.restore&&!gear::restore(event.targetItem,event.targetGear,result.wear,result.dirt))
  host.report("runtime","warn","restore_condition refused: repair jobs unavailable, no save backup, or the item changed");
 if(result.handled&&result.consume)gear::consumeLater(event.item);
 if(!result.handled)originalInventoryUse(control,inventory);
}
static uintptr_t useBody(uintptr_t control,uintptr_t extra){
 std::lock_guard<std::recursive_mutex> lock(scriptMutex);
 if(runtimeEnabled&&!session.ownsThread(GetCurrentThreadId()))return originalUse(control,extra);
 if(!readable(control,0x40))return 0;
 scripting::Event event;event.item=*(uint32_t*)(control+0x20);event.model=model(event.item);
 if(gear::use(control,event.item))return 0;   // weapons and apparel lifted by gear.use (gear_bridge.h)
 if(!host.handles(event.model))return originalUse(control,extra);
 if(!saveBackup()||!containerSnapshot(event))return 0;
 const auto mouse=*(uintptr_t*)(base+0x49a960);if(!readable(mouse,0x20))return 0;
 const auto focus=api.focus(mouse);
 if(focus!=*(uintptr_t*)(control+0x18))return originalUse(control,extra);
 alignas(16) unsigned char hit[64]{};*(uint32_t*)(hit+0x34)=1;
 if(!api.pick(focus,float(*(int*)(mouse+0x18)),float(*(int*)(mouse+0x1c)),hit))return 0;
 const auto object=api.object(*(uintptr_t*)hit);if(!readable(object,0x188))return 0;
 uintptr_t door=0,targetNode=0;uint32_t targetRecord=0;
 if(api.interaction(object)==4){
  if(*(int*)(object+0x118)<0||!readable(*(uintptr_t*)(object+0x120),8))return 0;
  const auto node=**(uintptr_t**)(object+0x120);if(!readable(node,0x168))return 0;
  targetNode=node;targetRecord=*(uint32_t*)(object+0x180);
  door=*(uintptr_t*)(node+0x160);if(!readable(door,0x608))return 0;
  const auto data=chunk(*(uint32_t*)(object+0x180),0x40000,16);if(!data)return 0;
  event.target="door";event.requiredKey=word(data+12);
  event.doorLocked=*(unsigned char*)(door+0x604)&&!*(unsigned char*)(door+0x603);
 }
 const auto result=dispatch(event);
 if(result.action==scripting::Action::unlockDoor&&event.doorLocked&&doorStillValid(control,object,targetNode,door,event,targetRecord))api.unlock(door);
 if(result.handled&&result.consume)gear::consumeLater(event.item);
 return result.handled?0:originalUse(control,extra);
}
static void inventoryUseSafe(uintptr_t a,uintptr_t b){try{inventoryUseBody(a,b);}catch(...){}}
static uintptr_t useSafe(uintptr_t a,uintptr_t b){try{return useBody(a,b);}catch(...){return 0;}}
static void inventoryUse(uintptr_t a,uintptr_t b){
 if(!active.load()){originalInventoryUse(a,b);return;}
 __try{inventoryUseSafe(a,b);}__except(EXCEPTION_EXECUTE_HANDLER){}
}
static uintptr_t use(uintptr_t a,uintptr_t b){
 if(!active.load())return originalUse(a,b);
 __try{return useSafe(a,b);}__except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
}
