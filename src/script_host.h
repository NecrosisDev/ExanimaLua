#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
#include <chrono>
#include <map>
#include <unordered_map>
#include <cmath>
#include <limits>
#include "timer_queue.h"
#include "settings_store.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "settings_lua.h"
#include "world_snapshot.h"
#include "character_snapshot.h"
#include "ai_mutation.h"
#include "container_contract.h"
#include "inventory_contract.h"
#include "item_stats.h"

namespace scripting {
enum Capability : uint64_t { events=1, items=2, contents=4, openStorage=8, unlockDoor=16, lifecycleEvents=32, timerEvents=64, settingsStorage=128, worldRead=256, charactersRead=512, awarenessRead=1024, aiWrite=2048, attitudeWrite=4096, containerEdit=8192, inventoryRead=16384, inventoryTransfer=32768, equipmentWrite=65536, weaponSwitch=131072, worldPickup=262144, worldDrop=524288, weaponEquip=1048576, inventoryBags=2097152, weaponUnequip=4194304, shieldEquip=8388608, itemsUse=16777216, worldUse=33554432, worldLevers=67108864, worldDoors=134217728, worldTeleporters=268435456, worldDevices=536870912, worldSpawn=1073741824, combatEvents=2147483648u, worldTime=uint64_t(1)<<32, saveData=uint64_t(1)<<33, scriptEvents=uint64_t(1)<<34, scriptFlags=uint64_t(1)<<35,
 itemStats=uint64_t(1)<<36, zoneEvents=uint64_t(1)<<37, gameMessages=uint64_t(1)<<38, titleRows=uint64_t(1)<<39, levelsServe=uint64_t(1)<<40, playerDamage=uint64_t(1)<<41, playerPush=uint64_t(1)<<42, levelsDrive=uint64_t(1)<<43,
 powerCast=uint64_t(1)<<44, focusHold=uint64_t(1)<<45, worldLights=uint64_t(1)<<46, gearUse=uint64_t(1)<<47, itemsCondition=uint64_t(1)<<48, levelsFlow=uint64_t(1)<<49, assetsRead=uint64_t(1)<<50, playerPlace=uint64_t(1)<<51, uiNotify=uint64_t(1)<<52, modSettings=uint64_t(1)<<53, uiSound=uint64_t(1)<<54, itemsWear=uint64_t(1)<<55, characterPowers=uint64_t(1)<<56, charactersDamage=uint64_t(1)<<57, characterMarks=uint64_t(1)<<58, damageScale=uint64_t(1)<<59, hitSound=uint64_t(1)<<60, limbStrength=uint64_t(1)<<61, bodyScale=uint64_t(1)<<62, effectsArc=uint64_t(1)<<63 };
inline constexpr uint64_t itemApiCapabilities=events|items|contents|openStorage|unlockDoor;
// The runtime's version as addons see it: api.version is the Lua API generation (unchanged while additions stay
// compatible), api.revision counts additive releases, so `api.revision >= 24` means "0.24.0 or later".
inline constexpr const char* runtimeVersion="0.28.0";inline constexpr int apiRevision=28;
struct CapabilityName {const char* name;uint64_t bit;};
inline constexpr CapabilityName capabilityNames[]={
 {"events.item_use",events},{"items.read",items},{"containers.read",contents},{"containers.open",openStorage},{"doors.unlock",unlockDoor},
 {"events.lifecycle",lifecycleEvents},{"timers",timerEvents},{"storage.settings",settingsStorage},{"world.read",worldRead},
 {"characters.read",charactersRead},{"characters.awareness",awarenessRead},{"characters.ai.write",aiWrite},{"characters.attitude.write",attitudeWrite},
 {"containers.edit",containerEdit},{"inventory.read",inventoryRead},{"inventory.transfer",inventoryTransfer},{"equipment.write",equipmentWrite},
 {"weapons.switch",weaponSwitch},{"world.pickup",worldPickup},{"world.drop",worldDrop},{"weapons.equip",weaponEquip},{"inventory.bags",inventoryBags},
 {"weapons.unequip",weaponUnequip},{"shields.equip",shieldEquip},{"items.use",itemsUse},{"world.use",worldUse},{"world.levers",worldLevers},
 {"world.doors",worldDoors},{"world.teleporters",worldTeleporters},{"world.devices",worldDevices},{"world.fire",worldDevices},{"world.spawn",worldSpawn},
 {"events.combat",combatEvents},{"world.time",worldTime},{"storage.save",saveData},{"events.script",scriptEvents},{"script.flags",scriptFlags},
 {"items.stats",itemStats},{"events.zone",zoneEvents},{"events.messages",gameMessages},{"title.rows",titleRows},{"levels.serve",levelsServe},{"levels.read",levelsServe},
 {"player.damage",playerDamage},{"player.push",playerPush},{"levels.drive",levelsDrive},
 {"powers.cast",powerCast},{"powers.focus",focusHold},{"world.lights",worldLights},{"events.gear_use",gearUse},{"items.condition",itemsCondition},
 {"levels.flow",levelsFlow},{"assets.read",assetsRead},{"player.place",playerPlace},{"ui.notify",uiNotify},{"ui.settings",modSettings},{"ui.sound",uiSound},{"items.wear",itemsWear},{"characters.powers",characterPowers},{"characters.damage",charactersDamage},{"characters.drop_held",charactersDamage},{"characters.sever",charactersDamage},{"characters.keep_down",charactersDamage},{"characters.motion",charactersDamage},{"characters.marks",characterMarks},{"characters.damage_scale",damageScale},{"characters.hit_sound",hitSound},{"characters.limb_strength",limbStrength},{"characters.body",bodyScale},{"effects.arc",effectsArc},{"effects.fire",effectsArc}};
inline uint64_t capability(const std::string& name){
 for(const auto& known:capabilityNames)if(name==known.name)return known.bit;
 return 0;
}
// The closest known name by edit distance, for "did you mean" in load errors; empty when nothing is close.
inline std::string closestCapability(const std::string& name){
 std::string best;size_t bestDistance=4;
 for(const auto& known:capabilityNames){
  const std::string other=known.name;std::vector<size_t> row(other.size()+1);
  for(size_t j=0;j<=other.size();++j)row[j]=j;
  for(size_t i=1;i<=name.size();++i){size_t diagonal=row[0];row[0]=i;
   for(size_t j=1;j<=other.size();++j){const size_t up=row[j];row[j]=(std::min)({row[j]+1,row[j-1]+1,diagonal+(name[i-1]==other[j-1]?0:1)});diagonal=up;}}
  if(row[other.size()]<bestDistance){bestDistance=row[other.size()];best=other;}
 }
 return best;
}
enum class Action { none, openStorage, unlockDoor };
struct Entry { uint32_t id=0;bool key=false,removed=false; };
struct Event {
 uint32_t item=0,targetItem=0,requiredKey=0;
 std::string model,target="other";
 bool storageOpen=false,container=false,doorLocked=false;
 std::vector<Entry> entries;
 // An inventory target that is a weapon or apparel piece: its kind, whether it is in the pack, and its condition.
 std::string targetGear;bool targetCarried=false;Condition targetCondition;
};
// restore: lower the target's wear/dirt (-1 unchanged); consume: remove the used item from the pack afterwards.
struct Result { bool handled=false;Action action=Action::none;bool restore=false,consume=false;int wear=-1,dirt=-1; };

struct Module { std::string name,source; };
// world.light_fire / world.put_out_fire, staged by an addon and run by the runtime (world_fire_bridge.h).
struct FireOrder { std::string owner,name;int kind=0;std::array<float,3> at{};float size=1;uint64_t handle=0;int hand=-1; };   // kind: 0 light, 1 put out, 2 attach, 3 detach
struct FireInfo { std::array<float,3> at{};float size=0,distance=0;bool mod=false; };
// characters.drop_held: a hand (0 right, 1 left) of a target lets go (held_items_bridge.h).
struct DropOrder { bool player=false;uint32_t record=0;uint64_t handle=0;int hand=0; };
// characters.sever: the joint of an arm part (character.hit numbering) to its parent is cut (held_items_bridge.h).
struct SeverOrder { bool player=false;uint32_t record=0;uint64_t handle=0;int part=0; };
// characters.play_motion: a motion index for a target (held_items_bridge.h motions).
struct MotionOrder { bool player=false;uint32_t record=0;uint64_t handle=0;uint32_t motion=0; };
// characters.keep_down: held characters stay on the ground (damage_scale_bridge.h footing).
struct KeepDown { bool player=false;uint32_t record=0;uint64_t handle=0; };
// tasks.worker: one job for a runtime worker thread. Everything the thread touches is in here (copied in at start);
// the result is read by the main thread only after `state` says it is finished.
struct WorkerJob {
 std::string addon,module,input,output,error;std::vector<Module> modules;size_t memory=0;std::vector<std::string> logs;
 std::atomic<int> state{0};std::atomic<bool> cancel{false};   // state: 0 waiting, 1 running, 2 done, 3 failed
};
struct ScriptEvent { std::string name,value;bool numberKnown=false;int32_t number=0; };
struct CombatHit {
 uint32_t record=0;std::string name;bool player=false,downed=false;float health=0,balance=0;
 std::string source;bool attacker=false,attackerPlayer=false;uint32_t attackerRecord=0;std::string attackerName;
 uint32_t weapon=0;int part=-1;uint64_t handle=0,attackerHandle=0;   // live-character handles (targets)
 bool weaponLit=false;   // a fire burns on the attacking weapon (a lit torch)
 bool blocked=false,parried=false;uint32_t shield=0,parryWeapon=0;   // the blow met the victim's shield or weapon
 uint32_t armour=0;   // the player's outermost worn piece covering the struck part
};
struct ZoneEntry { uint32_t zone=0,record=0;std::string name;bool player=false; };
struct GameMessage { std::string caption,text;int level=0; };
// power.cast: the player cast a Light-school power (domain byte 5), which no shipped power uses.
struct PowerCast { uint32_t power=0; };
// A point light an addon keeps alive near the player's head. Offsets are centimetres along the body's facing
// (forward), its right and world up; smoothing is the seconds the light takes to close most of a gap; bob is a
// slow vertical sway in centimetres. The runtime re-creates it after level changes while the addon keeps it set.
// A placed light (lights.place) stays at `position` (scene cm) instead and is forgotten when the level ends.
struct LightSpec {
 std::string name;std::array<float,3> color{1,1,1};float range=1200,intensity=2;
 float forward=0,right=0,up=80,smoothing=0.35f,bob=0;
 bool placed=false;std::array<float,3> position{};
 bool glow=false;float glowSize=8;std::array<float,3> glowColor{1,1,1};   // a small particle glow at the light (0..1 colour)
};
inline bool sameLight(const LightSpec& a,const LightSpec& b){
 return a.name==b.name&&a.color==b.color&&a.range==b.range&&a.intensity==b.intensity&&a.forward==b.forward&&a.right==b.right&&a.up==b.up&&a.smoothing==b.smoothing&&a.bob==b.bob&&
  a.placed==b.placed&&a.position==b.position&&a.glow==b.glow&&a.glowSize==b.glowSize&&a.glowColor==b.glowColor;
}
inline constexpr size_t lightsPerAddon=5;
struct OwnedLight { std::string addon;LightSpec light; };
// The player's thaumaturgy state as the game's own cost rules read it: focus fields, continuous bindings (the
// Continuity techniques, 0x1d3a80) and the technique flags at +0xc40 that change costs.
struct CasterState {
 float focus=0,ceiling=0,reserve=0;int bindings=0,bindingsUsed=0;uint32_t lastPower=0;   // lastPower: +0xdb0 & 0x3ffff
 bool iteration=false,sustain=false,abstraction=false,clarity=false;
};
struct LevelInfo { std::string name,mode;uint32_t id=0;bool idKnown=false,loaded=false; };
// gear.use: the player double-clicked a weapon or apparel item and clicked it on something in the world.
struct GearUse {
 uint32_t item=0;std::string model,kind;Condition condition;bool carried=false;   // carried: in the pack, not worn or held
 std::string targetKind,targetName;uint32_t targetRecord=0;float distance=NAN;
 float heat=NAN;   // cm from the target to the nearest burning fire within 300 cm; NaN for none
 bool ignitable=false;   // placed scenery the runtime may light (event:ignite_target)
};
// What the handler asked for: restore lowers wear/dirt (-1 = unchanged) through the game's own repair jobs;
// ignite lights a fire on the target object.
struct GearResult { bool handled=false,restore=false,ignite=false;int wear=-1,dirt=-1; };
// A native call staged by a callback and made after it returns (native_calls_bridge.h).
// One row an addon declares for the Mod Settings tab (settings.define). Values travel as text: "true"/"false",
// a number, a choice's text, or a virtual-key code; fallback is the default in that form.
struct SettingDef { std::string key,label,type;double minimum=0,maximum=1,step=1;std::vector<std::string> choices;std::string fallback,help;bool percent=false; };
struct SettingChange { std::string key,value; };
struct IdChange { uint32_t before=0,after=0; };   // item.id_changed
// A target: one character by its live handle (a characters.read row or a character.hit event), the player, or every
// live character with a record id.
template<class T,class C>inline bool targets(const T& w,const C& c){
 if(w.handle)return uint64_t(c.nativeCharacter)==w.handle;if(w.player)return c.playerKnown&&c.player;return c.record==w.record;}
// characters.set_mark: a named value on every live character with a record id (or the player) for other mods.
struct Mark {bool player=false;uint32_t record=0;uint64_t handle=0;char name[32]{};uint32_t value=0;};
// characters.set_damage_scale: per-kind multipliers on the damage a character takes (1 = unchanged).
struct DamageScale {bool player=false;uint32_t record=0;uint64_t handle=0;float cut=1,pierce=1,crush=1,impact=1;};
// characters.set_limb_strength: a muscle factor (0.05..1) for one body part (1..18, character.hit order).
struct LimbStrength {bool player=false;uint32_t record=0;uint64_t handle=0;int part=0;float factor=1;};
// characters.set_strength / set_anim_rate: field 0 motor strength, 1 base animation rate.
struct BodyScale {bool player=false;uint32_t record=0;uint64_t handle=0;int field=0;float factor=1;};
struct NativeCall { enum Kind { damage, push, drive, travel, place, damageCharacter } kind=damage;float values[4]{};bool flag=false;uint32_t record=0,state=0;char name[17]{};uintptr_t target=0; };
class Host {
public:
 std::string phaseName="loading";
 inline static bool(*subtitleSetter)(const char* label,const char* text)=nullptr;   // titlerows::setSubtitle   // the last lifecycle phase delivered to addons (menu, loading, session)
 using Logger=void(*)(void*,const char*,const char*,const char*);
 using WorldProvider=const WorldSnapshot*(*)();
 using TimeRead=bool(*)(int64_t&);using TimeWrite=bool(*)(uint32_t);
 using SaveGet=bool(*)(const std::string&,std::string&);using SaveSet=bool(*)(const std::string&,const std::string*);
 using FlagGet=bool(*)(const std::string&,int32_t&);using FlagSet=bool(*)(const std::string&,const int32_t*);
 using CharacterProvider=const CharacterSnapshot*(*)();
 using AiCommit=bool(*)(const std::vector<AiChange>&);
 using AiCapture=uint64_t(*)(uintptr_t);
 using ContainerCapture=uint64_t(*)(uintptr_t);
 using ContainerRead=const ContainerView*(*)(uint64_t);
 using ContainerCommit=bool(*)(const std::vector<ContainerChange>&);
 using InventoryProvider=const InventoryView*(*)();
 using InventoryCommit=bool(*)(const InventoryChange&);
 using BagRead=const ContainerView*(*)(uint64_t,size_t);
 using InteractionCapture=const InteractionView*(*)(uintptr_t);
 using LevelRead=bool(*)(LevelInfo&);using StatsRead=bool(*)(uint32_t,ItemStats&);
 using NearbyRead=bool(*)(float,const std::vector<std::string>&,NearbyResult&);
 using NativeCommit=bool(*)(const NativeCall&);using CasterRead=bool(*)(CasterState&);
 // Shared owners (title rows, served level members): every registration names its addon, so it can be dropped.
 struct SharedApi {
  bool(*addRow)(const std::string& owner,const std::string& label,int after,int actsAs)=nullptr;
  bool(*removeRow)(const std::string& owner,const std::string& label)=nullptr;
  bool(*enableRow)(const std::string& owner,const std::string& label,bool enabled)=nullptr;
  bool(*serve)(const std::string& owner,const std::string& package,const std::string& name,const std::string& bytes)=nullptr;
  void(*clearServed)(const std::string& owner)=nullptr;
  void(*forget)(const std::string& owner)=nullptr;
  bool(*newGame)(const std::string& member)=nullptr;
  bool(*readAsset)(const std::string& owner,const std::string& path,std::string& out,std::string& why)=nullptr;
  void(*notify)(const std::string& text)=nullptr;
  bool(*defineSettings)(const std::string& owner,const std::string& title,const std::vector<SettingDef>& rows)=nullptr;
  bool(*readSetting)(const std::string& owner,const std::string& key,std::string& value)=nullptr;
  bool(*sound)(const std::string& name,float volume)=nullptr;
  bool(*addPlacement)(const std::string& owner,const std::string& level,uint32_t item,const float* position,float yaw)=nullptr;
  bool(*gesture)(const std::string& kind)=nullptr;
  bool(*addWear)(uint32_t item,int wear,int dirt)=nullptr;
  bool(*grantPowers)(uint32_t record,const std::vector<uint32_t>& ids)=nullptr;
  bool(*dialog)(const std::string& title,const std::string& text)=nullptr;
  bool(*readPackage)(const std::string& package,const std::string& member,std::string& out,std::string& why)=nullptr;
  bool(*fires)(std::vector<FireInfo>& out)=nullptr;
  bool(*writeAsset)(const std::string& owner,const std::string& path,const std::string& bytes,std::string& why)=nullptr;
 };
private:
 // What a subscription listens to; item-use handlers also carry the model name they match.
 enum class Kind { item, lifecycle, combat, script, zone, message, power, gear, setting, idChange };
 struct Handler { std::string model;int ref=0;Kind kind=Kind::item; };
 struct Subscription { const char* name;Kind kind;uint64_t needs;const char* missing; };
 static constexpr Subscription subscriptions[]={
  {"runtime.phase",Kind::lifecycle,lifecycleEvents,"events.lifecycle"},{"character.hit",Kind::combat,combatEvents,"events.combat"},
  {"script.event",Kind::script,scriptEvents,"events.script"},{"zone.enter",Kind::zone,zoneEvents,"events.zone"},
  {"game.message",Kind::message,gameMessages,"events.messages"},{"power.cast",Kind::power,powerCast,"powers.cast"},
  {"gear.use",Kind::gear,gearUse,"events.gear_use"},{"settings.changed",Kind::setting,modSettings,"ui.settings"},{"item.id_changed",Kind::idChange,items,"items.read"}};
 // The addon profiler (on by default; the runtime's Mod Settings "Profiler" toggle): per addon and per kind of call,
 // calls, wall time, Lua instructions, memory and sampled hot lines; mods add their own scopes with profile.*.
 public:inline static bool profiling=true;private:
 struct ProfileBucket {uint64_t calls=0,instructions=0;double ms=0,worst=0;};
 struct Addon {
  Host* owner=nullptr;std::string id,error;lua_State* state=nullptr;
  uint64_t caps=0;size_t bytes=0,limit=8*1024*1024;
  std::map<std::string,ProfileBucket> profile,profileTotal;std::unordered_map<std::string,uint64_t> hotLines,hotTotal;size_t peakBytes=0;
  uint64_t gameCalls=0,gameCallsTotal=0,failures=0;std::chrono::steady_clock::time_point profileStart{};bool profileOpen=false;const char* profileLabel="callback";int budgetGiven=100000;
  std::vector<std::pair<std::string,std::chrono::steady_clock::time_point>> scopes;
  bool loading=true,enabled=true;int instructions=0,storageOps=0,worldOps=0,characterOps=0;
  bool timerRunning=false,taskRunning=false;std::vector<int> tasks,taskBudgets;
  struct Worker {uint32_t handle=0;std::shared_ptr<WorkerJob> job;int callback=LUA_NOREF;};std::vector<Worker> workers;std::vector<FireOrder> fireOrders;std::vector<DropOrder> drops;std::vector<SeverOrder> severs;std::vector<KeepDown> keepDown;std::vector<MotionOrder> motions;uint64_t execution=0;std::vector<AiChange> aiChanges;int statsReads=0;   // tasks: registry refs of tasks.run coroutines
  std::vector<std::pair<std::string,int>> titleHandlers;std::vector<int> titleAfter,titleActsAs;   // row label -> Lua function reference, anchor, base row it acts as
  std::vector<NativeCall> nativeCalls;
  std::string settingsTitle;std::vector<SettingDef> settingDefs;   // Mod Settings rows, published after a successful load
  const GearUse* gear=nullptr;GearResult gearPending;   // the gear.use event being handled, and its requested restore
  GearResult itemRestore;bool consumePending=false;      // item.use: restore of the target, consume of the used item
  std::vector<ContainerChange> containerChanges;
  int containerReads=0;
  std::vector<InventoryChange> inventoryChanges;int inventoryReads=0;
  const Event* event=nullptr;uint64_t token=0;Action pending=Action::none;
  std::vector<Handler> handlers;TimerQueue timers;
  settings::Cache* settingsData=nullptr;std::unique_ptr<settings::Document> staged;
  std::vector<Module> modules;std::vector<bool> importing;int cache=0,logs=0;
  std::vector<LightSpec> lights;lua_Number focus=0,spend=0,restore=0;int bindings=0;std::vector<uint32_t> teach;std::vector<uint32_t> wheel;std::vector<Mark> marks;std::vector<DamageScale> scales;std::vector<LimbStrength> limbs;int hitMaterial=-1;std::vector<BodyScale> body;std::vector<std::pair<std::string,int>> messageHandlers;std::vector<std::pair<std::string,std::string>> inbox;   /* inbox: (from, encoded topic+payload) */ std::vector<std::array<float,4>> arcs;uint32_t schools=0;   // teach: Light powers to make known; schools: open_school bits   // bindings: continuous-power slots this addon holds   // arcs: strands, segments, life, length
  ~Addon(){if(state)lua_close(state);}
 };
 std::vector<std::unique_ptr<Addon>> addons;
 uint64_t token=0,clock=0,nextTimer=0;size_t timerCursor=0;
 std::string lastFailure;
 uint64_t available=itemApiCapabilities;
 Logger logger=nullptr;void* loggerContext=nullptr;
 WorldProvider worldProvider=nullptr;
 CharacterProvider characterProvider=nullptr;
 CharacterProvider awarenessProvider=nullptr;
 AiCommit aiCommit=nullptr;
 AiCapture aiCapture=nullptr;
 ContainerCapture containerCapture=nullptr;ContainerRead containerRead=nullptr;ContainerCommit containerCommit=nullptr;
 InventoryProvider inventoryProvider=nullptr;InventoryCommit inventoryCommit=nullptr;
 TimeRead timeRead=nullptr;TimeWrite timeWrite=nullptr;SaveGet saveGet=nullptr;SaveSet saveSet=nullptr;FlagGet flagGet=nullptr;FlagSet flagSet=nullptr;
 BagRead bagRead=nullptr;
 ContainerCapture worldItemCapture=nullptr;InteractionCapture interactionCapture=nullptr;
 LevelRead levelRead=nullptr;StatsRead statsRead=nullptr;CharacterProvider detailsProvider=nullptr;SharedApi shared;NearbyRead nearbyRead=nullptr;NativeCommit nativeCommit=nullptr;CasterRead casterRead=nullptr;
 static int inventoryMethod(lua_State* L){
  auto a=context(L);const int op=int(lua_tointeger(L,lua_upvalueindex(4)));
  const auto operation=InventoryOp(op);
  const uint64_t needed=operation==InventoryOp::spawnItem?worldSpawn:operation==InventoryOp::teleporterUse?worldTeleporters:(operation==InventoryOp::scannerUse||operation==InventoryOp::emitterSet||operation==InventoryOp::emitterToggle||operation==InventoryOp::fireUse||operation==InventoryOp::fireExtinguish||operation==InventoryOp::torchIgnite)?worldDevices:operation==InventoryOp::doorOpen?worldDoors:(operation==InventoryOp::leverPosition||operation==InventoryOp::leverToggle)?worldLevers:(operation==InventoryOp::itemUse||operation==InventoryOp::useItemOnWorld)?itemsUse:(operation==InventoryOp::worldUse||operation==InventoryOp::worldPress)?worldUse:operation==InventoryOp::unequipWeapon?weaponUnequip:operation==InventoryOp::equipShield?shieldEquip:operation==InventoryOp::bagTransfer?inventoryBags:operation==InventoryOp::store?(inventoryTransfer|inventoryBags):operation==InventoryOp::equipWeapon?weaponEquip:operation==InventoryOp::bagTake?inventoryBags:operation==InventoryOp::switchWeapons?weaponSwitch:operation==InventoryOp::pickup?worldPickup:operation==InventoryOp::drop?worldDrop:(op<2)?inventoryTransfer:equipmentWrite;
  if(!(a->caps&needed)||!(a->owner->available&needed)||!a->timerRunning||a->loading||
   uint64_t(lua_tointeger(L,lua_upvalueindex(2)))!=a->execution)return luaL_error(L,"inventory method expired or unavailable");
  luaL_checktype(L,1,LUA_TTABLE);
  if(!a->inventoryChanges.empty()||!a->aiChanges.empty()||!a->containerChanges.empty())return luaL_error(L,"inventory operations require one operation in a separate callback");
  InventoryChange change;change.op=InventoryOp(op);change.inventory=uint64_t(lua_tointeger(L,lua_upvalueindex(1)));change.row=size_t(lua_tointeger(L,lua_upvalueindex(3)));
  if(change.op==InventoryOp::fireExtinguish||change.op==InventoryOp::torchIgnite||change.op==InventoryOp::fireUse||change.op==InventoryOp::emitterSet||change.op==InventoryOp::emitterToggle||change.op==InventoryOp::teleporterUse||change.op==InventoryOp::scannerUse||change.op==InventoryOp::worldUse||change.op==InventoryOp::worldPress||change.op==InventoryOp::leverPosition||change.op==InventoryOp::leverToggle||change.op==InventoryOp::doorOpen){
   change.worldItem=change.inventory;const auto view=a->owner->inventoryProvider?a->owner->inventoryProvider():nullptr;if(!view){lua_pushboolean(L,0);return 1;}change.inventory=view->ticket;change.active=true;
   if(change.op==InventoryOp::worldPress||change.op==InventoryOp::doorOpen||change.op==InventoryOp::emitterSet){luaL_checktype(L,2,LUA_TBOOLEAN);change.active=lua_toboolean(L,2)!=0;}
   if(change.op==InventoryOp::leverPosition){if(!lua_isinteger(L,2)||(lua_tointeger(L,2)!=0&&lua_tointeger(L,2)!=1))return luaL_error(L,"set_position expects endpoint 0 or 1");change.active=lua_tointeger(L,2)!=0;}
  }else if(change.op==InventoryOp::useItemOnWorld){
   change.worldItem=change.inventory;luaL_checktype(L,2,LUA_TTABLE);lua_getfield(L,2,"use");
   if(!lua_iscfunction(L,-1)||lua_tocfunction(L,-1)!=inventoryMethod)return luaL_error(L,"use_item requires a current carried row");
   lua_getupvalue(L,-1,4);const auto method=lua_tointeger(L,-1);lua_pop(L,1);lua_getupvalue(L,-1,2);const auto execution=uint64_t(lua_tointeger(L,-1));lua_pop(L,1);
   lua_getupvalue(L,-1,1);change.inventory=uint64_t(lua_tointeger(L,-1));lua_pop(L,1);lua_getupvalue(L,-1,3);change.row=size_t(lua_tointeger(L,-1));lua_pop(L,2);
   if(method!=int(InventoryOp::itemUse)||execution!=a->execution)return luaL_error(L,"carried item method expired or unavailable");
  }else if(change.op==InventoryOp::take||change.op==InventoryOp::pickup){
   if(change.op==InventoryOp::take){
    if(!lua_isinteger(L,2)||lua_tointeger(L,2)<1||lua_tointeger(L,2)>4096)return luaL_error(L,"take_item expects a one-based contents row index");
    change.container=change.inventory;change.row=size_t(lua_tointeger(L,2)-1);
   }else change.worldItem=change.inventory;
   const auto view=a->owner->inventoryProvider?a->owner->inventoryProvider():nullptr;if(!view){lua_pushboolean(L,0);return 1;}change.inventory=view->ticket;
  }else if(change.op==InventoryOp::store){
   luaL_checktype(L,2,LUA_TTABLE);lua_getfield(L,2,"take_item");
   if(!lua_iscfunction(L,-1)||lua_tocfunction(L,-1)!=inventoryMethod)return luaL_error(L,"store_in requires a current container or carried bag");
   lua_getupvalue(L,-1,4);const auto method=lua_tointeger(L,-1);lua_pop(L,1);
   lua_getupvalue(L,-1,2);const auto execution=uint64_t(lua_tointeger(L,-1));lua_pop(L,1);
   if((method!=int(InventoryOp::take)&&method!=int(InventoryOp::bagTake))||execution!=a->execution)return luaL_error(L,"container method expired");
   lua_getupvalue(L,-1,1);change.container=uint64_t(lua_tointeger(L,-1));lua_pop(L,2);
   if(method==int(InventoryOp::bagTake)){
    if(!(a->caps&inventoryBags)||!(a->owner->available&inventoryBags))return luaL_error(L,"inventory.bags unavailable");
    lua_getfield(L,2,"take_item");lua_getupvalue(L,-1,3);change.bag=size_t(lua_tointeger(L,-1));lua_pop(L,2);
    if(change.container!=change.inventory||!a->owner->bagRead||!a->owner->bagRead(change.container,change.bag)){lua_pushboolean(L,0);return 1;}
    change.op=InventoryOp::bagStore;
   }else if(!(a->caps&inventoryTransfer)||!(a->owner->available&inventoryTransfer))return luaL_error(L,"inventory.transfer unavailable");
  }else if(change.op==InventoryOp::bagTake){
   if(!lua_isinteger(L,2)||lua_tointeger(L,2)<1||lua_tointeger(L,2)>4096)return luaL_error(L,"take_item expects a one-based contents row index");
   change.bag=change.row;change.row=size_t(lua_tointeger(L,2)-1);
   if(!a->owner->bagRead||!a->owner->bagRead(change.inventory,change.bag)){lua_pushboolean(L,0);return 1;}
  }else if(change.op==InventoryOp::bagTransfer){
   if(!lua_isinteger(L,2)||lua_tointeger(L,2)<1||lua_tointeger(L,2)>4096)return luaL_error(L,"transfer_to expects a one-based contents row index");
   change.bag=change.row;change.row=size_t(lua_tointeger(L,2)-1);luaL_checktype(L,3,LUA_TTABLE);lua_getfield(L,3,"take_item");
   if(!lua_iscfunction(L,-1)||lua_tocfunction(L,-1)!=inventoryMethod)return luaL_error(L,"transfer_to requires a current carried bag");
   lua_getupvalue(L,-1,4);const auto method=lua_tointeger(L,-1);lua_pop(L,1);
   lua_getupvalue(L,-1,2);const auto execution=uint64_t(lua_tointeger(L,-1));lua_pop(L,1);
   lua_getupvalue(L,-1,1);const auto ticket=uint64_t(lua_tointeger(L,-1));lua_pop(L,1);
   lua_getupvalue(L,-1,3);change.destinationBag=size_t(lua_tointeger(L,-1));lua_pop(L,2);
   if(method!=int(InventoryOp::bagTake)||execution!=a->execution||ticket!=change.inventory)return luaL_error(L,"destination bag method expired or unavailable");
   if(!a->owner->bagRead||!a->owner->bagRead(change.inventory,change.bag)||!a->owner->bagRead(change.inventory,change.destinationBag)){lua_pushboolean(L,0);return 1;}
  }else if(change.op==InventoryOp::unequipWeapon){change.hand=unsigned(change.row);
  }else if(change.op==InventoryOp::equipWeapon){
   size_t size=0;const auto hand=luaL_checklstring(L,2,&size);
   if(size==5&&!std::memcmp(hand,"right",5))change.hand=0;
   else if(size==4&&!std::memcmp(hand,"left",4))change.hand=1;
   else return luaL_error(L,"equip_weapon expects 'right' or 'left'");
  }else if(change.op==InventoryOp::drop||change.op==InventoryOp::spawnItem){
   int at=2;
   if(change.op==InventoryOp::spawnItem){
    if(!lua_isinteger(L,2)||lua_tointeger(L,2)<1||lua_tointeger(L,2)>0x3fffffff)return luaL_error(L,"spawn_item expects an item database ID");
    change.item=uint32_t(lua_tointeger(L,2));at=3;
   }
   luaL_checktype(L,at,LUA_TTABLE);
   for(int i=0;i<3;++i){lua_rawgeti(L,at,i+1);if(lua_type(L,-1)!=LUA_TNUMBER)return luaL_error(L,"drop expects three finite player-sector coordinates");
    const double value=lua_tonumber(L,-1);if(!std::isfinite(value)||std::abs(value)>10000000)return luaL_error(L,"drop position outside supported range");change.position[size_t(i)]=float(value);lua_pop(L,1);
   }
  }
  a->inventoryChanges.push_back(change);lua_pushboolean(L,1);return 1;
 }
 static void inventoryClosure(lua_State* L,Addon* a,uint64_t ticket,size_t row,InventoryOp op,const char* name){
  lua_pushinteger(L,lua_Integer(ticket));lua_pushinteger(L,lua_Integer(a->execution));lua_pushinteger(L,lua_Integer(row));lua_pushinteger(L,int(op));lua_pushcclosure(L,inventoryMethod,4);lua_setfield(L,-2,name);
 }
 static int bagContents(lua_State* L){
  auto a=context(L);luaL_checktype(L,1,LUA_TTABLE);
  if(!(a->caps&inventoryBags)||!(a->owner->available&inventoryBags)||!a->timerRunning||a->loading||
   uint64_t(lua_tointeger(L,lua_upvalueindex(2)))!=a->execution)return luaL_error(L,"bag method expired or unavailable");
  if(++a->containerReads>4)return luaL_error(L,"container query limit exceeded");
  const auto view=a->owner->bagRead?a->owner->bagRead(uint64_t(lua_tointeger(L,lua_upvalueindex(1))),size_t(lua_tointeger(L,lua_upvalueindex(3)))):nullptr;
  if(!view){lua_pushnil(L);return 1;}lua_newtable(L);size_t index=0;
  for(const auto& row:view->entries){lua_newtable(L);number(L,"item_id",row.item);number(L,"state",row.state);lua_pushboolean(L,(row.state&0xc80100)!=0);lua_setfield(L,-2,"removed");lua_rawseti(L,-2,lua_Integer(++index));}return 1;
 }
 static int snapshotInventory(lua_State* L){
  auto a=context(L);if(!(a->caps&inventoryRead)||!(a->owner->available&inventoryRead))return luaL_error(L,"inventory.read unavailable");
  if(++a->inventoryReads>4)return luaL_error(L,"inventory snapshot query limit exceeded");
  const auto view=a->owner->inventoryProvider?a->owner->inventoryProvider():nullptr;if(!view){lua_pushnil(L);return 1;}
  lua_newtable(L);
  text(L,"space","player_sector_cm");lua_newtable(L);for(size_t i=0;i<3;++i){lua_pushnumber(L,view->position[i]);lua_rawseti(L,-2,lua_Integer(i+1));}lua_setfield(L,-2,"position");
  for(int alternate=0;alternate<2;++alternate){lua_newtable(L);for(size_t i=0;i<2;++i){lua_newtable(L);number(L,"item_id",view->weapons[size_t(alternate)*4+i*2]);number(L,"state",view->weapons[size_t(alternate)*4+i*2+1]);
   if(!alternate&&a->timerRunning&&(a->caps&weaponUnequip)&&(a->owner->available&weaponUnequip))inventoryClosure(L,a,view->ticket,i,InventoryOp::unequipWeapon,"unequip");
   lua_rawseti(L,-2,lua_Integer(i+1));}lua_setfield(L,-2,alternate?"alternate_weapons":"weapons");}
  if(a->timerRunning&&(a->caps&weaponSwitch)&&(a->owner->available&weaponSwitch))inventoryClosure(L,a,view->ticket,0,InventoryOp::switchWeapons,"switch_weapons");
  if(a->timerRunning&&(a->caps&worldSpawn)&&(a->owner->available&worldSpawn))inventoryClosure(L,a,view->ticket,0,InventoryOp::spawnItem,"spawn_item");
  for(int worn=0;worn<2;++worn){lua_newtable(L);const auto& rows=worn?view->worn:view->carried;size_t index=0;
   for(const auto& row:rows){lua_newtable(L);number(L,"item_id",row.item);number(L,"state",row.state);
    if(a->timerRunning){
     if(!worn&&((a->caps&a->owner->available)&(inventoryTransfer|inventoryBags)))inventoryClosure(L,a,view->ticket,index,InventoryOp::store,"store_in");
     if(!worn&&(a->caps&weaponEquip)&&(a->owner->available&weaponEquip))inventoryClosure(L,a,view->ticket,index,InventoryOp::equipWeapon,"equip_weapon");
     if(!worn&&(a->caps&itemsUse)&&(a->owner->available&itemsUse))inventoryClosure(L,a,view->ticket,index,InventoryOp::itemUse,"use");
     if(!worn&&(a->caps&shieldEquip)&&(a->owner->available&shieldEquip))inventoryClosure(L,a,view->ticket,index,InventoryOp::equipShield,"equip_shield");
     if(!worn&&(a->caps&inventoryBags)&&(a->owner->available&inventoryBags)){
      inventoryClosure(L,a,view->ticket,index,InventoryOp::bagTake,"take_item");
      inventoryClosure(L,a,view->ticket,index,InventoryOp::bagTransfer,"transfer_to");
      lua_pushinteger(L,lua_Integer(view->ticket));lua_pushinteger(L,lua_Integer(a->execution));lua_pushinteger(L,lua_Integer(index));lua_pushcclosure(L,bagContents,3);lua_setfield(L,-2,"contents");
     }
     if(!worn&&(a->caps&worldDrop)&&(a->owner->available&worldDrop))inventoryClosure(L,a,view->ticket,index,InventoryOp::drop,"drop");
     if((a->caps&equipmentWrite)&&(a->owner->available&equipmentWrite))inventoryClosure(L,a,view->ticket,index,worn?InventoryOp::unequip:InventoryOp::equip,worn?"unequip":"equip");
    }lua_rawseti(L,-2,lua_Integer(++index));
   }lua_setfield(L,-2,worn?"worn":"carried");
  }return 1;
 }
 static int containerMethod(lua_State* L){
  auto a=context(L);
  const int op=int(lua_tointeger(L,lua_upvalueindex(3)));const uint64_t needed=op?containerEdit:(containerEdit|inventoryTransfer);
  if(!(a->caps&needed)||!(a->owner->available&needed)||!a->timerRunning||a->loading||
     uint64_t(lua_tointeger(L,lua_upvalueindex(2)))!=a->execution)return luaL_error(L,"container method expired or unavailable");
  const auto ticket=uint64_t(lua_tointeger(L,lua_upvalueindex(1)));
  luaL_checktype(L,1,LUA_TTABLE);
  if(!op){
   if(++a->containerReads>4)return luaL_error(L,"container read limit exceeded");
   const auto view=a->owner->containerRead?a->owner->containerRead(ticket):nullptr;if(!view){lua_pushnil(L);return 1;}
   lua_newtable(L);int i=0;for(const auto& row:view->entries){lua_newtable(L);number(L,"item_id",row.item);number(L,"state",row.state);
    boolean(L,"removed",(row.state&0xc80000)!=0);lua_rawseti(L,-2,++i);}return 1;
  }
  if(!lua_isinteger(L,2))return luaL_error(L,"item ID must be an integer");const auto item=lua_tointeger(L,2);
  if(item<=0||item>=0x40000000)return luaL_error(L,"expected an objdb item ID");
  if(a->containerChanges.size()>=16)return luaL_error(L,"container edit limit exceeded");
  if(!a->inventoryChanges.empty())return luaL_error(L,"inventory operations require a separate callback");
  if(!a->aiChanges.empty())return luaL_error(L,"NPC and container edits require separate callbacks");
  if(!a->containerChanges.empty()&&a->containerChanges.front().target!=ticket)return luaL_error(L,"edit only one container per callback");
  a->containerChanges.push_back({ticket,uint32_t(item),op==1});lua_pushboolean(L,1);return 1;
 }
 static int stageCharacterChange(lua_State* L,const AiChange& change){
  auto a=context(L);
  if(!a->inventoryChanges.empty())return luaL_error(L,"inventory operations require a separate callback");
  if(!a->containerChanges.empty())return luaL_error(L,"NPC and container edits require separate callbacks");
  for(auto& previous:a->aiChanges)if(previous.target==change.target){
   if((previous.ai&&change.ai)||(previous.hostility&&change.hostility)||(previous.concern&&change.concern))return luaL_error(L,"NPC field already edited in this callback");
   if(change.ai){previous.ai=true;previous.values=change.values;previous.boostSeconds=change.boostSeconds;previous.owner=change.owner;previous.keepAttribute2=change.keepAttribute2;previous.keepAttribute3=change.keepAttribute3;}
   if(change.hostility){previous.hostility=true;previous.hostilityValue=change.hostilityValue;}
   if(change.concern){previous.concern=true;previous.concernValue=change.concernValue;}
   lua_pushboolean(L,1);return 1;
  }
  if(a->aiChanges.size()>=8)return luaL_error(L,"NPC edit callback limit exceeded");
  a->aiChanges.push_back(change);lua_pushboolean(L,1);return 1;
 }
 static int changeAttitude(lua_State* L){
  auto a=context(L);
  if(!(a->caps&attitudeWrite)||!(a->owner->available&attitudeWrite)||!a->timerRunning||a->loading||
     uint64_t(lua_tointeger(L,lua_upvalueindex(2)))!=a->execution)return luaL_error(L,"NPC attitude edit method expired or unavailable");
  luaL_checktype(L,1,LUA_TTABLE);luaL_checktype(L,2,LUA_TTABLE);
  AiChange change;change.target=uint64_t(lua_tointeger(L,lua_upvalueindex(1)));change.ai=false;
  for(int i=0;i<2;++i){lua_getfield(L,2,i?"concern":"hostility");
   if(!lua_isnil(L,-1)){
    if(lua_type(L,-1)!=LUA_TNUMBER)return luaL_error(L,"attitude fields must be finite numbers");
    const double value=lua_tonumber(L,-1),limit=i?npc::concernLimit:npc::hostilityLimit;
    if(!std::isfinite(value)||value<0||value>limit)return luaL_error(L,"attitude field outside supported range");
    if(i){change.concern=true;change.concernValue=float(value);}else{change.hostility=true;change.hostilityValue=float(value);}
   }lua_pop(L,1);
  }
  if(!change.hostility&&!change.concern)return luaL_error(L,"set_attitude requires hostility or concern");
  return stageCharacterChange(L,change);
 }
 // character:damage{health = h, balance = b, can_fall = bool}: the game's own damage applier on this (non-player)
 // character, staged like player.damage; the runtime re-finds it in this pulse's roster before the call. Each of
 // health and balance 0..1 (a full pool is about 1).
 static int damageCharacter(lua_State* L){
  auto a=context(L);
  if(uint64_t(lua_tointeger(L,lua_upvalueindex(2)))!=a->execution)return luaL_error(L,"character:damage expired: use the entry from this callback's characters.read()");
  stageNative(L,charactersDamage,"characters.damage");
  const int opts=lua_istable(L,2)?2:1;luaL_checktype(L,opts,LUA_TTABLE);
  auto field=[&](const char* key)->float{lua_getfield(L,opts,key);float v=0;if(!lua_isnil(L,-1)){const double d=luaL_checknumber(L,-1);if(!std::isfinite(d)||d<0||d>1)luaL_error(L,"character:damage %s must be 0..1",key);v=float(d);}lua_pop(L,1);return v;};
  NativeCall c;c.kind=NativeCall::damageCharacter;c.values[1]=field("health");c.values[0]=field("balance");
  lua_getfield(L,opts,"can_fall");c.flag=lua_toboolean(L,-1)!=0;lua_pop(L,1);c.target=uintptr_t(lua_tointeger(L,lua_upvalueindex(1)));
  if(!c.target)return luaL_error(L,"character:damage: no character");
  a->nativeCalls.push_back(c);lua_pushboolean(L,1);return 1;
 }
 // characters.set_mark(target, name, value): target a record_id or "player"; name 1..31 bytes of letters, digits,
 // _ . -; value 0..255 (0 clears). Held until a level change or the addon unloads; at most 32 per addon.
 // A target argument: "player", a record id, or a table with a `handle` (a characters.read row or a hit event).
 static void readTarget(lua_State* L,int index,const char* name,bool& player,uint32_t& record,uint64_t& handle){
  player=false;record=0;handle=0;
  if(lua_type(L,index)==LUA_TSTRING){if(std::strcmp(lua_tostring(L,index),"player"))luaL_error(L,"%s target is \"player\", a record_id or a row with a handle",name);player=true;return;}
  if(lua_istable(L,index)){lua_getfield(L,index,"handle");if(!lua_isinteger(L,-1)||lua_tointeger(L,-1)<=0)luaL_error(L,"%s: the row has no handle (use characters.read or a character.hit event)",name);handle=uint64_t(lua_tointeger(L,-1));lua_pop(L,1);return;}
  const lua_Integer r=luaL_checkinteger(L,index);if(r<=0||r>0xffffffffLL)luaL_error(L,"%s: bad record_id",name);record=uint32_t(r);
 }
 // addons.send(target, topic, value) / addons.on(topic, fn): messages between addons. `value` (nil, boolean, number,
 // string, or a table of those, nested at most 6 deep, 16 KiB encoded) is copied, never shared; `target` is an addon ID
 // or "*" for every other addon. Delivered at the next pulse to the target's handlers for `topic`, as fn(value, from).
 // At most 32 messages waiting per receiver; a full inbox refuses (false).
 static bool encodeValue(lua_State* L,int index,std::string& out,int depth,size_t maxBytes=16384,int maxDepth=6){
  if(out.size()>maxBytes||depth>maxDepth)return false;index=lua_absindex(L,index);
  switch(lua_type(L,index)){
   case LUA_TNIL:out+='n';return true;
   case LUA_TBOOLEAN:out+=lua_toboolean(L,index)?'t':'f';return true;
   case LUA_TNUMBER:if(lua_isinteger(L,index)){out+='i';const lua_Integer v=lua_tointeger(L,index);out.append((const char*)&v,8);}else{out+='d';const lua_Number v=lua_tonumber(L,index);out.append((const char*)&v,8);}return true;
   case LUA_TSTRING:{size_t n=0;const char* s=lua_tolstring(L,index,&n);if(n>maxBytes)return false;out+='s';const uint32_t k=uint32_t(n);out.append((const char*)&k,4);out.append(s,n);return true;}
   case LUA_TTABLE:{if(!lua_checkstack(L,4))return false;out+='{';lua_pushnil(L);
    while(lua_next(L,index)){const int key=lua_type(L,-2);if(key!=LUA_TSTRING&&key!=LUA_TNUMBER){lua_pop(L,2);return false;}
     if(!encodeValue(L,-2,out,depth+1,maxBytes,maxDepth)||!encodeValue(L,-1,out,depth+1,maxBytes,maxDepth)){lua_pop(L,2);return false;}lua_pop(L,1);}
    out+='}';return out.size()<=maxBytes;}
   default:return false;
  }
 }
 static bool decodeValue(lua_State* L,const std::string& in,size_t& at,int depth,int maxDepth=6){
  if(at>=in.size()||depth>maxDepth+1||!lua_checkstack(L,4))return false;const char tag=in[at++];
  switch(tag){
   case 'n':lua_pushnil(L);return true;case 't':lua_pushboolean(L,1);return true;case 'f':lua_pushboolean(L,0);return true;
   case 'i':{if(at+8>in.size())return false;lua_Integer v;std::memcpy(&v,in.data()+at,8);at+=8;lua_pushinteger(L,v);return true;}
   case 'd':{if(at+8>in.size())return false;lua_Number v;std::memcpy(&v,in.data()+at,8);at+=8;lua_pushnumber(L,v);return true;}
   case 's':{if(at+4>in.size())return false;uint32_t n;std::memcpy(&n,in.data()+at,4);at+=4;if(at+n>in.size())return false;lua_pushlstring(L,in.data()+at,n);at+=n;return true;}
   case '{':{lua_newtable(L);while(at<in.size()&&in[at]!='}'){if(!decodeValue(L,in,at,depth+1,maxDepth))return false;if(!decodeValue(L,in,at,depth+1,maxDepth))return false;lua_rawset(L,-3);}if(at>=in.size())return false;++at;return true;}
   default:return false;
  }
 }
 static int sendMessage(lua_State* L){
  auto a=context(L);const char* target=luaL_checkstring(L,1);size_t tn=0;const char* topic=luaL_checklstring(L,2,&tn);
  if(!tn||tn>64)return luaL_error(L,"addons.send topic must be 1..64 bytes");
  std::string payload(topic,tn);payload.insert(payload.begin(),char(tn));
  if(lua_isnone(L,3))payload+='n';
  else if(!encodeValue(L,3,payload,0))return luaL_error(L,"addons.send value must be nil, a boolean, number, string or a table of those (at most 6 deep, 16 KiB)");
  bool any=false;
  for(auto& other:a->owner->addons){if(other.get()==a||!other->enabled)continue;if(std::strcmp(target,"*")&&other->id!=target)continue;
   if(other->inbox.size()>=32)continue;other->inbox.push_back({a->id,payload});any=true;}
  lua_pushboolean(L,any);return 1;
 }
 static int onMessage(lua_State* L){
  auto a=context(L);size_t tn=0;const char* topic=luaL_checklstring(L,1,&tn);luaL_checktype(L,2,LUA_TFUNCTION);
  if(!tn||tn>64)return luaL_error(L,"addons.on topic must be 1..64 bytes");if(a->messageHandlers.size()>=32)return luaL_error(L,"addons.on: at most 32 handlers");
  lua_pushvalue(L,2);a->messageHandlers.push_back({std::string(topic,tn),luaL_ref(L,LUA_REGISTRYINDEX)});lua_pushboolean(L,1);return 1;
 }
 void deliverMessages(){
  for(auto& owned:addons){auto& a=*owned;if(!a.enabled||a.inbox.empty())continue;auto mail=std::move(a.inbox);a.inbox.clear();
   for(const auto& [from,payload]:mail){if(!a.enabled)break;const size_t tn=size_t((unsigned char)payload[0]);if(payload.size()<1+tn)continue;
    const std::string topic=payload.substr(1,tn);
    for(const auto& h:a.messageHandlers){if(h.first!=topic||!a.enabled)continue;auto L=a.state;if(!lua_checkstack(L,8))break;
     lua_rawgeti(L,LUA_REGISTRYINDEX,h.second);size_t at=1+tn;if(!decodeValue(L,payload,at,0)){lua_settop(L,0);continue;}lua_pushstring(L,from.c_str());
     startBudget(a,"message");a.timerRunning=true;const int status=protectedCall(L,2,0);a.timerRunning=false;stopBudget(a);
     if(status!=LUA_OK)failure(a,"addons message handler failed");else{commitAiChanges(a);commitSettings(a);lua_settop(L,0);}}
   }
  }
 }
 // profile.begin(name) / profile.finish(name) / profile.scope(name, fn, ...): the addon's own timed scopes in the
 // runtime profiler's log (name 1..48 bytes; scopes may nest, at most 16 open). scope returns fn's results.
 static void closeScope(Addon& a,size_t index){
  const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-a.scopes[index].second).count();
  auto& b=a.profile["scope "+a.scopes[index].first];++b.calls;b.ms+=ms;b.worst=(std::max)(b.worst,ms);a.scopes.erase(a.scopes.begin()+std::ptrdiff_t(index));
 }
 static int profileBegin(lua_State* L){
  auto a=context(L);size_t n=0;const char* name=luaL_checklstring(L,1,&n);if(!n||n>48)return luaL_error(L,"profile.begin name must be 1..48 bytes");
  if(!profiling){lua_pushboolean(L,0);return 1;}if(a->scopes.size()>=16)return luaL_error(L,"profile.begin: at most 16 open scopes");
  a->scopes.push_back({std::string(name,n),std::chrono::steady_clock::now()});lua_pushboolean(L,1);return 1;
 }
 static int profileFinish(lua_State* L){
  auto a=context(L);const std::string name=luaL_checkstring(L,1);
  for(size_t i=a->scopes.size();i-->0;)if(a->scopes[i].first==name){closeScope(*a,i);lua_pushboolean(L,1);return 1;}
  lua_pushboolean(L,0);return 1;
 }
 static int profileScope(lua_State* L){
  auto a=context(L);size_t n=0;const char* name=luaL_checklstring(L,1,&n);luaL_checktype(L,2,LUA_TFUNCTION);if(!n||n>48)return luaL_error(L,"profile.scope name must be 1..48 bytes");
  const int base=lua_gettop(L);const auto start=std::chrono::steady_clock::now();
  lua_call(L,base-2,LUA_MULTRET);
  if(profiling){const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();auto& b=a->profile["scope "+std::string(name,n)];++b.calls;b.ms+=ms;b.worst=(std::max)(b.worst,ms);}
  return lua_gettop(L)-1;
 }
 // The profiler's text: for the window since the last call (summary) and since the start (report). Clears the window.
 public:void profileText(std::string& summary,std::string& report,double windowSeconds){
  char line[512];
  snprintf(line,sizeof line,"== Lua addons, last %.0f s ==\r\n",windowSeconds);summary+=line;
  auto block=[&](std::string& out,const Addon& a,const std::map<std::string,ProfileBucket>& buckets,const std::unordered_map<std::string,uint64_t>& hot,uint64_t calls,double seconds){
   double ms=0;uint64_t n=0,ins=0;for(const auto& [k,b]:buckets){ms+=b.ms;n+=b.calls;ins+=b.instructions;}
   snprintf(line,sizeof line,"%s%s: %.2f ms in %llu calls (%.3f ms/s), %llu kinstr, memory %.1f KiB (peak %.1f KiB), %llu game calls\r\n",a.id.c_str(),a.enabled?"":" (disabled)",
    ms,(unsigned long long)n,seconds>0?ms/seconds:0.0,(unsigned long long)(ins/1000),double(a.bytes)/1024.0,double(a.peakBytes)/1024.0,(unsigned long long)calls);out+=line;
   std::vector<std::pair<std::string,ProfileBucket>> rows(buckets.begin(),buckets.end());
   std::sort(rows.begin(),rows.end(),[](const auto& x,const auto& y){return x.second.ms>y.second.ms;});
   for(const auto& [k,b]:rows){snprintf(line,sizeof line,"   %-34s %8llu calls %9.2f ms  avg %7.3f  worst %7.3f ms  %8llu kinstr\r\n",k.c_str(),(unsigned long long)b.calls,b.ms,b.calls?b.ms/double(b.calls):0.0,b.worst,(unsigned long long)(b.instructions/1000));out+=line;}
   std::vector<std::pair<std::string,uint64_t>> lines(hot.begin(),hot.end());
   std::sort(lines.begin(),lines.end(),[](const auto& x,const auto& y){return x.second>y.second;});
   if(a.failures){const auto first=a.error.substr(0,a.error.find('\n'));snprintf(line,sizeof line,"   FAILED %llu time(s), last: %s\r\n",(unsigned long long)a.failures,first.substr(0,400).c_str());out+=line;}
   for(size_t i=0;i<lines.size()&&i<8;++i){snprintf(line,sizeof line,"   hot  %-40s %llu samples (x10k instructions)\r\n",lines[i].first.c_str(),(unsigned long long)lines[i].second);out+=line;}
  };
  static double totalSeconds=0;totalSeconds+=windowSeconds;
  snprintf(line,sizeof line,"== Lua addons, since start (%.0f s) ==\r\n",totalSeconds);report+=line;
  for(auto& owned:addons){auto& a=*owned;
   block(summary,a,a.profile,a.hotLines,a.gameCalls,windowSeconds);
   for(const auto& [k,b]:a.profile){auto& t=a.profileTotal[k];t.calls+=b.calls;t.ms+=b.ms;t.worst=(std::max)(t.worst,b.worst);t.instructions+=b.instructions;}
   for(const auto& [k,v]:a.hotLines)if(a.hotTotal.size()<8192||a.hotTotal.count(k))a.hotTotal[k]+=v;
   a.gameCallsTotal+=a.gameCalls;a.profile.clear();a.hotLines.clear();a.gameCalls=0;
   block(report,a,a.profileTotal,a.hotTotal,a.gameCallsTotal,totalSeconds);
  }
 }
 private:
 // tasks.worker(module, input, fn): pure computation on a runtime worker thread. `module` (one of this addon's
 // [script.modules]) is run in a new Lua state of its own on that thread and must return a function; that function is
 // called with a copy of `input` and its first result is copied back. fn(result, nil) or fn(nil, error) then runs on
 // the main thread at a later pulse. The worker state has base/table/string/math/utf8 (the addon's own sandbox),
 // `require` for this addon's modules, and `print`/`log.*` (kept and logged with the result); no game API, no
 // `api`, `tasks`, `timers` or `storage`. Values cross as plain data (nil, booleans, numbers, strings, tables of
 // those; 32 deep; input 8 MiB, result 32 MiB encoded). Memory per job: the addon's own limit. At most 4 jobs per
 // addon and 2 running at once in all; more wait. Cancelled (fn not called) by tasks.cancel_worker(handle), leaving a
 // game for the menu, the addon failing or unloading, and the runtime stopping.
 static constexpr size_t workerInputBytes=8u*1024*1024,workerOutputBytes=32u*1024*1024;static constexpr int workerDepth=32;
 struct WorkerContext {size_t bytes=0,limit=0;WorkerJob* job=nullptr;int cache=LUA_NOREF;std::vector<bool> importing;};
 static void* workerAllocate(void* ud,void* ptr,size_t old,size_t size){
  auto c=static_cast<WorkerContext*>(ud);if(!ptr)old=0;
  if(!size){std::free(ptr);c->bytes-=old;return nullptr;}
  if(size>c->limit||c->bytes-old>c->limit-size)return nullptr;
  void* next=std::realloc(ptr,size);if(next)c->bytes=c->bytes-old+size;return next;
 }
 static WorkerContext* workerContext(lua_State* W){void* p=nullptr;lua_getallocf(W,&p);return static_cast<WorkerContext*>(p);}
 static void workerHook(lua_State* W,lua_Debug*){if(workerContext(W)->job->cancel.load())luaL_error(W,"worker cancelled");}
 static int workerLog(lua_State* W){
  auto c=workerContext(W);std::string line;const int n=lua_gettop(W);
  for(int i=1;i<=n;++i){size_t k=0;const char* s=luaL_tolstring(W,i,&k);if(i>1)line+=' ';line.append(s,(std::min)(k,size_t(400)));lua_pop(W,1);}
  if(c->job->logs.size()<32)c->job->logs.push_back(std::string(lua_tostring(W,lua_upvalueindex(1)))+": "+line);return 0;
 }
 static int workerRequire(lua_State* W){
  auto c=workerContext(W);size_t length=0;const char* name=luaL_checklstring(W,1,&length);const auto& modules=c->job->modules;
  size_t index=0;for(;index<modules.size();++index)if(modules[index].name==std::string(name,length))break;
  if(index==modules.size())return luaL_error(W,"module is not declared in this addon: %s",name);
  lua_rawgeti(W,LUA_REGISTRYINDEX,c->cache);lua_rawgeti(W,-1,int(index+1));if(!lua_isnil(W,-1))return 1;lua_pop(W,1);
  if(c->importing[index])return luaL_error(W,"cyclic module import: %s",name);c->importing[index]=true;
  const std::string chunk="@"+c->job->addon+"/"+modules[index].name;
  if(luaL_loadbufferx(W,modules[index].source.data(),modules[index].source.size(),chunk.c_str(),"t")!=LUA_OK)return lua_error(W);
  lua_call(W,0,1);c->importing[index]=false;
  if(lua_isnil(W,-1)){lua_pop(W,1);lua_pushboolean(W,1);}lua_pushvalue(W,-1);lua_rawseti(W,-3,int(index+1));return 1;
 }
 static int workerBody(lua_State* W){
  auto c=workerContext(W);auto& job=*c->job;
  const struct {const char* name;lua_CFunction open;} libs[]={{LUA_GNAME,luaopen_base},{LUA_TABLIBNAME,luaopen_table},{LUA_STRLIBNAME,luaopen_string},{LUA_MATHLIBNAME,luaopen_math},{LUA_UTF8LIBNAME,luaopen_utf8}};
  for(const auto& lib:libs){luaL_requiref(W,lib.name,lib.open,1);lua_pop(W,1);}
  for(const char* name:{"dofile","loadfile","load","collectgarbage","setmetatable","getmetatable"}){lua_pushnil(W);lua_setglobal(W,name);}
  lua_newtable(W);for(const char* level:{"info","warn","error"}){lua_pushstring(W,level);lua_pushcclosure(W,workerLog,1);lua_setfield(W,-2,level);}lua_setglobal(W,"log");
  lua_pushstring(W,"info");lua_pushcclosure(W,workerLog,1);lua_setglobal(W,"print");
  lua_newtable(W);c->cache=luaL_ref(W,LUA_REGISTRYINDEX);c->importing.assign(job.modules.size(),false);
  lua_pushcfunction(W,workerRequire);lua_setglobal(W,"require");
  lua_pushcfunction(W,workerRequire);lua_pushstring(W,job.module.c_str());lua_call(W,1,1);
  if(!lua_isfunction(W,-1))return luaL_error(W,"worker module %s must return a function",job.module.c_str());
  size_t at=0;if(!decodeValue(W,job.input,at,0,workerDepth))return luaL_error(W,"worker input could not be copied");
  lua_call(W,1,1);
  std::string out;if(!encodeValue(W,-1,out,0,workerOutputBytes,workerDepth))return luaL_error(W,"worker result must be nil, a boolean, number, string or a table of those (at most 32 deep, 32 MiB)");
  job.output.swap(out);return 0;
 }
 static void executeWorker(WorkerJob& job){
  if(job.cancel.load()){job.error="worker cancelled";job.state=3;return;}
  job.state=1;WorkerContext c;c.limit=job.memory;c.job=&job;
  lua_State* W=lua_newstate(workerAllocate,&c);if(!W){job.error="worker: Lua allocation failed";job.state=3;return;}
  lua_sethook(W,workerHook,LUA_MASKCOUNT,100000);
  lua_pushcfunction(W,workerBody);const int status=lua_pcall(W,0,0,0);
  if(status!=LUA_OK){const char* e=lua_tostring(W,-1);job.error=e?e:(status==LUA_ERRMEM?"not enough memory":"worker failed");}
  lua_close(W);job.state=status==LUA_OK?2:3;
 }
 struct WorkerPool {std::mutex lock;std::deque<std::shared_ptr<WorkerJob>> waiting;int running=0;};
 static WorkerPool& workerPool(){static WorkerPool pool;return pool;}
 static void workerLoop(std::shared_ptr<WorkerJob> job){
  for(;;){
   try{executeWorker(*job);}catch(...){job->error="worker failed";job->state=3;}
   auto& pool=workerPool();std::lock_guard<std::mutex> guard(pool.lock);
   if(pool.waiting.empty()){--pool.running;return;}job=pool.waiting.front();pool.waiting.pop_front();
  }
 }
 static void startWorker(const std::shared_ptr<WorkerJob>& job){
  auto& pool=workerPool();std::lock_guard<std::mutex> guard(pool.lock);
  if(pool.running<2){++pool.running;try{std::thread(workerLoop,job).detach();}catch(...){--pool.running;job->error="worker thread could not start";job->state=3;}}
  else pool.waiting.push_back(job);
 }
 static int runWorker(lua_State* L){
  auto a=context(L);if(!canTime(*a))return luaL_error(L,"tasks.worker needs the timers capability");
  const std::string module=luaL_checkstring(L,1);luaL_checktype(L,3,LUA_TFUNCTION);
  bool declared=false;for(const auto& m:a->modules)declared=declared||m.name==module;if(!declared)return luaL_error(L,"tasks.worker: module is not declared in this addon: %s",module.c_str());
  if(a->workers.size()>=4)return luaL_error(L,"tasks.worker: at most 4 jobs per addon");
  auto job=std::make_shared<WorkerJob>();job->addon=a->id;job->module=module;job->modules=a->modules;job->memory=a->limit;
  if(!encodeValue(L,2,job->input,0,workerInputBytes,workerDepth))return luaL_error(L,"tasks.worker input must be nil, a boolean, number, string or a table of those (at most 32 deep, 8 MiB)");
  static uint32_t nextHandle=0;Addon::Worker w;w.handle=++nextHandle;w.job=job;lua_pushvalue(L,3);w.callback=luaL_ref(L,LUA_REGISTRYINDEX);
  a->workers.push_back(w);startWorker(job);lua_pushinteger(L,lua_Integer(w.handle));return 1;
 }
 static void dropWorker(Addon& a,size_t i){a.workers[i].job->cancel=true;luaL_unref(a.state,LUA_REGISTRYINDEX,a.workers[i].callback);a.workers.erase(a.workers.begin()+std::ptrdiff_t(i));}
 static int cancelWorker(lua_State* L){
  auto a=context(L);const auto handle=uint32_t(luaL_checkinteger(L,1));
  for(size_t i=0;i<a->workers.size();++i)if(a->workers[i].handle==handle){dropWorker(*a,i);lua_pushboolean(L,1);return 1;}
  lua_pushboolean(L,0);return 1;
 }
 static int countWorkers(lua_State* L){lua_pushinteger(L,lua_Integer(context(L)->workers.size()));return 1;}
 void cancelWorkers(Addon& a){while(!a.workers.empty())dropWorker(a,a.workers.size()-1);}
 // Finished jobs: their callbacks run on this (the main) thread, one budgeted call each.
 void deliverWorkers(){
  for(auto& owned:addons){auto& a=*owned;if(a.workers.empty())continue;if(!a.enabled){cancelWorkers(a);continue;}
   for(size_t i=0;i<a.workers.size()&&a.enabled;){
    const int state=a.workers[i].job->state.load();if(state<2){++i;continue;}
    auto job=a.workers[i].job;const int callback=a.workers[i].callback;a.workers.erase(a.workers.begin()+std::ptrdiff_t(i));
    for(const auto& line:job->logs)report(a.id.c_str(),"info",("[worker] "+line).c_str());
    auto L=a.state;if(!lua_checkstack(L,8)){luaL_unref(L,LUA_REGISTRYINDEX,callback);continue;}
    lua_rawgeti(L,LUA_REGISTRYINDEX,callback);luaL_unref(L,LUA_REGISTRYINDEX,callback);
    size_t at=0;
    if(state==2&&decodeValue(L,job->output,at,0,workerDepth)){lua_pushnil(L);}
    else{lua_settop(L,1);lua_pushnil(L);lua_pushstring(L,state==2?"worker result is too large for this addon's memory":job->error.c_str());}
    job->output.clear();job->output.shrink_to_fit();
    startBudget(a,"worker.done");a.timerRunning=true;const int status=protectedCall(L,2,0);a.timerRunning=false;stopBudget(a);
    if(status!=LUA_OK){failure(a,"worker callback failed");cancelWorkers(a);break;}commitAiChanges(a);commitSettings(a);lua_settop(L,0);
   }
  }
 }
 public:void cancelAllWorkers(){for(auto& a:addons)cancelWorkers(*a);}private:
 // world.light_fire(name, {position = {x, y, z}, size = s}) / world.put_out_fire(name): see world_fire_bridge.h.
 // Staged from timer callbacks (world.fire, an alias of world.devices); true when staged. size 0.25..2 (1 is a
 // medium fire); name 1..31 bytes, this addon's handle; at most 4 orders a callback.
 static int fireOrder(lua_State* L,bool light){
  auto a=context(L);if(!(a->caps&worldDevices)||!(a->owner->available&worldDevices))return luaL_error(L,"world.%s needs the world.fire capability",light?"light_fire":"put_out_fire");
  if(a->loading)return luaL_error(L,"world.%s is only allowed in callbacks, not while the addon loads",light?"light_fire":"put_out_fire");
  size_t n=0;const char* name=luaL_checklstring(L,1,&n);if(!n||n>31||std::memchr(name,0,n))return luaL_error(L,"fire name must be 1..31 bytes");
  if(a->fireOrders.size()>=4)return luaL_error(L,"at most 4 fire orders a callback");
  FireOrder o;o.owner=a->id;o.name.assign(name,n);o.kind=light?0:1;
  if(light){luaL_checktype(L,2,LUA_TTABLE);lua_getfield(L,2,"position");if(!lua_istable(L,-1))return luaL_error(L,"light_fire needs position = {x, y, z}");
   for(int i=0;i<3;++i){lua_rawgeti(L,-1,i+1);if(!lua_isnumber(L,-1))return luaL_error(L,"light_fire position needs three numbers");const double v=lua_tonumber(L,-1);
    if(!std::isfinite(v)||std::abs(v)>10000000)return luaL_error(L,"light_fire position outside supported range");o.at[size_t(i)]=float(v);lua_pop(L,1);}
   lua_pop(L,1);lua_getfield(L,2,"size");if(!lua_isnil(L,-1)){const double s=luaL_checknumber(L,-1);if(!std::isfinite(s)||s<0.25||s>2)return luaL_error(L,"light_fire size is 0.25..2");o.size=float(s);}lua_pop(L,1);}
  a->fireOrders.push_back(o);lua_pushboolean(L,1);return 1;
 }
 static int lightFire(lua_State* L){return fireOrder(L,true);}
 // characters.drop_held(target, side): from callbacks, the target ("player", a record_id or a row with a handle) lets
 // go of what that hand ("right" or "left") holds, as the game drops a dead character's weapons; staged, it runs on
 // the game thread at the next pulse; nothing when the hand is empty. characters.drop_held (an alias of
 // characters.damage). At most 8 a callback.
 // characters.sever(target, part): from callbacks, the joint between an arm part (12/15 shoulder, 13/16 elbow, in
 // character.hit numbering) and its parent is cut, as the game's solver breaks a joint; true when staged. Blows on
 // the cut limb no longer hurt its owner. characters.sever (an alias of characters.damage). At most 8 a callback.
 // characters.keep_down(target, on): while on, the target ("player", a record_id or a row with a handle) stays on the
 // ground the way the game keeps a character with no footing down; off lets it get up as usual. Held until released,
 // a level change, or unload. characters.keep_down (an alias of characters.damage). 32 per addon.
 // characters.play_motion(target, motion): from callbacks, the game's motion player plays motion `motion` (an index
 // into the character's motion table: the game's own, or 48+ for clips an addon spliced into humanoid.rsq) on the
 // target; staged, checked against the table at the job; not on a ragdolled (dead, unconscious) body.
 // characters.motion (an alias of characters.damage). At most 8 a callback.
 static int playMotion(lua_State* L){
  auto a=context(L);if(!(a->caps&charactersDamage)||!(a->owner->available&charactersDamage))return luaL_error(L,"characters.play_motion needs the characters.motion capability");
  if(a->loading)return luaL_error(L,"characters.play_motion is only allowed in callbacks, not while the addon loads");
  MotionOrder o;readTarget(L,1,"characters.play_motion",o.player,o.record,o.handle);
  const lua_Integer motion=luaL_checkinteger(L,2);if(motion<0||motion>1023)return luaL_error(L,"characters.play_motion motion is an index 0..1023");
  if(a->motions.size()>=8)return luaL_error(L,"at most 8 motions a callback");
  o.motion=uint32_t(motion);a->motions.push_back(o);lua_pushboolean(L,1);return 1;
 }
 static int keepDown(lua_State* L){
  auto a=context(L);if(!(a->caps&charactersDamage)||!(a->owner->available&charactersDamage))return luaL_error(L,"characters.keep_down needs the characters.keep_down capability");
  KeepDown k;readTarget(L,1,"characters.keep_down",k.player,k.record,k.handle);luaL_checktype(L,2,LUA_TBOOLEAN);const bool on=lua_toboolean(L,2)!=0;
  auto same=[&](const KeepDown& x){return x.player==k.player&&x.record==k.record&&x.handle==k.handle;};
  a->keepDown.erase(std::remove_if(a->keepDown.begin(),a->keepDown.end(),same),a->keepDown.end());
  if(on){if(a->keepDown.size()>=32)return luaL_error(L,"characters.keep_down: at most 32 targets per addon");a->keepDown.push_back(k);}
  lua_pushboolean(L,1);return 1;
 }
 static int sever(lua_State* L){
  auto a=context(L);if(!(a->caps&charactersDamage)||!(a->owner->available&charactersDamage))return luaL_error(L,"characters.sever needs the characters.sever capability");
  if(a->loading)return luaL_error(L,"characters.sever is only allowed in callbacks, not while the addon loads");
  SeverOrder s;readTarget(L,1,"characters.sever",s.player,s.record,s.handle);
  const lua_Integer part=luaL_checkinteger(L,2);
  // Arms (12/15 upper arm at the shoulder, 13/16 forearm at the elbow) and legs (2/5 thigh at the hip, 3/6 shin at the
  // knee): a leg pairs with characters.keep_down.
  if(part!=12&&part!=13&&part!=15&&part!=16&&part!=2&&part!=3&&part!=5&&part!=6)return luaL_error(L,"characters.sever part is 12/15 or 13/16 (arm) or 2/5 or 3/6 (leg)");
  if(a->severs.size()>=8)return luaL_error(L,"at most 8 severs a callback");
  s.part=int(part);a->severs.push_back(s);lua_pushboolean(L,1);return 1;
 }
 static int dropHeld(lua_State* L){
  auto a=context(L);if(!(a->caps&charactersDamage)||!(a->owner->available&charactersDamage))return luaL_error(L,"characters.drop_held needs the characters.drop_held capability");
  if(a->loading)return luaL_error(L,"characters.drop_held is only allowed in callbacks, not while the addon loads");
  DropOrder d;readTarget(L,1,"characters.drop_held",d.player,d.record,d.handle);
  const char* side=luaL_checkstring(L,2);if(!std::strcmp(side,"right"))d.hand=0;else if(!std::strcmp(side,"left"))d.hand=1;else return luaL_error(L,"characters.drop_held side is \"right\" or \"left\"");
  if(a->drops.size()>=8)return luaL_error(L,"at most 8 drops a callback");
  a->drops.push_back(d);lua_pushboolean(L,1);return 1;
 }
 // effects.attach_fire(name, target, {size = s}) / effects.remove_fire(name): the game's small fire following
 // `target` ("player", or a row with a handle) until removed or the level ends; size 0.1..1; 16 per addon; not saved,
 // never a damage source (effects.fire, an alias of effects.arc). "weapon:right"/"weapon:left": the item in that hand.
 static int attachFire(lua_State* L,bool attach){
  auto a=context(L);if(!(a->caps&effectsArc)||!(a->owner->available&effectsArc))return luaL_error(L,"effects.%s needs the effects.fire capability",attach?"attach_fire":"remove_fire");
  if(a->loading)return luaL_error(L,"effects.%s is only allowed in callbacks, not while the addon loads",attach?"attach_fire":"remove_fire");
  size_t n=0;const char* name=luaL_checklstring(L,1,&n);if(!n||n>31||std::memchr(name,0,n))return luaL_error(L,"fire name must be 1..31 bytes");
  if(a->fireOrders.size()>=4)return luaL_error(L,"at most 4 fire orders a callback");
  FireOrder o;o.owner=a->id;o.name.assign(name,n);o.kind=attach?2:3;
  if(attach){
   if(lua_type(L,2)==LUA_TSTRING){const char* t=lua_tostring(L,2);
    if(!std::strcmp(t,"weapon:right"))o.hand=0;else if(!std::strcmp(t,"weapon:left"))o.hand=1;
    else if(std::strcmp(t,"player"))return luaL_error(L,"attach_fire target is \"player\", \"weapon:right\", \"weapon:left\" or a row with a handle");}
   else{luaL_checktype(L,2,LUA_TTABLE);lua_getfield(L,2,"handle");if(!lua_isinteger(L,-1)||lua_tointeger(L,-1)<=0)return luaL_error(L,"attach_fire: the row has no handle");o.handle=uint64_t(lua_tointeger(L,-1));lua_pop(L,1);}
   o.size=0.5f;if(lua_istable(L,3)){lua_getfield(L,3,"size");if(!lua_isnil(L,-1)){const double s=luaL_checknumber(L,-1);if(!std::isfinite(s)||s<0.1||s>1)return luaL_error(L,"attach_fire size is 0.1..1");o.size=float(s);}lua_pop(L,1);}
  }
  a->fireOrders.push_back(o);lua_pushboolean(L,1);return 1;
 }
 static int attachFireLua(lua_State* L){return attachFire(L,true);}
 static int removeFireLua(lua_State* L){return attachFire(L,false);}
 // world.fires(radius): the game's fires (and attached flames) near the player, nearest first, at most 64:
 // {position = {x, y, z}, size, mod}; refreshed about six times a second (world.read).
 static int listFires(lua_State* L){
  auto a=context(L);if(!(a->caps&worldRead)||!(a->owner->available&worldRead))return luaL_error(L,"world.fires needs world.read");
  const double radius=luaL_optnumber(L,1,1000);if(!std::isfinite(radius)||radius<0||radius>5000)return luaL_error(L,"world.fires radius is 0..5000");
  std::vector<FireInfo> list;if(!a->owner->shared.fires||!a->owner->shared.fires(list)){lua_newtable(L);return 1;}
  lua_newtable(L);int i=0;for(const auto& f:list){if(f.distance<0||f.distance>radius)continue;lua_newtable(L);lua_newtable(L);
   for(int k=0;k<3;++k){lua_pushnumber(L,f.at[size_t(k)]);lua_rawseti(L,-2,k+1);}lua_setfield(L,-2,"position");
   lua_pushnumber(L,f.size);lua_setfield(L,-2,"size");lua_pushboolean(L,f.mod);lua_setfield(L,-2,"mod");lua_rawseti(L,-2,++i);if(i>=64)break;}
  return 1;
 }
 static int putOutFire(lua_State* L){return fireOrder(L,false);}
 static int setMark(lua_State* L){
  auto a=context(L);if(!(a->caps&characterMarks)||!(a->owner->available&characterMarks))return luaL_error(L,"characters.marks unavailable");
  Mark m;readTarget(L,1,"characters.set_mark",m.player,m.record,m.handle);
  size_t n=0;const char* name=luaL_checklstring(L,2,&n);if(!n||n>31)return luaL_error(L,"characters.set_mark name must be 1..31 bytes");
  for(size_t i=0;i<n;++i){const char c=name[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.'||c=='-'))return luaL_error(L,"characters.set_mark name may use letters, digits, _ . - only");}
  const lua_Integer v=luaL_checkinteger(L,3);if(v<0||v>255)return luaL_error(L,"characters.set_mark value must be 0..255");
  std::memcpy(m.name,name,n);m.value=uint32_t(v);
  for(auto it=a->marks.begin();it!=a->marks.end();++it)if(it->player==m.player&&it->record==m.record&&it->handle==m.handle&&!std::strcmp(it->name,m.name)){
   if(m.value)*it=m;else a->marks.erase(it);lua_pushboolean(L,1);return 1;}
  if(!m.value){lua_pushboolean(L,1);return 1;}
  if(a->marks.size()>=32)return luaL_error(L,"characters.set_mark: at most 32 marks per addon");
  a->marks.push_back(m);lua_pushboolean(L,1);return 1;
 }
 // characters.set_damage_scale(target, {cut, pierce, crush, impact}) or (target, nil): target a record_id or
 // "player"; each multiplier 0.05..4, missing 1. Applied inside the game's damage computation before armour, so both
 // health and balance follow and character.hit reports the scaled result. Held until a level change or unload.
 static int setDamageScale(lua_State* L){
  auto a=context(L);if(!(a->caps&damageScale)||!(a->owner->available&damageScale))return luaL_error(L,"characters.damage_scale unavailable");
  DamageScale d;readTarget(L,1,"characters.set_damage_scale",d.player,d.record,d.handle);
  auto same=[&](const DamageScale& x){return x.player==d.player&&x.record==d.record&&x.handle==d.handle;};
  a->scales.erase(std::remove_if(a->scales.begin(),a->scales.end(),same),a->scales.end());
  if(lua_isnoneornil(L,2)){lua_pushboolean(L,1);return 1;}
  luaL_checktype(L,2,LUA_TTABLE);
  for(auto [key,slot]:{std::pair<const char*,float*>{"cut",&d.cut},{"pierce",&d.pierce},{"crush",&d.crush},{"impact",&d.impact}}){
   lua_getfield(L,2,key);if(!lua_isnil(L,-1)){const double v=luaL_checknumber(L,-1);if(!std::isfinite(v)||v<0.05||v>4)return luaL_error(L,"characters.set_damage_scale %s must be 0.05..4",key);*slot=float(v);}lua_pop(L,1);}
  if(a->scales.size()>=32)return luaL_error(L,"characters.set_damage_scale: at most 32 targets per addon");
  a->scales.push_back(d);lua_pushboolean(L,1);return 1;
 }
 // characters.set_hit_sound("player", material | nil): the player's own hit sounds come from the game's material
 // sound table (0 flesh, 1 cloth, 2 leather, 3 chain, 4 plate, 5 hard, 6 golem stone) while set; nil restores them.
 static int setHitSound(lua_State* L){
  auto a=context(L);if(!(a->caps&hitSound)||!(a->owner->available&hitSound))return luaL_error(L,"characters.hit_sound unavailable");
  if(lua_type(L,1)!=LUA_TSTRING||std::strcmp(lua_tostring(L,1),"player"))return luaL_error(L,"characters.set_hit_sound takes \"player\"");
  if(lua_isnoneornil(L,2)){a->hitMaterial=-1;lua_pushboolean(L,1);return 1;}
  const lua_Integer m=luaL_checkinteger(L,2);if(m<0||m>6)return luaL_error(L,"characters.set_hit_sound material must be 0..6");
  a->hitMaterial=int(m);lua_pushboolean(L,1);return 1;
 }
 // characters.set_limb_strength(target, part, factor): target a record_id or "player"; part 1..18 (character.hit
 // order); factor 0.05..1, 1 clears. Held until a level change or unload; with several addons the lowest wins.
 static int setLimbStrength(lua_State* L){
  auto a=context(L);if(!(a->caps&limbStrength)||!(a->owner->available&limbStrength))return luaL_error(L,"characters.limb_strength unavailable");
  LimbStrength s;readTarget(L,1,"characters.set_limb_strength",s.player,s.record,s.handle);
  const lua_Integer part=luaL_checkinteger(L,2);if(part<1||part>18)return luaL_error(L,"characters.set_limb_strength part is 1..18");s.part=int(part);
  const double f=luaL_checknumber(L,3);if(!std::isfinite(f)||f<0.05||f>1)return luaL_error(L,"characters.set_limb_strength factor is 0.05..1");s.factor=float(f);
  a->limbs.erase(std::remove_if(a->limbs.begin(),a->limbs.end(),[&](const LimbStrength& x){return x.player==s.player&&x.record==s.record&&x.handle==s.handle&&x.part==s.part;}),a->limbs.end());
  if(s.factor<1.f){if(a->limbs.size()>=64)return luaL_error(L,"characters.set_limb_strength: at most 64 per addon");a->limbs.push_back(s);}
  lua_pushboolean(L,1);return 1;
 }
 // characters.set_strength("player", factor) (0.25..3) and characters.set_anim_rate("player", factor) (0.5..2): the
 // player's motor strength and base animation rate as a held multiple of the game's own values; 1 clears. Several
 // addons multiply. Held until a level change or unload.
 static int setBodyScale(lua_State* L){
  auto a=context(L);if(!(a->caps&bodyScale)||!(a->owner->available&bodyScale))return luaL_error(L,"characters.body unavailable");
  const bool strength=lua_toboolean(L,lua_upvalueindex(1))!=0;const char* name=strength?"characters.set_strength":"characters.set_anim_rate";
  BodyScale s;s.field=strength?0:1;
  readTarget(L,1,name,s.player,s.record,s.handle);
  a->body.erase(std::remove_if(a->body.begin(),a->body.end(),[&](const BodyScale& x){return x.player==s.player&&x.record==s.record&&x.handle==s.handle&&x.field==s.field;}),a->body.end());
  if(lua_isnoneornil(L,2)){lua_pushboolean(L,1);return 1;}
  const double low=strength?0.25:(s.player?0.5:0.02),high=strength?3.0:2.0;const double f=luaL_checknumber(L,2);
  if(!std::isfinite(f)||f<low||f>high)return luaL_error(L,"%s factor is %g..%g",name,low,high);
  if(f!=1.0){if(a->body.size()>=64)return luaL_error(L,"%s: at most 64 targets per addon",name);s.factor=float(f);a->body.push_back(s);}
  lua_pushboolean(L,1);return 1;
 }
 // effects.spawn_arc{from = "hand", strands, segments, life, length}: the game's own lightning arc (the Energy Shock
 // charge's builder) on the player's casting hand; it follows the hand and removes itself after `life` ticks.
 // strands 1..40, segments 2..16, life 1..200 ticks (83 per second), length 0.5..8. Staged for the game thread.
 static int spawnArc(lua_State* L){
  auto a=context(L);if(!(a->caps&effectsArc)||!(a->owner->available&effectsArc))return luaL_error(L,"effects.arc unavailable");
  if(a->loading)return luaL_error(L,"effects.spawn_arc is not available while the addon loads");
  luaL_checktype(L,1,LUA_TTABLE);
  lua_getfield(L,1,"from");if(!lua_isnil(L,-1)&&(lua_type(L,-1)!=LUA_TSTRING||std::strcmp(lua_tostring(L,-1),"hand")))return luaL_error(L,"effects.spawn_arc from is \"hand\"");lua_pop(L,1);
  std::array<float,4> spec{20,5,10,2};const char* keys[4]={"strands","segments","life","length"};const double low[4]={1,2,1,0.5},high[4]={40,16,200,8};
  for(int i=0;i<4;++i){lua_getfield(L,1,keys[i]);if(!lua_isnil(L,-1)){const double v=luaL_checknumber(L,-1);if(!std::isfinite(v)||v<low[i]||v>high[i])return luaL_error(L,"effects.spawn_arc %s is %g..%g",keys[i],low[i],high[i]);spec[size_t(i)]=float(v);}lua_pop(L,1);}
  if(a->owner->phaseName!="session"){lua_pushboolean(L,0);return 1;}
  if(a->arcs.size()>=2){lua_pushboolean(L,0);return 1;}
  a->arcs.push_back(spec);lua_pushboolean(L,1);return 1;
 }
 static int changeAi(lua_State* L){
  auto a=context(L);
  if(!(a->caps&aiWrite)||!(a->owner->available&aiWrite)||!a->timerRunning||a->loading||
     uint64_t(lua_tointeger(L,lua_upvalueindex(2)))!=a->execution)return luaL_error(L,"NPC edit method expired or unavailable");
  luaL_checktype(L,1,LUA_TTABLE);luaL_checktype(L,2,LUA_TTABLE);
  AiChange change;change.target=uint64_t(lua_tointeger(L,lua_upvalueindex(1)));
  const char* fields[]={"skill","attribute2","attribute3"};double* values[]={&change.values.skill,&change.values.attribute2,&change.values.attribute3};
  for(int i=0;i<3;++i){lua_getfield(L,2,fields[i]);if(lua_type(L,-1)!=LUA_TNUMBER)return luaL_error(L,"set_ai requires numeric skill, attribute2 and attribute3");*values[i]=lua_tonumber(L,-1);lua_pop(L,1);}
  const char* reason="";if(!hot::aiInputOk(change.values,reason))return luaL_error(L,"%s",reason);
  return stageCharacterChange(L,change);
 }
 // row:boost_ai{skill, attribute2, attribute3, seconds}: set_ai's inputs (absolute, as set_ai), held for `seconds`
 // (0.5..10). The first call for a character saves the AI fields as stored; later calls only move the lapse time.
 // On lapse, a phase change, the character's death or leaving, or this addon's unload, the saved fields are written
 // back exactly. A set_ai on a boosted character ends the boost and keeps the set_ai values.
 static int boostAi(lua_State* L){
  auto a=context(L);
  if(!(a->caps&aiWrite)||!(a->owner->available&aiWrite)||!a->timerRunning||a->loading||
     uint64_t(lua_tointeger(L,lua_upvalueindex(2)))!=a->execution)return luaL_error(L,"NPC edit method expired or unavailable");
  luaL_checktype(L,1,LUA_TTABLE);luaL_checktype(L,2,LUA_TTABLE);
  AiChange change;change.target=uint64_t(lua_tointeger(L,lua_upvalueindex(1)));change.owner=a->id;
  const char* fields[]={"skill","attribute2","attribute3"};double* values[]={&change.values.skill,&change.values.attribute2,&change.values.attribute3};
  // skill is required; attribute2 and attribute3 may be left out to keep the NPC's own (read back from its stored
  // AI fields when the boost is applied).
  for(int i=0;i<3;++i){lua_getfield(L,2,fields[i]);
   if(lua_isnil(L,-1)&&i>0){if(i==1)change.keepAttribute2=true;else change.keepAttribute3=true;*values[i]=0;lua_pop(L,1);continue;}
   if(lua_type(L,-1)!=LUA_TNUMBER)return luaL_error(L,"boost_ai requires a numeric skill, and numeric attribute2 and attribute3 when given");*values[i]=lua_tonumber(L,-1);lua_pop(L,1);}
  lua_getfield(L,2,"seconds");const double seconds=lua_isnumber(L,-1)?lua_tonumber(L,-1):-1;lua_pop(L,1);
  if(!std::isfinite(seconds)||seconds<0.5||seconds>10)return luaL_error(L,"boost_ai seconds must be 0.5..10");
  change.boostSeconds=float(seconds);
  const char* reason="";if(!hot::aiInputOk(change.values,reason))return luaL_error(L,"%s",reason);
  return stageCharacterChange(L,change);
 }
 void commitAiChanges(Addon& a){
  if(!a.nativeCalls.empty()){
   const auto calls=std::move(a.nativeCalls);a.nativeCalls.clear();unsigned refused=0;
   for(const auto& call:calls)if(!nativeCommit||!nativeCommit(call))++refused;
   if(refused)report(a.id.c_str(),"warn",(std::to_string(refused)+" staged game call(s) refused: no living player, no session, no save backup, or the game function did not match").c_str());
  }
  if(!a.inventoryChanges.empty()){
   const auto change=a.inventoryChanges.front();a.inventoryChanges.clear();
   const bool applied=inventoryCommit&&inventoryCommit(change);
    report(a.id.c_str(),applied?"info":"warn",applied?((change.op==InventoryOp::switchWeapons||change.op==InventoryOp::equipWeapon||change.op==InventoryOp::unequipWeapon||change.op==InventoryOp::equipShield)?"Weapon request accepted; completion is native":(change.op==InventoryOp::fireExtinguish||change.op==InventoryOp::torchIgnite||change.op==InventoryOp::fireUse||change.op==InventoryOp::emitterSet||change.op==InventoryOp::emitterToggle||change.op==InventoryOp::teleporterUse||change.op==InventoryOp::scannerUse||change.op==InventoryOp::worldUse||change.op==InventoryOp::worldPress||change.op==InventoryOp::leverPosition||change.op==InventoryOp::leverToggle||change.op==InventoryOp::doorOpen)?"World interaction accepted; presentation and linked effects are native":"Inventory operation applied"):"Inventory operation refused or faulted; no success claimed");
  }
  if(!a.containerChanges.empty()){
   const bool applied=containerCommit&&containerCommit(a.containerChanges);a.containerChanges.clear();
   report(a.id.c_str(),applied?"info":"warn",applied?"Container edit batch applied":"Container edit batch refused or faulted");
  }
  if(a.aiChanges.empty())return;
  const bool applied=aiCommit&&aiCommit(a.aiChanges);a.aiChanges.clear();
  report(a.id.c_str(),applied?"info":"warn",applied?"NPC edit batch applied":"NPC edit batch refused or faulted; no success claimed");
 }
 static void finiteNumber(lua_State* L,const char* key,float value){if(std::isfinite(value)){lua_pushnumber(L,value);lua_setfield(L,-2,key);}}
 static int snapshotCharacters(lua_State* L){
  const bool awareness=lua_toboolean(L,lua_upvalueindex(1))!=0;const uint64_t needed=awareness?awarenessRead:charactersRead;
  auto a=context(L);if(!(a->caps&needed)||!(a->owner->available&needed))return luaL_error(L,"%s unavailable",awareness?"characters.awareness":"characters.read");
  // Other arguments are ignored, as before 0.24.0 (colon calls pass the characters table itself).
  bool details=false;
  if(!awareness&&lua_istable(L,1)){lua_getfield(L,1,"details");details=lua_toboolean(L,-1)!=0;lua_pop(L,1);}
  if(++a->characterOps>4)return luaL_error(L,"character snapshot query limit exceeded (4 per callback)");
  const auto provider=awareness?a->owner->awarenessProvider:details?a->owner->detailsProvider:a->owner->characterProvider;
  const auto snapshot=provider?provider():nullptr;
  if(!snapshot||!snapshot->ready){lua_pushnil(L);return 1;}
  lua_newtable(L);boolean(L,"truncated",snapshot->truncated);text(L,"space","scene_local_cm");lua_newtable(L);
  int index=0;for(const auto& character:snapshot->characters){
   lua_newtable(L);text(L,"name",character.name);number(L,"record_id",character.record);lua_pushinteger(L,lua_Integer(character.nativeCharacter));lua_setfield(L,-2,"handle");if(character.playerKnown)boolean(L,"is_player",character.player);
   if((a->caps&(aiWrite|attitudeWrite))&&(a->owner->available&(aiWrite|attitudeWrite))&&a->timerRunning&&a->owner->aiCapture){
    const auto ticket=a->owner->aiCapture(character.nativeCharacter);
    if(ticket)for(const auto& method:{std::pair<uint64_t,const char*>{aiWrite,"set_ai"},{attitudeWrite,"set_attitude"}}){
     if(!(a->caps&method.first)||!(a->owner->available&method.first))continue;
     lua_pushinteger(L,lua_Integer(ticket));lua_pushinteger(L,lua_Integer(a->execution));lua_pushcclosure(L,method.first==aiWrite?changeAi:changeAttitude,2);lua_setfield(L,-2,method.second);
    }
    if(ticket&&(a->caps&aiWrite)&&(a->owner->available&aiWrite)){lua_pushinteger(L,lua_Integer(ticket));lua_pushinteger(L,lua_Integer(a->execution));lua_pushcclosure(L,boostAi,2);lua_setfield(L,-2,"boost_ai");}
   }
   if((a->caps&charactersDamage)&&(a->owner->available&charactersDamage)&&a->timerRunning&&character.nativeCharacter&&!(character.playerKnown&&character.player)){
    lua_pushinteger(L,lua_Integer(character.nativeCharacter));lua_pushinteger(L,lua_Integer(a->execution));lua_pushcclosure(L,damageCharacter,2);lua_setfield(L,-2,"damage");}
   if(character.facingKnown){lua_newtable(L);lua_pushnumber(L,character.facing[0]);lua_rawseti(L,-2,1);lua_pushnumber(L,0);lua_rawseti(L,-2,2);lua_pushnumber(L,character.facing[1]);lua_rawseti(L,-2,3);lua_setfield(L,-2,"facing");}
   if(character.held[0]||character.held[1]){lua_newtable(L);if(character.held[0])number(L,"right",character.held[0]);if(character.held[1])number(L,"left",character.held[1]);lua_setfield(L,-2,"held");
    lua_newtable(L);boolean(L,"right",character.heldLit[0]);boolean(L,"left",character.heldLit[1]);lua_setfield(L,-2,"held_lit");}
   if(character.stateKnown){boolean(L,"dead",character.dead);boolean(L,"down",character.down);}
   if(character.stageKnown)number(L,"life_stage",character.lifeStage);
   if(character.poolKnown)boolean(L,"has_health_pool",character.healthPool);
   finiteNumber(L,"health",character.health);finiteNumber(L,"balance",character.balance);if(character.poolKnown&&character.healthPool)finiteNumber(L,"health_max",character.maximum);finiteNumber(L,"balance_max",character.maximum);
   finiteNumber(L,"stamina",character.stamina);finiteNumber(L,"stamina_max",character.staminaMax);finiteNumber(L,"focus",character.focus);finiteNumber(L,"focus_max",character.focusMax);
   if(details&&character.skillsKnown){lua_newtable(L);
    static const char* skillNames[]={"close_combat","ranged_combat","armour","shields","insight","concentration"};
    for(const auto& skill:character.skills){lua_newtable(L);number(L,"index",skill.index);text(L,"name",skillNames[skill.index%6]);number(L,"rating",skill.rating);
     const unsigned active=(std::min)(6u,unsigned(skill.rating/5000));number(L,"active_techniques",active);lua_newtable(L);int n=0;
     for(unsigned t=0;t<6;++t)if(skill.techniques[t]){lua_pushinteger(L,skill.techniques[t]);lua_rawseti(L,-2,++n);}
     lua_setfield(L,-2,"techniques");lua_setfield(L,-2,skillNames[skill.index%6]);}
    lua_setfield(L,-2,"skills");}
   if(details&&character.rolesKnown){lua_newtable(L);int n=0;
    for(const auto& role:character.roles){lua_newtable(L);number(L,"id",role.id);if(!role.name.empty())text(L,"name",role.name);
     lua_newtable(L);int v=0;for(const auto value:role.variables){lua_pushinteger(L,value);lua_rawseti(L,-2,++v);}lua_setfield(L,-2,"variables");lua_rawseti(L,-2,++n);}
    lua_setfield(L,-2,"roles");}
   if(awareness&&character.awareness.available){
    const auto& brain=character.awareness;lua_newtable(L);
    for(const auto& field:brain.flags)boolean(L,field.first.c_str(),field.second);
    for(const auto& field:brain.values)finiteNumber(L,field.first.c_str(),field.second);
    for(const auto& field:brain.labels)text(L,field.first.c_str(),field.second);
    if(!brain.towardPlayer.empty()){lua_newtable(L);for(const auto& field:brain.towardPlayer)finiteNumber(L,field.first.c_str(),field.second);lua_setfield(L,-2,"toward_player");}
    lua_setfield(L,-2,"awareness");
   }
   lua_newtable(L);for(int axis=0;axis<3;++axis){lua_pushnumber(L,character.position[size_t(axis)]);lua_rawseti(L,-2,axis+1);}
   lua_setfield(L,-2,"position");lua_rawseti(L,-2,++index);
  }
  lua_setfield(L,-2,"characters");return 1;
 }
 static int readFlag(lua_State* L){
  auto a=context(L);if(!(a->caps&scriptFlags)||!(a->owner->available&scriptFlags))return luaL_error(L,"script.flags unavailable");
  int32_t value=0;if(!a->owner->flagGet||!a->owner->flagGet(luaL_checkstring(L,1),value)){lua_pushnil(L);return 1;}
  lua_pushinteger(L,value);return 1;
 }
 static int writeFlag(lua_State* L){
  auto a=context(L);if(!(a->caps&scriptFlags)||!(a->owner->available&scriptFlags))return luaL_error(L,"script.flags unavailable");
  const std::string name=luaL_checkstring(L,1);
  if(lua_isnoneornil(L,2)){lua_pushboolean(L,a->owner->flagSet&&a->owner->flagSet(name,nullptr));return 1;}
  if(!lua_isinteger(L,2)||lua_tointeger(L,2)<INT32_MIN||lua_tointeger(L,2)>INT32_MAX)return luaL_error(L,"script flags are 32-bit integers");
  const int32_t value=int32_t(lua_tointeger(L,2));lua_pushboolean(L,a->owner->flagSet&&a->owner->flagSet(name,&value));return 1;
 }
 static int readSave(lua_State* L){
  auto a=context(L);if(!(a->caps&saveData)||!(a->owner->available&saveData))return luaL_error(L,"storage.save unavailable");
  std::string value;if(!a->owner->saveGet||!a->owner->saveGet(a->id,value)){lua_pushnil(L);return 1;}
  lua_pushlstring(L,value.data(),value.size());return 1;
 }
 static int writeSave(lua_State* L){
  auto a=context(L);if(!(a->caps&saveData)||!(a->owner->available&saveData)||a->loading)return luaL_error(L,"storage.save unavailable during loading");
  if(lua_isnoneornil(L,1)){lua_pushboolean(L,a->owner->saveSet&&a->owner->saveSet(a->id,nullptr));return 1;}
  size_t size=0;const char* data=luaL_checklstring(L,1,&size);if(size>4096)return luaL_error(L,"save data is at most 4096 bytes");
  const std::string value(data,size);lua_pushboolean(L,a->owner->saveSet&&a->owner->saveSet(a->id,&value));return 1;
 }
 static int readTime(lua_State* L){
  auto a=context(L);if(!(a->caps&worldRead)||!(a->owner->available&worldRead))return luaL_error(L,"world.read unavailable");
  int64_t seconds=0;if(!a->owner->timeRead||!a->owner->timeRead(seconds)){lua_pushnil(L);return 1;}
  lua_pushinteger(L,lua_Integer(seconds));return 1;
 }
 static int writeTime(lua_State* L){
  auto a=context(L);
  if(!(a->caps&worldTime)||!(a->owner->available&worldTime)||!a->timerRunning||a->loading)return luaL_error(L,"world.time unavailable outside a timer callback");
  if(!lua_isinteger(L,1)||lua_tointeger(L,1)<0||lua_tointeger(L,1)>0xffffffffLL)return luaL_error(L,"set_time expects whole seconds 0..4294967295");
  lua_pushboolean(L,a->owner->timeWrite&&a->owner->timeWrite(uint32_t(lua_tointeger(L,1))));return 1;
 }
 static std::string rowLabel(lua_State* L){
  size_t n=0;const char* label=luaL_checklstring(L,1,&n);if(!n||n>31)luaL_error(L,"a row label is 1..31 characters");
  for(size_t i=0;i<n;++i)if(label[i]<0x20||label[i]>0x7e)luaL_error(L,"a row label is printable ASCII");
  return std::string(label,n);
 }
 // title.add_row(label, after_base_row, fn): rows named while the addon loads are added once the load succeeds.
 static int addTitleRow(lua_State* L){
  auto a=context(L);if(!(a->caps&titleRows)||!(a->owner->available&titleRows))return luaL_error(L,"title.rows unavailable");
  const auto label=rowLabel(L);const auto after=luaL_checkinteger(L,2);luaL_checktype(L,3,LUA_TFUNCTION);
  if(after<1||after>6)return luaL_error(L,"after_base_row is 1 (BEGIN) .. 6 (EXIT)");
  // Options: {acts_as = 1..6} also runs that base row's own game action after `fn` is queued.
  lua_Integer actsAs=0;if(lua_istable(L,4)){lua_getfield(L,4,"acts_as");if(!lua_isnil(L,-1)){if(!lua_isinteger(L,-1)||lua_tointeger(L,-1)<1||lua_tointeger(L,-1)>6)return luaL_error(L,"acts_as is a base row 1..6");actsAs=lua_tointeger(L,-1);}lua_pop(L,1);}
  else if(!lua_isnoneornil(L,4))return luaL_error(L,"title.add_row options are a table such as {acts_as = 1}");
  for(size_t i=0;i<a->titleHandlers.size();++i){auto& h=a->titleHandlers[i];if(h.first!=label)continue;
   lua_pushvalue(L,3);luaL_unref(L,LUA_REGISTRYINDEX,h.second);h.second=luaL_ref(L,LUA_REGISTRYINDEX);a->titleAfter[i]=int(after);a->titleActsAs[i]=int(actsAs);
   lua_pushboolean(L,a->loading||(a->owner->shared.addRow&&a->owner->shared.addRow(a->id,label,int(after),int(actsAs))));return 1;}
  if(a->titleHandlers.size()>=3)return luaL_error(L,"an addon may add at most 3 title rows");
  if(!a->loading&&!(a->owner->shared.addRow&&a->owner->shared.addRow(a->id,label,int(after),int(actsAs)))){lua_pushboolean(L,0);return 1;}
  lua_pushvalue(L,3);a->titleHandlers.push_back({label,luaL_ref(L,LUA_REGISTRYINDEX)});a->titleAfter.push_back(int(after));a->titleActsAs.push_back(int(actsAs));lua_pushboolean(L,1);return 1;
 }
 static int removeTitleRow(lua_State* L){
  auto a=context(L);if(!(a->caps&titleRows)||!(a->owner->available&titleRows))return luaL_error(L,"title.rows unavailable");
  const auto label=rowLabel(L);
  for(size_t i=0;i<a->titleHandlers.size();++i)if(a->titleHandlers[i].first==label){
   if(!a->loading&&a->owner->shared.removeRow)a->owner->shared.removeRow(a->id,label);
   luaL_unref(L,LUA_REGISTRYINDEX,a->titleHandlers[i].second);a->titleHandlers.erase(a->titleHandlers.begin()+ptrdiff_t(i));a->titleAfter.erase(a->titleAfter.begin()+ptrdiff_t(i));a->titleActsAs.erase(a->titleActsAs.begin()+ptrdiff_t(i));
   lua_pushboolean(L,1);return 1;}
  lua_pushboolean(L,0);return 1;
 }
 static int enableTitleRow(lua_State* L){
  auto a=context(L);if(!(a->caps&titleRows)||!(a->owner->available&titleRows))return luaL_error(L,"title.rows unavailable");
  const auto label=rowLabel(L);luaL_checktype(L,2,LUA_TBOOLEAN);
  if(a->loading)return luaL_error(L,"title.set_enabled works after loading; add the row and enable it from a callback");
  lua_pushboolean(L,a->owner->shared.enableRow&&a->owner->shared.enableRow(a->id,label,lua_toboolean(L,2)!=0));return 1;
 }
 // title.set_subtitle(label, text): a small second line under this addon's row ("" removes it); printable ASCII,
 // at most 48 bytes. After loading, once the row exists.
 static int setTitleSubtitle(lua_State* L){
  auto a=context(L);if(!(a->caps&titleRows)||!(a->owner->available&titleRows))return luaL_error(L,"title.rows unavailable");
  const auto label=rowLabel(L);size_t n=0;const char* text=luaL_checklstring(L,2,&n);
  if(n>48)return luaL_error(L,"title.set_subtitle: at most 48 bytes");for(size_t i=0;i<n;++i)if(text[i]<0x20||text[i]>0x7e)return luaL_error(L,"title.set_subtitle: printable ASCII only");
  if(a->loading)return luaL_error(L,"title.set_subtitle works after loading; call it from a callback or timer");
  bool mine=false;for(const auto& h:a->titleHandlers)mine=mine||h.first==label;
  lua_pushboolean(L,mine&&subtitleSetter&&subtitleSetter(label.c_str(),text));return 1;
 }
 // levels.serve(member, bytes, package): the game reads `bytes` instead of that package's copy of the member.
 static int serveLevel(lua_State* L){
  auto a=context(L);if(!(a->caps&levelsServe)||!(a->owner->available&levelsServe))return luaL_error(L,"levels.serve unavailable");
  if(a->loading)return luaL_error(L,"levels.serve works from callbacks, not while the addon loads");
  size_t n=0,size=0,p=0;const char* name=luaL_checklstring(L,1,&n);const char* bytes=luaL_checklstring(L,2,&size);const char* package=luaL_checklstring(L,3,&p);
  if(!n||n>16||std::memchr(name,0,n))return luaL_error(L,"a member name is 1..16 bytes");
  if(!p||p>64||std::memchr(package,0,p)||std::memchr(package,'\\',p)||std::memchr(package,'/',p))return luaL_error(L,"the package is its file name, such as Resource.rpk");
  if(!size||size>4*1024*1024)return luaL_error(L,"served bytes are 1 byte .. 4 MiB");
  lua_pushboolean(L,a->owner->shared.serve&&a->owner->shared.serve(a->id,std::string(package,p),std::string(name,n),std::string(bytes,size)));return 1;
 }
 // levels.new_game(member): the next new game starts in `member` instead of exanima01 (one start; any base title
 // row cancels it). Pair it with a title row that acts as BEGIN.
 static int newGameLevel(lua_State* L){
  auto a=context(L);if(!(a->caps&levelsFlow)||!(a->owner->available&levelsFlow))return luaL_error(L,"levels.flow unavailable");
  if(a->loading)return luaL_error(L,"levels.new_game works from callbacks");
  size_t n=0;const char* member=luaL_checklstring(L,1,&n);if(!n||n>16||std::memchr(member,0,n))return luaL_error(L,"a level member name is 1..16 bytes");
  lua_pushboolean(L,a->owner->shared.newGame&&a->owner->shared.newGame(std::string(member,n)));return 1;
 }
 // levels.travel(member[, entry]): save and travel to that level the way an exit does (the game's own job, next frame).
 static int travelLevel(lua_State* L){
  auto a=stageNative(L,levelsFlow,"levels.travel");NativeCall c;c.kind=NativeCall::travel;
  size_t n=0;const char* member=luaL_checklstring(L,1,&n);if(!n||n>16||std::memchr(member,0,n))return luaL_error(L,"a level member name is 1..16 bytes");
  std::memcpy(c.name,member,n);c.record=uint32_t(luaL_optinteger(L,2,0));a->nativeCalls.push_back(c);lua_pushboolean(L,1);return 1;
 }
 // player.place(x, y, z): put the living player at a scene position, keeping the facing; the game snaps to the ground.
 static int placePlayer(lua_State* L){
  auto a=stageNative(L,playerPlace,"player.place");NativeCall c;c.kind=NativeCall::place;
  for(int i=0;i<3;++i)c.values[i]=finiteArg(L,i+1,-1e7,1e7,"position");a->nativeCalls.push_back(c);lua_pushboolean(L,1);return 1;
 }
 // ui.notify(text): a short message that fades, bottom centre of the screen (the game has no notice line).
 static int notifyText(lua_State* L){
  auto a=context(L);if(!(a->caps&uiNotify)||!(a->owner->available&uiNotify))return luaL_error(L,"ui.notify unavailable");
  size_t n=0;const char* message=luaL_checklstring(L,1,&n);if(a->owner->shared.notify)a->owner->shared.notify(std::string(message,(std::min)(n,size_t(160))));return 0;
 }
 // characters.grant_powers(record_id, {power ids}): a live NPC learns the powers and prepares them (next pulse).
 static int grantPowers(lua_State* L){
  auto a=context(L);if(!(a->caps&characterPowers)||!(a->owner->available&characterPowers))return luaL_error(L,"characters.powers unavailable");
  if(a->loading)return luaL_error(L,"characters.grant_powers is not available while the addon loads");
  const lua_Integer record=luaL_checkinteger(L,1);if(record<=0||record>0xffffffffLL)return luaL_error(L,"record_id expected");
  luaL_checktype(L,2,LUA_TTABLE);const lua_Integer n=luaL_len(L,2);if(n<1||n>10)return luaL_error(L,"1..10 power ids");
  std::vector<uint32_t> ids;
  for(lua_Integer i=1;i<=n;++i){lua_rawgeti(L,2,i);if(!lua_isinteger(L,-1)||lua_tointeger(L,-1)<=0||lua_tointeger(L,-1)>0x0E0FFFFF)return luaL_error(L,"power ids are integers such as 0x0204");ids.push_back(uint32_t(lua_tointeger(L,-1)));lua_pop(L,1);}
  lua_pushboolean(L,a->owner->shared.grantPowers&&a->owner->shared.grantPowers(uint32_t(record),ids));return 1;
 }
 // items.add_wear(id, {wear = n, dirt = n}): raise an instanced weapon's or apparel piece's wear/dirt by n (the game
 // moves it there over two seconds, as repairs do in reverse); next pulse; false when refused or 32 already wait.
 static int addWear(lua_State* L){
  auto a=context(L);if(!(a->caps&itemsWear)||!(a->owner->available&itemsWear))return luaL_error(L,"items.wear unavailable");
  const lua_Integer id=luaL_checkinteger(L,1);if(id<=0||id>0xffffffffLL)return luaL_error(L,"item id expected");luaL_checktype(L,2,LUA_TTABLE);
  int values[2]={0,0};const char* names[2]={"wear","dirt"};
  for(int i=0;i<2;++i){lua_getfield(L,2,names[i]);if(!lua_isnil(L,-1)){if(!lua_isinteger(L,-1)||lua_tointeger(L,-1)<0||lua_tointeger(L,-1)>255)return luaL_error(L,"%s is an integer 0..255",names[i]);values[i]=int(lua_tointeger(L,-1));}lua_pop(L,1);}
  if(!values[0]&&!values[1]){lua_pushboolean(L,1);return 1;}
  lua_pushboolean(L,a->owner->shared.addWear&&a->owner->shared.addWear(uint32_t(id),values[0],values[1]));return 1;
 }
 // ui.dialog(title, text): the game's own message box with an OK button, on the game thread at the next pulse.
 static int showDialog(lua_State* L){
  auto a=context(L);if(!(a->caps&uiNotify)||!(a->owner->available&uiNotify))return luaL_error(L,"ui.notify unavailable");
  if(a->loading)return luaL_error(L,"ui.dialog is not available while the addon loads");
  size_t t=0,x=0;const char* title=luaL_checklstring(L,1,&t);const char* text=luaL_checklstring(L,2,&x);
  if(!t||t>60||x>480)return luaL_error(L,"ui.dialog takes a title of 1..60 bytes and text of 0..480");
  lua_pushboolean(L,a->owner->shared.dialog&&a->owner->shared.dialog(std::string(title,t),std::string(text,x)));return 1;
 }
 // ui.sound(name[, volume]): one of the game's single sounds at the player, on the next pulse; 4 per callback.
 static int playSound(lua_State* L){
  auto a=context(L);if(!(a->caps&uiSound)||!(a->owner->available&uiSound))return luaL_error(L,"ui.sound unavailable");
  size_t n=0;const char* name=luaL_checklstring(L,1,&n);const double volume=luaL_optnumber(L,2,0.15);
  if(!std::isfinite(volume)||volume<0||volume>1)return luaL_error(L,"ui.sound volume is 0..1");
  if(++a->worldOps>64)return luaL_error(L,"ui.sound budget exceeded");
  lua_pushboolean(L,a->owner->shared.sound&&a->owner->shared.sound(std::string(name,n),float(volume)));return 1;
 }
 // settings.define(title, rows): this addon's section of the Mod Settings tab, declared while the addon loads.
 // Each row: {key=, label=, type="toggle"|"number"|"choice"|"key", default=, min=, max=, step=, choices={...}}, or a
 // sub-header {type="header", label=} (no key, no value). A number row with format="percent" shows value x 100 with
 // a % sign (display only; the stored value is unchanged). Up to 48 rows.
 static std::string settingText(lua_State* L,int index,const SettingDef& d){
  if(d.type=="toggle"){if(!lua_isboolean(L,index))luaL_error(L,"setting '%s': default must be a boolean",d.key.c_str());return lua_toboolean(L,index)?"true":"false";}
  if(d.type=="number"){if(!lua_isnumber(L,index))luaL_error(L,"setting '%s': default must be a number",d.key.c_str());
   const double v=lua_tonumber(L,index);if(!std::isfinite(v)||v<d.minimum||v>d.maximum)luaL_error(L,"setting '%s': default is outside min..max",d.key.c_str());
   char b[32];snprintf(b,sizeof b,"%.9g",v);return b;}
  if(d.type=="key"){if(!lua_isinteger(L,index)||lua_tointeger(L,index)<0||lua_tointeger(L,index)>254)luaL_error(L,"setting '%s': default must be a key code 0..254 (0 = unbound)",d.key.c_str());
   return std::to_string(lua_tointeger(L,index));}
  size_t n=0;const char* s=lua_tolstring(L,index,&n);if(lua_type(L,index)!=LUA_TSTRING)luaL_error(L,"setting '%s': default must be one of its choices",d.key.c_str());
  const std::string v(s,n);if(std::find(d.choices.begin(),d.choices.end(),v)==d.choices.end())luaL_error(L,"setting '%s': default must be one of its choices",d.key.c_str());return v;
 }
 static std::string shortText(lua_State* L,int index,const char* field,size_t limit,bool required){
  lua_getfield(L,index,field);size_t n=0;const char* s=lua_tolstring(L,-1,&n);
  if(lua_type(L,-1)!=LUA_TSTRING){lua_pop(L,1);if(required)luaL_error(L,"setting rows need a string '%s'",field);return {};}
  if(!n||n>limit||std::memchr(s,0,n))luaL_error(L,"setting '%s' must be 1..%d bytes",field,int(limit));std::string v(s,n);lua_pop(L,1);return v;
 }
 static int defineSettings(lua_State* L){
  auto a=context(L);if(!(a->caps&modSettings))return luaL_error(L,"settings.define needs the ui.settings capability");
  if(!a->loading)return luaL_error(L,"settings.define is only allowed while the addon loads");
  if(!a->settingDefs.empty())return luaL_error(L,"settings.define was already called");
  size_t n=0;const char* title=luaL_checklstring(L,1,&n);if(!n||n>32||std::memchr(title,0,n))return luaL_error(L,"settings title must be 1..32 bytes");
  luaL_checktype(L,2,LUA_TTABLE);const lua_Integer count=luaL_len(L,2);if(count<1||count>48)return luaL_error(L,"settings.define takes 1..48 rows");
  std::vector<SettingDef> defs;
  for(lua_Integer i=1;i<=count;++i){
   lua_rawgeti(L,2,i);if(!lua_istable(L,-1))return luaL_error(L,"settings row %d is not a table",int(i));const int row=lua_gettop(L);
   SettingDef d;d.type=shortText(L,row,"type",8,true);d.label=shortText(L,row,"label",40,true);d.help=shortText(L,row,"help",160,false);
   if(d.type=="header"){d.key="#header"+std::to_string(i);lua_settop(L,row-1);defs.push_back(std::move(d));continue;}   // a sub-header line
   d.key=shortText(L,row,"key",48,true);
   for(const auto& x:defs)if(x.key==d.key)return luaL_error(L,"duplicate setting key '%s'",d.key.c_str());
   for(char c:d.key)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.'||c=='-'))return luaL_error(L,"setting key '%s' may use letters, digits, _ . - only",d.key.c_str());
   if(d.type=="number"){
    for(auto [field,target]:{std::pair<const char*,double*>{"min",&d.minimum},{"max",&d.maximum},{"step",&d.step}}){lua_getfield(L,row,field);
     if(!lua_isnumber(L,-1))return luaL_error(L,"setting '%s': number rows need min, max and step",d.key.c_str());*target=lua_tonumber(L,-1);lua_pop(L,1);}
    if(!std::isfinite(d.minimum)||!std::isfinite(d.maximum)||!std::isfinite(d.step)||d.step<=0||d.maximum<=d.minimum||(d.maximum-d.minimum)/d.step>1000.5)
     return luaL_error(L,"setting '%s': min < max, step > 0 and at most 1000 steps",d.key.c_str());
    const auto format=shortText(L,row,"format",8,false);if(!format.empty()&&format!="percent")return luaL_error(L,"setting '%s': format is \"percent\" or absent",d.key.c_str());
    d.percent=format=="percent";
   }else if(d.type=="choice"){
    lua_getfield(L,row,"choices");if(!lua_istable(L,-1))return luaL_error(L,"setting '%s': choice rows need choices={...}",d.key.c_str());
    const lua_Integer k=luaL_len(L,-1);if(k<2||k>16)return luaL_error(L,"setting '%s': 2..16 choices",d.key.c_str());
    for(lua_Integer j=1;j<=k;++j){lua_rawgeti(L,-1,j);size_t m=0;const char* s=lua_tolstring(L,-1,&m);
     if(lua_type(L,-1)!=LUA_TSTRING||!m||m>24||std::memchr(s,0,m))return luaL_error(L,"setting '%s': choices are 1..24-byte strings",d.key.c_str());
     d.choices.emplace_back(s,m);lua_pop(L,1);}
    lua_pop(L,1);
   }else if(d.type!="toggle"&&d.type!="key")return luaL_error(L,"setting '%s': type is toggle, number, choice, key or header",d.key.c_str());
   lua_getfield(L,row,"default");if(lua_isnil(L,-1))return luaL_error(L,"setting '%s' needs a default",d.key.c_str());
   d.fallback=settingText(L,-1,d);lua_settop(L,row-1);defs.push_back(std::move(d));
  }
  a->settingsTitle.assign(title,n);a->settingDefs=std::move(defs);return 0;
 }
 static void pushSetting(lua_State* L,const SettingDef& d,const std::string& v){
  if(d.type=="toggle")lua_pushboolean(L,v=="true");
  else if(d.type=="number"){char* end=nullptr;const double x=std::strtod(v.c_str(),&end);lua_pushnumber(L,end&&!*end&&std::isfinite(x)?x:std::strtod(d.fallback.c_str(),nullptr));}
  else if(d.type=="key")lua_pushinteger(L,std::atoi(v.c_str()));
  else lua_pushlstring(L,v.data(),v.size());
 }
 // settings.get(key): the current value from the active Mod Settings profile (the default until the player changes it).
 static int getModSetting(lua_State* L){
  auto a=context(L);if(!(a->caps&modSettings))return luaL_error(L,"settings.get needs the ui.settings capability");
  const std::string key=luaL_checkstring(L,1);
  for(const auto& d:a->settingDefs)if(d.key==key){std::string v;if(!a->owner->shared.readSetting||!a->owner->shared.readSetting(a->id,key,v))v=d.fallback;pushSetting(L,d,v);return 1;}
  return luaL_error(L,"unknown setting '%s' (declare it with settings.define)",key.c_str());
 }
 static int invokeSetting(lua_State* L){
  auto a=context(L);const int ref=int(lua_tointeger(L,1));const auto& c=*static_cast<const SettingChange*>(lua_touserdata(L,2));
  lua_rawgeti(L,LUA_REGISTRYINDEX,ref);lua_newtable(L);text(L,"key",c.key);
  for(const auto& d:a->settingDefs)if(d.key==c.key){pushSetting(L,d,c.value);lua_setfield(L,-2,"value");}
  lua_call(L,1,0);return 0;
 }
 // assets.read(path): bytes of a file in the addon's own folder (mods/<id>), 8 MiB at most; nil and a reason otherwise.
 static int readAsset(lua_State* L){
  auto a=context(L);if(!(a->caps&assetsRead)||!(a->owner->available&assetsRead))return luaL_error(L,"assets.read unavailable");
  size_t n=0;const char* path=luaL_checklstring(L,1,&n);if(!n||n>260||std::memchr(path,0,n))return luaL_error(L,"assets.read takes a relative path inside the addon");
  std::string out,why;
  if(!a->owner->shared.readAsset||!a->owner->shared.readAsset(a->id,std::string(path,n),out,why)){lua_pushnil(L);lua_pushstring(L,why.empty()?"unavailable":why.c_str());return 2;}
  lua_pushlstring(L,out.data(),out.size());return 1;
 }
 // levels.read(member, package): the installed bytes of a package member (the .rpk in the game folder), whatever a
 // mod serves for it; uncompressed (type 0) members only, 8 MiB at most; nil and a reason otherwise. Allowed while
 // loading, so an addon can build what it serves from the game's own file.
 static int readPackageMember(lua_State* L){
  auto a=context(L);if(!(a->caps&levelsServe)||!(a->owner->available&levelsServe))return luaL_error(L,"levels.read needs the levels.read (levels.serve) capability");
  size_t n=0,p=0;const char* member=luaL_checklstring(L,1,&n);const char* package=luaL_checklstring(L,2,&p);
  if(!n||n>16||std::memchr(member,0,n))return luaL_error(L,"a member name is 1..16 bytes");
  if(!p||p>64||std::memchr(package,0,p)||std::memchr(package,'\\',p)||std::memchr(package,'/',p)||std::memchr(package,':',p))return luaL_error(L,"the package is its file name, such as Resource.rpk");
  std::string out,why;
  if(!a->owner->shared.readPackage||!a->owner->shared.readPackage(std::string(package,p),std::string(member,n),out,why)){lua_pushnil(L);lua_pushstring(L,why.empty()?"unavailable":why.c_str());return 2;}
  lua_pushlstring(L,out.data(),out.size());return 1;
 }
 // assets.write(path, bytes): a file in this addon's cache/ folder (8 MiB at most), true or nil and a reason. Pair it
 // with a [script.overrides] entry pointing at it to serve what was computed this session from the next game start.
 static int writeAssetFile(lua_State* L){
  auto a=context(L);if(!(a->caps&assetsRead)||!(a->owner->available&assetsRead))return luaL_error(L,"assets.write needs assets.read");
  size_t n=0,size=0;const char* path=luaL_checklstring(L,1,&n);const char* bytes=luaL_checklstring(L,2,&size);
  if(!n||n>260||std::memchr(path,0,n))return luaL_error(L,"assets.write takes a path such as cache/humanoid.rsq");
  std::string why;
  if(!a->owner->shared.writeAsset||!a->owner->shared.writeAsset(a->id,std::string(path,n),std::string(bytes,size),why)){lua_pushnil(L);lua_pushstring(L,why.empty()?"unavailable":why.c_str());return 2;}
  lua_pushboolean(L,1);return 1;
 }
 // levels.add_placement(level, item_id, {x, y, z}[, yaw_degrees]): a loose item added to `level` (a member such
 // as "exanima01.rfc") whenever the game reads it; declare while loading, before the level is read.
 static int addPlacement(lua_State* L){
  auto a=context(L);if(!(a->caps&levelsServe)||!(a->owner->available&levelsServe))return luaL_error(L,"levels.serve unavailable");
  size_t n=0;const char* level=luaL_checklstring(L,1,&n);if(!n||n>16||std::memchr(level,0,n))return luaL_error(L,"level member name is 1..16 bytes, such as \"exanima01.rfc\"");
  const lua_Integer item=luaL_checkinteger(L,2);if(item<=0||item>0x7fffffff)return luaL_error(L,"item is an objdb id");
  luaL_checktype(L,3,LUA_TTABLE);float pos[3];
  for(int i=0;i<3;++i){lua_rawgeti(L,3,i+1);const lua_Number v=lua_isnumber(L,-1)?lua_tonumber(L,-1):NAN;lua_pop(L,1);if(!std::isfinite(v)||std::fabs(v)>1e7)return luaL_error(L,"position must be three finite numbers (cm)");pos[i]=float(v);}
  const lua_Number yaw=luaL_optnumber(L,4,0);if(!std::isfinite(yaw))return luaL_error(L,"yaw must be finite (degrees)");
  lua_pushboolean(L,a->owner->shared.addPlacement&&a->owner->shared.addPlacement(a->id,std::string(level,n),uint32_t(item),pos,float(yaw*0.017453292519943295)));return 1;
 }
 static int clearServedLevels(lua_State* L){
  auto a=context(L);if(!(a->caps&levelsServe)||!(a->owner->available&levelsServe))return luaL_error(L,"levels.serve unavailable");
  if(a->owner->shared.clearServed)a->owner->shared.clearServed(a->id);return 0;
 }
 // Staged native calls: callbacks only (timers, title rows), at most 4 per callback, made after it returns.
 static Addon* stageNative(lua_State* L,uint64_t needed,const char* name){
  auto a=context(L);if(!(a->caps&needed)||!(a->owner->available&needed))luaL_error(L,"%s unavailable",name);
  if(!a->timerRunning||a->loading)luaL_error(L,"%s works from timer or title-row callbacks",name);
  if(a->nativeCalls.size()>=4)luaL_error(L,"at most 4 staged game calls per callback");
  return a;
 }
 static float finiteArg(lua_State* L,int index,double low,double high,const char* what){
  const double v=luaL_checknumber(L,index);if(!std::isfinite(v)||v<low||v>high)luaL_error(L,"%s must be between %g and %g",what,low,high);return float(v);
 }
 // player.damage(balance, health, can_fall): the game's own damage applier on the player, in its native units
 // (a full health pool is about 1).
 static int damagePlayer(lua_State* L){
  auto a=stageNative(L,playerDamage,"player.damage");NativeCall c;c.kind=NativeCall::damage;
  c.values[0]=finiteArg(L,1,0,10,"balance");c.values[1]=finiteArg(L,2,0,10,"health");c.flag=lua_toboolean(L,3)!=0;
  a->nativeCalls.push_back(c);lua_pushboolean(L,1);return 1;
 }
 // player.push(dx, dy, dz, amount): the game's push-back along a direction (normalised here).
 static int pushPlayer(lua_State* L){
  auto a=stageNative(L,playerPush,"player.push");NativeCall c;c.kind=NativeCall::push;
  for(int i=0;i<3;++i)c.values[i]=finiteArg(L,i+1,-1e6,1e6,"direction");c.values[3]=finiteArg(L,4,0,100,"amount");
  const float length=std::sqrt(c.values[0]*c.values[0]+c.values[1]*c.values[1]+c.values[2]*c.values[2]);
  if(!(length>1e-6f))return luaL_error(L,"player.push needs a non-zero direction");for(int i=0;i<3;++i)c.values[i]/=length;
  a->nativeCalls.push_back(c);lua_pushboolean(L,1);return 1;
 }
 // levels.drive(record_id, state): the game's device/door driver; "open", "close", "toggle" or 0, 1, -1.
 static int driveLevel(lua_State* L){
  auto a=stageNative(L,levelsDrive,"levels.drive");NativeCall c;c.kind=NativeCall::drive;
  if(!lua_isinteger(L,1)||lua_tointeger(L,1)<1||lua_tointeger(L,1)>0xffffffffLL)return luaL_error(L,"levels.drive expects a level record ID");
  c.record=uint32_t(lua_tointeger(L,1));
  if(lua_type(L,2)==LUA_TSTRING){const std::string s=lua_tostring(L,2);
   if(s=="open")c.state=1;else if(s=="close")c.state=0;else if(s=="toggle")c.state=0xffffffffu;else return luaL_error(L,"levels.drive state is open, close or toggle");}
  else if(lua_isinteger(L,2)&&(lua_tointeger(L,2)==0||lua_tointeger(L,2)==1||lua_tointeger(L,2)==-1))c.state=uint32_t(int32_t(lua_tointeger(L,2)));
  else return luaL_error(L,"levels.drive state is open, close or toggle (or 1, 0, -1)");
  a->nativeCalls.push_back(c);lua_pushboolean(L,1);return 1;
 }
 // world.nearby(radius_cm, {model names}): placed scenery near the player, live positions, nearest first.
 static int readNearbyObjects(lua_State* L){
  auto a=context(L);if(!(a->caps&worldRead)||!(a->owner->available&worldRead))return luaL_error(L,"world.read unavailable");
  const double radius=luaL_checknumber(L,1);luaL_checktype(L,2,LUA_TTABLE);
  if(!std::isfinite(radius)||radius<=0||radius>5000)return luaL_error(L,"world.nearby radius is 0..5000 cm");
  // Names compare without case; one ending in '*' (or every name, with {prefix = true}) matches as a prefix.
  bool allPrefix=false;if(lua_istable(L,3)){lua_getfield(L,3,"prefix");allPrefix=lua_toboolean(L,-1)!=0;lua_pop(L,1);}
  std::vector<std::string> names;
  for(lua_Integer i=1;;++i){lua_rawgeti(L,2,i);if(lua_isnil(L,-1)){lua_pop(L,1);break;}
   size_t n=0;const char* name=lua_tolstring(L,-1,&n);if(!name||!n||n>17||lua_type(L,-1)!=LUA_TSTRING)return luaL_error(L,"world.nearby names are model names of 1..16 characters (a trailing * makes a prefix)");
   std::string w(name,n);if(allPrefix&&w.back()!='*')w+='*';if(w.size()>17||(w.back()!='*'&&w.size()>16))return luaL_error(L,"world.nearby names are model names of 1..16 characters");
   names.push_back(std::move(w));lua_pop(L,1);if(names.size()>64)return luaL_error(L,"world.nearby takes at most 64 names");}
  if(names.empty())return luaL_error(L,"world.nearby needs at least one model name");
  if(++a->worldOps>4)return luaL_error(L,"world snapshot query limit exceeded (4 per callback)");
  NearbyResult result;if(!a->owner->nearbyRead||!a->owner->nearbyRead(float(radius),names,result)){lua_pushnil(L);return 1;}
  lua_newtable(L);boolean(L,"truncated",result.truncated);lua_newtable(L);int i=0;
  for(const auto& o:result.objects){lua_newtable(L);text(L,"name",o.name);text(L,"kind","object");lua_pushnumber(L,o.distance);lua_setfield(L,-2,"distance");
   lua_newtable(L);for(int k=0;k<3;++k){lua_pushnumber(L,o.position[size_t(k)]);lua_rawseti(L,-2,k+1);}lua_setfield(L,-2,"position");lua_rawseti(L,-2,++i);}
  lua_setfield(L,-2,"objects");return 1;
 }
 static int readLevel(lua_State* L){
  auto a=context(L);if(!(a->caps&worldRead)||!(a->owner->available&worldRead))return luaL_error(L,"world.read unavailable");
  LevelInfo level;if(!a->owner->levelRead||!a->owner->levelRead(level)){lua_pushnil(L);return 1;}
  lua_newtable(L);text(L,"name",level.name);if(!level.mode.empty())text(L,"mode",level.mode);if(level.idKnown)number(L,"id",level.id);boolean(L,"loaded",level.loaded);return 1;
 }
 // Declarative state: the runtime reconciles the game toward these values at each session pulse, so a call made
 // during loading or from any callback is just a request; nothing touches the game from Lua directly.
 static int holdFocus(lua_State* L){
  auto a=context(L);if(!(a->caps&focusHold)||!(a->owner->available&focusHold))return luaL_error(L,"powers.focus unavailable");
  if(a->loading)return luaL_error(L,"powers.hold_focus is not available while the addon loads; call it from a timer or event");
  const lua_Number value=luaL_checknumber(L,1);
  if(!std::isfinite(value)||value<0||value>0.5)return luaL_error(L,"hold_focus expects an amount from 0 to 0.5 of the focus ceiling");
  a->focus=value;lua_pushboolean(L,1);return 1;
 }
 // powers.spend_focus(amount): the player pays `amount` of focus once, the way the game's casts do (+0xd80 drains
 // out of current focus each tick and pauses recovery). Staged for the next session pulse; false outside a session.
 static int spendFocus(lua_State* L){
  auto a=context(L);if(!(a->caps&focusHold)||!(a->owner->available&focusHold))return luaL_error(L,"powers.focus unavailable");
  if(a->loading)return luaL_error(L,"powers.spend_focus is not available while the addon loads; call it from a timer or event");
  const lua_Number value=luaL_checknumber(L,1);
  if(!std::isfinite(value)||value<0||value>0.5)return luaL_error(L,"spend_focus expects an amount from 0 to 0.5");
  if(a->owner->phaseName!="session"){lua_pushboolean(L,0);return 1;}
  a->spend=(std::min)(a->spend+value,lua_Number(0.5));lua_pushboolean(L,1);return 1;
 }
 // powers.restore_focus(amount): the player regains 0..0.5 focus at once, up to what the ceiling (less what is held
 // back) allows. Staged like spend_focus; false outside a session.
 static int restoreFocus(lua_State* L){
  auto a=context(L);if(!(a->caps&focusHold)||!(a->owner->available&focusHold))return luaL_error(L,"powers.focus unavailable");
  if(a->loading)return luaL_error(L,"powers.restore_focus is not available while the addon loads; call it from a timer or event");
  const lua_Number value=luaL_checknumber(L,1);
  if(!std::isfinite(value)||value<0||value>0.5)return luaL_error(L,"restore_focus expects an amount from 0 to 0.5");
  if(a->owner->phaseName!="session"){lua_pushboolean(L,0);return 1;}
  a->restore=(std::min)(a->restore+value,lua_Number(0.5));lua_pushboolean(L,1);return 1;
 }
 // powers.teach(power): the controlled character comes to know a Light- or Body-school power (id & 0xff00 is 0x500
 // or 0x300, the mod schools the game's dispatch does not handle), so the
 // spell book's Light page opens. Staged for the game thread; a power already known is left as it is (its
 // learning progress is never reset). True when staged; false outside a session.
 static int teachPower(lua_State* L){
  auto a=context(L);if(!(a->caps&powerCast)||!(a->owner->available&powerCast))return luaL_error(L,"powers.cast unavailable");
  if(a->loading)return luaL_error(L,"powers.teach is not available while the addon loads; call it from a timer or event");
  const lua_Integer power=luaL_checkinteger(L,1);
  if(power<=0||power>0x3ffff||((power&0xff00)!=0x500&&(power&0xff00)!=0x300))return luaL_error(L,"powers.teach takes a Light (0x..05..) or Body (0x..03..) power id");
  if(a->owner->phaseName!="session"){lua_pushboolean(L,0);return 1;}
  if(a->teach.size()<8&&std::find(a->teach.begin(),a->teach.end(),uint32_t(power))==a->teach.end())a->teach.push_back(uint32_t(power));
  lua_pushboolean(L,1);return 1;
 }
 // powers.open_school(school): the spell-book school wheel lets the player pick that school (1..8, Light is 5)
 // even before the character knows one of its powers, including at character creation. Declarative: call it
 // while loading; it lasts while the addon is enabled.
 static int openSchool(lua_State* L){
  auto a=context(L);if(!(a->caps&powerCast)||!(a->owner->available&powerCast))return luaL_error(L,"powers.cast unavailable");
  const lua_Integer school=luaL_checkinteger(L,1);if(school<1||school>8)return luaL_error(L,"powers.open_school takes a school 1..8");
  a->schools|=1u<<unsigned(school);lua_pushboolean(L,1);return 1;
 }
 // powers.wheel(power, "cast" | "fail"): the power wheel reacts as it does to the game's own casts: shown for a
 // second, and for "cast" the slot holding `power` flashes; "fail" plays the game's ThaumFail01. Staged for the next
 // session pulse (a pulse after the key press); false outside a session or for another outcome.
 static int wheelReaction(lua_State* L){
  auto a=context(L);if(!(a->caps&powerCast)||!(a->owner->available&powerCast))return luaL_error(L,"powers.cast unavailable");
  if(a->loading)return luaL_error(L,"powers.wheel is not available while the addon loads");
  const lua_Integer power=luaL_checkinteger(L,1);const std::string outcome=luaL_checkstring(L,2);
  if(power<=0||power>0x3ffff)return luaL_error(L,"powers.wheel takes a power id");
  if(outcome!="cast"&&outcome!="fail")return luaL_error(L,"powers.wheel outcome is \"cast\" or \"fail\"");
  if(a->owner->phaseName!="session"){lua_pushboolean(L,0);return 1;}
  if(a->wheel.size()<8)a->wheel.push_back(uint32_t(power)|(outcome=="cast"?0x80000000u:0u));
  if(outcome=="fail"&&a->owner->shared.sound)a->owner->shared.sound("ThaumFail01",1.0f);
  lua_pushboolean(L,1);return 1;
 }
 static int heldFocus(lua_State* L){
  auto a=context(L);if(!(a->caps&focusHold)||!(a->owner->available&focusHold))return luaL_error(L,"powers.focus unavailable");
  lua_pushnumber(L,a->focus);return 1;
 }
 // powers.hold_bindings(n): n of the player's Continuity bindings are taken by this addon's held powers, so the
 // game's own continuous powers see that many fewer (the runtime's 0x1d3a80 detour; powers.caster() is unchanged).
 static int holdBindings(lua_State* L){
  auto a=context(L);if(!(a->caps&powerCast)||!(a->owner->available&powerCast))return luaL_error(L,"powers.cast unavailable");
  if(a->loading)return luaL_error(L,"powers.hold_bindings is not available while the addon loads; call it from a timer or event");
  const lua_Integer n=luaL_checkinteger(L,1);if(n<0||n>4)return luaL_error(L,"hold_bindings expects 0..4");
  a->bindings=int(n);lua_pushboolean(L,1);return 1;
 }
 // powers.gesture(kind): the player plays a cast motion, "channel" or "thrust", and no power fires.
 static int playGesture(lua_State* L){
  auto a=context(L);if(!(a->caps&powerCast)||!(a->owner->available&powerCast))return luaL_error(L,"powers.cast unavailable");
  if(a->loading)return luaL_error(L,"powers.gesture is not available while the addon loads");
  const std::string kind=luaL_checkstring(L,1);if(kind!="channel"&&kind!="thrust")return luaL_error(L,"gesture is \"channel\" or \"thrust\"");
  lua_pushboolean(L,a->owner->shared.gesture&&a->owner->shared.gesture(kind));return 1;
 }
 static int readCaster(lua_State* L){
  auto a=context(L);if(!(a->caps&powerCast)||!(a->owner->available&powerCast))return luaL_error(L,"powers.cast unavailable");
  CasterState c;if(a->loading||!a->owner->casterRead||!a->owner->casterRead(c)){lua_pushnil(L);return 1;}
  lua_newtable(L);
  for(const auto& f:{std::pair<const char*,float>{"focus",c.focus},{"focus_ceiling",c.ceiling},{"focus_reserve",c.reserve}}){lua_pushnumber(L,f.second);lua_setfield(L,-2,f.first);}
  number(L,"bindings",c.bindings);number(L,"bindings_used",c.bindingsUsed);number(L,"last_power",c.lastPower);
  lua_newtable(L);boolean(L,"iteration",c.iteration);boolean(L,"sustain",c.sustain);boolean(L,"abstraction",c.abstraction);boolean(L,"clarity",c.clarity);
  lua_setfield(L,-2,"techniques");return 1;
 }
 static std::string lightName(lua_State* L,int index){
  size_t n=0;const char* name=luaL_checklstring(L,index,&n);
  if(!n||n>16)luaL_error(L,"a light name is 1..16 letters, digits or _");
  for(size_t i=0;i<n;++i){const unsigned char c=(unsigned char)name[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))luaL_error(L,"a light name is 1..16 letters, digits or _");}
  return std::string(name,n);
 }
 // Optional number field `key` of the table at `index`, within [low, high]; absent keeps `fallback`.
 static float lightField(lua_State* L,int index,const char* key,float fallback,float low,float high){
  lua_getfield(L,index,key);float value=fallback;
  if(!lua_isnil(L,-1)){
   if(!lua_isnumber(L,-1))luaL_error(L,"light field '%s' must be a number",key);
   const lua_Number v=lua_tonumber(L,-1);if(!std::isfinite(v)||v<low||v>high)luaL_error(L,"light field '%s' must be from %f to %f",key,double(low),double(high));
   value=float(v);
  }
  lua_pop(L,1);return value;
 }
 static int setLight(lua_State* L){
  auto a=context(L);if(!(a->caps&worldLights)||!(a->owner->available&worldLights))return luaL_error(L,"world.lights unavailable");
  const bool placed=lua_toboolean(L,lua_upvalueindex(1))!=0;const char* call=placed?"lights.place":"lights.set";
  if(a->loading)return luaL_error(L,"%s is not available while the addon loads; call it from a timer or event",call);
  LightSpec light;light.name=lightName(L,1);luaL_checktype(L,2,LUA_TTABLE);
  if(placed){
   lua_getfield(L,2,"position");if(!lua_istable(L,-1))return luaL_error(L,"lights.place needs position = {x, y, z} (scene cm)");
   for(int i=0;i<3;++i){lua_rawgeti(L,-1,i+1);const lua_Number v=lua_isnumber(L,-1)?lua_tonumber(L,-1):NAN;lua_pop(L,1);
    if(!std::isfinite(v)||std::fabs(v)>1e7)return luaL_error(L,"lights.place position must be three finite numbers");light.position[size_t(i)]=float(v);}
   lua_pop(L,1);light.placed=true;
  }
  lua_getfield(L,2,"color");
  if(!lua_isnil(L,-1)){
   if(!lua_istable(L,-1))return luaL_error(L,"light color must be {r, g, b}");
   for(int i=0;i<3;++i){lua_rawgeti(L,-1,i+1);const lua_Number v=lua_isnumber(L,-1)?lua_tonumber(L,-1):-1;lua_pop(L,1);
    if(!std::isfinite(v)||v<0||v>4)return luaL_error(L,"light color must be {r, g, b}, each from 0 to 4");light.color[size_t(i)]=float(v);}
  }
  lua_pop(L,1);
  light.range=lightField(L,2,"range",light.range,100,5000);light.intensity=lightField(L,2,"intensity",light.intensity,0,8);
  lua_getfield(L,2,"glow");
  if(!lua_isnil(L,-1)){
   if(!lua_istable(L,-1))return luaL_error(L,"light glow must be {size = cm, color = {r, g, b}}");const int at=lua_gettop(L);
   light.glow=true;light.glowSize=lightField(L,at,"size",8,2,20);
   for(int i=0;i<3;++i)light.glowColor[size_t(i)]=(std::min)(1.0f,light.color[size_t(i)]);
   lua_getfield(L,at,"color");
   if(!lua_isnil(L,-1)){if(!lua_istable(L,-1))return luaL_error(L,"glow color must be {r, g, b}");
    for(int i=0;i<3;++i){lua_rawgeti(L,-1,i+1);const lua_Number v=lua_isnumber(L,-1)?lua_tonumber(L,-1):-1;lua_pop(L,1);
     if(!std::isfinite(v)||v<0||v>1)return luaL_error(L,"glow color must be {r, g, b}, each from 0 to 1");light.glowColor[size_t(i)]=float(v);}}
   lua_pop(L,1);
  }
  lua_pop(L,1);
  lua_getfield(L,2,"follow");
  if(!lua_isnil(L,-1)){
   if(!lua_istable(L,-1))return luaL_error(L,"light follow must be a table");const int at=lua_gettop(L);
   light.forward=lightField(L,at,"forward",light.forward,-300,300);light.right=lightField(L,at,"right",light.right,-300,300);
   light.up=lightField(L,at,"up",light.up,-100,300);light.smoothing=lightField(L,at,"smoothing",light.smoothing,0,5);light.bob=lightField(L,at,"bob",light.bob,0,30);
  }
  lua_pop(L,1);
  for(auto& current:a->lights)if(current.name==light.name){current=light;lua_pushboolean(L,1);return 1;}
  if(a->lights.size()>=lightsPerAddon)return luaL_error(L,"an addon keeps at most 5 lights; remove one first");
  a->lights.push_back(light);lua_pushboolean(L,1);return 1;
 }
 static int removeLight(lua_State* L){
  auto a=context(L);if(!(a->caps&worldLights)||!(a->owner->available&worldLights))return luaL_error(L,"world.lights unavailable");
  const auto name=lightName(L,1);
  const auto end=std::remove_if(a->lights.begin(),a->lights.end(),[&](const LightSpec& l){return l.name==name;});
  const bool found=end!=a->lights.end();a->lights.erase(end,a->lights.end());lua_pushboolean(L,found);return 1;
 }
 static int listLights(lua_State* L){
  auto a=context(L);if(!(a->caps&worldLights)||!(a->owner->available&worldLights))return luaL_error(L,"world.lights unavailable");
  lua_newtable(L);int i=0;for(const auto& light:a->lights){lua_pushlstring(L,light.name.data(),light.name.size());lua_rawseti(L,-2,++i);}return 1;
 }
 static void grip(lua_State* L,const char* key,const Grip& g){
  lua_newtable(L);number(L,"kind",g.kind);lua_pushnumber(L,g.edgeStart);lua_setfield(L,-2,"edge_start");lua_pushnumber(L,g.edgeEnd);lua_setfield(L,-2,"edge_end");
  lua_pushnumber(L,g.cut);lua_setfield(L,-2,"cut");lua_pushnumber(L,g.crush);lua_setfield(L,-2,"crush");lua_pushnumber(L,g.point);lua_setfield(L,-2,"point");lua_setfield(L,-2,key);
 }
 static int readItemStats(lua_State* L){
  auto a=context(L);if(!(a->caps&itemStats)||!(a->owner->available&itemStats))return luaL_error(L,"items.stats unavailable");
  if(a->loading)return luaL_error(L,"items.stats is not available while the addon loads; call it from a timer or event");
  if(!lua_isinteger(L,1)||lua_tointeger(L,1)<1||lua_tointeger(L,1)>0xffffffffLL)return luaL_error(L,"items.stats expects an item ID (integer)");
  if(++a->statsReads>32)return luaL_error(L,"items.stats query limit exceeded (32 per callback)");
  ItemStats stats;if(!a->owner->statsRead||!a->owner->statsRead(uint32_t(lua_tointeger(L,1)),stats)){lua_pushnil(L);return 1;}
  lua_newtable(L);number(L,"item_id",lua_tointeger(L,1));
  if(stats.weapon){const auto& w=stats.w;lua_newtable(L);number(L,"flags",w.flags);number(L,"class",w.flags&0xf);boolean(L,"polearm",(w.flags&0x30)!=0);
   for(const auto& field:{std::pair<const char*,double>{"heft",w.heft},{"balance",w.balance},{"effective_heft",w.effectiveHeft},{"thrust",w.thrust},{"parry_weight",w.parryWeight},
    {"power_multiplier",w.powerMultiplier},{"recoil",w.recoil},{"recoil_two_handed",w.recoilTwoHanded}}){lua_pushnumber(L,field.second);lua_setfield(L,-2,field.first);}
   number(L,"sound",w.sound);grip(L,"grip",w.grips[0]);if(w.grips[1].kind)grip(L,"alternate_grip",w.grips[1]);
   lua_newtable(L);for(size_t i=0;i<w.raw.size();++i){lua_pushinteger(L,w.raw[i]);lua_rawseti(L,-2,lua_Integer(i+1));}lua_setfield(L,-2,"raw");
   lua_setfield(L,-2,"weapon");}
  if(stats.armour){const auto& ar=stats.a;lua_newtable(L);
   lua_newtable(L);for(size_t i=0;i<ar.coverage.size();++i)if(ar.coverage[i]){lua_pushinteger(L,ar.coverage[i]);lua_setfield(L,-2,bodyParts[i]);}lua_setfield(L,-2,"coverage");
   for(const auto& field:{std::pair<const char*,double>{"impact",ar.impact},{"cut",ar.cut},{"crush",ar.crush},{"pierce",ar.pierce}}){lua_pushnumber(L,field.second);lua_setfield(L,-2,field.first);}
   number(L,"encumbrance",ar.encumbrance);number(L,"layer",ar.layer);number(L,"vision_limit",ar.vision);lua_setfield(L,-2,"armour");}
  if(stats.condition.known){lua_newtable(L);number(L,"quality",stats.condition.quality);number(L,"wear",stats.condition.wear);number(L,"dirt",stats.condition.dirt);lua_setfield(L,-2,"condition");}
  return 1;
 }
 static int snapshotWorld(lua_State* L){
  auto a=context(L);if(!(a->caps&worldRead)||!(a->owner->available&worldRead))return luaL_error(L,"world.read unavailable");
  if(++a->worldOps>4)return luaL_error(L,"world snapshot query limit exceeded");
  const auto snapshot=a->owner->worldProvider?a->owner->worldProvider():nullptr;
  if(!snapshot||!snapshot->ready){lua_pushnil(L);return 1;}
  lua_newtable(L);boolean(L,"truncated",snapshot->truncated);text(L,"space","level_saved");lua_newtable(L);
  int index=0;for(const auto& object:snapshot->objects){
   lua_newtable(L);text(L,"kind",object.kind);text(L,"name",object.name);number(L,"item_id",object.item);
   if((a->caps&a->owner->available&(itemsUse|worldUse|worldLevers|worldDoors|worldTeleporters|worldDevices))&&a->timerRunning&&a->owner->interactionCapture){
    const auto view=a->owner->interactionCapture(object.nativeNode);
    if(view){
     text(L,"interaction_kind",view->kind==InteractionKind::door?"door":view->kind==InteractionKind::button?"button":view->kind==InteractionKind::lever?"lever":view->kind==InteractionKind::teleporter?"teleporter":view->kind==InteractionKind::scanner?"scanner":view->kind==InteractionKind::emitter?"emitter":view->kind==InteractionKind::fireSource?"fire_source":view->kind==InteractionKind::torch?"torch":"container");
     if(view->kind==InteractionKind::door){boolean(L,"locked",view->locked);number(L,"required_key",view->requiredKey);
      if(a->caps&a->owner->available&itemsUse)inventoryClosure(L,a,view->ticket,0,InventoryOp::useItemOnWorld,"use_item");
      if(view->doorControl&&(a->caps&a->owner->available&worldDoors)){boolean(L,"closed",view->closed);number(L,"door_leaves",view->doorLeaves);text(L,"door_kind",view->doorKind==0?"hinged":view->doorKind==1?"sliding":"mixed");
       if(view->doorCommand)text(L,"command",view->doorCommand==4?"opening":"closing");boolean(L,"moving",view->doorMoving);inventoryClosure(L,a,view->ticket,0,InventoryOp::doorOpen,"set_open");}
     }else if(view->kind==InteractionKind::fireSource||view->kind==InteractionKind::torch){
      if(a->caps&a->owner->available&worldDevices){boolean(L,"lit",view->pressed);text(L,"device_kind",view->kind==InteractionKind::torch?"torch":"fire");
       if(view->igniteControl)inventoryClosure(L,a,view->ticket,0,view->kind==InteractionKind::torch?InventoryOp::torchIgnite:InventoryOp::fireUse,"use");
       if(view->igniteControl&&view->kind==InteractionKind::torch)inventoryClosure(L,a,view->ticket,0,InventoryOp::torchIgnite,"ignite");
       if(view->extinguishControl)inventoryClosure(L,a,view->ticket,0,InventoryOp::fireExtinguish,"extinguish");}
     }else if(view->kind==InteractionKind::emitter){
      if(a->caps&a->owner->available&worldDevices){boolean(L,"active",view->pressed);text(L,"device_kind","electrical");inventoryClosure(L,a,view->ticket,0,InventoryOp::emitterSet,"set_active");inventoryClosure(L,a,view->ticket,0,InventoryOp::emitterToggle,"use");}
     }else if(view->kind==InteractionKind::teleporter||view->kind==InteractionKind::scanner){
      const bool scan=view->kind==InteractionKind::scanner;if(a->caps&a->owner->available&(scan?worldDevices:worldTeleporters))inventoryClosure(L,a,view->ticket,0,scan?InventoryOp::scannerUse:InventoryOp::teleporterUse,"use");
     }else if(view->kind==InteractionKind::lever){
      if(a->caps&a->owner->available&worldLevers){boolean(L,"light_control",view->lightControl);finiteNumber(L,"lever_position",view->position);number(L,"target_position",view->targetPosition);inventoryClosure(L,a,view->ticket,0,InventoryOp::leverPosition,"set_position");inventoryClosure(L,a,view->ticket,0,InventoryOp::leverToggle,"use");}
     }else if(a->caps&a->owner->available&worldUse){
      inventoryClosure(L,a,view->ticket,0,InventoryOp::worldUse,"use");
      if(view->kind==InteractionKind::button){boolean(L,"pressed",view->pressed);inventoryClosure(L,a,view->ticket,0,InventoryOp::worldPress,"set_pressed");}
      else boolean(L,"locked",view->locked);
     }
    }
   }
   if((a->caps&worldPickup)&&(a->owner->available&worldPickup)&&a->timerRunning&&a->owner->worldItemCapture){
    const auto ticket=a->owner->worldItemCapture(object.nativeNode);if(ticket)inventoryClosure(L,a,ticket,0,InventoryOp::pickup,"pickup");
   }
   if((a->caps&(containerEdit|inventoryTransfer))&&(a->owner->available&(containerEdit|inventoryTransfer))&&a->timerRunning&&a->owner->containerCapture){
    const auto ticket=a->owner->containerCapture(object.nativeNode);
    if(ticket&&(a->caps&containerEdit)&&(a->owner->available&containerEdit))for(int op=0;op<3;++op){lua_pushinteger(L,lua_Integer(ticket));lua_pushinteger(L,lua_Integer(a->execution));lua_pushinteger(L,op);
     lua_pushcclosure(L,containerMethod,3);lua_setfield(L,-2,op==0?"contents":op==1?"add_item":"remove_item");}
    if(ticket&&(a->caps&inventoryTransfer)&&(a->owner->available&inventoryTransfer))inventoryClosure(L,a,ticket,0,InventoryOp::take,"take_item");
    if(ticket&&!(a->caps&containerEdit)&&(a->caps&inventoryTransfer)){
     lua_pushinteger(L,lua_Integer(ticket));lua_pushinteger(L,lua_Integer(a->execution));lua_pushinteger(L,0);lua_pushcclosure(L,containerMethod,3);lua_setfield(L,-2,"contents");
    }
   }
   lua_newtable(L);for(int axis=0;axis<3;++axis){lua_pushnumber(L,object.position[size_t(axis)]);lua_rawseti(L,-2,axis+1);}
   lua_setfield(L,-2,"position");lua_rawseti(L,-2,++index);
  }
  lua_setfield(L,-2,"objects");return 1;
 }
 std::unique_ptr<settings::FileStore> settingsFiles;std::map<std::string,settings::Cache> settingsCache;uint64_t nextSettingsFlush=0;std::string settingsCursor;
 static Addon* context(lua_State* L){void* p=nullptr;lua_getallocf(L,&p);return static_cast<Addon*>(p);}
 static void* allocate(void* ud,void* ptr,size_t old,size_t size){
  auto a=static_cast<Addon*>(ud);if(!ptr)old=0;
  if(!size){std::free(ptr);a->bytes-=old;return nullptr;}
  if(size>a->limit||a->bytes-old>a->limit-size)return nullptr;
  void* next=std::realloc(ptr,size);if(next)a->bytes=a->bytes-old+size;
  return next;
 }
 static void budget(lua_State* L,lua_Debug* ar){
  auto a=context(L);a->instructions-=1000;
  // Every 10,000 instructions: where the addon is, for the profiler's hot lines.
  if(profiling&&(a->instructions%10000)==0&&ar&&lua_getinfo(L,"Sl",ar)&&a->hotLines.size()<4096){
   char key[96];snprintf(key,sizeof key,"%s:%d",ar->short_src,ar->currentline);++a->hotLines[key];}
  if(a->instructions<=0){
   // A tasks.run job pauses here and goes on at the next pulse; anything else stops with an error.
   if(a->taskRunning&&lua_isyieldable(L)){lua_yield(L,0);return;}
   lua_sethook(L,budget,LUA_MASKCOUNT,1);luaL_error(L,"addon instruction budget exceeded");}
 }
 static void startBudget(Addon& a,const char* label="callback"){
  if(profiling){a.profileLabel=label;a.profileStart=std::chrono::steady_clock::now();a.profileOpen=true;}a.budgetGiven=100000;
  a.logs=0;a.storageOps=0;a.worldOps=0;a.characterOps=0;a.containerReads=0;a.inventoryReads=0;a.statsReads=0;a.nativeCalls.clear();a.aiChanges.clear();a.containerChanges.clear();a.inventoryChanges.clear();++a.execution;a.instructions=100000;lua_sethook(a.state,budget,LUA_MASKCOUNT,1000);}
 static void stopBudget(Addon& a){
  lua_sethook(a.state,nullptr,0,0);
  if(profiling&&a.profileOpen){const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-a.profileStart).count();a.profileOpen=false;
   std::string key(a.profileLabel);const auto cut=key.find(" handler failed");if(cut!=std::string::npos)key.resize(cut);
   auto& b=a.profile[key];++b.calls;b.ms+=ms;b.worst=(std::max)(b.worst,ms);
   const int used=a.budgetGiven-a.instructions;if(used>0)b.instructions+=uint64_t(used);
   a.peakBytes=(std::max)(a.peakBytes,a.bytes);a.gameCalls+=a.nativeCalls.size();}
 }
 // tasks.run(fn [, {budget = n}]): fn runs as a task that is paused whenever it uses its instruction budget and
 // resumed at the next pulse with a fresh one (n per pulse, 10,000..200,000, default 200,000), so a long job (level
 // generation) finishes over several frames instead of failing. tasks.yield() inside it pauses it early until the
 // next pulse. A task runs like a timer callback (the same APIs work in it); an error in it fails the addon as a timer
 // error does. At most 8 tasks per addon.
 static constexpr int taskBudget=200000;
 static int runTask(lua_State* L){
  auto a=context(L);if(!canTime(*a))return luaL_error(L,"tasks.run needs the timers capability");
  luaL_checktype(L,1,LUA_TFUNCTION);if(a->tasks.size()>=8)return luaL_error(L,"tasks.run: at most 8 tasks per addon");
  int perPulse=taskBudget;
  if(!lua_isnoneornil(L,2)){luaL_checktype(L,2,LUA_TTABLE);lua_getfield(L,2,"budget");
   if(!lua_isnil(L,-1)){if(!lua_isinteger(L,-1)||lua_tointeger(L,-1)<10000||lua_tointeger(L,-1)>taskBudget)return luaL_error(L,"tasks.run budget is an integer 10000..200000");
    perPulse=int(lua_tointeger(L,-1))/1000*1000;}
   lua_pop(L,1);}
  lua_State* co=lua_newthread(L);lua_pushvalue(L,1);lua_xmove(L,co,1);a->tasks.push_back(luaL_ref(L,LUA_REGISTRYINDEX));a->taskBudgets.push_back(perPulse);
  lua_pushboolean(L,1);return 1;
 }
 static int yieldTask(lua_State* L){
  auto a=context(L);if(!a->taskRunning||!lua_isyieldable(L))return luaL_error(L,"tasks.yield is only allowed inside a task (tasks.run), outside C callbacks");
  return lua_yield(L,0);
 }
 static int countTasks(lua_State* L){lua_pushinteger(L,lua_Integer(context(L)->tasks.size()));return 1;}
 static int subscribe(lua_State* L){
  auto a=context(L);
  if(!a->loading)return luaL_error(L,"events.on is only allowed while the addon loads (top level of the entry script)");
  const char* name=luaL_checkstring(L,1);
  size_t n=0;const char* model="";Kind kind=Kind::item;const Subscription* found=nullptr;
  for(const auto& s:subscriptions)if(!std::strcmp(name,s.name))found=&s;
  if(found){
   if(!(a->caps&found->needs))return luaL_error(L,"events.on('%s') needs the %s capability in requires or optional",name,found->missing);
   luaL_checktype(L,2,LUA_TFUNCTION);kind=found->kind;
  }else if(!std::strcmp(name,"item.use")){
   if((a->caps&(events|items))!=(events|items))return luaL_error(L,"events.on('item.use') needs the events.item_use and items.read capabilities");
   model=luaL_checklstring(L,2,&n);luaL_checktype(L,3,LUA_TFUNCTION);
   if(!n||n>16||std::memchr(model,0,n))return luaL_error(L,"item.use expects a model name of 1..16 bytes");
  }else return luaL_error(L,"unknown event '%s' (events: item.use, runtime.phase, character.hit, script.event, zone.enter, game.message, power.cast, gear.use, settings.changed, item.id_changed)",name);
  if(a->handlers.size()>=64)return luaL_error(L,"too many subscriptions");
  lua_pushvalue(L,kind==Kind::item?3:2);const int ref=luaL_ref(L,LUA_REGISTRYINDEX);
  bool added=false;try{a->handlers.push_back({std::string(model,n),ref,kind});added=true;}catch(...){}
  if(!added){luaL_unref(L,LUA_REGISTRYINDEX,ref);return luaL_error(L,"subscription allocation failed");}
  return 0;
 }
 static int requestAction(lua_State* L){
  auto a=context(L);const auto expected=static_cast<uint64_t>(lua_tointeger(L,lua_upvalueindex(1)));
  const int kind=int(lua_tointeger(L,lua_upvalueindex(2)));
  if(!a->event||a->token!=expected)return luaL_error(L,"expired event handle");
  const Action action=static_cast<Action>(kind);
  if(a->pending!=Action::none)return luaL_error(L,"event already has an action");
  if(action==Action::openStorage){
   if(!(a->caps&openStorage)||!a->event->container||a->event->target!="inventory"||a->event->targetItem!=a->event->item)
    return luaL_error(L,"container open is unavailable for this target");
  }else if(action==Action::unlockDoor){
   if(!(a->caps&unlockDoor)||a->event->target!="door"||!a->event->doorLocked||a->event->storageOpen)
    return luaL_error(L,"door unlock is unavailable for this target");
  }else return luaL_error(L,"unknown action");
  a->pending=action;return 0;
 }
 static void number(lua_State* L,const char* name,lua_Integer value){lua_pushinteger(L,value);lua_setfield(L,-2,name);}
 static void boolean(lua_State* L,const char* name,bool value){lua_pushboolean(L,value);lua_setfield(L,-2,name);}
 static void text(lua_State* L,const char* name,const std::string& value){lua_pushlstring(L,value.data(),value.size());lua_setfield(L,-2,name);}
 static int traceback(lua_State* L){
  const char* message=lua_tostring(L,1);luaL_traceback(L,L,message?message:"non-string Lua error",1);return 1;
 }
 static int protectedCall(lua_State* L,int arguments,int results){
  const int function=lua_gettop(L)-arguments;lua_pushcfunction(L,traceback);lua_insert(L,function);
  const int status=lua_pcall(L,arguments,results,function);lua_remove(L,function);return status;
 }
 static int hasCapability(lua_State* L){
  auto a=context(L);const auto bit=capability(luaL_checkstring(L,1));
  lua_pushboolean(L,bit&&(a->caps&bit)&&(a->owner->available&bit));return 1;
 }
 // Every capability this addon may use right now, sorted by name: the list has_capability would say true to.
 static int listCapabilities(lua_State* L){
  auto a=context(L);std::vector<std::string> names;
  for(const auto& known:capabilityNames)if((a->caps&known.bit)&&(a->owner->available&known.bit))names.push_back(known.name);
  std::sort(names.begin(),names.end());lua_newtable(L);int i=0;for(const auto& name:names){lua_pushlstring(L,name.data(),name.size());lua_rawseti(L,-2,++i);}return 1;
 }
 static int writeLog(lua_State* L){
  auto a=context(L);const char* level=lua_tostring(L,lua_upvalueindex(1));
  if(a->logs++>=32)return 0;
  std::string message;const int count=lua_gettop(L);
  for(int i=1;i<=count&&message.size()<2048;++i){
   if(i>1)message+='\t';size_t size=0;const char* part=luaL_tolstring(L,i,&size);
   message.append(part,(std::min)(size,size_t(2048)-message.size()));lua_pop(L,1);
  }
  a->owner->report(a->id.c_str(),level,message.c_str());return 0;
 }
 static int requireModule(lua_State* L){
  auto a=context(L);size_t length=0;const char* name=luaL_checklstring(L,1,&length);
  size_t index=0;for(;index<a->modules.size();++index)if(a->modules[index].name==std::string(name,length))break;
  if(index==a->modules.size())return luaL_error(L,"module is not declared in this addon: %s",name);
  lua_rawgeti(L,LUA_REGISTRYINDEX,a->cache);lua_rawgeti(L,-1,int(index+1));
  if(!lua_isnil(L,-1))return 1;
  lua_pop(L,1);
  if(a->importing[index])return luaL_error(L,"cyclic module import: %s",name);
  a->importing[index]=true;const auto& module=a->modules[index];
  const std::string chunk="@"+a->id+"/"+module.name;
  int status=luaL_loadbufferx(L,module.source.data(),module.source.size(),chunk.c_str(),"t");
  if(status==LUA_OK)status=protectedCall(L,0,1);
  a->importing[index]=false;
  if(status!=LUA_OK)return lua_error(L);
  if(lua_isnil(L,-1)){lua_pop(L,1);lua_pushboolean(L,1);}
  lua_pushvalue(L,-1);lua_rawseti(L,-3,int(index+1));return 1;
 }
 static bool canTime(const Addon& a){return (a.caps&timerEvents)&&(a.owner->available&timerEvents);}
 static void clearTimers(Addon& a){for(const auto& timer:a.timers.entries())luaL_unref(a.state,LUA_REGISTRYINDEX,timer.reference);a.timers.clear();}
 static int scheduleTimer(lua_State* L){
  auto a=context(L);if(!canTime(*a))return luaL_error(L,"timers capability is unavailable");
  const bool repeating=lua_toboolean(L,lua_upvalueindex(1))!=0;
  const double seconds=luaL_checknumber(L,1);luaL_checktype(L,2,LUA_TFUNCTION);
  if(!std::isfinite(seconds)||seconds<0||seconds>86400||(repeating&&seconds<0.01))return luaL_error(L,"invalid timer delay (after: 0..86400 seconds; every: 0.01..86400)");
  const uint64_t delay=static_cast<uint64_t>((std::max)(1.0,std::ceil(seconds*1000.0)));
  auto& owner=*a->owner;
  if(owner.nextTimer>=static_cast<uint64_t>(LUA_MAXINTEGER)||owner.clock>(std::numeric_limits<uint64_t>::max)()-delay)return luaL_error(L,"timer clock or ID exhausted");
  lua_pushvalue(L,2);const int ref=luaL_ref(L,LUA_REGISTRYINDEX);const auto id=++owner.nextTimer;
  bool added=false;try{added=a->timers.add({id,owner.clock+delay,repeating?delay:0,ref});}catch(...){}
  if(!added){luaL_unref(L,LUA_REGISTRYINDEX,ref);return luaL_error(L,"addon timer limit exceeded (64)");}
  lua_pushinteger(L,static_cast<lua_Integer>(id));return 1;
 }
 static int cancelTimer(lua_State* L){
  auto a=context(L);if(!canTime(*a))return luaL_error(L,"timers capability is unavailable");
  const auto id=luaL_checkinteger(L,1);const int ref=id>0?a->timers.cancel(static_cast<uint64_t>(id)):0;
  if(ref)luaL_unref(L,LUA_REGISTRYINDEX,ref);lua_pushboolean(L,ref!=0);return 1;
 }
 static int timerNow(lua_State* L){
  auto a=context(L);if(!canTime(*a))return luaL_error(L,"timers capability is unavailable");
  lua_pushnumber(L,double(a->owner->clock)/1000.0);return 1;
 }
 static Addon* settingsContext(lua_State* L){
  auto a=context(L);if(!(a->caps&settingsStorage)||!(a->owner->available&settingsStorage)||!a->settingsData)luaL_error(L,"storage.settings capability is unavailable");
  if(++a->storageOps>128)luaL_error(L,"settings operation budget exceeded (128)");return a;
 }
 static std::string settingsKey(lua_State* L){
  luaL_checktype(L,1,LUA_TSTRING);size_t size=0;const char* key=luaL_checklstring(L,1,&size);
  if(!size||size>64||std::memchr(key,0,size))luaL_error(L,"settings key must be 1..64 UTF-8 bytes without NUL");
  std::string result(key,size);if(!settings::utf8(result))luaL_error(L,"settings key must be UTF-8");return result;
 }
 static int getSetting(lua_State* L){
  auto a=settingsContext(L);try{const auto key=settingsKey(L);const auto& values=a->staged?*a->staged:a->settingsData->values;
   const auto found=values.find(key);if(found==values.end()){if(lua_gettop(L)>1)lua_pushvalue(L,2);else lua_pushnil(L);}
   else settings::toLua(L,found->second);return 1;
  }catch(const std::exception& error){return luaL_error(L,"%s",error.what());}
 }
 static int setSetting(lua_State* L){
  auto a=settingsContext(L);try{
   const auto key=settingsKey(L);auto next=a->staged?*a->staged:a->settingsData->values;
   if(lua_isnoneornil(L,2))next.erase(key);
   else{size_t nodes=0;std::vector<const void*> active;next[key]=settings::fromLua(L,2,0,nodes,active);}
   settings::pack(a->id,next);a->staged=std::make_unique<settings::Document>(std::move(next));return 0;
  }catch(const std::exception& error){return luaL_error(L,"%s",error.what());}
 }
 static int settingsStatus(lua_State* L){
  auto a=settingsContext(L);const char* status=a->settingsData->conflict?"conflict":!a->settingsData->error.empty()?"error":a->staged||a->settingsData->dirty?"pending":"saved";
  lua_pushstring(L,status);return 1;
 }
 static void commitSettings(Addon& a){if(a.staged){a.settingsData->values.swap(*a.staged);a.staged.reset();a.settingsData->dirty=true;}}
 static int initialize(lua_State* L){
  const struct {const char* name;lua_CFunction open;} libs[]={
   {LUA_GNAME,luaopen_base},{LUA_TABLIBNAME,luaopen_table},{LUA_STRLIBNAME,luaopen_string},
   {LUA_MATHLIBNAME,luaopen_math},{LUA_UTF8LIBNAME,luaopen_utf8}
  };
  for(const auto& lib:libs){luaL_requiref(L,lib.name,lib.open,1);lua_pop(L,1);}
  for(const char* name:{"dofile","loadfile","load","collectgarbage","setmetatable","getmetatable"}){lua_pushnil(L);lua_setglobal(L,name);}
  lua_newtable(L);lua_pushcfunction(L,subscribe);lua_setfield(L,-2,"on");lua_setglobal(L,"events");
  lua_newtable(L);number(L,"version",1);number(L,"revision",apiRevision);lua_pushstring(L,runtimeVersion);lua_setfield(L,-2,"runtime");
  text(L,"addon",context(L)->id);
  lua_pushcfunction(L,hasCapability);lua_setfield(L,-2,"has_capability");lua_pushcfunction(L,listCapabilities);lua_setfield(L,-2,"capabilities");lua_setglobal(L,"api");
  lua_newtable(L);for(const char* level:{"info","warn","error"}){lua_pushstring(L,level);lua_pushcclosure(L,writeLog,1);lua_setfield(L,-2,level);}lua_setglobal(L,"log");
  lua_pushstring(L,"info");lua_pushcclosure(L,writeLog,1);lua_setglobal(L,"print");
  lua_newtable(L);context(L)->cache=luaL_ref(L,LUA_REGISTRYINDEX);
  lua_pushcfunction(L,requireModule);lua_setglobal(L,"require");
  lua_newtable(L);lua_pushboolean(L,0);lua_pushcclosure(L,scheduleTimer,1);lua_setfield(L,-2,"after");
  lua_pushboolean(L,1);lua_pushcclosure(L,scheduleTimer,1);lua_setfield(L,-2,"every");
  lua_pushcfunction(L,cancelTimer);lua_setfield(L,-2,"cancel");lua_pushcfunction(L,timerNow);lua_setfield(L,-2,"now");lua_setglobal(L,"timers");
  lua_newtable(L);lua_pushcfunction(L,runTask);lua_setfield(L,-2,"run");lua_pushcfunction(L,countTasks);lua_setfield(L,-2,"count");lua_pushcfunction(L,yieldTask);lua_setfield(L,-2,"yield");lua_pushcfunction(L,runWorker);lua_setfield(L,-2,"worker");lua_pushcfunction(L,cancelWorker);lua_setfield(L,-2,"cancel_worker");lua_pushcfunction(L,countWorkers);lua_setfield(L,-2,"workers");lua_setglobal(L,"tasks");
  lua_newtable(L);lua_pushcfunction(L,getSetting);lua_setfield(L,-2,"get");lua_pushcfunction(L,setSetting);lua_setfield(L,-2,"set");lua_pushcfunction(L,settingsStatus);lua_setfield(L,-2,"status");lua_setglobal(L,"storage");
  lua_newtable(L);lua_pushcfunction(L,snapshotWorld);lua_setfield(L,-2,"snapshot");
  lua_pushcfunction(L,readTime);lua_setfield(L,-2,"time");lua_pushcfunction(L,writeTime);lua_setfield(L,-2,"set_time");
  lua_pushcfunction(L,readLevel);lua_setfield(L,-2,"level");lua_pushcfunction(L,readNearbyObjects);lua_setfield(L,-2,"nearby");lua_pushcfunction(L,lightFire);lua_setfield(L,-2,"light_fire");lua_pushcfunction(L,putOutFire);lua_setfield(L,-2,"put_out_fire");lua_pushcfunction(L,listFires);lua_setfield(L,-2,"fires");lua_setglobal(L,"world");
  lua_newtable(L);lua_pushcfunction(L,readItemStats);lua_setfield(L,-2,"stats");lua_pushcfunction(L,addWear);lua_setfield(L,-2,"add_wear");lua_setglobal(L,"items");
  lua_newtable(L);lua_pushcfunction(L,addTitleRow);lua_setfield(L,-2,"add_row");lua_pushcfunction(L,removeTitleRow);lua_setfield(L,-2,"remove_row");
  lua_pushcfunction(L,enableTitleRow);lua_setfield(L,-2,"set_enabled");lua_pushcfunction(L,setTitleSubtitle);lua_setfield(L,-2,"set_subtitle");lua_setglobal(L,"title");
  lua_newtable(L);lua_pushcfunction(L,serveLevel);lua_setfield(L,-2,"serve");lua_pushcfunction(L,readPackageMember);lua_setfield(L,-2,"read");lua_pushcfunction(L,clearServedLevels);lua_setfield(L,-2,"clear_served");
  lua_pushcfunction(L,driveLevel);lua_setfield(L,-2,"drive");lua_pushcfunction(L,newGameLevel);lua_setfield(L,-2,"new_game");lua_pushcfunction(L,travelLevel);lua_setfield(L,-2,"travel");lua_pushcfunction(L,addPlacement);lua_setfield(L,-2,"add_placement");lua_setglobal(L,"levels");
  lua_newtable(L);lua_pushcfunction(L,readAsset);lua_setfield(L,-2,"read");lua_pushcfunction(L,writeAssetFile);lua_setfield(L,-2,"write");lua_setglobal(L,"assets");
  lua_newtable(L);lua_pushcfunction(L,notifyText);lua_setfield(L,-2,"notify");lua_pushcfunction(L,playSound);lua_setfield(L,-2,"sound");lua_pushcfunction(L,showDialog);lua_setfield(L,-2,"dialog");lua_setglobal(L,"ui");
  lua_newtable(L);lua_pushcfunction(L,defineSettings);lua_setfield(L,-2,"define");lua_pushcfunction(L,getModSetting);lua_setfield(L,-2,"get");lua_setglobal(L,"settings");
  lua_newtable(L);lua_pushcfunction(L,damagePlayer);lua_setfield(L,-2,"damage");lua_pushcfunction(L,pushPlayer);lua_setfield(L,-2,"push");lua_pushcfunction(L,placePlayer);lua_setfield(L,-2,"place");lua_setglobal(L,"player");
  lua_newtable(L);lua_pushcfunction(L,readSave);lua_setfield(L,-2,"get");lua_pushcfunction(L,writeSave);lua_setfield(L,-2,"set");lua_setglobal(L,"save_data");
  lua_newtable(L);lua_pushcfunction(L,readFlag);lua_setfield(L,-2,"get");lua_pushcfunction(L,writeFlag);lua_setfield(L,-2,"set");lua_setglobal(L,"script_flags");
  lua_newtable(L);lua_pushcfunction(L,snapshotInventory);lua_setfield(L,-2,"snapshot");lua_setglobal(L,"inventory");
  lua_newtable(L);lua_pushboolean(L,0);lua_pushcclosure(L,snapshotCharacters,1);lua_setfield(L,-2,"snapshot");
  lua_pushboolean(L,1);lua_pushcclosure(L,snapshotCharacters,1);lua_setfield(L,-2,"awareness");lua_pushcfunction(L,grantPowers);lua_setfield(L,-2,"grant_powers");lua_pushcfunction(L,setMark);lua_setfield(L,-2,"set_mark");lua_pushcfunction(L,setDamageScale);lua_setfield(L,-2,"set_damage_scale");lua_pushcfunction(L,setHitSound);lua_setfield(L,-2,"set_hit_sound");lua_pushcfunction(L,setLimbStrength);lua_setfield(L,-2,"set_limb_strength");lua_pushcfunction(L,dropHeld);lua_setfield(L,-2,"drop_held");lua_pushcfunction(L,sever);lua_setfield(L,-2,"sever");lua_pushcfunction(L,keepDown);lua_setfield(L,-2,"keep_down");lua_pushcfunction(L,playMotion);lua_setfield(L,-2,"play_motion");lua_pushboolean(L,1);lua_pushcclosure(L,setBodyScale,1);lua_setfield(L,-2,"set_strength");lua_pushboolean(L,0);lua_pushcclosure(L,setBodyScale,1);lua_setfield(L,-2,"set_anim_rate");lua_setglobal(L,"characters");
  lua_newtable(L);lua_pushcfunction(L,spawnArc);lua_setfield(L,-2,"spawn_arc");lua_pushcfunction(L,attachFireLua);lua_setfield(L,-2,"attach_fire");lua_pushcfunction(L,removeFireLua);lua_setfield(L,-2,"remove_fire");lua_setglobal(L,"effects");
  lua_newtable(L);lua_pushcfunction(L,profileBegin);lua_setfield(L,-2,"begin");lua_pushcfunction(L,profileFinish);lua_setfield(L,-2,"finish");lua_pushcfunction(L,profileScope);lua_setfield(L,-2,"scope");lua_setglobal(L,"profile");
  lua_newtable(L);lua_pushcfunction(L,sendMessage);lua_setfield(L,-2,"send");lua_pushcfunction(L,onMessage);lua_setfield(L,-2,"on");lua_setglobal(L,"addons");
  lua_newtable(L);lua_pushcfunction(L,holdFocus);lua_setfield(L,-2,"hold_focus");lua_pushcfunction(L,spendFocus);lua_setfield(L,-2,"spend_focus");lua_pushcfunction(L,restoreFocus);lua_setfield(L,-2,"restore_focus");lua_pushcfunction(L,teachPower);lua_setfield(L,-2,"teach");lua_pushcfunction(L,openSchool);lua_setfield(L,-2,"open_school");lua_pushcfunction(L,wheelReaction);lua_setfield(L,-2,"wheel");lua_pushcfunction(L,heldFocus);lua_setfield(L,-2,"held_focus");
  lua_pushcfunction(L,readCaster);lua_setfield(L,-2,"caster");lua_pushcfunction(L,holdBindings);lua_setfield(L,-2,"hold_bindings");lua_pushcfunction(L,playGesture);lua_setfield(L,-2,"gesture");lua_setglobal(L,"powers");
  lua_newtable(L);lua_pushboolean(L,0);lua_pushcclosure(L,setLight,1);lua_setfield(L,-2,"set");lua_pushboolean(L,1);lua_pushcclosure(L,setLight,1);lua_setfield(L,-2,"place");lua_pushcfunction(L,removeLight);lua_setfield(L,-2,"remove");
  lua_pushcfunction(L,listLights);lua_setfield(L,-2,"list");lua_setglobal(L,"lights");
  return 0;
 }
 static int invoke(lua_State* L){
  auto a=context(L);const auto& e=*a->event;
  const int ref=int(lua_tointeger(L,1));lua_rawgeti(L,LUA_REGISTRYINDEX,ref);
  lua_newtable(L);boolean(L,"storage_open",e.storageOpen);
  lua_newtable(L);number(L,"id",e.item);text(L,"model",e.model);
  lua_newtable(L);
  if((a->caps&(contents|items))==(contents|items)){int i=0;for(const auto& item:e.entries){
   lua_newtable(L);number(L,"id",item.id);boolean(L,"is_key",item.key);boolean(L,"removed",item.removed);
   lua_rawseti(L,-2,++i);
  }}
  lua_setfield(L,-2,"contents");lua_setfield(L,-2,"item");
  lua_newtable(L);text(L,"kind",e.target);number(L,"item_id",e.targetItem);number(L,"required_key",e.requiredKey);
  boolean(L,"locked",e.doorLocked);
  if(!e.targetGear.empty()){text(L,"gear",e.targetGear);boolean(L,"carried",e.targetCarried);
   if(e.targetCondition.known){lua_newtable(L);number(L,"quality",e.targetCondition.quality);number(L,"wear",e.targetCondition.wear);number(L,"dirt",e.targetCondition.dirt);lua_setfield(L,-2,"condition");}}
  lua_setfield(L,-2,"target");
  for(const auto& method: {std::pair<const char*,Action>{"open_storage",Action::openStorage},{"unlock",Action::unlockDoor}}){
   lua_pushinteger(L,static_cast<lua_Integer>(a->token));lua_pushinteger(L,int(method.second));
   lua_pushcclosure(L,requestAction,2);lua_setfield(L,-2,method.first);
  }
  lua_pushinteger(L,lua_Integer(a->token));lua_pushcclosure(L,restoreTarget,1);lua_setfield(L,-2,"restore_condition");
  lua_pushinteger(L,lua_Integer(a->token));lua_pushcclosure(L,consumeUsed,1);lua_setfield(L,-2,"consume");
  lua_call(L,1,1);
  if(!lua_isnil(L,-1)&&!lua_isboolean(L,-1))return luaL_error(L,"item-use handlers return a boolean or nil");
  if((a->pending!=Action::none||a->itemRestore.restore||a->consumePending)&&!lua_toboolean(L,-1))return luaL_error(L,"an action requires a handled result");
  return 1;
 }
 static int invokeScript(lua_State* L){
  const int ref=int(lua_tointeger(L,1));const auto& event=*static_cast<const ScriptEvent*>(lua_touserdata(L,2));
  lua_rawgeti(L,LUA_REGISTRYINDEX,ref);lua_newtable(L);text(L,"name",event.name.c_str());
  if(!event.value.empty())text(L,"value",event.value.c_str());
  if(event.numberKnown)number(L,"number",event.number);
  lua_call(L,1,0);return 0;
 }
 // event:restore_condition{wear=, dirt=}: lower only, carried instanced items only, once per event.
 static int restoreCondition(lua_State* L){
  auto a=context(L);const auto expected=uint64_t(lua_tointeger(L,lua_upvalueindex(1)));
  if(!a->gear||a->token!=expected)return luaL_error(L,"expired gear.use event");
  if(!(a->caps&itemsCondition)||!(a->owner->available&itemsCondition))return luaL_error(L,"restore_condition needs the items.condition capability");
  if(a->gearPending.restore)return luaL_error(L,"restore_condition was already requested for this event");
  luaL_checktype(L,2,LUA_TTABLE);const auto& g=*a->gear;
  if(g.kind=="shield")return luaL_error(L,"restore_condition does not apply to shields (the runtime reads no condition for them)");
  if(!g.carried)return luaL_error(L,"restore_condition works on carried items only, not worn or held ones");
  if(!(g.item&0xf0000000u))return luaL_error(L,"restore_condition works on instanced items (generated loot) only");
  if(!g.condition.known)return luaL_error(L,"this item's condition is unreadable");
  int values[2]={-1,-1};const char* names[2]={"wear","dirt"};const unsigned current[2]={g.condition.wear,g.condition.dirt};
  for(int i=0;i<2;++i){lua_getfield(L,2,names[i]);
   if(!lua_isnil(L,-1)){if(!lua_isinteger(L,-1)||lua_tointeger(L,-1)<0||lua_tointeger(L,-1)>255)return luaL_error(L,"%s is an integer 0..255",names[i]);
    if(lua_tointeger(L,-1)>lua_Integer(current[i]))return luaL_error(L,"restore_condition only lowers %s (now %d)",names[i],int(current[i]));values[i]=int(lua_tointeger(L,-1));}
   lua_pop(L,1);}
  if(values[0]<0&&values[1]<0)return luaL_error(L,"restore_condition needs wear or dirt");
  a->gearPending.restore=true;a->gearPending.wear=values[0];a->gearPending.dirt=values[1];lua_pushboolean(L,1);return 1;
 }
 // item.use event:restore_condition{wear=, dirt=}: the same rules as gear.use, applied to the inventory target
 // (target.item_id), which must be a carried, instanced weapon or apparel piece with a readable condition.
 static int restoreTarget(lua_State* L){
  auto a=context(L);const auto expected=uint64_t(lua_tointeger(L,lua_upvalueindex(1)));
  if(!a->event||a->token!=expected)return luaL_error(L,"expired item.use event");
  if(!(a->caps&itemsCondition)||!(a->owner->available&itemsCondition))return luaL_error(L,"restore_condition needs the items.condition capability");
  if(a->itemRestore.restore)return luaL_error(L,"restore_condition was already requested for this event");
  luaL_checktype(L,2,LUA_TTABLE);const auto& e=*a->event;
  if(e.target!="inventory"||e.targetGear.empty()||e.targetGear=="shield")return luaL_error(L,"restore_condition needs a weapon or apparel target in the inventory");
  if(!e.targetCarried)return luaL_error(L,"restore_condition works on carried items only, not worn or held ones");
  if(!(e.targetItem&0xf0000000u))return luaL_error(L,"restore_condition works on instanced items (generated loot) only");
  if(!e.targetCondition.known)return luaL_error(L,"the target's condition is unreadable");
  int values[2]={-1,-1};const char* names[2]={"wear","dirt"};const unsigned current[2]={e.targetCondition.wear,e.targetCondition.dirt};
  for(int i=0;i<2;++i){lua_getfield(L,2,names[i]);
   if(!lua_isnil(L,-1)){if(!lua_isinteger(L,-1)||lua_tointeger(L,-1)<0||lua_tointeger(L,-1)>255)return luaL_error(L,"%s is an integer 0..255",names[i]);
    if(lua_tointeger(L,-1)>lua_Integer(current[i]))return luaL_error(L,"restore_condition only lowers %s (now %d)",names[i],int(current[i]));values[i]=int(lua_tointeger(L,-1));}
   lua_pop(L,1);}
  if(values[0]<0&&values[1]<0)return luaL_error(L,"restore_condition needs wear or dirt");
  a->itemRestore.restore=true;a->itemRestore.wear=values[0];a->itemRestore.dirt=values[1];lua_pushboolean(L,1);return 1;
 }
 // item.use event:consume(): the used item leaves the pack once the use ends (next pulse), like a spent salve.
 static int consumeUsed(lua_State* L){
  auto a=context(L);const auto expected=uint64_t(lua_tointeger(L,lua_upvalueindex(1)));
  if(!a->event||a->token!=expected)return luaL_error(L,"expired item.use event");
  if(!(a->caps&inventoryTransfer)||!(a->owner->available&inventoryTransfer))return luaL_error(L,"consume needs the inventory.transfer capability");
  a->consumePending=true;lua_pushboolean(L,1);return 1;
 }
 // gear.use event:ignite_target(): light a fire on the picked object (a forge) through the game's own fire
 // builder, as a job after the handler returns; refused when a fire already burns there.
 static int igniteTarget(lua_State* L){
  auto a=context(L);const auto expected=uint64_t(lua_tointeger(L,lua_upvalueindex(1)));
  if(!a->gear||a->token!=expected)return luaL_error(L,"expired gear.use event");
  if(!(a->caps&worldDevices)||!(a->owner->available&worldDevices))return luaL_error(L,"ignite_target needs the world.devices capability");
  if(!a->gear->ignitable)return luaL_error(L,"ignite_target works on placed objects only, and not where a fire already burns");
  a->gearPending.ignite=true;lua_pushboolean(L,1);return 1;
 }
 static int invokeGear(lua_State* L){
  auto a=context(L);const auto& g=*a->gear;const int ref=int(lua_tointeger(L,1));lua_rawgeti(L,LUA_REGISTRYINDEX,ref);
  lua_newtable(L);
  lua_newtable(L);number(L,"id",g.item);text(L,"model",g.model);text(L,"kind",g.kind);boolean(L,"carried",g.carried);
  if(g.condition.known){lua_newtable(L);number(L,"quality",g.condition.quality);number(L,"wear",g.condition.wear);number(L,"dirt",g.condition.dirt);lua_setfield(L,-2,"condition");}
  lua_setfield(L,-2,"item");
  lua_newtable(L);text(L,"kind",g.targetKind);if(!g.targetName.empty())text(L,"name",g.targetName);if(g.targetRecord)number(L,"record_id",g.targetRecord);
  finiteNumber(L,"distance",g.distance);boolean(L,"heat",std::isfinite(g.heat));finiteNumber(L,"heat_distance",g.heat);lua_setfield(L,-2,"target");
  lua_pushinteger(L,lua_Integer(a->token));lua_pushcclosure(L,restoreCondition,1);lua_setfield(L,-2,"restore_condition");
  lua_pushinteger(L,lua_Integer(a->token));lua_pushcclosure(L,igniteTarget,1);lua_setfield(L,-2,"ignite_target");
  lua_call(L,1,1);
  if(!lua_isnil(L,-1)&&!lua_isboolean(L,-1))return luaL_error(L,"gear.use handlers return a boolean or nil");
  if((a->gearPending.restore||a->gearPending.ignite)&&!lua_toboolean(L,-1))return luaL_error(L,"restore_condition and ignite_target require a handled (true) result");
  return 1;
 }
 static int invokeIdChange(lua_State* L){
  const int ref=int(lua_tointeger(L,1));const auto& c=*static_cast<const IdChange*>(lua_touserdata(L,2));
  lua_rawgeti(L,LUA_REGISTRYINDEX,ref);lua_newtable(L);number(L,"old_id",c.before);number(L,"new_id",c.after);lua_call(L,1,0);return 0;
 }
 static int invokeZone(lua_State* L){
  const int ref=int(lua_tointeger(L,1));const auto& zone=*static_cast<const ZoneEntry*>(lua_touserdata(L,2));
  lua_rawgeti(L,LUA_REGISTRYINDEX,ref);lua_newtable(L);number(L,"zone_id",zone.zone);number(L,"record_id",zone.record);text(L,"name",zone.name);boolean(L,"is_player",zone.player);
  lua_call(L,1,0);return 0;
 }
 static int invokePower(lua_State* L){
  const int ref=int(lua_tointeger(L,1));const auto& cast=*static_cast<const PowerCast*>(lua_touserdata(L,2));
  static const char* const modes[]={"immediate","sustained","continuous","unknown"};
  lua_rawgeti(L,LUA_REGISTRYINDEX,ref);lua_newtable(L);number(L,"power",cast.power&0x3ffff);number(L,"number",cast.power&0xff);
  {const uint32_t s=cast.power&0xff00;lua_pushstring(L,s==0x300?"body":s==0x400?"energy":"light");}lua_setfield(L,-2,"school");lua_pushstring(L,modes[(cast.power>>16)&3]);lua_setfield(L,-2,"mode");
  lua_call(L,1,0);return 0;
 }
 static int invokeMessage(lua_State* L){
  const int ref=int(lua_tointeger(L,1));const auto& message=*static_cast<const GameMessage*>(lua_touserdata(L,2));
  lua_rawgeti(L,LUA_REGISTRYINDEX,ref);lua_newtable(L);text(L,"caption",message.caption);text(L,"text",message.text);number(L,"level",message.level);
  lua_call(L,1,0);return 0;
 }
 static int invokeHit(lua_State* L){
  const int ref=int(lua_tointeger(L,1));const auto& hit=*static_cast<const CombatHit*>(lua_touserdata(L,2));
  lua_rawgeti(L,LUA_REGISTRYINDEX,ref);lua_newtable(L);
  number(L,"record_id",hit.record);text(L,"name",hit.name.c_str());lua_pushboolean(L,hit.player);lua_setfield(L,-2,"is_player");
  if(hit.handle){lua_pushinteger(L,lua_Integer(hit.handle));lua_setfield(L,-2,"handle");}if(hit.attackerHandle){lua_pushinteger(L,lua_Integer(hit.attackerHandle));lua_setfield(L,-2,"attacker_handle");}
  lua_pushnumber(L,hit.health);lua_setfield(L,-2,"health_damage");lua_pushnumber(L,hit.balance);lua_setfield(L,-2,"balance_damage");
  lua_pushboolean(L,hit.downed);lua_setfield(L,-2,"downed");
  if(!hit.source.empty())text(L,"source",hit.source.c_str());
  if(hit.attacker){lua_newtable(L);number(L,"record_id",hit.attackerRecord);text(L,"name",hit.attackerName.c_str());lua_pushboolean(L,hit.attackerPlayer);lua_setfield(L,-2,"is_player");lua_setfield(L,-2,"attacker");}
  if(hit.weapon){number(L,"weapon_id",hit.weapon);boolean(L,"weapon_lit",hit.weaponLit);}
  if(hit.part>=0&&hit.part<18){number(L,"part",hit.part+1);text(L,"part_name",bodyParts[hit.part]);}
  boolean(L,"blocked",hit.blocked);boolean(L,"parried",hit.parried);
  if(hit.shield)number(L,"shield_id",hit.shield);if(hit.armour)number(L,"armour_id",hit.armour);if(hit.parryWeapon)number(L,"parry_weapon_id",hit.parryWeapon);
  lua_call(L,1,0);return 0;
 }
 static int invokeLifecycle(lua_State* L){
  const int ref=int(lua_tointeger(L,1));const char* phase=lua_tostring(L,2);const auto generation=lua_tointeger(L,3);
  lua_rawgeti(L,LUA_REGISTRYINDEX,ref);lua_newtable(L);
  lua_pushstring(L,phase);lua_setfield(L,-2,"phase");number(L,"generation",generation);lua_call(L,1,0);return 0;
 }
 void failure(Addon& a,const char* fallback){
  const char* detail=lua_type(a.state,-1)==LUA_TSTRING?lua_tostring(a.state,-1):nullptr;
  a.error=detail?detail:fallback;lastFailure=a.id+": "+a.error;++a.failures;
  // One line a modder can act on: where it failed, what Lua said (with its traceback), and how to recover.
  const std::string line=std::string(fallback)+": "+a.error+(a.loading?"":"\nThe addon is now disabled; fix it, return to the title menu and write its ID to reload.request.");
  report(a.id.c_str(),"error",line.c_str());
  a.staged.reset();a.aiChanges.clear();a.containerChanges.clear();a.inventoryChanges.clear();a.nativeCalls.clear();clearTimers(a);lua_settop(a.state,0);a.event=nullptr;a.pending=Action::none;
  // A disabled addon's rows and served members go with it; a failed load or reload never registered any.
  if(!a.loading&&a.enabled&&shared.forget)shared.forget(a.id);
  a.enabled=false;
 }
public:
 explicit Host(uint64_t capabilities=itemApiCapabilities):available(capabilities){}
 void setInventoryApi(InventoryProvider provider,InventoryCommit commit){inventoryProvider=provider;inventoryCommit=commit;}
 void setBagRead(BagRead read){bagRead=read;}
 void setInteractionCapture(InteractionCapture capture){interactionCapture=capture;}
 void setLevelRead(LevelRead read){levelRead=read;}
 void setNearbyRead(NearbyRead read){nearbyRead=read;}
 void setNativeCommit(NativeCommit commit){nativeCommit=commit;}
 void setCasterRead(CasterRead read){casterRead=read;}
 void setStatsRead(StatsRead read){statsRead=read;}
 void setDetailsProvider(CharacterProvider provider){detailsProvider=provider;}
 void setWorldItemCapture(ContainerCapture capture){worldItemCapture=capture;}
 void setWorldProvider(WorldProvider provider){worldProvider=provider;}
 void setTimeApi(TimeRead read,TimeWrite write){timeRead=read;timeWrite=write;}
 void setSaveApi(SaveGet get,SaveSet set){saveGet=get;saveSet=set;}
 void setFlagApi(FlagGet get,FlagSet set){flagGet=get;flagSet=set;}
 void setCharacterProvider(CharacterProvider provider){characterProvider=provider;}
 void setAwarenessProvider(CharacterProvider provider){awarenessProvider=provider;}
 void setAiCommit(AiCommit commit){aiCommit=commit;}
 void setAiCapture(AiCapture capture){aiCapture=capture;}
 void setContainerApi(ContainerCapture capture,ContainerRead read,ContainerCommit commit){containerCapture=capture;containerRead=read;containerCommit=commit;}
 void capabilities(uint64_t value){available=value;}
 bool setSettingsRoot(const std::filesystem::path& root){
  if(!settingsCache.empty())return false;auto files=std::make_unique<settings::FileStore>(root);if(!files->ready())return false;settingsFiles=std::move(files);return true;
 }
 unsigned flushSettings(size_t budget=1,bool force=false){
  if(!settingsFiles||(!force&&clock<nextSettingsFlush))return 0;nextSettingsFlush=clock+1000;unsigned written=0;
  auto iterator=settingsCache.upper_bound(settingsCursor);size_t visited=0;
  while(visited++<settingsCache.size()){
   if(iterator==settingsCache.end())iterator=settingsCache.begin();auto& item=*iterator++;auto& cache=item.second;
   if(!cache.dirty||cache.conflict||(!force&&clock<cache.retry))continue;if(!budget--)break;settingsCursor=item.first;
   settings::Revision revision;std::string error;const auto result=settingsFiles->write(item.first,cache.values,cache.revision,revision,error);
   if(result==settings::Written::saved){cache.revision=std::move(revision);cache.dirty=false;cache.error.clear();++written;}
   else{cache.conflict=result==settings::Written::conflict;cache.error=error;cache.retry=clock+5000;report(item.first.c_str(),"error",error.c_str());}
  }return written;
 }
 void setLogger(Logger value,void* context=nullptr){logger=value;loggerContext=context;}
 void report(const char* id,const char* level,const char* message)const noexcept{
  if(logger)try{logger(loggerContext,id,level,message);}catch(...){}
 }
 static bool validModuleName(const std::string& name){
  if(name.empty()||name.size()>128||name.front()=='.'||name.back()=='.')return false;
  char previous=0;for(unsigned char c:name){if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.')||(c=='.'&&previous=='.'))return false;previous=c;}return true;
 }
 const std::string& error()const{return lastFailure;}
 size_t size()const{return addons.size();}
 void clear(){if(shared.forget)for(const auto& a:addons)shared.forget(a->id);addons.clear();timerCursor=0;}
 bool unload(const std::string& id){
  for(auto& a:addons)if(a->id==id)cancelWorkers(*a);
  const auto end=std::remove_if(addons.begin(),addons.end(),[&](const auto& a){return a->id==id;});
  const bool found=end!=addons.end();addons.erase(end,addons.end());if(found&&shared.forget)shared.forget(id);return found;
 }
 void setSharedApi(const SharedApi& api){shared=api;}
 // A title row an addon added was activated: run its function on this (the presentation) thread.
 void notifyTitle(const std::string& owner,const std::string& label){
  for(auto& a:addons){if(a->id!=owner||!a->enabled)continue;
   for(const auto& h:a->titleHandlers){if(h.first!=label)continue;
    auto L=a->state;if(!lua_checkstack(L,8))return;
    startBudget(*a,"title.row");a->timerRunning=true;lua_rawgeti(L,LUA_REGISTRYINDEX,h.second);const int status=protectedCall(L,0,0);a->timerRunning=false;stopBudget(*a);
    if(status!=LUA_OK)failure(*a,"title row handler failed");else{commitAiChanges(*a);commitSettings(*a);lua_settop(L,0);}
    return;}
  }
 }
 bool load(const std::string& id,const char* source,size_t size,const std::string& required,size_t memory=8*1024*1024,const std::vector<Module>& modules={},const std::string& optional=""){
  if(id.empty()||id.size()>128||!source||size>1024*1024||memory<64*1024){lastFailure="invalid addon input";return false;}
  auto a=std::make_unique<Addon>();a->owner=this;a->id=id;a->limit=memory;
  size_t total=size;if(modules.size()>64){lastFailure="too many modules";return false;}
  for(const auto& module:modules){
   if(!validModuleName(module.name)||module.source.size()>1024*1024-total){lastFailure="invalid module or source bundle too large";return false;}
   for(const auto& prior:a->modules)if(prior.name==module.name){lastFailure="duplicate module: "+module.name;return false;}
   total+=module.source.size();a->modules.push_back(module);
  }
  a->importing.resize(modules.size());
  size_t begin=0;
  while(begin<required.size()){
   const auto end=required.find(',',begin);const auto name=required.substr(begin,end==std::string::npos?end:end-begin);
   const auto bit=capability(name);
   if(!bit){const auto closest=closestCapability(name);lastFailure="unknown capability in requires: '"+name+"'"+(closest.empty()?"":" (did you mean '"+closest+"'?)");return false;}
   if(!(available&bit)){lastFailure="missing capability: "+name+" (this runtime has not enabled it: its game functions did not match, or it is still waiting for the title menu; see the runtime lines above)";return false;}
   a->caps|=bit;
   if(end==std::string::npos)break;begin=end+1;
  }
  const uint64_t requiredCapabilities=a->caps;
  begin=0;while(begin<optional.size()){
   const auto end=optional.find(',',begin);const auto name=optional.substr(begin,end==std::string::npos?end:end-begin);
   const auto bit=capability(name);if(bit&&(available&bit))a->caps|=bit;
   // Optional names this runtime does not know: a likely typo (a close match) warns; a name no runtime capability is
   // close to is an addon listing a capability ahead of this runtime, logged quietly (info).
   if(!bit&&!name.empty()){const auto closest=closestCapability(name);report(id.c_str(),closest.empty()?"info":"warn",("unknown capability in optional: '"+name+"'"+(closest.empty()?"":" (did you mean '"+closest+"'?)")+"; it is ignored").c_str());}
   if(end==std::string::npos)break;begin=end+1;
  }
  if(a->caps&settingsStorage){
   std::string error;
   if(!settingsFiles)error="settings backend unavailable";
   else{
    auto found=settingsCache.find(id);
    if(found==settingsCache.end()){
     if(settingsCache.size()>=128)error="settings namespace limit exceeded";
     else{settings::Cache cache;if(settingsFiles->read(id,cache.values,cache.revision,error))found=settingsCache.emplace(id,std::move(cache)).first;}
    }
    if(error.empty())a->settingsData=&found->second;
   }
   if(!error.empty()){
    lastFailure=id+": "+error;if(requiredCapabilities&settingsStorage)return false;
    a->caps&=~settingsStorage;report(id.c_str(),"warn",error.c_str());
   }
  }
  a->state=lua_newstate(allocate,a.get());if(!a->state){lastFailure="Lua allocation failed";return false;}
  startBudget(*a,"load");lua_pushcfunction(a->state,initialize);
  if(protectedCall(a->state,0,0)!=LUA_OK){failure(*a,"initialization failed");return false;}
  const std::string chunk="@"+id;
  if(luaL_loadbufferx(a->state,source,size,chunk.c_str(),"t")!=LUA_OK||protectedCall(a->state,0,0)!=LUA_OK){failure(*a,"addon load failed");return false;}
  stopBudget(*a);a->loading=false;
  Addon* previous=nullptr;for(auto& current:addons)if(current->id==id)previous=current.get();
  if(!previous&&addons.size()>=128){lastFailure="too many addons";return false;}
  // Title rows: the replacement's set wins; the old state's served members end with it.
  if(previous){
   for(const auto& old:previous->titleHandlers){bool kept=false;for(const auto& now:a->titleHandlers)kept=kept||now.first==old.first;if(!kept&&shared.removeRow)shared.removeRow(id,old.first);}
   if(shared.clearServed)shared.clearServed(id);
  }
  if(!a->settingDefs.empty()&&(!shared.defineSettings||!shared.defineSettings(id,a->settingsTitle,a->settingDefs)))
   report(id.c_str(),"warn","its Mod Settings section was not added (the settings owner is unavailable); settings.get returns defaults");
  for(size_t i=0;i<a->titleHandlers.size();++i)if(!shared.addRow||!shared.addRow(id,a->titleHandlers[i].first,a->titleAfter[i],a->titleActsAs[i]))
   report(id.c_str(),"warn",("title row '"+a->titleHandlers[i].first+"' was not added (label taken, too many rows, or title rows unavailable)").c_str());
  for(auto& current:addons)if(current->id==id){commitSettings(*a);current=std::move(a);report(id.c_str(),"info","reloaded");return true;}
  addons.reserve(addons.size()+1);commitSettings(*a);addons.push_back(std::move(a));report(id.c_str(),"info","loaded");return true;
 }
 bool enabled(const std::string& id)const{for(const auto& a:addons)if(a->id==id)return a->enabled;return false;}
 void notifyPhase(const std::string& phase,uint64_t generation,const std::string& only=""){
  if(only.empty()&&phase=="menu"&&phaseName=="session")cancelAllWorkers();   // leaving a game: its jobs are moot
  if(only.empty())phaseName=phase;
  // A spend asked for in a session that ended is dropped with it.
  if(only.empty()&&phase!="session")for(auto& a:addons){a->spend=0;a->restore=0;a->teach.clear();a->wheel.clear();a->marks.clear();a->scales.clear();a->limbs.clear();a->hitMaterial=-1;a->body.clear();a->arcs.clear();a->keepDown.clear();}
  if(only.empty())for(auto& a:addons)clearTimers(*a);
  // Placed lights belong to the level that was playing; leaving the session forgets them (follow lights stay).
  if(only.empty()&&phase!="session")for(auto& a:addons)a->lights.erase(std::remove_if(a->lights.begin(),a->lights.end(),[](const LightSpec& l){return l.placed;}),a->lights.end());
  for(auto& a:addons){if(!a->enabled||(!only.empty()&&a->id!=only))continue;
   for(const auto& handler:a->handlers){if(handler.kind!=Kind::lifecycle)continue;
    auto L=a->state;if(!lua_checkstack(L,8)){a->enabled=false;break;}
    startBudget(*a,"runtime.phase");lua_pushcfunction(L,invokeLifecycle);lua_pushinteger(L,handler.ref);
    lua_pushlstring(L,phase.data(),phase.size());lua_pushinteger(L,static_cast<lua_Integer>(generation));
    const int status=protectedCall(L,3,0);stopBudget(*a);
    if(status!=LUA_OK){failure(*a,"runtime.phase handler failed");break;}commitSettings(*a);lua_settop(L,0);
   }
  }
 }
 void setTime(uint64_t now){if(now>clock)clock=now;}
 // Each pulse, every task of every addon goes on for one budget (see runTask).
 void runTasks(){
  for(auto& owned:addons){auto& a=*owned;if(!a.enabled||a.tasks.empty()||!canTime(a))continue;
   for(size_t i=0;i<a.tasks.size()&&a.enabled;){
    auto L=a.state;if(!lua_checkstack(L,4))break;lua_rawgeti(L,LUA_REGISTRYINDEX,a.tasks[i]);lua_State* co=lua_tothread(L,-1);lua_pop(L,1);
    if(!co){luaL_unref(L,LUA_REGISTRYINDEX,a.tasks[i]);a.tasks.erase(a.tasks.begin()+std::ptrdiff_t(i));a.taskBudgets.erase(a.taskBudgets.begin()+std::ptrdiff_t(i));continue;}
    const int perPulse=i<a.taskBudgets.size()?a.taskBudgets[i]:taskBudget;
    startBudget(a,"task");a.instructions=perPulse;a.budgetGiven=perPulse;lua_sethook(co,budget,LUA_MASKCOUNT,1000);a.timerRunning=true;a.taskRunning=true;
    int results=0;const int status=lua_resume(co,L,0,&results);
    a.taskRunning=false;a.timerRunning=false;lua_sethook(co,nullptr,0,0);stopBudget(a);
    if(status==LUA_YIELD){lua_pop(co,results);commitAiChanges(a);commitSettings(a);++i;continue;}
    if(status!=LUA_OK){luaL_traceback(L,co,lua_tostring(co,-1),0);}
    luaL_unref(L,LUA_REGISTRYINDEX,a.tasks[i]);a.tasks.erase(a.tasks.begin()+std::ptrdiff_t(i));if(i<a.taskBudgets.size())a.taskBudgets.erase(a.taskBudgets.begin()+std::ptrdiff_t(i));
    if(status!=LUA_OK){failure(a,"task failed");lua_settop(L,0);break;}
    commitAiChanges(a);commitSettings(a);lua_settop(L,0);
   }
  }
 }
 void runTimers(){
  runTasks();deliverMessages();deliverWorkers();
  if(addons.empty())return;
  const size_t count=addons.size(),start=timerCursor%count;size_t callbacks=0;
  for(size_t round=0;round<4&&callbacks<16;++round)for(size_t offset=0;offset<count&&callbacks<16;++offset){
   const size_t index=(start+offset)%count;auto& a=*addons[index];
   if(!a.enabled||!canTime(a))continue;Timer timer;
   if(!a.timers.take(clock,timer))continue;
   auto L=a.state;
   if(!lua_checkstack(L,8)){if(!timer.interval)luaL_unref(L,LUA_REGISTRYINDEX,timer.reference);clearTimers(a);a.enabled=false;report(a.id.c_str(),"error","Lua timer stack exhausted");continue;}
   lua_rawgeti(L,LUA_REGISTRYINDEX,timer.reference);
   if(!timer.interval)luaL_unref(L,LUA_REGISTRYINDEX,timer.reference);
   startBudget(a,"timer");a.timerRunning=true;const int status=protectedCall(L,0,0);a.timerRunning=false;stopBudget(a);
   ++callbacks;timerCursor=(index+1)%count;
   if(status!=LUA_OK)failure(a,"timer callback failed");else{commitAiChanges(a);commitSettings(a);lua_settop(L,0);}
  }
 }
 bool handles(const std::string& model)const{
  for(const auto& a:addons)if(a->enabled)for(const auto& h:a->handlers)if(h.kind==Kind::item&&h.model==model)return true;
  return false;
 }
 bool listens(Kind kind)const{
  for(const auto& a:addons)if(a->enabled)for(const auto& h:a->handlers)if(h.kind==kind)return true;
  return false;
 }
 bool handlesCombat()const{return listens(Kind::combat);}
 bool handlesScriptEvents()const{return listens(Kind::script);}
 bool handlesZones()const{return listens(Kind::zone);}
 bool handlesMessages()const{return listens(Kind::message);}
 // Each queued event runs every matching handler in its own protected call; an error disables only that addon.
 template<class T>void notify(Kind kind,const std::vector<T>& items,lua_CFunction invoker,const char* where){
  for(auto& a:addons){if(!a->enabled)continue;
   for(const auto& handler:a->handlers){if(handler.kind!=kind)continue;
    auto L=a->state;bool failed=false;
    for(const auto& item:items){
     if(!lua_checkstack(L,8)){a->enabled=false;failed=true;break;}
     startBudget(*a,where);lua_pushcfunction(L,invoker);lua_pushinteger(L,handler.ref);lua_pushlightuserdata(L,const_cast<T*>(&item));
     const int status=protectedCall(L,2,0);stopBudget(*a);
     if(status!=LUA_OK){failure(*a,where);failed=true;break;}commitSettings(*a);lua_settop(L,0);
    }
    if(failed||!a->enabled)break;
   }
  }
 }
 void notifyScriptEvents(const std::vector<ScriptEvent>& events){notify(Kind::script,events,invokeScript,"script.event handler failed");}
 void notifyHits(const std::vector<CombatHit>& hits){notify(Kind::combat,hits,invokeHit,"character.hit handler failed");}
 void notifyZones(const std::vector<ZoneEntry>& zones){notify(Kind::zone,zones,invokeZone,"zone.enter handler failed");}
 void notifyMessages(const std::vector<GameMessage>& messages){notify(Kind::message,messages,invokeMessage,"game.message handler failed");}
 // settings.changed: the player applied new Mod Settings values; only the owning addon hears its own keys.
 void notifySettings(const std::string& owner,const std::vector<SettingChange>& changes){
  for(auto& a:addons){if(!a->enabled||a->id!=owner)continue;
   for(const auto& handler:a->handlers){if(handler.kind!=Kind::setting)continue;
    auto L=a->state;bool failed=false;
    for(const auto& c:changes){if(!lua_checkstack(L,8)){a->enabled=false;failed=true;break;}
     startBudget(*a,"settings.changed");a->timerRunning=true;lua_pushcfunction(L,invokeSetting);lua_pushinteger(L,handler.ref);lua_pushlightuserdata(L,const_cast<SettingChange*>(&c));
     const int status=protectedCall(L,2,0);a->timerRunning=false;stopBudget(*a);
     if(status!=LUA_OK){failure(*a,"settings.changed handler failed");failed=true;break;}commitAiChanges(*a);commitSettings(*a);lua_settop(L,0);}
    if(failed||!a->enabled)break;
   }
  }
 }
 bool handlesIdChanges()const{return listens(Kind::idChange);}
 void notifyIdChanges(const std::vector<IdChange>& changes){notify(Kind::idChange,changes,invokeIdChange,"item.id_changed handler failed");}
 bool handlesCasts()const{return listens(Kind::power);}
 void notifyCasts(const std::vector<PowerCast>& casts){notify(Kind::power,casts,invokePower,"power.cast handler failed");}
 // What the enabled addons currently ask for; disabled or unloaded addons drop out on their own.
 std::vector<OwnedLight> desiredLights()const{
  std::vector<OwnedLight> out;
  for(const auto& a:addons)if(a->enabled&&(a->caps&worldLights)&&(available&worldLights))for(const auto& light:a->lights)out.push_back({a->id,light});
  return out;
 }
 int desiredBindings()const{
  int total=0;for(const auto& a:addons)if(a->enabled&&(a->caps&powerCast)&&(available&powerCast))total+=a->bindings;return (std::min)(total,4);
 }
 uint32_t openSchools()const{uint32_t m=0;for(const auto& a:addons)if(a->enabled&&(a->caps&powerCast)&&(available&powerCast))m|=a->schools;return m;}
 // Every enabled addon's marks, for the pulse to resolve against the live roster.
 void collectMarks(std::vector<Mark>& out)const{out.clear();for(const auto& a:addons)if(a->enabled&&(a->caps&characterMarks)&&(available&characterMarks))out.insert(out.end(),a->marks.begin(),a->marks.end());}
 void collectDamageScales(std::vector<DamageScale>& out)const{out.clear();for(const auto& a:addons)if(a->enabled&&(a->caps&damageScale)&&(available&damageScale))out.insert(out.end(),a->scales.begin(),a->scales.end());}
 void collectLimbs(std::vector<LimbStrength>& out)const{out.clear();for(const auto& a:addons)if(a->enabled&&(a->caps&limbStrength)&&(available&limbStrength))out.insert(out.end(),a->limbs.begin(),a->limbs.end());}
 void collectMotions(std::vector<MotionOrder>& out){out.clear();for(auto& a:addons){if(a->enabled)out.insert(out.end(),a->motions.begin(),a->motions.end());a->motions.clear();}}
 void collectKeepDown(std::vector<KeepDown>& out)const{out.clear();for(const auto& a:addons)if(a->enabled&&(a->caps&charactersDamage)&&(available&charactersDamage))out.insert(out.end(),a->keepDown.begin(),a->keepDown.end());}
 void collectSevers(std::vector<SeverOrder>& out){out.clear();for(auto& a:addons){if(a->enabled)out.insert(out.end(),a->severs.begin(),a->severs.end());a->severs.clear();}}
 void collectDrops(std::vector<DropOrder>& out){out.clear();for(auto& a:addons){if(a->enabled)out.insert(out.end(),a->drops.begin(),a->drops.end());a->drops.clear();}}
 void collectFires(std::vector<FireOrder>& out){out.clear();for(auto& a:addons){if(a->enabled)out.insert(out.end(),a->fireOrders.begin(),a->fireOrders.end());a->fireOrders.clear();}}
 void collectBody(std::vector<BodyScale>& out)const{out.clear();for(const auto& a:addons)if(a->enabled&&(a->caps&bodyScale)&&(available&bodyScale))out.insert(out.end(),a->body.begin(),a->body.end());}
 bool arcsWanted()const{for(const auto& a:addons)if(!a->arcs.empty())return true;return false;}
 bool takeArc(std::array<float,4>& out){for(auto& a:addons){if(a->arcs.empty())continue;const bool ok=a->enabled&&(a->caps&effectsArc)&&(available&effectsArc);out=a->arcs.front();a->arcs.erase(a->arcs.begin());if(ok)return true;}return false;}
 int hitMaterial()const{for(const auto& a:addons)if(a->enabled&&(a->caps&hitSound)&&(available&hitSound)&&a->hitMaterial>=0)return a->hitMaterial;return -1;}
 bool wheelWanted()const{for(const auto& a:addons)if(!a->wheel.empty())return true;return false;}
 size_t takeWheel(uint32_t* out,size_t room){
  size_t n=0;for(auto& a:addons){if(a->enabled&&(a->caps&powerCast)&&(available&powerCast))for(uint32_t w:a->wheel)if(n<room)out[n++]=w;a->wheel.clear();}
  return n;
 }
 bool teachWanted()const{for(const auto& a:addons)if(!a->teach.empty())return true;return false;}
 // Powers the addons asked to teach since the last call (taken once), at most `room`.
 size_t takeTeach(uint32_t* out,size_t room){
  size_t n=0;for(auto& a:addons){if(a->enabled&&(a->caps&powerCast)&&(available&powerCast))for(uint32_t p:a->teach){bool dup=false;for(size_t i=0;i<n;++i)dup|=out[i]==p;if(!dup&&n<room)out[n++]=p;}a->teach.clear();}
  return n;
 }
 bool spendWanted()const{for(const auto& a:addons)if(a->spend>0||a->restore>0)return true;return false;}
 float takeRestore(){
  lua_Number total=0;for(auto& a:addons){if(a->enabled&&(a->caps&focusHold)&&(available&focusHold))total+=a->restore;a->restore=0;}
  return float((std::min)(total,lua_Number(0.5)));
 }
 // Focus the addons asked to spend since the last call (taken once); at most 0.5 in one pulse.
 float takeSpend(){
  lua_Number total=0;for(auto& a:addons){if(a->enabled&&(a->caps&focusHold)&&(available&focusHold))total+=a->spend;a->spend=0;}
  return float((std::min)(total,lua_Number(0.5)));
 }
 float desiredFocus()const{
  lua_Number total=0;for(const auto& a:addons)if(a->enabled&&(a->caps&focusHold)&&(available&focusHold))total+=a->focus;
  return float((std::min)(total,lua_Number(0.5)));
 }
 bool handlesGear()const{return listens(Kind::gear);}
 // gear.use runs synchronously, in the game's own click handler, like item.use: handlers in order until one
 // returns true; that one's restore request (if any) is returned for the caller to apply.
 GearResult dispatchGear(const GearUse& event){
  for(auto& a:addons){if(!a->enabled)continue;
   for(const auto& handler:a->handlers){if(handler.kind!=Kind::gear)continue;
    auto L=a->state;if(!lua_checkstack(L,8)){a->enabled=false;break;}
    a->gear=&event;a->token=++token;a->gearPending={};startBudget(*a,"gear.use");
    lua_pushcfunction(L,invokeGear);lua_pushinteger(L,handler.ref);
    const int status=protectedCall(L,1,1);stopBudget(*a);a->gear=nullptr;
    if(status!=LUA_OK){a->gearPending={};failure(*a,"gear.use handler failed");break;}
    commitSettings(*a);GearResult result=a->gearPending;result.handled=lua_toboolean(L,-1)!=0;lua_settop(L,0);a->gearPending={};
    if(result.handled)return result;
   }
  }
  return {};
 }
 Result dispatch(const Event& event){
  for(auto& a:addons){if(!a->enabled)continue;
   for(const auto& handler:a->handlers){if(handler.kind!=Kind::item||handler.model!=event.model)continue;
    auto L=a->state;
    if(!lua_checkstack(L,8)){lastFailure=a->id+": Lua stack exhausted";a->enabled=false;break;}
    a->event=&event;a->token=++token;a->pending=Action::none;a->itemRestore={};a->consumePending=false;startBudget(*a,"item.use");
    lua_pushcfunction(L,invoke);lua_pushinteger(L,handler.ref);
    const int status=protectedCall(L,1,1);stopBudget(*a);a->event=nullptr;
    if(status!=LUA_OK){a->itemRestore={};a->consumePending=false;failure(*a,"item.use handler failed");break;}
    commitSettings(*a);Result result{lua_toboolean(L,-1)!=0,a->pending};
    result.restore=a->itemRestore.restore;result.wear=a->itemRestore.wear;result.dirt=a->itemRestore.dirt;result.consume=a->consumePending;
    lua_settop(L,0);a->pending=Action::none;a->itemRestore={};a->consumePending=false;
    if(result.handled)return result;
   }
  }
  return {};
 }
};
}
