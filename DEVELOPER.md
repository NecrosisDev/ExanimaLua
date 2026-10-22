# Start here: Exanima Lua developer candidate

This package contains runtime 0.28.0, Lua API 1 (revision 28) and an EMTK
framework update. New to it? Read GETTING-STARTED.md first, then the Reference
section of API.md. It needs no keyring, ExaniMate service or ExCamera (with
ExCamera installed, the Mod Settings tab needs an ExCamera build that hosts it).
Owned-process tests passed; actual game startup, gameplay and coexistence are
still unverified.

## Install once

1. Close Exanima. Back up the framework DLL beside your existing EMTK launcher.
2. If your framework lacks this script transport, copy the supplied
   EMTK-framework-update/emtk_framework.dll beside that launcher. Read the
   accompanying EMTK-STARTUP.md for source and rollback details.
3. Copy the package's mods/com.necrosis.script-runtime into your EMTK mods
   directory. Enable Exanima Lua Runtime in the profile.
4. Copy its examples/com.example.hello-lua directory into that same mods
   directory as a sibling addon. Enable Hello Lua in the profile.
5. Launch normally through EMTK. There is no additional addon executable.

Do not replace a DLL while the game is running. To roll back, close the game,
disable these profile entries and restore the backed-up framework if replaced.
Keep unrelated profile entries and their files intact.

## First edit and reload

Inventory Tools demonstrates player transfers and apparel changes. Its config
defaults to disabled/zero IDs. Choose one action and an existing item ID;
take/store also need a closed level-local container record. Enable only in a
disposable session, close native windows, stop dragging and leave combat. It
tries once after settling; check the commit log before judging success. Item
state is preserved and equip conflicts return displaced clothing to inventory.
Choose `switch` to request the native alternate set, `pickup` with a world item
record, or `drop` with a carried raw item ID and a nearby player-sector offset.
Switch acceptance is not animation completion; drop acceptance is not settling.
Choose `equip_weapon` with `hand = "right"` or `"left"`; left requires native
dual wield and an occupied one-handed primary. `bag_store` / `bag_take` use raw
`bag_item_id` and `item_id`. Shared bags receive private IDs on first edit;
inspect fresh inventory for the new ID. `unequip_weapon` targets an active
`hand`; `equip_shield` selects a carried item; `bag_transfer` uses source
`bag_item_id`, `destination_bag_item_id` and the contents `item_id`. Direct bag
transfers do not use spare player space. The engine can promote an offhand
weapon when the primary is removed. Inspect fresh slots after completion.
Choose `use` for a carried salve/elixir; `use_item` for a carried key and nearby
door `world_item_record`; `world_use` to open an unlocked container or press a
manual button; `set_pressed` plus `pressed = false` releases that button.
These reuse native behavior. Button acceptance does not prove linked effects,
and window acceptance does not prove UI rendering. Keys remain carried.
Choose `set_position` plus integer `position = 0` or `1` for a nearby ordinary
lever, or `world_use` to toggle it. `set_open` plus boolean `open` requests an
unlocked door's movement, including supported sliding/mechanical and paired
doors. `door_kind` and `door_leaves` describe eligible snapshots. Subtype-6
light-control levers use the same endpoint/toggle actions and expose
`light_control`. Native physics and links retain ownership; these calls do not
establish completion or bypass locks. Held/timed levers, malformed constraints
and broken/locked pairs refuse. Paired writes can fail after one leaf changes.
Independent
`world.levers` and `world.doors` capabilities can be optional in real addons.
`world_use` also activates supported teleporters/scanners using independent
`world.teleporters` / `world.devices` capabilities. An eligible crystal must
actually be installed at the native socket. Teleporter use opens the game's
destination interface; the player chooses there. Scanner use starts its native
effect. Staged gameplay mutations pause while the native interaction is active,
even after script reload/disable, then resume when close/removal is observed.
Device IDs may change after native instancing; inspect fresh snapshots. Verify
travel, charge consumption and scanner results in a disposable session with
backups; offline activation checks do not establish those results.
`set_active` plus Boolean `active` enables/disables eligible private-record
electrical emitters through `world.devices`; `world_use` toggles them. Native
queued execution owns creation and retirement. Pending requests pause shared
staged mutations across addon reload/disable, and stale scene/ownership/reach
cancels before writing. Actual visual/damage/save behavior remains unverified.
`world_use` also ignites eligible native fire sources through `world.devices`.
It uses the source's configured size on the same native queue, with duplicate-fire
suppression across Lua reload. The native game owns effects and lifetime; this
does not add torch ignition, arbitrary fire placement or extinguish controls.
Other special devices/torches, physical bag
pickup/drop and nested-bag transfers remain unsupported. Native physics is used;
no cursor/surface/occlusion test is promised.
Check inventory ownership, clothing appearance and save/reload with backups.
Read API.md for refusal conditions and partial-native-fault handling.

Reward Chest demonstrates container mutations. Its config module defaults to
disabled with zero IDs; set an intended level-local container record and objdb
item ID in scripts/config.lua, then enable the example in a disposable session.
After settling, it checks the closed container's current contents and stages a
missing reward. Check the commit log, open the chest and verify the item. Repeat
after taking loot and save/reload only with backups: grants can persist, and the
example is not campaign-once reward tracking. Read API.md before adapting it.

Encounter Truce is an opt-in attitude-editing example. Copy its example folder
into mods and enable it only in a disposable session. It lowers hostility and
concern on up to eight NPCs with existing player memories once the session
settles. Check commit logs and compare awareness/behavior afterward. The game
can raise these values again; this is not a permanent peace or companion API.

Practice AI is an opt-in behavior-changing example. Copy its example directory
into mods and enable it only for a disposable session. After the session settles
it stages skill/personality changes on up to eight living NPCs, then logs the
commit result. Read API.md's NPC mutation contract before adapting it. Disable
does not undo already applied live edits; restarting/reloading restores the
game's normal reconstruction path. Do not treat an offline test as acceptance
for your saves or gameplay.

For NPC awareness, copy examples/com.example.awareness-observer into the mods
directory and enable Awareness Observer. It logs encounter-state counts from
verified NPC brains. Test with a disposable session and compare observations
against NPC behavior. It does not control AI or add an overlay.

For live character observation, copy examples/com.example.character-observer
into the mods directory and enable Character Observer. During a disposable
session it logs a character census and known player-health ratio every two
seconds. This exercises characters.read independently of world.read and the
keyring. It observes state; it does not change actors or draw an overlay.

For scene inspection, copy examples/com.example.world-survey into the mods
directory and enable World Survey. Enter a disposable session and inspect its
tagged item/object/light counts. It requires no keyring addon.

The example declares scripts/main.lua and its util.format module in
manifest.toml. Copy the example under your own unique addon ID to start a mod.
Declare only capabilities you need; use optional capabilities plus
api.has_capability when an enhancement can be absent. See API.md for contracts.

Edit the enabled addon's scripts, return to the title menu and write its ID
(for example com.example.hello-lua) into reload.request beside ScriptRuntime.dll.
The runtime consumes requests at the settled title menu. Use all to reload all
selected script addons. Successful reload replaces Lua state and cancels old
timers; failure preserves the running version. Changed assets, native DLLs and
profile selection require a normal restart. A loader acceptance message alone
does not prove the script executed: look for the example's tagged loaded log.

Armour Check (disabled by default) logs what your worn armour stops on chosen
body parts, from `items.stats`; its `layers` module is the game's two-row
stacking rule in Lua. Zone Journal logs the first time you enter each zone and,
with `storage.save`, remembers it in the save.

## Diagnose failures

In game, open the console (bind its key in Settings > Mod Settings, or set
`[console] key` in ScriptRuntime.ini): warnings in amber, errors in red. The same
lines go to a JSON log for tools (API.md "In-game console and the agent log").
Read ScriptRuntime.log beside ScriptRuntime.dll and the EMTK log. Lines from
`game` are the game's own error messages, including dialogue and role script
errors; lines from `script` are LuaLog output and refused LuaEvent/LuaFlag
calls. Logs identify
the addon and include Lua tracebacks. Check both profile entries are enabled,
module names are declared and paths remain inside the addon folder. A required
unavailable capability prevents execution; optional unavailable capabilities
are withheld. Native item operations can wait for safe menu initialization.

Preferences live under LOCALAPPDATA/ExanimaLua/settings, isolated by addon.
They are shared across campaigns. storage.status reports pending, saved, error
or conflict; a successful set is not a disk-flush receipt. Preserve corrupt or
conflicting files when investigating. Never use these preferences for campaign
progress. Timers use presentation wall time and may run while gameplay pauses.

## Game acceptance checklist (not yet performed)

- Verify the example's loaded message after normal EMTK startup; disable the
  example and restart to confirm it stays unloaded.
- Enter a session and return to the menu. Inspect phase and deferred callback
  logs; phase observations do not guarantee that the world is ready.
- Change a log message and request reload at the menu. Introduce a Lua syntax
  error and verify the prior version survives; fix it and reload again.
- Change an addon preference and confirm saved status before exiting; restart
  and read it back. Test only backed-up/disposable saves for gameplay addons.
- Test alongside your actual other mods, including ExCamera and ExaniMate
  when used. Record versions, enabled profile, logs and observed behavior.
- Open Settings: the Mod Settings tab shows each addon's section; change a
  value, Apply, restart and confirm the profile kept it.
- Check the log for `[script.records] merged into objdb.rdb`: it proves the item
  merge ran before the game read the database.

Offline package hashes and fixture tests do not complete this checklist.

