#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <iterator>
#include <mutex>
#include "saves.h"
#include "script_host.h"
#include "diagnostics.h"
#include "runtime_session.h"
#include "vendor/minhook/include/MinHook.h"
static uintptr_t base=0;
static HMODULE module=nullptr;
static SRWLOCK lifecycle=SRWLOCK_INIT;
static bool installed=false;
static scripting::Host host(0);
static scripting::Session session(host);
static uint64_t settingsCapability=0;static bool settingsInitialized=false;
static void initializeSettings(){
 if(settingsInitialized)return;settingsInitialized=true;
 wchar_t path[32768]{};const auto size=GetEnvironmentVariableW(L"LOCALAPPDATA",path,32768);
 if(size&&size<32768&&host.setSettingsRoot(std::filesystem::path(path)/L"ExanimaLua"/L"settings"))settingsCapability=scripting::settingsStorage;
}
static std::unique_ptr<scripting::FileLog> diagnostics;
#include "console_bridge.h"
static void runtimeLog(void*,const char* id,const char* level,const char* message){
 if(diagnostics)diagnostics->write(id,level,message);
 try{console::report(id,level,message);}catch(...){}
 OutputDebugStringA(message);
}
static void initializeLog(){
 if(diagnostics)return;
 wchar_t path[32768]{};const auto size=GetModuleFileNameW(module,path,32768);
 if(size&&size<32768)diagnostics=std::make_unique<scripting::FileLog>(std::filesystem::path(path).parent_path()/L"ScriptRuntime.log");
 host.setLogger(runtimeLog);
}
static void coreOnly(const char* reason){host.report("runtime","warn",reason);}
static std::atomic<bool> runtimeEnabled{false};
// Native forwarding can re-enter the inventory hook on the same event thread.
static std::recursive_mutex scriptMutex;
static bool readable(uintptr_t p,size_t n){
 MEMORY_BASIC_INFORMATION m{};
 return p>=0x10000&&p+n>=p&&VirtualQuery((void*)p,&m,sizeof m)==sizeof m&&m.State==MEM_COMMIT&&
  !(m.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&p+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
#include "game_bridge.h"
#include "runtime_contract.h"
#include "world_bridge.h"
#include "ai_bridge.h"
#include "character_bridge.h"
#include "container_bridge.h"
#include "inventory_bridge.h"
#include "world_item_bridge.h"
#include "carried_item_bridge.h"
#include "world_device_bridge.h"
#include "world_emitter_bridge.h"
#include "interaction_bridge.h"
#include "combat_bridge.h"
#include "power_bridge.h"
#include "time_bridge.h"
#include "save_bridge.h"
#include "script_command_bridge.h"
#include "item_stats_bridge.h"
#include "zone_bridge.h"
#include "message_bridge.h"
#include "level_bridge.h"
#include "title_rows_bridge.h"
#include "member_reads_bridge.h"
#include "level_placements.h"
#include "record_merge.h"
// The member-read edit stage runs every edit that wants a member: record merges first, then level placements.
static bool editWants(const char* name){return recordmerge::wants(name)||placements::wants(name);}
static bool editMember(const char* name,std::string& bytes){bool changed=false;if(recordmerge::wants(name))changed=recordmerge::edit(name,bytes)||changed;if(placements::wants(name))changed=placements::transform(name,bytes)||changed;return changed;}
#include "shared_lua_bridge.h"
#include "native_calls_bridge.h"
#include "gear_bridge.h"
#include "gesture_bridge.h"
#include "item_ids_bridge.h"
#include "npc_powers_bridge.h"
#include "dialog_bridge.h"
#include "new_game_bridge.h"
#include "settings_tabs.h"
#include "mod_settings.h"
#include "sound_bridge.h"
// An addon's shared registrations end together (unload, error, disable): rows and served members, then placements.
static void forgetShared(const std::string& owner){sharedlua::forget(owner);placements::forget(owner);}
static bool addPlacement(const std::string& owner,const std::string& level,uint32_t item,const float* position,float yaw){
 if(!memberreads::ready||!memberreads::installHook())return false;
 placements::Placement p;p.owner=owner;p.level=level;p.item=item;std::memcpy(p.pos,position,12);p.yaw=yaw;
 if(!placements::add(p))return false;memberreads::edit=editMember;memberreads::wants=editWants;return true;
}
static uint64_t sharedCapabilities(){return scripting::modSettings|(npcpowers::possible&&charactersPossible?scripting::characterPowers:0)|(sounds::possible&&worldPossible?scripting::uiSound:0)|(titlerows::ready?scripting::titleRows:0)|(memberreads::ready?scripting::levelsServe:0)|
 (worldPossible&&charactersPossible&&nativecalls::damagePossible?scripting::playerDamage:0)|(worldPossible&&charactersPossible&&nativecalls::pushPossible?scripting::playerPush:0)|
 (worldPossible&&nativecalls::drivePossible?scripting::levelsDrive:0)|(gear::installed&&itemstats::lookup?scripting::gearUse:0)|(gear::restorePossible?scripting::itemsCondition|scripting::itemsWear:0)|
 scripting::uiNotify|(newgame::possible&&titlerows::ready&&nativecalls::travelPossible?scripting::levelsFlow:0)|(worldPossible&&nativecalls::placePossible?scripting::playerPlace:0)|(!sharedlua::modsDirectory.empty()?scripting::assetsRead:0);}
static bool worldMutationsIdle(){return worldemitters::idle()&&worlddevices::idle();}
// Lights and focus hold need the world contract; casts need the cast detour, installed with the item API.
static uint64_t powerCapabilities(){
 return (powers::installed?scripting::powerCast:0)|(powers::lightsPossible&&worldPossible?scripting::worldLights:0)|(powers::focusPossible&&charactersPossible?scripting::focusHold:0);
}
static uint64_t inventoryCapabilities(){return (worldtime::set?scripting::worldTime:0)|(inventorybridge::possible?scripting::inventoryRead:0)|(inventorybridge::possible&&containerWritePossible?scripting::inventoryTransfer:0)|(inventorybridge::equipmentPossible?scripting::equipmentWrite:0)|
 (worlditems::switchPossible?scripting::weaponSwitch:0)|(worlditems::pickupPossible?scripting::worldPickup:0)|(worlditems::dropPossible?scripting::worldDrop|scripting::worldSpawn:0)|
 (carrieditems::weaponsPossible?scripting::weaponEquip:0)|(carrieditems::bagsPossible?scripting::inventoryBags:0)|
 (carrieditems::unequipPossible?scripting::weaponUnequip:0)|(carrieditems::shieldsPossible?scripting::shieldEquip:0)|
 ((interactions::consumePossible||interactions::keyPossible)?scripting::itemsUse:0)|((interactions::openPossible||interactions::buttonPossible)?scripting::worldUse:0)|
 (interactions::leverPossible?scripting::worldLevers:0)|(interactions::doorPossible?scripting::worldDoors:0)|
 (worlddevices::teleporterPossible?scripting::worldTeleporters:0)|((worlddevices::scannerPossible||worldemitters::possible||worldemitters::firePossible||worldemitters::extinguishPossible||worldemitters::torchPossible)?scripting::worldDevices:0);}
static bool hooksInitialized=false,nativePossible=false;
static std::atomic<bool> frameReady{false};
static bool inventoryHookCreated=false,useHookCreated=false;
static bool hookLibrary(){
 if(hooksInitialized)return true;
 const auto status=MH_Initialize();hooksInitialized=status==MH_OK||status==MH_ERROR_ALREADY_INITIALIZED;return hooksInitialized;
}
static bool activateItems(){
 if(installed){gamebridge::active=true;return true;}
 if(!gamebridge::runtimeAvailable(base)||!gamebridge::atMenu())return false;
 gamebridge::backupFailed=false;
 if(!gamebridge::saveBackup()){coreOnly("Native item API waiting: save backup failed; retrying at the title menu");return false;}
 gamebridge::api={(uintptr_t(*)(uint32_t,uint32_t))(base+0x1041b0),
  (uintptr_t(*)(uint32_t))(base+0x10e7c0),(uintptr_t(*)(uintptr_t))(base+0x2f540),
  (bool(*)(uintptr_t,float,float,void*))(base+0x8f720),(uintptr_t(*)(uintptr_t))(base+0x4fcb0),
  (int(*)(uintptr_t))(base+0x118f80),(void(*)(uintptr_t))(base+0x14cb60),
  (void(*)(uintptr_t,int*,int*))(base+0x2cd90)};
 if(!hookLibrary())return false;
 if(!inventoryHookCreated){
  if(MH_CreateHook((void*)(base+0x11c9a0),(void*)gamebridge::inventoryUse,(void**)&gamebridge::originalInventoryUse)!=MH_OK)return false;
  inventoryHookCreated=true;
 }
 if(!useHookCreated){
  if(MH_CreateHook((void*)(base+0x11cb70),(void*)gamebridge::use,(void**)&gamebridge::originalUse)!=MH_OK)return false;
  useHookCreated=true;
 }
 const auto first=MH_EnableHook((void*)(base+0x11c9a0));
 const auto second=MH_EnableHook((void*)(base+0x11cb70));
 if((first!=MH_OK&&first!=MH_ERROR_ENABLED)||(second!=MH_OK&&second!=MH_ERROR_ENABLED)){
  MH_DisableHook((void*)(base+0x11c9a0));MH_DisableHook((void*)(base+0x11cb70));return false;
 }
 installed=true;gamebridge::active=true;host.report("runtime","info","Native item API enabled");
 if(!charactersPossible||!combat::install())host.report("runtime","warn","Combat events unavailable: damage entry guard or hook failed");
 else host.report("runtime","info",combat::impactInstalled?"Combat events ready (with attackers)":"Combat events ready (no attacker: impact hook failed)");
 if(!savedata::install())host.report("runtime","warn","Save data unavailable: save entry guard or hook failed");
 else host.report("runtime","info","Save data ready");
 if(!scriptcommands::install())host.report("runtime","warn","LuaEvent/LuaFlag unavailable: registrar guard failed or the names are taken");
 else host.report("runtime","info",scriptcommands::logRegistered?(scriptcommands::number?"LuaEvent/LuaFlag/LuaLog registered":"LuaEvent/LuaFlag/LuaLog registered (event.number unavailable: argument reader did not match)"):"LuaEvent/LuaFlag registered (LuaLog name taken)");
 if(!zones::install())host.report("runtime","warn","Zone events unavailable: zone runner guard or hook failed");
 else host.report("runtime","info","Zone events ready");
 if(!gamemessages::install())host.report("runtime","warn",("Game messages unavailable: "+gamemessages::why).c_str());
 else host.report("runtime","info","Game messages forwarded to this log");
 host.report("runtime","info",worldtime::set?"World time ready":"World time unavailable: clock functions did not match");
 if(!inventorybridge::possible||!gear::install())host.report("runtime","warn","gear.use unavailable: use gate guard or hook failed");
 else{gear::restoreAvailable();host.report("runtime","info",gear::restorePossible?"gear.use ready (with restore_condition)":"gear.use ready (restore_condition unavailable: job list functions did not match)");}
 if(!powers::install())host.report("runtime","warn","Power casts unavailable: cast dispatcher guard or hook failed");
 else host.report("runtime","info","Light-school power casts ready");
 return true;
}
static bool(*itemActivator)()=activateItems;
using ReloadCallback=bool(*)(const char*,const char*);
static ReloadCallback reloadCallback=nullptr;static std::string reloadRuntime,reloadWanted;
static std::filesystem::path controlDirectory;
static uint64_t nextControlPoll=0;
static void reloadControl(uint64_t now){
 if(!controlDirectory.empty()&&now>=nextControlPoll){
  nextControlPoll=now+1000;const auto path=controlDirectory/L"reload.request";
  std::ifstream input(path,std::ios::binary);
  if(input){
   char request[130]{};input.read(request,129);const auto count=input.gcount();input.close();
   std::error_code error;std::filesystem::remove(path,error);
   if(!error&&count>0&&count<=128){reloadWanted.assign(request,size_t(count));
    if(reloadWanted.find('\0')!=std::string::npos)reloadWanted.clear();
    while(!reloadWanted.empty()&&(reloadWanted.back()=='\n'||reloadWanted.back()=='\r'))reloadWanted.pop_back();
   }else host.report("runtime","warn","Invalid reload.request or request file could not be consumed");
  }
 }
 if(reloadWanted.empty())return;
 const auto request=std::move(reloadWanted);reloadWanted.clear();
 if(!reloadCallback||!reloadCallback(reloadRuntime.c_str(),request.c_str()))host.report("runtime","error","Reload request failed; inspect EMTK and runtime logs");
 else host.report("runtime","info","Reload source queued; callback success is not script acceptance");
}
static void pumpPresentation(uint64_t thread,uint64_t windowThread,const std::string& phase,uint64_t player,uint64_t now){
 if(!runtimeEnabled||!frameReady||!session.begin(thread,windowThread,phase,player,now))return;
 worldPulse=phase=="session";worldCaptured=false;worldCopy={};
 charactersCaptured=false;charactersCopy={};
 awarenessCaptured=false;awarenessCopy={};
 detailsCaptured=false;detailsCopy={};
 beginAiPulse(phase,now);
 carrieditems::checkFault();
 containerbridge::targets.clear();
 inventorybridge::target={};
 worlditems::beginPulse();
 carrieditems::bags.clear();
 interactions::targets.clear();
 struct ClearWorld{~ClearWorld(){worldPulse=false;}} clearWorld;
 struct Finish{scripting::Session& value;~Finish(){value.end();}} finish{session};
 if(session.settledMenu(now)){
  if((aiWritePossible||attitudeWritePossible||containerWritePossible||inventoryWritePossible)&&!gamebridge::backupDone&&now>=nextAiBackup){nextAiBackup=now+5000;gamebridge::backupFailed=false;gamebridge::saveBackup();}
  if(!gamebridge::active&&nativePossible&&session.retryDue(now)){
   const bool ready=itemActivator();
   session.configure(settingsCapability|inventoryCapabilities()|(containerWritePossible?scripting::containerEdit:0)|(attitudeWritePossible?scripting::attitudeWrite:0)|(aiWritePossible?scripting::aiWrite:0)|(worldPossible?scripting::worldRead:0)|(charactersPossible?(scripting::charactersRead|scripting::awarenessRead):0)|(scripting::lifecycleEvents|scripting::timerEvents)|(ready?scripting::itemApiCapabilities:0)|(combat::installed?scripting::combatEvents:0)|(savedata::writerOn?scripting::saveData:0)|(scriptcommands::registered?(scripting::scriptEvents|scripting::scriptFlags):0)|(itemstats::lookup?scripting::itemStats:0)|(zones::installed&&charactersPossible?scripting::zoneEvents:0)|(gamemessages::installed?scripting::gameMessages:0)|sharedCapabilities()|powerCapabilities(),!ready);
   if(!ready)host.report("runtime","warn","Native item API not ready; next menu retry in five seconds");
  }
  if(gamebridge::active)session.finish();
  // The Settings-form hook waits for a settled menu, so a Settings owner that enables after this runtime
  // (ExCamera) has already either claimed the host role or hooked the step itself.
  const int tabs=settingstabs::poll();
  if(tabs>0)host.report("runtime","info","Mod Settings tab ready (Settings menu)");
  else if(tabs<0)host.report("runtime","warn","Mod Settings tab unavailable: another mod manages the Settings form and does not host runtime tabs (update ExCamera)");
  reloadControl(now);session.consume();
 }
 combat::deliver(host,captureCharacters());zones::deliver(host,captureCharacters());scriptcommands::deliver(host,worldPulse);gamemessages::deliver(host);
 sharedlua::deliver(host);memberreads::deliver(host);gear::deliver(host);modsettings::deliver(host);sounds::deliver(host);gestures::deliver(host);itemids::deliver(host);recordmerge::deliver(host);npcpowers::deliver(host,captureCharacters());dialogs::deliver();powers::deliver(host);powers::step(host,phase);
 session.tickTimers();host.flushSettings();
}
#include "presentation.h"
static bool start(){
 if(runtimeEnabled.load())return true;
 initializeLog();initializeSettings();base=(uintptr_t)GetModuleHandleW(nullptr);nativePossible=gamebridge::runtimeAvailable(base);
 if(!controlDirectory.empty())nextControlPoll=0;
 else{wchar_t path[32768]{};const auto size=GetModuleFileNameW(module,path,32768);if(size&&size<32768)controlDirectory=std::filesystem::path(path).parent_path();}
 runtimeEnabled=true;
 if(!frameReady&&gamebridge::lifecycleAvailable(base))frameReady=installPresentation();
 worldPossible=frameReady&&worldContract();host.setWorldProvider(captureWorld);host.setNearbyRead(readNearby);
 if(worldPossible&&worldtime::available()){worldtime::get=(worldtime::Get)(base+0x17cf80);worldtime::set=(worldtime::Set)(base+0x17cfc0);}
 host.setTimeApi(worldtime::read,worldtime::write);host.setSaveApi(savedata::get,savedata::set);host.setFlagApi(scriptcommands::getFlag,scriptcommands::setFlag);
 charactersPossible=frameReady&&charactersContract();host.setCharacterProvider(captureCharacters);
 host.setAwarenessProvider(captureAwareness);host.setDetailsProvider(captureDetails);host.setLevelRead(levelinfo::read);
 if(frameReady&&itemstats::available())itemstats::lookup=(itemstats::Lookup)(base+0x1041b0);host.setStatsRead(itemstats::read);
 // Shared owners other mods register with: installed at enable (startup reads go through the member read), and
 // only on untouched entries, so a mod that already hooked them keeps them and these stay not-ready.
 if(nativePossible&&hookLibrary()){
  if(!titlerows::ready){const auto why=titlerows::available(frameReady);host.report("runtime",why.empty()?"info":"warn",why.empty()?"Title rows available (hooked at the first add_title_row)":("Title rows unavailable: "+why).c_str());}
  if(!memberreads::ready){const auto why=memberreads::available();host.report("runtime",why.empty()?"info":"warn",why.empty()?"Member reads available (hooked at the first provider)":("Level serving unavailable: "+why).c_str());}
 }
 if(nativePossible){nativecalls::available();nativecalls::travelAvailable();}host.setNativeCommit(nativecalls::commit);
 if(nativePossible)newgame::available();if(!controlDirectory.empty())sharedlua::modsDirectory=controlDirectory.parent_path();
 if(!controlDirectory.empty()){console::readSettings(controlDirectory/L"ScriptRuntime.ini");console::startTailing(controlDirectory.parent_path());}
 if(nativePossible&&frameReady&&hookLibrary()&&!settingstabs::ready){const auto why=settingstabs::available();if(!why.empty())host.report("runtime","warn",("Mod Settings tab unavailable: "+why).c_str());}
 titlerows::formOpen=settingstabs::optionsOpen;modsettings::start(controlDirectory);
 if(nativePossible&&!sounds::possible&&!controlDirectory.empty()){sounds::available(controlDirectory.parent_path().parent_path());if(!sounds::possible)host.report("runtime","warn","ui.sound unavailable: Sound.rpk index or the sound functions did not match");}
 host.setSharedApi({sharedlua::addRow,sharedlua::removeRow,sharedlua::enableRow,sharedlua::serve,sharedlua::clearServed,forgetShared,newgame::arm,sharedlua::readAsset,console::notify,
  modsettings::defineSettings,modsettings::readSetting,sounds::request,addPlacement,gestures::request,gear::queueWear,npcpowers::request,dialogs::request});
 if(nativePossible&&!gestures::possible)gestures::available();
 if(nativePossible&&!dialogs::possible)dialogs::available();
 if(nativePossible&&!npcpowers::possible)npcpowers::available();
 hot::AiConstants aiConstants{};aiWritePossible=charactersPossible&&readAiConstants(aiConstants);host.setAiCommit(commitAi);host.setAiCapture(captureAiTarget);
 attitudeWritePossible=charactersPossible&&attitudeContract();concernSetter=attitudeWritePossible?(ConcernSetter)(base+0x26f160):nullptr;
 containerWritePossible=worldPossible&&containerbridge::available();
 if(containerWritePossible){containerbridge::find=(containerbridge::Find)(base+0x104660);containerbridge::replace=(containerbridge::Replace)(base+0x1048e0);containerbridge::open=(containerbridge::Open)(base+0x1327f0);containerbridge::chunk=(containerbridge::Chunk)(base+0x1041b0);}
 host.setContainerApi(containerbridge::capture,containerbridge::read,containerbridge::commit);
 inventorybridge::possible=frameReady&&inventorybridge::available(false);inventoryWritePossible=inventorybridge::possible;
 inventorybridge::equipmentPossible=inventorybridge::possible&&inventorybridge::available(true);
 if(inventorybridge::possible){inventorybridge::append=(inventorybridge::Append)(base+0x113a20);inventorybridge::notify=(inventorybridge::Refresh)(base+0x137b10);inventorybridge::recount=(inventorybridge::Recount)(base+0x111b70);containerbridge::chunk=(containerbridge::Chunk)(base+0x1041b0);}
 if(inventorybridge::equipmentPossible){inventorybridge::wear=(inventorybridge::Append)(base+0x116480);inventorybridge::unwear=(inventorybridge::RemoveWorn)(base+0x116500);inventorybridge::fit=(inventorybridge::Fit)(base+0x115e90);inventorybridge::refresh=(inventorybridge::Refresh)(base+0x112540);inventorybridge::rebuild=(inventorybridge::Rebuild)(base+0xf1420);}
 host.setInventoryApi(inventorybridge::capture,inventorybridge::commit);
 inventorybridge::extraCommit=interactions::commit;inventorybridge::extraReady=carrieditems::queuesIdle;host.setWorldItemCapture(worlditems::capture);host.setBagRead(carrieditems::readBag);host.setInteractionCapture(interactions::capture);
 carrieditems::weaponsPossible=inventorybridge::possible&&carrieditems::available(true);
 carrieditems::bagsPossible=inventorybridge::possible&&carrieditems::available(false);
 carrieditems::unequipPossible=inventorybridge::possible&&carrieditems::availableAction(true);
 carrieditems::shieldsPossible=inventorybridge::possible&&carrieditems::availableAction(false);
 if(carrieditems::weaponsPossible){carrieditems::equip=(carrieditems::Equip)(base+0x135810);carrieditems::allocate=(carrieditems::Allocate)(base+0xed70);carrieditems::release=(carrieditems::Free)(base+0xedd0);carrieditems::hide=(carrieditems::Hide)(base+0x1115a0);carrieditems::convert=(carrieditems::Clone)(base+0x102700);carrieditems::displacedCallback=base+0x24a950;}
 if(carrieditems::bagsPossible){carrieditems::clone=(carrieditems::Clone)(base+0x1035c0);containerbridge::find=(containerbridge::Find)(base+0x104660);containerbridge::replace=(containerbridge::Replace)(base+0x1048e0);containerbridge::open=(containerbridge::Open)(base+0x1327f0);}
 if(carrieditems::unequipPossible){carrieditems::allocate=(carrieditems::Allocate)(base+0xed70);carrieditems::release=(carrieditems::Free)(base+0xedd0);carrieditems::removeAction=(carrieditems::RemoveAction)(base+0x1345d0);carrieditems::enqueue=(carrieditems::Queue)(base+0x1a890);}
 if(carrieditems::shieldsPossible){carrieditems::shield=(carrieditems::Shield)(base+0x135b00);carrieditems::convert=(carrieditems::Clone)(base+0x102700);carrieditems::hide=(carrieditems::Hide)(base+0x1115a0);}
 if(carrieditems::unequipPossible||carrieditems::shieldsPossible)carrieditems::unhide=(carrieditems::Hide)(base+0x111530);
 interactions::consumePossible=inventorybridge::possible&&interactions::available(scripting::InventoryOp::itemUse);
 interactions::keyPossible=inventorybridge::possible&&worldPossible&&interactions::available(scripting::InventoryOp::useItemOnWorld);
 interactions::openPossible=inventorybridge::possible&&worldPossible&&interactions::available(scripting::InventoryOp::worldUse);
 interactions::buttonPossible=inventorybridge::possible&&worldPossible&&interactions::available(scripting::InventoryOp::worldPress);
 interactions::leverPossible=inventorybridge::possible&&worldPossible&&interactions::available(scripting::InventoryOp::leverPosition);
 interactions::doorPossible=inventorybridge::possible&&worldPossible&&interactions::available(scripting::InventoryOp::doorOpen);
 interactions::slidingPossible=interactions::doorPossible&&interactions::availableDevice(interactions::DeviceContract::sliding);
 interactions::pairedPossible=interactions::doorPossible&&interactions::availableDevice(interactions::DeviceContract::paired);
 interactions::lightLeverPossible=interactions::leverPossible&&interactions::availableDevice(interactions::DeviceContract::lightLever);
 worlddevices::teleporterPossible=inventorybridge::possible&&worldPossible&&worlddevices::available(false);
 worlddevices::scannerPossible=inventorybridge::possible&&worldPossible&&worlddevices::available(true);
 if(worlddevices::teleporterPossible)worlddevices::teleporter=(worlddevices::Use)(base+0x2749c0);
 if(worlddevices::scannerPossible)worlddevices::scanner=(worlddevices::Use)(base+0x276420);
 worldemitters::possible=inventorybridge::possible&&worldPossible&&worldemitters::available();
 worldemitters::firePossible=inventorybridge::possible&&worldPossible&&worldemitters::availableFire();
 if(worldemitters::possible)worldemitters::signal=(worldemitters::Signal)(base+0x149bf0);
 if(worldemitters::firePossible)worldemitters::fire=(worldemitters::Fire)(base+0x1494d0);
 worldemitters::extinguishPossible=inventorybridge::possible&&worldPossible&&worldemitters::availableFlame(false);
 worldemitters::torchPossible=inventorybridge::possible&&worldPossible&&worldemitters::availableFlame(true);
 if(worldemitters::extinguishPossible)worldemitters::remove=(worldemitters::Remove)(base+0x5bed0);
 if(worldemitters::torchPossible){worldemitters::torch=(worldemitters::Torch)(base+0x149740);worldemitters::publish=(worldemitters::Publish)(base+0x5a420);worldemitters::pose=(worldemitters::Pose)(base+0x50eb0);}
 if(worldemitters::possible||worldemitters::firePossible||worldemitters::extinguishPossible||worldemitters::torchPossible)worldemitters::enqueue=(worldemitters::Queue)(base+0x1a890);
 if(frameReady&&nativePossible&&worldPossible)powers::available();
 host.setCasterRead(powers::readCaster);
 worldMutationReady=worldMutationsIdle;
 if(interactions::leverPossible||interactions::doorPossible)interactions::wake=(interactions::Wake)(base+0x8ade0);
 if(interactions::doorPossible){interactions::unlatch=(interactions::Unlock)(base+0x14cc30);interactions::doorCommand=(interactions::DoorCommand)(base+0x14cb00);}
 if(interactions::consumePossible)interactions::effect=(interactions::Effect)(base+0x11c310);
 if(interactions::keyPossible)interactions::unlock=(interactions::Unlock)(base+0x14cb60);
 if(interactions::openPossible){interactions::open=(interactions::Open)(base+0x10e7c0);interactions::isOpen=(interactions::Closed)(base+0x1327f0);}
 if(interactions::buttonPossible){interactions::trigger=(interactions::Trigger)(base+0x14a3e0);interactions::find=(interactions::Find)(base+0x101830);}
 worlditems::switchPossible=inventorybridge::possible&&worlditems::available(scripting::InventoryOp::switchWeapons);
 worlditems::pickupPossible=inventorybridge::possible&&worldPossible&&worlditems::available(scripting::InventoryOp::pickup);
 worlditems::dropPossible=inventorybridge::possible&&worldPossible&&worlditems::available(scripting::InventoryOp::drop);
 worlditems::requestSwitch=worlditems::switchPossible?(worlditems::Switch)(base+0x136e50):nullptr;
 worlditems::remove=worlditems::pickupPossible?(worlditems::Remove)(base+0x50a60):nullptr;
 if(worlditems::dropPossible){worlditems::convert=(worlditems::Convert)(base+0x102700);worlditems::create=(worlditems::Create)(base+0x11cee0);worlditems::initialize=(worlditems::Initialize)(base+0x11d0e0);worlditems::registerNode=(worlditems::Register)(base+0x5a420);worlditems::release=(worlditems::Release)(base+0x117000);}
 session.configure(settingsCapability|inventoryCapabilities()|(containerWritePossible?scripting::containerEdit:0)|(attitudeWritePossible?scripting::attitudeWrite:0)|(aiWritePossible?scripting::aiWrite:0)|(worldPossible?scripting::worldRead:0)|(charactersPossible?(scripting::charactersRead|scripting::awarenessRead):0)|(frameReady?(scripting::lifecycleEvents|scripting::timerEvents):0)|(itemstats::lookup?scripting::itemStats:0)|sharedCapabilities()|powerCapabilities(),frameReady&&nativePossible);
 if(frameReady&&reloadCallback)reloadWanted="all";
 if(!frameReady)coreOnly("Core Lua enabled; verified presentation/menu lifecycle is unavailable");
 else host.report("runtime","info","Lifecycle installed; native initialization and addons await a settled title menu");
 return true;
}
extern "C" __declspec(dllexport) bool enable(){
 AcquireSRWLockExclusive(&lifecycle);bool result=false;
 try{std::lock_guard<std::recursive_mutex> lock(scriptMutex);result=start();}catch(...){gamebridge::active=false;runtimeEnabled=false;}
 ReleaseSRWLockExclusive(&lifecycle);return result;
}
extern "C" __declspec(dllexport) bool disable(){
 AcquireSRWLockExclusive(&lifecycle);
 {std::lock_guard<std::recursive_mutex> lock(scriptMutex);runtimeEnabled=false;gamebridge::active=false;aiTargets.clear();containerbridge::targets.clear();inventorybridge::target={};worlditems::beginPulse();carrieditems::bags.clear();interactions::targets.clear();aiPulseReady=false;aiWorld=aiPlayer=aiScene=0;host.flushSettings(128,true);session.clear();session.configure(0,false);reloadWanted.clear();presentationWindow=nullptr;titleDrawThread=0;combat::listening=false;combat::clear();powers::listening=false;powers::clear();gear::listening=false;zones::listening=false;zones::clear();gamemessages::clear();scriptcommands::listening=false;scriptcommands::clear();}
 ReleaseSRWLockExclusive(&lifecycle);return true;
}
// EMTK submits each enabled addon at startup (and again on reload): its manifest's startup overrides are read
// then, from the sibling folder mods/<id>, so they are in place before the game reads those files.
// [script] memory_mb = N in the addon's manifest raises its Lua heap cap from the default 8 MiB (at most 64 MiB),
// for one-off heavy work such as level generation. Read from mods/<id>/manifest.toml; anything else keeps 8.
static size_t addonMemory(const char* id){
 constexpr size_t mib=1024*1024;if(controlDirectory.empty())return 8*mib;
 try{std::error_code e;const auto mods=controlDirectory.parent_path();const auto folder=std::filesystem::weakly_canonical(mods/std::filesystem::u8path(id),e);
  if(e||folder.parent_path()!=std::filesystem::weakly_canonical(mods,e))return 8*mib;
  std::ifstream in(folder/L"manifest.toml",std::ios::binary);std::string line;bool script=false;
  while(std::getline(in,line)){while(!line.empty()&&(line.back()=='\r'||line.back()==' '))line.pop_back();size_t s=0;while(s<line.size()&&(line[s]==' '||line[s]=='\t'))++s;line.erase(0,s);
   if(!line.empty()&&line[0]=='['){script=line=="[script]";continue;}
   if(!script||line.rfind("memory_mb",0)!=0)continue;const auto eq=line.find('=');if(eq==std::string::npos)continue;
   const long v=std::strtol(line.c_str()+eq+1,nullptr,10);if(v>=8&&v<=64)return size_t(v)*mib;host.report(id,"warn","[script] memory_mb must be 8..64; 8 is used");return 8*mib;}
 }catch(...){}
 return 8*mib;
}
static void submitted(const char* id){
 if(!controlDirectory.empty()&&memberreads::ready)try{sharedlua::loadOverrides(id,controlDirectory.parent_path(),host);}catch(...){host.report(id,"warn","[script.overrides] could not be read");}
 // [script.records]: database records merged as the game reads objdb/objstrings (record_merge.h).
 if(!controlDirectory.empty()&&memberreads::ready)try{
  std::error_code e;const auto mods=controlDirectory.parent_path();const auto folder=std::filesystem::weakly_canonical(mods/std::filesystem::u8path(id),e);
  if(!e&&folder.parent_path()==std::filesystem::weakly_canonical(mods,e)){std::ifstream in(folder/L"manifest.toml",std::ios::binary);
   if(in){const std::string toml{std::istreambuf_iterator<char>(in),{}};std::string why;
    if(recordmerge::registerMod(id,folder,toml,why)){
     if(memberreads::installHook()){memberreads::edit=editMember;memberreads::wants=editWants;host.report(id,"info","[script.records] ready");}
     else host.report(id,"warn","[script.records] ignored: member reads are unavailable (another mod owns 0x6f2d0)");}
    else if(!why.empty())host.report(id,"warn",("[script.records] not used: "+why).c_str());}}
 }catch(...){host.report(id,"warn","[script.records] could not be read");}
}
extern "C" __declspec(dllexport) bool load_script_addon(const char* id,const unsigned char* source,size_t size,const char* capabilities){
 if(!id||!source||!capabilities||strnlen_s(id,129)>128||strnlen_s(capabilities,513)>512||size>1024*1024)return false;
 std::lock_guard<std::recursive_mutex> lock(scriptMutex);
 if(!runtimeEnabled)return false;
 try{
  scripting::ScriptSource code{id,std::string((const char*)source,size),capabilities,"",{}};code.memory=addonMemory(id);
  const bool result=session.submit(code,frameReady);
  if(!result)host.report(id,"error",session.error().c_str());else submitted(id);
  if(result&&!frameReady)host.flushSettings(1,true);return result;
 }catch(...){return false;}
}
// ABI 2 adds a copied module bundle; ABI 1 remains available for existing loaders.
struct ScriptModule {const char* name;const unsigned char* source;size_t size;};
extern "C" __declspec(dllexport) bool load_script_addon_v2(const char* id,const unsigned char* source,size_t size,const char* capabilities,const char* optional,const ScriptModule* modules,size_t count){
 if(!id||!source||!capabilities||!optional||strnlen_s(optional,513)>512||strnlen_s(id,129)>128||strnlen_s(capabilities,513)>512||size>1024*1024||count>64||(count&&!modules))return false;
 std::lock_guard<std::recursive_mutex> lock(scriptMutex);if(!runtimeEnabled)return false;
 try{
  std::vector<scripting::Module> bundle;size_t total=size;
  for(size_t i=0;i<count;++i){const auto& m=modules[i];
   if(!m.name||!m.source||strnlen_s(m.name,129)>128||m.size>1024*1024-total)return false;
   total+=m.size;bundle.push_back({m.name,std::string((const char*)m.source,m.size)});
  }
  scripting::ScriptSource code{id,std::string((const char*)source,size),capabilities,optional,std::move(bundle)};code.memory=addonMemory(id);
  const bool result=session.submit(code,frameReady);
  if(!result)host.report(id,"error",session.error().c_str());else submitted(id);if(result&&!frameReady)host.flushSettings(1,true);return result;
 }catch(...){host.report(id,"error","Addon bundle allocation failed");return false;}
}
extern "C" __declspec(dllexport) bool set_script_reload_callback(ReloadCallback callback,const char* runtime){
 if(!callback||!runtime||strnlen_s(runtime,129)>128)return false;
 std::lock_guard<std::recursive_mutex> lock(scriptMutex);reloadRuntime=runtime;reloadCallback=callback;return true;
}
extern "C" __declspec(dllexport) bool request_script_reload(const char* id){
 if(!id||!id[0]||strnlen_s(id,129)>128)return false;
 std::lock_guard<std::recursive_mutex> lock(scriptMutex);if(!runtimeEnabled||!frameReady||!reloadCallback)return false;
 reloadWanted=id;return true;
}
// Native zone-entry listeners for other mods (see zone_bridge.h): one shared detour of the zone runner instead of
// one per mod. Registration works at any time; calls start once the detour installs at the settled title menu.
// zone_listener_ready() says whether it has. The listener runs on the game thread; see zone_bridge.h for its rules.
extern "C" __declspec(dllexport) bool add_zone_listener(zones::Listener listener,void* context){return zones::addListener(listener,context);}
extern "C" __declspec(dllexport) bool remove_zone_listener(zones::Listener listener,void* context){return zones::removeListener(listener,context);}
extern "C" __declspec(dllexport) bool zone_listener_ready(){return zones::installed;}
// Shared hooks, version 1. extern "C", default x64 convention.
// A caller checks shared_hooks_version() and the *_ready() export first; when either is absent, unknown or false it
// keeps its own detours. These hooks are only installed on untouched entries, so the two never stack.
extern "C" __declspec(dllexport) uint32_t shared_hooks_version(){return 1;}
extern "C" __declspec(dllexport) bool add_title_row(const char* label,int after_base_row,void(*activate)(void*),void* context){return titlerows::registerRow(label,after_base_row,activate,context);}
// A Lua addon's rows belong to it: other mods cannot remove or hide them by label.
extern "C" __declspec(dllexport) bool remove_title_row(const char* label){return !titlerows::heldBy(label,sharedlua::pressed)&&titlerows::unregisterRow(label);}
extern "C" __declspec(dllexport) bool set_title_row_enabled(const char* label,bool enabled){return !titlerows::heldBy(label,sharedlua::pressed)&&titlerows::setRowEnabled(label,enabled);}
extern "C" __declspec(dllexport) bool title_rows_ready(){return titlerows::ready;}
extern "C" __declspec(dllexport) bool add_member_provider(memberreads::Provider provider,memberreads::Claims claims,void* context,int priority){return priority>50&&memberreads::addProvider(provider,claims,context,priority);}
extern "C" __declspec(dllexport) bool remove_member_provider(memberreads::Provider provider,void* context){return memberreads::removeProvider(provider,context);}
extern "C" __declspec(dllexport) bool resize_member_buffer(uintptr_t buffer,size_t size){return memberreads::resizeBuffer(buffer,size);}
extern "C" __declspec(dllexport) bool member_providers_ready(){return memberreads::ready;}
// Settings tabs, version 1 (settings_tabs.h). A native mod adds a tab (or rows under Controls with the title
// "@Controls") and supplies its rows on demand; the runtime draws them in the game's Settings form. A mod that
// already manages that form (ExCamera) calls settings_tabs_claim_host() at its enable and draws them itself with
// the settings_tab_host_* calls: open when its form opens (returns the tab count), title, rows and change while it
// is open, event for Apply/Cancel/Defaults/close. Everything runs on the game's GUI thread.
extern "C" __declspec(dllexport) uint32_t settings_tabs_version(){return 1;}
extern "C" __declspec(dllexport) bool add_settings_tab(const wchar_t* title,void* context,settingstabs::RowsFn rows,settingstabs::ChangeFn change,settingstabs::EventFn event){return settingstabs::addTab(title,context,rows,change,event);}
extern "C" __declspec(dllexport) bool remove_settings_tab(const wchar_t* title,void* context){return title&&std::wcscmp(title,L"Mod Settings")&&settingstabs::removeTab(title,context);}
extern "C" __declspec(dllexport) bool settings_tabs_ready(){return settingstabs::ready;}
extern "C" __declspec(dllexport) bool settings_tabs_claim_host(){return settingstabs::claimHost();}
extern "C" __declspec(dllexport) int settings_tab_host_open(){return settingstabs::hostedOpen();}
extern "C" __declspec(dllexport) int settings_tab_host_title(int tab,wchar_t* out,int capacity){return settingstabs::hostedTitle(tab,out,capacity);}
extern "C" __declspec(dllexport) int settings_tab_host_rows(int tab,int page,settingstabs::Row* out,int capacity){return settingstabs::hostedRows(tab,page,out,capacity);}
extern "C" __declspec(dllexport) void settings_tab_host_change(int tab,int page,int id,int kind,int delta,float position,int checked,int key){settingstabs::hostedChange(tab,page,id,kind,delta,position,checked,key);}
// Help text for a row of the Mod Settings tab, by the id from its rows (0 when none): for a host's tooltip.
extern "C" __declspec(dllexport) int mod_settings_help(int id,wchar_t* out,int capacity){return modsettings::help(id,out,capacity);}
extern "C" __declspec(dllexport) void settings_tab_host_event(int tab,int event){settingstabs::hostedEvent(tab,event);}
BOOL WINAPI DllMain(HINSTANCE h,DWORD why,LPVOID){if(why==DLL_PROCESS_ATTACH){module=h;DisableThreadLibraryCalls(h);}return TRUE;}
