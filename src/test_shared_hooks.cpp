#include "plugin.cpp"
#include <cstdio>
#include "test_assert.h"
// Shared owners (runtime 0.25.0): title rows and member reads, their export rules, the refusal to hook an already
// detoured entry, and the Lua title.add_row / levels.serve paths. The menu draw/hit arithmetic itself is the
// reviewed editor/Endless method and needs the game's own drawing functions; it is not exercised here.
static std::vector<std::string> logged;
static void capture(void*,const char* id,const char* level,const char* message){logged.push_back(std::string(id)+"|"+level+"|"+message);}
static bool saw(const std::string& part){for(const auto& line:logged)if(line.find(part)!=std::string::npos)return true;return false;}
static bool load(const char* id,const std::string& source,const char* caps){return host.load(id,source.data(),source.size(),caps);}
template<class T>static void put(uint8_t* at,T value){std::memcpy(at,&value,sizeof value);}
static int actions=0;static void* lastContext=nullptr;
static void action(void* context){++actions;lastContext=context;}
static void other(void*){}
static void selfRemove(void*){++actions;assert(remove_title_row("SELF"));}   // must not wait for itself
static uint64_t baseActivations=0;static int lastBaseRow=0;
static uint64_t activateFixture(uintptr_t obj,uintptr_t,uintptr_t){++baseActivations;lastBaseRow=*(int*)(obj+0x70);return 7;}
// Member reads: a fake package {count +8, table +0x10} and buffer {data +0, length +8, capacity +0xc, type +0x10}.
struct Buffer {void* data=nullptr;uint32_t length=0,capacity=0,type=0;};
static std::vector<uint8_t> heap;
static char resizeFixture(void* b,unsigned size){auto buffer=(Buffer*)b;heap.assign(size,0);buffer->data=heap.data();buffer->length=size;buffer->capacity=size;return 1;}
static char originalRead(uintptr_t,unsigned,void* b){auto buffer=(Buffer*)b;resizeFixture(b,3);std::memcpy(buffer->data,"OLD",3);return 1;}
static int highCalls=0,lowCalls=0;
static bool high(void*,uintptr_t,int,const char* name,uint32_t,uintptr_t buffer){++highCalls;if(std::strcmp(name,"arena1.rfc"))return false;return resize_member_buffer(buffer,4)&&(std::memcpy(((Buffer*)buffer)->data,"HIGH",4),true);}
static bool low(void*,uintptr_t,int,const char*,uint32_t,uintptr_t buffer){++lowCalls;return resize_member_buffer(buffer,3)&&(std::memcpy(((Buffer*)buffer)->data,"LOW",3),true);}
static bool lowClaims(void*,const char* name){return !std::strcmp(name,"arena1.rfc");}
static bool faulting(void*,uintptr_t,int,const char*,uint32_t,uintptr_t){*(volatile int*)nullptr=1;return true;}
static bool remover(void*,uintptr_t,int,const char*,uint32_t,uintptr_t){assert(remove_member_provider(remover,nullptr));return false;}
int main(){
 using namespace scripting;setvbuf(stdout,nullptr,_IONBF,0);host.setLogger(capture);
 auto image=(uint8_t*)VirtualAlloc(nullptr,0x600000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);assert(image);base=(uintptr_t)image;
 assert(shared_hooks_version()==1&&!title_rows_ready()&&!member_providers_ready());
 assert(!add_title_row("ENDLESS",3,action,nullptr));   // nothing is offered before the runtime checked the targets
 // Install refuses a target that is already detoured, and hooks nothing; available() itself never hooks.
 titlerows::drawWasOriginal=true;image[titlerows::rva::menuBuild]=0xe9;assert(titlerows::available(true).find("menu build 0x122c40 is already detoured")!=std::string::npos&&!title_rows_ready());
 titlerows::drawWasOriginal=false;assert(titlerows::available(true).find("menu draw 0x128f30 is not the runtime's own")!=std::string::npos);
 image[0x6f2d0]=0xe9;assert(memberreads::available().find("already detoured")!=std::string::npos&&!member_providers_ready());
 // A target hooked by someone else after availability was decided: the first registration checks again.
 titlerows::ready=true;titlerows::originalMenuDraw=(titlerows::MenuDraw)(base+0x1000);assert(!add_title_row("LATE",3,action,nullptr)&&!titlerows::ready&&!titlerows::installed);
 std::puts("LUA_SHARED_HOOKS_REFUSE_PASSED");
 // The rest runs as if the targets were free and the hooks made (no real hooking in this process).
 titlerows::ready=true;titlerows::installed=true;memberreads::ready=true;memberreads::hooked=true;

 // Registration rules.
 assert(!add_title_row("",3,action,nullptr)&&!add_title_row("A label that is far too long for a row",3,action,nullptr)&&!add_title_row("BAD\n",3,action,nullptr));
 assert(!add_title_row("ENDLESS",0,action,nullptr)&&!add_title_row("ENDLESS",7,action,nullptr)&&!add_title_row("ENDLESS",3,nullptr,nullptr));
 assert(add_title_row("ENDLESS",3,action,(void*)1)&&add_title_row("ENDLESS",3,action,(void*)2)&&!add_title_row("ENDLESS",3,other,nullptr));   // same action may move; another may not take it
 assert(add_title_row("EDITOR",4,other,nullptr)&&set_title_row_enabled("EDITOR",false)&&!set_title_row_enabled("NOPE",true));
 for(int i=0;i<4;++i)assert(add_title_row(("R"+std::to_string(i)).c_str(),6,other,nullptr));
 assert(!add_title_row("R4",6,other,nullptr));   // six extra rows at most (the inline row records hold six)
 for(int i=0;i<4;++i)assert(remove_title_row(("R"+std::to_string(i)).c_str()));assert(!remove_title_row("R0"));
 assert(!add_title_row("R5",6,other,nullptr));   // removed rows' slots wait for the menu to be rebuilt
 titlerows::releaseRetired();

 // Activation routing: an extra row runs its action; a base row passes to the game with its original number.
 // Its own page: readable() requires one memory region, and a static array can straddle the end of .bss.
 const auto form=(uint8_t*)VirtualAlloc(nullptr,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(form);put(image+titlerows::rva::menuObject,(uintptr_t)form);put(form+0x208,int32_t(0));
 titlerows::originalMenuActivate=activateFixture;auto& armed=titlerows::armed;armed.on=true;armed.obj=(uintptr_t)form;armed.count=7;
 for(int i=0;i<7;++i){armed.extraIndex[i]=-1;armed.baseIndex[i]=i<4?i+1:i;}armed.extraIndex[3]=0;armed.baseIndex[3]=0;   // BEGIN, CONTINUE, ARENA, ENDLESS, MANUAL...
 put(form+0x70,int32_t(4));assert(titlerows::menuActivate((uintptr_t)form,0,0)==0&&actions==1&&lastContext==(void*)2&&*(int*)(form+0x70)==0&&baseActivations==0);
 put(form+0x70,int32_t(5));assert(titlerows::menuActivate((uintptr_t)form,0,0)==7&&lastBaseRow==4&&baseActivations==1);
 assert(add_title_row("SELF",3,selfRemove,nullptr));for(int e=0;e<titlerows::maxExtra;++e)if(titlerows::extras[e].used&&!std::strcmp(titlerows::extras[e].label.text,"SELF"))armed.extraIndex[3]=e;
 put(form+0x70,int32_t(4));titlerows::menuActivate((uintptr_t)form,0,0);assert(actions==2&&titlerows::activating==0);
 put(form+0x70,int32_t(4));titlerows::menuActivate((uintptr_t)form,0,0);assert(actions==2);   // removed: never called again
 armed.on=false;titlerows::releaseRetired();
 std::puts("LUA_TITLE_ROWS_PASSED");

 // Member reads: ascending priority, the first true wins, later claims are noted, faults pass on.
 memberreads::original=originalRead;memberreads::resize=resizeFixture;
 static uint8_t table[0x40]{},package[0x40]{};std::memcpy(table,"arena1.rfc",10);std::memcpy(table+0x20,"exanima01.rfc",13);
 static const char path[]="C:\\Games\\Exanima\\Resource.rpk";
 put(package+8,uint32_t(2));put(package+0x10,(uintptr_t)table);put(package+0x18,(uintptr_t)path);
 Buffer buffer;assert(memberreads::memberRead((uintptr_t)package,0,&buffer)==1&&!std::memcmp(buffer.data,"OLD",3));   // no providers
 assert(!add_member_provider(high,nullptr,nullptr,50)&&add_member_provider(low,lowClaims,nullptr,200)&&add_member_provider(high,nullptr,nullptr,100)&&!add_member_provider(high,nullptr,nullptr,100));
 assert(memberreads::memberRead((uintptr_t)package,0,&buffer)==1&&!std::memcmp(buffer.data,"HIGH",4)&&highCalls==1&&lowCalls==0);
 memberreads::deliver(host);assert(saw("Member arena1.rfc was served by a higher-priority provider"));
 assert(memberreads::memberRead((uintptr_t)package,1,&buffer)==1&&!std::memcmp(buffer.data,"LOW",3)&&lowCalls==1);
 buffer.type=2;assert(memberreads::memberRead((uintptr_t)package,1,&buffer)==1&&!std::memcmp(buffer.data,"OLD",3));buffer.type=0;   // a buffer kind the game does not heap-resize
 assert(add_member_provider(faulting,nullptr,nullptr,60));assert(memberreads::memberRead((uintptr_t)package,0,&buffer)==1&&!std::memcmp(buffer.data,"HIGH",4)&&memberreads::faults==1);
 assert(remove_member_provider(faulting,nullptr)&&add_member_provider(remover,nullptr,nullptr,55));
 memberreads::memberRead((uintptr_t)package,0,&buffer);assert(!remove_member_provider(remover,nullptr));   // removed itself, without waiting
 assert(remove_member_provider(high,nullptr)&&remove_member_provider(low,nullptr)&&!remove_member_provider(low,nullptr));
 std::puts("LUA_MEMBER_READS_PASSED");

 // Lua: title.add_row while loading (added on success), its function on activation; levels.serve from callbacks.
 host.setSharedApi({sharedlua::addRow,sharedlua::removeRow,sharedlua::enableRow,sharedlua::serve,sharedlua::clearServed,sharedlua::forget});
 host.capabilities(titleRows|levelsServe|timerEvents);
 assert(!load("early","levels.serve('arena1.rfc','x','Resource.rpk')","levels.serve"));
 assert(!load("taken","title.add_row('EDITOR',4,function()end)","title.rows")||saw("title row 'EDITOR' was not added"));
 assert(load("endless",R"(local runs=0
assert(title.add_row('ENDLESS LUA',3,function()
 runs=runs+1
 assert(levels.serve('ARENA1.rfc','GENERATED','resource.RPK') and levels.serve('exanima01.rfc','ELSEWHERE','Characters.rpk'))
 assert(not pcall(levels.serve,'a name longer than 16','x','Resource.rpk') and not pcall(levels.serve,'x','y') and not pcall(levels.serve,'x','y','dir/Resource.rpk'))
 if runs==2 then error('second run') end
end))
assert(not pcall(title.add_row,'X',9,function()end) and not pcall(title.set_enabled,'ENDLESS LUA',false))
)","title.rows,levels.serve"));
 bool listed=false;for(const auto& e:titlerows::extras)listed=listed||(e.used&&!std::strcmp(e.label.text,"ENDLESS LUA"));assert(listed);
 sharedlua::Row* row=nullptr;for(auto& r:sharedlua::rows)if(r.used&&r.label=="ENDLESS LUA")row=&r;assert(row);
 sharedlua::pressed(row);sharedlua::deliver(host);assert(host.enabled("endless"));
 assert(memberreads::memberRead((uintptr_t)package,0,&buffer)==1&&buffer.length==9&&!std::memcmp(buffer.data,"GENERATED",9));
 assert(memberreads::memberRead((uintptr_t)package,1,&buffer)==1&&!std::memcmp(buffer.data,"OLD",3));   // served for another package only
 put(table+0x18,uint32_t(1));assert(memberreads::memberRead((uintptr_t)package,0,&buffer)==1&&!std::memcmp(buffer.data,"OLD",3));put(table+0x18,uint32_t(0));   // other archive types keep their bytes
 sharedlua::pressed(row);sharedlua::deliver(host);assert(!host.enabled("endless")&&saw("title row handler failed"));
 listed=false;for(const auto& e:titlerows::extras)listed=listed||(e.used&&!std::strcmp(e.label.text,"ENDLESS LUA"));assert(!listed);   // gone with the addon
 assert(memberreads::memberRead((uintptr_t)package,0,&buffer)==1&&!std::memcmp(buffer.data,"OLD",3));   // and so are its served members
 // A reload keeps rows the new version adds again and drops the rest.
 assert(load("menu","title.add_row('ONE',3,function()end) title.add_row('TWO',3,function()end)","title.rows"));
 assert(load("menu","title.add_row('ONE',3,function()end)","title.rows"));
 bool one=false,two=false;for(const auto& e:titlerows::extras){one=one||(e.used&&!std::strcmp(e.label.text,"ONE"));two=two||(e.used&&!std::strcmp(e.label.text,"TWO"));}assert(one&&!two);
 assert(host.unload("menu"));one=false;for(const auto& e:titlerows::extras)one=one||(e.used&&!std::strcmp(e.label.text,"ONE"));assert(!one);
 // Startup overrides from mods/<id>/manifest.toml: inside the folder only, priority 60.
 std::filesystem::create_directories("ovr/com.test.override/assets");
 {std::ofstream("ovr/com.test.override/manifest.toml")<<"[script]\nentry = \"main.lua\"\n\n[script.overrides]\n\"Resource.rpk/exanima01.rfc\" = \"assets/level.bin\"\n\"Resource.rpk/arena1.rfc\" = \"../escape.bin\"\n\"Resource.rpk/missing.rfc\" = \"assets/none.bin\"\n# a comment\n";
  std::ofstream("ovr/com.test.override/assets/level.bin",std::ios::binary)<<"OVERRIDE";std::ofstream("ovr/escape.bin",std::ios::binary)<<"ESCAPED!";}
 sharedlua::loadOverrides("com.test.override","ovr",host);
 assert(saw("override Resource.rpk/exanima01.rfc ready")&&saw("override Resource.rpk/arena1.rfc not used: the file must be inside the addon folder")&&saw("override Resource.rpk/missing.rfc not used: the file is missing"));
 assert(memberreads::memberRead((uintptr_t)package,1,&buffer)==1&&buffer.length==8&&!std::memcmp(buffer.data,"OVERRIDE",8));
 assert(memberreads::memberRead((uintptr_t)package,0,&buffer)==1&&!std::memcmp(buffer.data,"OLD",3));
 std::puts("LUA_SHARED_LUA_PASSED");

 // world.nearby: placed objects by model name around the player, live positions, nearest first, no 512 cap.
 static uint8_t root[0x200]{},player[0x1400]{};static std::vector<std::array<uint8_t,0x200>> nodes(700);static std::vector<uintptr_t> children(700);
 put(root+8,uint32_t(0x11));put(root+fp::game::sceneNode::lastChild,int32_t(699));put(root+fp::game::sceneNode::children,(uintptr_t)children.data());
 for(size_t i=0;i<nodes.size();++i){auto* n=nodes[i].data();put(n+8,uint32_t(0x2000));std::memcpy(n+fp::game::sceneNode::name,i==650?"SharpeningWheel":i==10?"sharpenwheel a01":"Barrel",i==650?15:i==10?16:6);
  const float at[3]={float(i==650?100:i==10?250:0),0.f,0.f};std::memcpy(n+0x64,at,12);children[i]=(uintptr_t)n;}
 put(image+0x4f0bd0,(uintptr_t)root);put(image+0x4f0830,(uintptr_t)player);const float centre[3]={0,0,0};std::memcpy(player+0xab4,centre,12);
 worldPossible=true;host.setNearbyRead(readNearby);host.capabilities(worldRead|timerEvents);
 assert(load("grind",R"(timers.after(0,function()
 local r=world.nearby(300,{'sharpeningwheel','SHARPENWHEEL A01'})
 assert(#r.objects==2 and r.objects[1].name=='SharpeningWheel' and math.abs(r.objects[1].distance-100)<1e-3 and r.objects[2].distance>249 and not r.truncated)
 assert(#world.nearby(150,{'sharpenwheel a01'}).objects==0)
 assert(not pcall(world.nearby,0,{'x'}) and not pcall(world.nearby,10,{}) and not pcall(world.nearby,10,{'a name far too long'}))
end))","world.read,timers"));
 worldPulse=true;host.setTime(1000);host.runTimers();worldPulse=false;assert(host.enabled("grind"));
 assert(load("menu-nearby","timers.after(0,function()assert(world.nearby(300,{'Barrel'})==nil)end)","world.read,timers"));host.setTime(2000);host.runTimers();assert(host.enabled("menu-nearby"));
 std::puts("LUA_WORLD_NEARBY_PASSED");

 // Staged game calls: made after the callback returns, on the living player only, with a save backup.
 struct Calls {uintptr_t character=0;float balance=0,health=0,dir[3]{},amount=0;char fall=0;uintptr_t scene=0;uint32_t record=0,state=0;unsigned damages=0,pushes=0,drives=0;};
 static Calls calls;
 nativecalls::damage=[](uintptr_t c,float b,float h,char f)->unsigned char{calls.character=c;calls.balance=b;calls.health=h;calls.fall=f;++calls.damages;return 0;};
 nativecalls::push=[](uintptr_t c,const float* d,float a,char){calls.character=c;std::memcpy(calls.dir,d,12);calls.amount=a;++calls.pushes;};
 nativecalls::drive=[](uintptr_t s,uint32_t r,uint32_t,uint32_t st){calls.scene=s;calls.record=r;calls.state=st;++calls.drives;};
 host.setNativeCommit(nativecalls::commit);host.capabilities(playerDamage|playerPush|levelsDrive|timerEvents);gamebridge::backupDone=true;
 put(player+scripting::lifeStageAt,uint32_t(0));put(player+npc::off::health,0.8f);
 assert(!load("early-damage","player.damage(0.1,0.1)","player.damage"));
 assert(load("hazards",R"(timers.after(0,function()
 assert(player.damage(0.2,0.1,true) and player.push(0,0,3,2) and levels.drive(0x80000042,'open') and levels.drive(7,-1))
 assert(not pcall(player.damage,-1,0) and not pcall(player.push,0,0,0,1) and not pcall(levels.drive,7,'ajar') and not pcall(levels.drive,7,2))
 assert(not pcall(player.damage,0,0))   -- a fifth staged call
end))","player.damage,player.push,levels.drive,timers"));
 worldPulse=true;host.setTime(3000);host.runTimers();worldPulse=false;
 assert(host.enabled("hazards")&&calls.damages==1&&calls.character==(uintptr_t)player&&calls.balance>0.19f&&calls.health>0.09f&&calls.fall==1);
 assert(calls.pushes==1&&calls.dir[2]==1.f&&calls.amount==2.f&&calls.drives==2&&calls.scene==(uintptr_t)root+0x220&&calls.record==7&&calls.state==0xffffffffu);
 assert(load("staged-error","timers.after(0,function()player.damage(0.1,0.1) error('after staging')end)","player.damage,timers"));
 worldPulse=true;host.setTime(4000);host.runTimers();worldPulse=false;assert(!host.enabled("staged-error")&&calls.damages==1);   // a failed callback makes no call
 put(player+scripting::lifeStageAt,uint32_t(2));
 assert(load("downed","timers.after(0,function()player.damage(0.1,0.1)end)","player.damage,timers"));
 worldPulse=true;host.setTime(5000);host.runTimers();worldPulse=false;assert(host.enabled("downed")&&calls.damages==1&&saw("staged game call(s) refused"));
 std::puts("LUA_NATIVE_CALLS_PASSED");

 // gear.use: the gate lifts carried gear only while someone listens; the click runs the handler; restore adds jobs.
 static std::map<std::pair<uint32_t,uint32_t>,std::vector<uint8_t>> records;
 auto record=[](uint32_t id,uint32_t tag,size_t payload)->uint8_t*{auto& c=records[{id,tag}];c.assign(8+payload,0);put(c.data(),tag);put(c.data()+4,uint32_t(8+payload));return c.data()+8;};
 const uint32_t sword=0xa0000001u,helm=0xa0000002u,orb=0xa0000003u,worn=0xa0000004u;
 uint8_t* w=record(sword,0x4000,0x60);w[0x0c]=180;w[0x0d]=200;w[0x0e]=90;record(sword,0x100000,0x30);
 uint8_t* h=record(helm,0x1000,0x224);h[0x1b0]=100;h[0x1b1]=150;h[0x1b2]=40;put(h+0x1ac,int32_t(-1));record(helm,0x100000,0x30);
 record(orb,0x10000,0x20);record(orb,0x4000,0x60);record(worn,0x4000,0x60);
 containerbridge::chunk=[](uint32_t id,uint32_t tag)->uintptr_t{const auto f=records.find({id,tag});return f==records.end()?0:(uintptr_t)f->second.data();};
 itemstats::lookup=containerbridge::chunk;gamebridge::api.getChunk=containerbridge::chunk;
 static uint8_t stats[0x200]{},carried[4*12]{},wornRows[8]{};put(player+0x10a0,(uintptr_t)stats);
 put(carried,sword);put(carried+12,helm);put(carried+24,orb);put(wornRows,worn);
 put(stats+0xf0,int32_t(3));put(stats+0xf8,(uintptr_t)carried);put(stats+0x84,int32_t(1));put(stats+0x88,(uintptr_t)wornRows);
 gear::originalGate=[](uint32_t,uint32_t)->unsigned char{return 0;};gear::installed=true;host.capabilities(gearUse|itemsCondition);
 assert(gear::gate(sword,0)==0);   // nobody listens yet
 assert(load("grindstone",R"(local n=0
events.on('gear.use',function(e)
 n=n+1
 if n==1 then
  assert(e.item.id==0xa0000001 and e.item.kind=='weapon' and e.item.carried and e.item.condition.wear==200 and e.item.condition.dirt==90)
  assert(e.target.kind=='other' and e.target.distance==nil)
  assert(not pcall(e.restore_condition,e,{wear=201}) and not pcall(e.restore_condition,e,{}))
  assert(e:restore_condition({wear=150,dirt=0}))
  assert(not pcall(e.restore_condition,e,{wear=100}))
  return true
 elseif n==2 then
  assert(e.item.kind=='apparel' and e:restore_condition({wear=10}))
  return true
 else return false end
end))","events.gear_use,items.condition"));
 gear::deliver(host);assert(gear::listening);
 assert(gear::gate(sword,0)==2&&gear::gate(helm,0)==2&&gear::gate(orb,0)==0&&gear::gate(worn,0)==0&&gear::gate(0x12345,0)==0);
 static uint8_t control[0x200]{};put(image+0x4efdd0,(uintptr_t)control);put(image+0x4997d0,uint32_t(777000));
 static std::vector<uint8_t> jobs;
 gear::setLength=[](void** array,void*,int64_t,int64_t* length){jobs.resize(size_t(*length)*0x20);*array=jobs.data();};gear::restorePossible=true;gamebridge::backupDone=true;
 assert(gear::use((uintptr_t)control,sword));
 assert(*(int32_t*)(control+0x108)==2&&*(uint32_t*)jobs.data()==10&&*(uint32_t*)(jobs.data()+4)==sword&&*(uint32_t*)(jobs.data()+8)==777000&&*(int32_t*)(jobs.data()+0xc)==200&&*(int32_t*)(jobs.data()+0x10)==150);
 assert(*(uint32_t*)(jobs.data()+0x20)==9&&*(int32_t*)(jobs.data()+0x2c)==90&&*(int32_t*)(jobs.data()+0x30)==0);
 assert(gear::use((uintptr_t)control,helm)&&*(int32_t*)(control+0x108)==2&&saw("restore_condition refused"));   // this helmet is marked unrepairable (+0x1ac == -1)
 assert(!gear::use((uintptr_t)control,orb));   // not gear: the game's own use
 assert(gear::use((uintptr_t)control,sword)&&host.enabled("grindstone"));   // third: handler returns false, the click is still taken
 std::puts("LUA_GEAR_USE_PASSED");

 // levels.new_game: one start swaps exanima01; any other name or a base-row choice passes untouched.
 static std::string loaded;newgame::original=[](const char* name)->uintptr_t{loaded.assign(name,strnlen(name,16));return 1;};
 newgame::possible=true;newgame::installed=true;titlerows::onBaseRow=newgame::baseRowChosen;
 host.setSharedApi({sharedlua::addRow,sharedlua::removeRow,sharedlua::enableRow,sharedlua::serve,sharedlua::clearServed,sharedlua::forget,newgame::arm,sharedlua::readAsset});
 host.capabilities(levelsFlow|titleRows|timerEvents|assetsRead);
 assert(load("endless-start",R"(assert(title.add_row('ENDLESS RUN',3,function() assert(levels.new_game('arena1.rfc')) end,{acts_as=1}))
assert(not pcall(title.add_row,'X',3,function()end,{acts_as=7}))
)","levels.flow,title.rows"));
 static const char exanima01[16]="exanima01";static const char arena[16]="arena";
 sharedlua::Row* start=nullptr;for(auto& r:sharedlua::rows)if(r.used&&r.label=="ENDLESS RUN")start=&r;assert(start);
 sharedlua::pressed(start);sharedlua::deliver(host);
 newgame::load(arena);assert(loaded=="arena");newgame::load(exanima01);assert(loaded=="arena1.rfc");newgame::load(exanima01);assert(loaded=="exanima01");
 sharedlua::pressed(start);sharedlua::deliver(host);newgame::baseRowChosen(1);newgame::load(exanima01);assert(loaded=="exanima01");   // a plain BEGIN after cancelling
 // The row runs its action, then the base row's own game action.
 int slot=-1;for(int e=0;e<titlerows::maxExtra;++e)if(titlerows::extras[e].used&&!std::strcmp(titlerows::extras[e].label.text,"ENDLESS RUN"))slot=e;assert(slot>=0&&titlerows::extras[slot].actsAs==1);
 armed.on=true;armed.obj=(uintptr_t)form;armed.count=7;for(int i=0;i<7;++i){armed.extraIndex[i]=-1;armed.baseIndex[i]=i<4?i+1:i;}armed.extraIndex[3]=slot;
 baseActivations=0;put(form+0x70,int32_t(4));assert(titlerows::menuActivate((uintptr_t)form,0,0)==7&&lastBaseRow==1&&baseActivations==1&&start->pressed);
 armed.on=false;
 // assets.read: inside mods/<id> only.
 sharedlua::modsDirectory="ovr";
 assert(host.load("com.test.override","local b=assets.read('assets/level.bin') assert(b=='OVERRIDE') local x,why=assets.read('../escape.bin') assert(x==nil and why:find('inside')) assert(assets.read('assets/none.bin')==nil)",
  std::strlen("local b=assets.read('assets/level.bin') assert(b=='OVERRIDE') local x,why=assets.read('../escape.bin') assert(x==nil and why:find('inside')) assert(assets.read('assets/none.bin')==nil)"),"assets.read"));
 std::puts("LUA_ENDLESS_START_PASSED");
}
