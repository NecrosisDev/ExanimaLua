# Exanima Lua API

This is the reference for runtime 0.28.0: Lua API generation 1, revision 28.
It runs on Exanima 0.9.5.2g (Steam build 25803178, Windows x64) and its Hellmode
build. The Reference section lists everything an addon can call; the sections
after it are the detailed contracts, newest release first.

**Evidence.** Every call below is checked in owned-process tests that run real
Lua against the real bridges and fake game memory, and each game operation has a
static safety review. **None of it has been run in the
game yet.** Treat a field as verified in play only after you have seen it there.

## Reference

### An addon

An addon is an EMTK mod folder (EMTK is what installs and loads it) with a
`manifest.toml` and Lua source. The runtime itself is the mod
`com.necrosis.script-runtime`; enable both in the profile.

```toml
[plugin]
name = "My Addon"
version = "1.0.0"

[script]
runtime = "com.necrosis.script-runtime"
entry = "scripts/main.lua"
requires = ["events.lifecycle", "timers"]      # load fails without these
optional = ["world.read", "items.stats"]       # granted when available
memory_mb = 8                                  # optional: Lua heap cap in MiB, 8..64 (default 8)

[script.modules]
config = "scripts/config.lua"                   # require("config")
```

To start one, the editor's scaffold writes this folder for you:
`python -m exanimate addon-new <folder> <id> "Name" [--optional ...] [--settings]`
(see the editor's own docs for its options).

Each addon gets its own Lua 5.4 state with `base`, `table`, `string`, `math`
and `utf8` only: no `io`, `os`, `package`, `debug`, `coroutine`, `load`,
`loadfile`, `dofile` or `collectgarbage`, and no metatables (`setmetatable`
and `getmetatable` are removed; `rawget`/`rawset` remain), so use plain tables
and closures rather than `__index` classes. Limits per state: 8 MiB of memory, 1 MiB of
source in at most 64 modules, about 100,000 instructions and 32 log lines per
callback.

### Rules that apply everywhere

- **Register during load.** `events.on` works only at the top level of the
  entry script (or a module it requires while loading).
- **Read the world from timers.** World, character, inventory and item calls
  answer inside timer and event callbacks during a session; during loading or
  at the menu they return nil. A `runtime.phase` "session" handler that starts
  a `timers.after(0, ...)` is the usual way in.
- **Writes are staged.** A write method returns true when it was accepted for
  this callback. It is applied after the callback returns without error, and
  only one kind of write runs per callback. True never means the game has
  finished the action; check the next snapshot.
- **Copies, not handles.** Every table is a copy. Methods on a row (such as
  `:set_ai`) expire when the callback that produced the row returns.
- **Errors disable one addon.** A Lua error in any callback disables that
  addon only and is logged with its traceback. Fix the source, return to the
  title menu and write the addon ID to `reload.request` beside
  `ScriptRuntime.dll`.
- **Unknown stays unknown.** A field the runtime could not read is nil, never
  a guessed 0 or false.

### Versions

| Field | Meaning |
|---|---|
| `api.version` | Lua API generation, 1. It changes only for a breaking change. |
| `api.revision` | Additive revision: 24 for runtime 0.24.0. Test `api.revision >= 24` before using something added in 0.24.0. |
| `api.runtime` | Runtime version string, `"0.24.0"`. |
| `api.addon` | This addon's ID. |

A call that is missing from an older runtime is nil, so
`if items and items.stats then ... end` also works.

### Capabilities

A capability is a permission named in `requires` or `optional`. The runtime
grants it only when the game functions behind it match on the running
executable. A misspelled name in `requires` fails the load with a "did you
mean" hint; in `optional` it is logged and ignored.

| Capability | Gives | Kind | Since |
|---|---|---|---|
| `events.lifecycle` | `events.on("runtime.phase", fn)` | event | 0.3.0 |
| `timers` | `timers.after/every/cancel/now` | core | 0.4.0 |
| `storage.settings` | `storage.get/set/status`: per-user preferences | file | 0.5.0 |
| `storage.save` | `save_data.get/set`: 4 KB kept inside the player's save | save | 0.23.0 |
| `world.read` | `world.snapshot()`, `world.time()`, `world.level()`, `world.nearby()` | read | 0.6.0 (`level` 0.24.0, `nearby` 0.25.0) |
| `world.time` | `world.set_time(seconds)` | write | 0.23.0 |
| `characters.read` | `characters.snapshot([{details = true}])` | read | 0.7.0 (details 0.24.0) |
| `characters.awareness` | `characters.awareness()`: AI state per NPC | read | 0.8.0 |
| `characters.powers` | `characters.grant_powers(record_id, {power ids})` | write | 0.28.0 |
| `characters.damage` | `character:damage{health, balance, can_fall}` on `characters.read()` entries | write | 0.28.0 |
| `characters.damage_scale` | `characters.set_damage_scale(target, {cut, pierce, crush, impact})` | write | 0.28.0 |
| `characters.hit_sound` | `characters.set_hit_sound("player", material)` | write | 0.28.0 |
| `characters.limb_strength` | `characters.set_limb_strength(target, part, factor)` | write | 0.28.0 |
| `characters.body` | `characters.set_strength(target, f)`, `characters.set_anim_rate(target, f)` | write | 0.28.0 |
| `effects.arc` | `effects.spawn_arc{from = "hand", strands, segments, life, length}` | write | 0.28.0 |
| `characters.marks` | `characters.set_mark(target, name, value)`: a value other mods read with `character_mark_nodes` | write | 0.28.0 |
| `characters.ai.write` | `row:set_ai{skill, attribute2, attribute3}`, `row:boost_ai{skill, attribute2, attribute3, seconds}` (0.28.0: attribute2 and attribute3 may be left out to keep the NPC's own; a set_ai that restores the saved fields exactly when it lapses, on a phase change, death, leaving or unload; later calls only renew it; a set_ai ends it) | write | 0.9.0 |
| `characters.attitude.write` | `row:set_attitude{hostility, concern}` | write | 0.10.0 |
| `items.stats` | `items.stats(id)`: weapon and armour numbers | read | 0.24.0 |
| `inventory.read` | `inventory.snapshot()`: the player's carried, worn and held items | read | 0.12.0 |
| `inventory.transfer` | `row:store_in(chest)`, `chest:take_item(i)` | write | 0.12.0 |
| `equipment.write` | `row:equip()`, `row:unequip()` for apparel | write | 0.12.0 |
| `weapons.switch` | `player:switch_weapons()` | write | 0.13.0 |
| `weapons.equip` | `row:equip_weapon("right" or "left")` | write | 0.14.0 |
| `weapons.unequip` | `weapon:unequip()` | write | 0.15.0 |
| `shields.equip` | `row:equip_shield()` | write | 0.15.0 |
| `inventory.bags` | `bag:contents()`, `bag:take_item(i)`, `row:store_in(bag)`, `bag:transfer_to(i, bag)` | write | 0.14.0 |
| `world.pickup` | `object:pickup()` | write | 0.13.0 |
| `world.drop` | `row:drop(position)` | write | 0.13.0 |
| `world.spawn` | `player:spawn_item(id, position)` | write | 0.23.0 |
| `containers.edit` | `chest:contents()`, `chest:add_item(id)`, `chest:remove_item(id)` | write | 0.11.0 |
| `items.use` | `row:use()` for salves/elixirs, `door:use_item(key_row)` | write | 0.16.0 |
| `world.use` | `object:use()` for containers and buttons, `button:set_pressed(b)` | write | 0.16.0 |
| `world.levers` | `lever:set_position(0 or 1)`, `lever:use()` | write | 0.17.0 |
| `world.doors` | `door:set_open(b)`, door `command`/`moving` progress | write | 0.17.0 |
| `world.teleporters` | `teleporter:use()` opens the destination screen | write | 0.19.0 |
| `world.devices` | scanners, electrical emitters (`set_active`), fires and static torches (`use`, `ignite`, `extinguish`) | write | 0.19.0 |
| `characters.drop_held` | `characters.drop_held(target, side)` (another name for `characters.damage`: either grants both) | write | 0.28.0 |
| `characters.sever` | `characters.sever(target, part)` (another name for `characters.damage`: either grants both) | write | 0.28.0 |
| `characters.keep_down` | `characters.keep_down(target, on)` (another name for `characters.damage`: either grants both) | write | 0.28.0 |
| `characters.motion` | `characters.play_motion(target, motion)` (another name for `characters.damage`: either grants both) | write | 0.28.0 |
| `world.fire` | `world.light_fire`, `world.put_out_fire` (another name for `world.devices`: either grants both) | write | 0.28.0 |
| `effects.fire` | `effects.attach_fire`, `effects.remove_fire` (another name for `effects.arc`: either grants both) | write | 0.28.0 |
| `events.combat` | `events.on("character.hit", fn)` | event | 0.23.0 |
| `events.script` | `events.on("script.event", fn)` for `LuaEvent(...)` | event | 0.23.0 |
| `script.flags` | `script_flags.get/set` for `LuaFlag(...)` | core | 0.23.0 |
| `events.zone` | `events.on("zone.enter", fn)` | event | 0.24.0 |
| `events.messages` | `events.on("game.message", fn)` | event | 0.24.0 |
| `title.rows` | `title.add_row/remove_row/set_enabled`: rows on the title menu | menu | 0.25.0 |
| `player.damage` | `player.damage(balance, health, can_fall)` | write | 0.26.0 |
| `player.push` | `player.push(dx, dy, dz, amount)` | write | 0.26.0 |
| `levels.drive` | `levels.drive(record_id, state)`: open/close/toggle a door or drive a device record | write | 0.26.0 |
| `powers.cast` | `events.on("power.cast", fn)` for Light-school powers, `powers.caster()` | event | 0.26.0 |
| `powers.focus` | `powers.hold_focus(amount)`, `powers.held_focus()`, `powers.spend_focus(amount)` (0.28.0) | write | 0.26.0 |
| `world.lights` | `lights.set(name, spec)`, `lights.remove(name)`, `lights.list()` | write | 0.26.0 |
| `items.wear` | `items.add_wear(id, {wear, dirt})` | write | 0.28.0 |
| `ui.sound` | `ui.sound(name[, volume])`: one of the game's own sounds at the player | screen | 0.28.0 |
| `ui.settings` | `settings.define`, `settings.get`, `events.on("settings.changed", fn)`: your section of Settings > Mod Settings | settings | 0.27.0 |
| `events.gear_use` | `events.on("gear.use", fn)`: double-click a carried weapon or armour piece, then click it on something | event | 0.26.0 |
| `items.condition` | `event:restore_condition{wear, dirt}` in `gear.use` | write | 0.26.0 |
| `levels.flow` | `levels.new_game(member)`, `levels.travel(member, entry)` | write | 0.26.0 |
| `player.place` | `player.place(x, y, z)` | write | 0.26.0 |
| `ui.notify` | `ui.notify(text)`: a short message on screen that fades | screen | 0.26.0 |
| `assets.read` | `assets.read(path)`: a file from the addon's own folder | files | 0.26.0 |
| `levels.serve` | `levels.serve/clear_served`: the game reads your bytes for a package member | files | 0.25.0 |
| `levels.read` | `levels.read`: the installed bytes of a package member (another name for `levels.serve`: either grants both) | files | 0.28.0 |
| `events.item_use` | `events.on("item.use", model, fn)` with `items.read` | event | 0.1.0 |
| `items.read` | item IDs and models in item-use events | read | 0.1.0 |
| `containers.read` | `event.item.contents` in item-use events | read | 0.1.0 |
| `containers.open` | `event:open_storage()` in item-use events | write | 0.1.0 |
| `doors.unlock` | `event:unlock()` in item-use events | write | 0.1.0 |

### Functions

| Call | Returns | Notes |
|---|---|---|
| `api.has_capability(name)` | boolean | true when granted to this addon and available now |
| `api.capabilities()` | sorted list of names | everything `has_capability` would say true to |
| `log.info/warn/error(...)`, `print(...)` | nothing | tab-joined, to `ScriptRuntime.log` with the addon ID |
| `require(name)` | the module's value | only modules declared in `[script.modules]` |
| `events.on(name, fn)` | nothing | see Events; `events.on("item.use", model, fn)` takes a model name |
| `timers.after(seconds, fn)` | timer ID | one call after at least `seconds` (0..86400) of wall time |
| `timers.every(seconds, fn)` | timer ID | repeats (0.01..86400) until cancelled |
| `timers.cancel(id)` | boolean | true when a pending timer of this addon was removed |
| `timers.now()` | seconds | presentation clock, not game time |
| `tasks.run(fn [, {budget = n}])` | true | runs `fn` as a task: when it uses up its instruction budget (`n` per pulse, 10,000..200,000, default 200,000) it pauses and goes on at the next pulse, so a long job such as level generation finishes over several frames; works like a timer callback; at most 8 per addon; needs `timers` (0.28.0) |
| `tasks.worker(module, input, fn)` | handle | runs this addon's module `module` on a runtime worker thread, in a separate Lua state with no game API (base, table, string, math, utf8, `require` for this addon's modules, `print`/`log`); the module must return a function, which is called with a copy of `input`; its first result is copied back and `fn(result, nil)` (or `fn(nil, error)`) runs on the main thread at a later pulse. Values are plain data (nil, booleans, numbers, strings, tables of those; 32 deep; input 8 MiB, result 32 MiB). Memory per job is the addon's limit; at most 4 jobs per addon, 2 running at once in all. Cancelled without calling `fn` by `tasks.cancel_worker`, leaving a game for the menu, the addon failing or unloading; needs `timers` (0.28.0) |
| `tasks.cancel_worker(handle)` | boolean | cancels a job this addon started; `fn` is not called (0.28.0) |
| `tasks.workers()` | integer | this addon's unfinished jobs (0.28.0) |
| `tasks.yield()` | nothing | inside a task only: pauses it until the next pulse (outside a task, or inside a C callback such as a `table.sort` comparator, it is an error) (0.28.0) |
| `tasks.count()` | integer | this addon's unfinished tasks |
| `characters.play_motion(target, motion)` | true | from callbacks: the game's motion player plays motion index `motion` on the target (the game's own motions, or 48+ for clips an addon spliced into `humanoid.rsq`); checked against the character's motion table; not on a ragdolled body; the game's own update may replace it next tick; 8 a callback (0.28.0) |
| `characters.keep_down(target, on)` | true | while on, the target (`"player"`, a `record_id` or a row with a `handle`) cannot get up: the game's own footing value is held low, so it stays down by the game's rule; `false` releases it; held until a level change or unload; 32 per addon (0.28.0) |
| `characters.sever(target, part)` | true | from callbacks: the joint between a limb part (arms 12/15 shoulder, 13/16 elbow; legs 2/5 hip, 3/6 knee, pair a leg with `keep_down`; `character.hit` numbering) and its parent is cut, the way the game's solver breaks a joint; the limb hangs and falls free; blows on it (or on what hangs from it) no longer hurt its owner; staged, it runs on the game thread at the next pulse; a level change or load restores the joints; 8 a callback (0.28.0) |
| `characters.drop_held(target, side)` | true | from callbacks: the target (`"player"`, a `record_id` or a row with a `handle`) lets go of what its `"right"` or `"left"` hand holds, the way the game drops a dead character's weapons; the item falls as a loose physical item; staged, it runs on the game thread at the next pulse; nothing when the hand is empty or the character is dying; 8 a callback (0.28.0) |
| `world.light_fire(name, {position = {x, y, z}, size = s})` | true | from timer and event callbacks: stages the game's own fire on the placed object nearest `position` (within 60 cm, the player's sector, scene cm as `world.nearby`); `size` 0.25..2 (1 medium); refused later (logged) when a fire already burns within 50 cm; `name` (1..31 bytes) is this addon's handle; 24 lit at most; not kept by saves (0.28.0) |
| `world.put_out_fire(name)` | true | stages removal of this addon's fire `name` through the game's own removal (0.28.0) |
| `world.fires([radius])` | list | with `world.read`: the game's fires and attached flames within `radius` cm (0..5000, default 1000) of the player, nearest first, at most 64: `{position = {x, y, z}, size, mod}` (`mod` for fires and flames addons made); refreshed about six times a second (0.28.0) |
| `effects.attach_fire(name, target, {size = s})` | true | from callbacks (`effects.fire`, another name for `effects.arc`): the game's small fire following `target` (`"player"`, a row with a `handle`, or `"weapon:right"`/`"weapon:left"`: the item in the player's hand) until removed or the level ends; `size` 0.1..1 (default 0.5); 16 per addon; not saved, never a damage source (0.28.0) |
| `effects.remove_fire(name)` | true | removes this addon's attached flame `name` (0.28.0) |
| `profile.begin(name)`, `profile.finish(name)` | boolean | a timed scope (name 1..48 bytes, up to 16 open) that appears in the runtime profiler's log as `scope name`; false while the profiler is off (0.28.0) |
| `profile.scope(name, fn, ...)` | fn's results | runs `fn(...)` and records its time as `scope name` (0.28.0) |
| `addons.send(target, topic, value)` | boolean | a message to another addon (its ID, or `"*"` for all others): `value` is nil, a boolean, number, string or a table of those (at most 6 deep, 16 KiB) and is copied; delivered at the next pulse; false when no receiver took it (at most 32 waiting each) (0.28.0) |
| `addons.on(topic, fn)` | true | `fn(value, from_id)` runs for each message on `topic` sent to this addon; at most 32 handlers (0.28.0) |
| `storage.get(key, default)` | copy of the value | per-user, shared across campaigns |
| `storage.set(key, value)` | nothing | nil removes; written to disk later |
| `storage.status()` | `"saved"`, `"pending"`, `"error"` or `"conflict"` | |
| `save_data.get()` | string or nil | this addon's data in the loaded save |
| `save_data.set(text)` | boolean | at most 4096 bytes; nil removes; written when the game saves |
| `script_flags.get(name)` | integer or nil | |
| `script_flags.set(name, value)` | boolean | 32-bit integer; nil removes; shared by all addons, so prefix names |
| `world.snapshot()` | `{space, truncated, objects}` or nil | saved level objects with their interaction methods |
| `world.time()` | whole seconds or nil | the game's world clock |
| `world.set_time(seconds)` | boolean | timer callbacks only |
| `world.level()` | `{name, mode, id, loaded}` or nil | see below |
| `world.nearby(radius_cm, {model names})` | `{truncated, objects}` or nil | placed scenery by model name (case-insensitive) within `radius_cm` (≤ 5000) of the player's body, each `{name, kind, distance, position}` with live positions, nearest first, 64 rows; no 512-row cap, so it finds objects `world.snapshot` cuts off |
| `world.nearby(radius, names, {prefix = true})` | same | names may number 64; a name ending in `*`, or every name with `{prefix = true}`, matches models whose names start with it (`"crate*"`), without case (0.28.0) |
| `characters.read()` rows: `facing`, `held`, `held_lit` | | `facing = {x, 0, z}`: the horizontal unit vector the body faces (character `+0xa08`, the one `lights.set` follows); `held = {right, left}`: the item ID in each hand; `held_lit`: whether a fire burns on it (a lit torch) (0.28.0) |
| `characters.snapshot([options])` | `{space, truncated, characters}` or nil | `{details = true}` adds `skills` and `roles` |
| `characters.awareness()` | same, with `awareness` per NPC | |
| `characters.grant_powers(record_id, {power ids})` | boolean | a live NPC (by its `record_id` from a snapshot) learns 1..10 powers and prepares them in free slots, so its brain starts casting them; Force (0x2xx), Energy (0x4xx) and Displacement (0x6xx) ids only (the game's NPC caster never casts Mind); applied on the game thread at a following pulse; refused for the player, for a character whose powers are shared with its spawner, and after the level changes. Continuous powers (Shell, continuous Displacement) also need the caster's Concentration tier at Continuity (a technique in the skill slots, not a power id): set it in the actor record; this call does not change skills. Not saved by this call: whether the game's save writes live changes is untraced |
| `inventory.snapshot()` | player inventory or nil | carried, worn, weapons, position and methods |
| `items.stats(id)` | `{item_id, weapon, armour, condition}` or nil | session callbacks only; 32 per callback |
| `title.add_row(label, after_base_row, fn[, {acts_as = n}])` | boolean | a title-menu row after base row 1 BEGIN .. 6 EXIT; `fn` runs at the next pulse after it is chosen; with `acts_as`, choosing it also does what base row `n` does (1 = BEGIN starts character creation); at most 3 per addon, 6 extra rows in all |
| `levels.new_game(member)` | boolean | from callbacks: the next new game starts in level `member` (such as `"arena1.rfc"`) instead of `exanima01`; one start only, and choosing any base title row cancels it; pair it with a row that acts as BEGIN |
| `levels.travel(member[, entry])` | boolean (staged) | save and travel to level `member` the way a level exit does (arrival marker `entry`, 0 = default spot), as a job on the game's own frame queue; only while playing |
| `player.place(x, y, z)` | boolean (staged) | put the standing, living player at that scene position (cm), keeping the facing; the game snaps to the ground and resets the stance |
| `ui.notify(text)` | nothing | a message of up to 160 characters at the bottom centre of the screen for about four seconds; four at most at once |
| `items.add_wear(id, {wear = n, dirt = n})` | boolean | raise an instanced weapon's or apparel piece's wear and/or dirt by `n` (0..255, capped at 255), anyone's (worn, held or carried), through the same timed jobs the game's repairs use (it moves there over about two seconds); applied at the next pulse, never inside a hit; false when refused or 32 already wait; items the game marks unrepairable and shields are skipped. When the piece is one the player wears, the game's equipment refresh then rebuilds the player's armour rows (NPCs' at the game's own next refresh) |
| `ui.dialog(title, text)` | boolean | the game's own message box with an OK button (the one the arena shows for "Insufficient funds"), opened on the game thread at the next pulse; title 1..60 bytes, text up to 480 (shown in the game's code page, unknown characters as `?`); one at a time, four waiting at most; needs `ui.notify` |
| `ui.sound(name[, volume])` | boolean | a single sound from `Sound.rpk` by its member name, any case (such as `"ReprSphereA1"`, `"cleanitem01"`), played at the player on the next pulse the way the game's repair and clean sounds are (volume 0..1, default 0.15, slight random pitch); false for a name the archive does not have or when 8 are already waiting |
| `settings.define(title, rows)` | nothing | while loading only, once: this addon's section of the Mod Settings tab; see Mod Settings |
| `settings.get(key)` | boolean, number, string or key code | the value in the player's active profile, or the row's default |
| `assets.write(path, bytes)` | true, or nil and a reason | a file in the addon's own `cache/` folder (`mods/<id>/cache/...`, made as needed), 8 MiB at most, replaced whole; with `assets.read`. Point a `[script.overrides]` entry at it (`"Resource.rpk/humanoid.rsq" = "cache/humanoid.rsq"`) to serve what this session computed from the next game start, before the game first reads the member (0.28.0) |
| `assets.read(path)` | bytes, or nil and a reason | a file under the addon's own folder (`mods/<id>`), 8 MiB at most; the bytes count against the addon's 8 MiB Lua memory, not its 1 MiB source budget |
| `title.remove_row(label)` | boolean | |
| `title.set_enabled(label, on)` | boolean | from callbacks; hides or shows the row from the next menu draw |
| `title.set_subtitle(label, text)` | boolean | from callbacks; a small second line under this addon's row (printable ASCII, at most 48 bytes; `""` removes it), drawn in the game's small font, not the title font (0.28.0) |
| `levels.read(member, package)` | bytes, or nil and a reason | the installed copy of `member` from the package file `package` in the game folder (such as `"Resource.rpk"`), read from the file itself so no serve applies; uncompressed members only, 8 MiB at most; allowed while loading (0.28.0) |
| `levels.serve(member, bytes, package)` | boolean | from callbacks: when the game reads `member` (16 bytes at most) from the package file named `package` (such as `"Resource.rpk"`, no folders), it gets `bytes` instead; names compare without case; only uncompressed (type 0) entries; 4 MiB each, 16 members and 16 MiB in all |
| `levels.clear_served()` | nothing | stops serving this addon's members |
| `levels.add_placement(level, item_id, {x, y, z}[, yaw])` | boolean | a loose item (objdb id) added to `level` (a member such as `"exanima01.rfc"`, any case) every time the game reads it, on top of whatever serves that member; position in level cm, yaw in degrees (upright). Declare it while the addon loads, before the level is read. A placement the level already has (same item within 1 cm) is not doubled. Like any level edit it shows in levels the game generates fresh, not in a saved game's copy. 64 per addon; needs `levels.serve`. Use this instead of shipping a whole level file, which replaces other mods' changes and the installed game's own level |
| `powers.hold_focus(amount)` | true | hold back 0..0.5 of the player's focus ceiling; 0 gives it back; not while loading |
| `characters.set_damage_scale(target, scales)` | true | target a row with a `handle` (one character: a `characters.read()` row or a `character.hit` event), a `record_id` (every live character with it) or `"player"`; `scales` `{cut, pierce, crush, impact}`, each 0.05..4, missing 1; `nil` clears. Applied inside the game's damage computation to each kind **before armour** (impact is the blunt energy every hit carries), so health and balance both follow and `character.hit` reports the result; several addons multiply; held until a level change or unload (0.28.0) |
| `characters.set_hit_sound("player", material)` | true | the player's hit sounds (wound sound and every body part's impact sound) come from the game's material table while set: 0 flesh, 1 cloth, 2 leather, 3 chain, 4 plate, 5 hard, 6 golem stone; `nil` restores them; kept through equipment changes; dropped at a level change or unload (0.28.0) |
| `characters.set_limb_strength(target, part, factor)` | true | target a `record_id` or `"player"`; part 1..18 (`character.hit` order); factor 0.05..1 scales that limb's muscle strength in the game's joint solver for each solve only (the game's own value stays the owner); 1 clears; with several addons the lowest wins; held until a level change or unload (0.28.0) |
| `characters.set_strength(target, factor)` | true | target a `record_id` (every live character with it) or `"player"`; motor strength (how hard the arms drive) as a held multiple 0.25..3 of the game's own; `nil` or 1 clears; restored exactly on release, death or unload; several addons multiply; held until a level change (0.28.0) |
| `characters.set_anim_rate(target, factor)` | true | target as `set_strength`; base animation rate as a held multiple of the game's own, 0.5..2 for the player and 0.02..2 for other characters (0.02 all but freezes them) (a knockdown's own change becomes the new base); `nil` or 1 clears; restored exactly; ground speed is not changed (0.28.0) |
| `effects.spawn_arc{from = "hand", strands, segments, life, length}` | boolean | the game's own lightning arc (the Energy Shock charge) on the player's casting hand; it follows the hand and removes itself after `life` ticks (83 per second); strands 1..40 (20), segments 2..16 (5), life 1..200 (10), length 0.5..8 (2); at most 2 per pulse per addon; false outside a session (0.28.0) |
| `characters.set_mark(target, name, value)` | true | target a `record_id` (every live character with it) or `"player"`; name 1..31 bytes of letters, digits, `_ . -` (prefix it with your addon's name); value 0..255, 0 clears; held until a level change or the addon unloads; at most 32 per addon (0.28.0) |
| `powers.restore_focus(amount)` | boolean | the player regains 0..0.5 focus at once, up to the ceiling less what is held back; false outside a session; not while loading |
| `character:damage{health = h, balance = b, can_fall = c}` | true | on a `characters.read()` entry (not the player) from this callback: the game's own damage applier on that character (each 0..1; a full pool is about 1); staged, run after the callback only if the character is still in the roster and alive; needs `characters.damage` (0.28.0) |
| `powers.spend_focus(amount)` | boolean | the player pays 0..0.5 focus once, as a cast does (drained over a few ticks, recovery paused); false outside a session; not while loading |
| `powers.held_focus()` | number | what this addon asked for last |
| `powers.caster()` | `{focus, focus_ceiling, focus_reserve, bindings, bindings_used, last_power, techniques}` or nil | the player's thaumaturgy state; needs `powers.cast`; see Powers and lights. `last_power` is the last instant power cast (any school, 0 after a sustained or continuous one), the value Iteration compares; a Light cast the runtime takes records itself there too |
| `powers.gesture(kind)` | boolean | the player plays a cast motion and no power fires: `"channel"` (the push motion 0x22) or `"thrust"` (Shock's 0x27); next pulse, only while standing with no action running; it ends after about 1.2 s, or at once when the player attacks or guards; false when one is already waiting |
| `powers.hold_bindings(n)` | true | this addon's held powers use `n` (0..4) of the player's Continuity bindings, so the game's own continuous powers see that many fewer; `powers.caster().bindings` still reports the full count; needs `powers.cast` |
| `powers.wheel(power, outcome)` | boolean | the power wheel reacts as to the game's own casts: `"cast"` keeps it shown a second and flashes the slot holding `power`; `"fail"` shows it and plays ThaumFail01; a pulse after the key press; false outside a session; needs `powers.cast` (0.28.0) |
| `powers.open_school(school)` | true | the spell-book school wheel lets that school (1..8, Light is 5) be picked even before the character knows one of its powers, including at character creation; call while loading; needs `powers.cast` (0.28.0) |
| `powers.teach(power)` | boolean | the player's character comes to know a Light (`0x500`) or Body (`0x300`) school power (`power & 0xff00`), so the spell book's Light page opens; a power already known keeps its progress; saved with the character; false outside a session; needs `powers.cast` (0.28.0) |
| `lights.set(name, spec)` | true | keep a point light near the player's head; see Powers and lights; not while loading |
| `lights.place(name, spec)` | true | a point light standing at `spec.position` until the level ends; see Powers and lights |
| `lights.remove(name)` | boolean | true when this addon had that light |
| `lights.list()` | list of names | this addon's lights |
| `levels.drive(record_id, state)` | boolean (staged) | the game's own device driver on a level record: `"open"`, `"close"`, `"toggle"` (or 1, 0, -1) for doors (a door flagged inverted swaps open and close); buttons and other devices take the value as their state |
| `player.damage(balance, health, can_fall)` | boolean (staged) | the game's own damage applier on the player, in native units (a full pool is about 1; 0..10 accepted); `character.hit` reports it |
| `player.push(dx, dy, dz, amount)` | boolean (staged) | the game's push-back along that direction (normalised), `amount` 0..100 |

Staged calls are made after the callback returns without error, at most four
per callback, from timer or title-row callbacks only. Damage and push go to the
living player only (death stage 0, health above 0); every call needs the save
backup the runtime makes at the title menu. A refused call is logged. They run
on the game's own thread between frames, never inside a physics callback.

Title rows and served members belong to the addon: they go away when it is
unloaded, replaced by a reload, or disabled by an error. A reload keeps the rows
the new version adds again. Labels are 1..31 printable ASCII characters and
unique across all mods.

`world.level()` gives `name` (such as `"exanima01"` or `"arena"`), `mode`
(`"story"`, `"arena"`, `"practice"` or `"versus"`; nil if unrecognised), the
numeric level `id`, and `loaded`, true while the game is in its playing state
with a controlled character. There is no engine flag for "fully loaded"; this is
the closest the game keeps.

`items.stats(id)` accepts database, instanced and level-local item IDs. It is
nil for an ID that is not an item. `weapon` (when the item has a weapon chunk)
holds the numbers the combat code derives from it:

| Field | Meaning |
|---|---|
| `heft`, `balance` | payload bytes `/100` |
| `effective_heft` | `0.1 + 0.4*heft + 0.6*(1-balance) + parry offset`, the parry/recoil weight |
| `thrust` | thrust power `0.5*(b/100)^0.7` |
| `parry_weight` | `0.8 - ((heft+1) - balance)*0.125` |
| `power_multiplier` | `1 + (b-128)*0.00156`; what reads it is not traced |
| `recoil`, `recoil_two_handed` | stagger scale one-handed, and with the two-handed heft/balance change |
| `grip`, `alternate_grip` | `{kind, edge_start, edge_end, cut, crush, point}`; edges are fractions of the blade length |
| `class`, `polearm`, `flags` | swing class nibble, the thrust/polearm bits, the raw flag word |
| `sound`, `raw` | swing sound set index, and the 18 raw payload bytes from +0x30 |

Blade length comes from the model and is not included. `condition`
(`{quality, wear, dirt}`, 0..255 each) is present for weapons and apparel:
the bytes loot and merchants set and repair lowers. `armour` (for apparel)
holds `coverage` (percent per body part, keyed by part name, such as
`coverage.chest`), `impact` (`*0.005`), `cut`, `crush`, `pierce` (`/140`),
`encumbrance`, `layer` and `vision_limit`. A worn part keeps its outermost piece
plus one merged row for everything under it; coverage is a hit chance per piece,
not a multiplier.

Body part names, in the game's order: pelvis, right_thigh, right_shin,
right_foot, left_thigh, left_shin, left_foot, belly, chest, head,
right_shoulder, right_upper_arm, right_forearm, left_shoulder, left_upper_arm,
left_forearm, right_hand, left_hand.

`characters.snapshot({details = true})` adds to each row:

- `skills`: keyed `close_combat`, `ranged_combat`, `armour`, `shields`,
  `insight`, `concentration`, each `{index, name, rating, active_techniques,
  techniques}`. `rating` is the raw value (25,000 = five techniques);
  `active_techniques` is `floor(rating/5000)`; `techniques` lists every
  non-empty technique slot as a technique number (1-based, in the game's
  technique table order). Only the first `active_techniques` slots count in
  play.
- (every snapshot) `dead`, `down` and `life_stage` come from the character's
  death stage: 0 alive, 1 collapsing, 2 down, 3 player only, 4 dead (NPCs reach
  4 about three seconds after 2). Before 0.25.0, `dead` was also true for some
  knocked-down living NPCs.
- `roles`: `{id, name, variables}` for up to 16 roles the character holds, with
  the first 32 script variables of each. Names come from `charroles.rdb`.

Details cost more reads, so ask only when you need them; the copy is shared by
all addons for that pulse.

### Events

| Event | Capability | Fields |
|---|---|---|
| `"runtime.phase"` | `events.lifecycle` | `phase` (`"loading"`, `"menu"`, `"session"`), `generation` |
| `"character.hit"` | `events.combat` | `record_id`, `name`, `is_player`, `health_damage`, `balance_damage`, `downed`, `source` (`"weapon"`/`"impact"`), `attacker` `{record_id, name, is_player}`, `weapon_id`, `part`, `part_name`, `blocked` + `shield_id` / `parried` + `parry_weapon_id` (the blow met the victim's shield or weapon; such a blow arrives even with no damage when the player held the item, with 0 damage and `source` `"weapon"`), `armour_id` (player only: the outermost worn piece covering `part`) |
| `"item.id_changed"` | `items.read` | `old_id`, `new_id`: an item got a new ID, as a level-local item (0x8…) does once when it leaves its level with the player (and its contents with it); follow items you remember by ID |
| `"settings.changed"` | `ui.settings` | `key`, `value`: one call per changed row of this addon, after the player presses Apply (a profile switch counts) |
| `"script.event"` | `events.script` | `name`, `value` (text as written), `number` (the value as the game evaluates it: a decimal literal or a variable's value; 0 for a word that is neither) |
| `"zone.enter"` | `events.zone` | `zone_id`, `record_id`, `name`, `is_player` |
| `"game.message"` | `events.messages` | `caption`, `text`, `level` (4 and 5 are errors the game shows in a box; lower levels are its log notes) |
| `"power.cast"` | `powers.cast` | `power` (the id, e.g. `0x20501`), `number` (low byte), `school` (`"light"`, `"body"` or `"energy"`), `mode` (`"immediate"`, `"sustained"`, `"continuous"`) |
| `"gear.use"` | `events.gear_use` | `item` `{id, model, kind = "weapon"/"apparel", carried, condition}`, `target` `{kind, name, record_id, distance}`; method `restore_condition`; return true when handled |
| `"item.use"` | `events.item_use`, `items.read` | `item`, `target`, `storage_open`; methods `open_storage`, `unlock`, `restore_condition{wear, dirt}` (`items.condition`; an inventory target that is a carried, instanced weapon or apparel piece; lower only, as in `gear.use`), `consume()` (`inventory.transfer`; the used item leaves the pack at the next pulse); each needs a handled (true) result. A weapon or apparel target also has `target.gear` (`"weapon"`/`"apparel"`), `target.carried` and `target.condition` |

Events are copies delivered at the next presentation pulse, in order, with
per-pulse caps (64 hits, 32 zone entries, 32 script events, 32 game messages).
An event for a character no longer in the level's list is dropped.

### Powers and lights

The spell book has six schools; Light (domain byte 5, ids `0x5NN`) has a page
(`pwr_light.pwr`, icons `thmlgtico1.rfi`) but the game ships neither and casts
nothing there. An addon that ships those two files can add Light powers:

- **`power.cast`** fires when the player casts a Light (0x5NN) or Body (0x3NN) power from the wheel, or an Energy
  power (0x4NN) the game's own Energy table does not have (it keeps 0x401 Shock and 0x2040B).
  While any enabled addon listens, the cast is taken (no failure sound) and the
  addon decides what happens; with no listener the game behaves as before.
  Every other school is left to the game. The game does not charge focus for
  these powers: use `powers.hold_focus` or your own rule.
- **`powers.hold_focus(amount)`** lowers the player's focus ceiling by
  `amount` for as long as it is set, the way the game's continuous powers do,
  and `0` gives it back. Holds of all addons add up, at most 0.5; the ceiling
  never goes below 0.05 or back above full. After a level change or load the
  hold is applied again from scratch.
- **`powers.caster()`** copies what the game's own cost rules read: `focus`,
  `focus_ceiling`, `focus_reserve`, `bindings` (how many continuous powers
  the Continuity techniques allow, plus one with an extra-binding item),
  `bindings_used` (continuous powers running now) and `techniques`
  `{iteration, sustain, abstraction, clarity}`. The game halves a repeated
  instant cost with Iteration, halves sustained drain with Sustain, holds
  0.75 of a continuous cost with Abstraction and recovers focus 1.5 times
  faster with Clarity; an addon applies the first three itself. Clarity
  applies on its own. A hold made here does not count as a binding for
  the game's own powers.
- **`lights.set(name, spec)`** keeps a point light (the same kind the game
  uses for its own player light) floating near the player's head:

| `spec` field | Default | Range |
|---|---|---|
| `color` `{r, g, b}` | `{1, 1, 1}` | 0..4 each |
| `range` (cm) | 1200 (a torch reaches about 1500) | 100..5000 |
| `intensity` | 2 | 0..8 |
| `follow.forward`, `follow.right` (cm, along the body's facing) | 0 | -300..300 |
| `follow.up` (cm above the head) | 80 | -100..300 |
| `follow.smoothing` (seconds to close most of a gap; 0 snaps) | 0.35 | 0..5 |
| `follow.bob` (cm of slow sway) | 0 | 0..30 |
| `glow` `{size = cm, color = {r, g, b}}` | none | a small particle glow that moves with the light: `size` 2..20 (8), `color` 0..1 each (the light's colour, capped at 1); built like the game's portal particles with its own colour ramp, not saved with the level and made again after a load; changing `color` recolours it in place |

  Calling `set` again with the same name changes that light. An addon keeps at
  most 3 lights and the runtime shows at most 6. Lights never go through the
  ceiling over the head (as the game's own light). They are re-created after
  a level change while still set, and vanish when the addon is disabled.

  `lights.place(name, spec)` takes the same spec plus `position = {x, y, z}`
  (scene cm, the space `world.nearby` positions are in; static reading, not
  yet seen in game; `follow` is ignored) and keeps that light standing there. Placed lights count toward the
  3, share names with `set` (the same name moves or converts the light), and
  are forgotten when the level ends: leaving the session removes them from the
  addon's list.
  Lights and holds are requests: the runtime applies them at the next session
  pulse, on the game's own work queue. If the runtime itself is disabled,
  lights and the focus hold already applied stay until the level changes.

Not verified in game: the light's look, its colour scale, which side `right`
is, and whether NPCs notice it.

### Using weapons and armour on things (`gear.use`)

The game has no "use" for gear: double-clicking a sword does nothing. While an
addon listens to `gear.use`, double-clicking a weapon or armour piece that is
in the pack (not worn, not held) puts it on the cursor the way the game does
for keys and repair orbs, and clicking it on something in the world fires
`gear.use` instead of any game action. Handlers run at once, in the game's own
click, in profile order until one returns true.

`target.kind` is what the game's pick found: `"object"` (placed scenery such as
a grindstone or anvil: no record, or no physics body), `"door"`, `"item"`,
`"lever"`, `"button"`, `"corpse"` or `"other"` (nothing, or something else);
`name` is its model name (for example `"sharpeningwheel"`); `distance` is in cm
from the player's body. Check both before acting: the game puts no reach limit
on this click. `target.heat` is true when a fire burns within 300 cm of the
target (any fire effect in the player's sector: a lit brazier, fireplace or
forge fire), and `heat_distance` is the nearest one's distance in cm.

`event:ignite_target()` (with `world.devices`, and a true return) lights a
medium fire on the picked object the way the game lights a fire (its fire
builder at the object's position), as a job after the click. It is offered
for placed scenery (`target.kind == "object"`) with no fire already within
150 cm, and checked again just before lighting; the fire is saved and rebuilt
with the level like the game's own. A furnace model has no lighting of its
own, so this is how a mod lights a forge. Static reading, not seen in game.

`event:restore_condition{wear = n, dirt = n}` (with `items.condition`, and a
true return) lowers the item's wear and/or dirt to `n` (0..255; never raises
them) through the same repair jobs the game's repair orb and brush use: the
values move there over two seconds and the item, its icon and open windows are
refreshed by the game. It works on instanced items in the pack only, and is
refused for items the game marks unrepairable. The orb is not involved and
nothing is used up. One request per event.

Shields (chunk 0x8000, no device chunk) take part too since 0.28.0:
`item.kind` is `"shield"`, `item.condition` is nil and `restore_condition` is
refused for them, so a mod keeps a shield's wear itself.

### Item records without whole databases

A mod that adds items should not ship a whole `objdb.rdb`/`objstrings.rdb`:
the highest-priority copy replaces every other mod's. Declare the ranges it
owns and ship only its own records and strings:

```toml
[script.records]
ids = [0x590, 0x59f]                     # objdb ids this mod adds (1..0xFFFF, 256 at most)
strings = [1201, 1216]                   # objstrings indices its strings take (1-based)
objdb = "records/objdb.rdb"              # only its new records and the game records it changes
objstrings = "records/objstrings.rdb"    # only its strings, in index order
```

The runtime merges them as the game reads the two files: new records go in by
id, a changed drop table (chunk 0x200) gets the mod's extra rows appended (so
several mods can add loot to one table), and any other changed game record is
replaced (the log names a second mod that also changed it). Strings land at
their declared indices, the gap after the game's own 1125 strings filled with
empty entries, so display-string indices need no rewriting. Overlapping ranges
are refused at startup. `records_delta.py` (beside `build.py`) makes these
files from a mod's whole databases, renumbering its new records into its range
together with drop-table rows and Lua constants. Static reading: whether the
game reads `objdb.rdb` before EMTK hands the addons over has not been seen.

### Startup file overrides

An addon can replace a package member from the moment the game starts, for
files the game reads only once (the role database `charroles.rdb` is read at
session setup):

```toml
[script.overrides]
"Resource.rpk/charroles.rdb" = "assets/charroles.rdb"
```

The runtime reads these when EMTK hands it the addon at startup, from the
addon's own folder (`mods/<id>`; anything outside it is refused), 8 MiB per
file. They are served at priority 60 (after `levels.serve`, before the
editor's project files) for the rest of the session. A missing or refused file
is logged and the game's own member stays. A name the package does not list is
added: the package's name lookup (0x1faf0) answers a runtime index for it, and
only this runtime's read serves that index. The package's remove-row function
is not hooked; nothing is known to remove such names.

### Commands for the game's own scripts

The runtime adds three commands to the game's role, dialogue and zone scripts.
Without the runtime they do nothing and read 0.

| Command | Does |
|---|---|
| `LuaEvent(name, value);` | sends `script.event` to addons; `value` arrives as text and as `number`, so `LuaEvent(lamp_zone, ZoneID);` passes the zone's number |
| `LuaFlag(name)` | reads the integer an addon set with `script_flags.set`, else 0 |
| `LuaLog(any words);` | writes the words to `ScriptRuntime.log`, for tracing a script |

They work wherever the game runs a script: zone scripts, role scripts, and
dialogue reply actions, reply conditions and topic entries, which all go
through the same parser. So a reply action `LuaEvent(EndlessReveal, 1);` reaches an
addon's `script.event` handler (`events.script`) with `name = "EndlessReveal"`
and `number = 1`.

### Native exports for other mods

`ScriptRuntime.dll` owns its game hooks so that two mods never detour the same
function. A native mod finds these with `GetModuleHandleW(L"ScriptRuntime.dll")`
and `GetProcAddress`:

| Export | Does |
|---|---|
| `bool add_zone_listener(void (*fn)(void* context, uintptr_t character, uint32_t zone_id, const void* payload), void* context)` | up to 8 listeners on zone entry; works any time |
| `bool remove_zone_listener(fn, context)` | returns after the listener can no longer be running |
| `bool zone_listener_ready()` | true once the zone detour is installed (settled title menu) |

A listener runs on the game's thread inside the detour, before the zone's own
script. `payload` is the zone marker's data, read-only. It must return quickly
and must not run game scripts. A listener that faults is caught and removed.

Shared hooks, version 1 (runtime 0.25.0). Check `shared_hooks_version()` and
the matching `*_ready()` first; if an export is missing, the version is not 1,
or ready is false, keep your own detours. The runtime installs these hooks only
on untouched entries, so it never stacks on a mod that hooked them first.

| Export | Does |
|---|---|
| `uint32_t shared_hooks_version()` | 1 |
| `bool add_title_row(const char* label, int after_base_row, void (*activate)(void* context), void* context)` | a title row after base row 1..6; at most 6 extra rows in all; the same `activate` may register its label again to move it; the menu hooks are made at the first call |
| `bool remove_title_row(const char* label)` | returns after `activate` can no longer run, except when called from inside an `activate` (then at once); a Lua addon's rows cannot be removed this way |
| `bool set_title_row_enabled(const char* label, bool enabled)` | applied at the next menu draw |
| `bool title_rows_ready()` | the runtime can own the title menu: build 0x122c40, hit 0x128d80, nav 0x128a70 and activate 0x128580 untouched, and draw 0x128f30 the runtime's own lifecycle hook (taken from the game's own entry) |
| `bool add_member_provider(bool (*serve)(void* context, uintptr_t package, int index, const char* name16, uint32_t archive_type, uintptr_t buffer), bool (*claims)(void* context, const char* name16), void* context, int priority)` | asked in ascending priority (more than 50; the editor uses 100); the first that returns true has filled the buffer and no later one is asked; `claims` (optional) only reports a member a higher provider already served; the member read is hooked at the first call |
| `bool remove_member_provider(serve, context)` | returns once no package read is running, except when called from inside a provider (then at once: other threads may still be inside `serve`, so free its context only after a removal from outside) |
| `bool resize_member_buffer(uintptr_t buffer, size_t size)` | the game's own resize 0x21260 for heap buffers (type bits 0 or 1) |
| `bool member_providers_ready()` | the runtime can own the member read 0x6f2d0 (still the game's own entry) |

Settings tabs, version 1 (runtime 0.27.0); `settings_tabs.h` has the `Row`
struct (`{int id, kind; const wchar_t* label, *value; int steps; float position;
int checked, inactive;}`, kind 0 section title, 1 checkbox, 2 slider, 3 value
button, 4 key binding, 5 text).

| Export | Does |
|---|---|
| `uint32_t settings_tabs_version()` | 1 |
| `bool add_settings_tab(const wchar_t* title, void* context, int (*rows)(void* context, int page, Row* out, int capacity), void (*change)(void* context, int page, int id, int kind, int delta, float position, int checked, int key), void (*event)(void* context, int event))` | a tab (24 characters, at most 5 beside Graphics and Controls) or, titled `@Controls`, rows under the native Controls list (page 1); events 0 opened, 1 applied, 2 cancelled, 3 defaults, 4 closed |
| `bool remove_settings_tab(title, context)` | the open form keeps its copy until it closes |
| `bool settings_tabs_ready()` | the Settings functions matched |
| `bool settings_tabs_claim_host()` | for a mod that manages the Settings form itself: call at enable; the runtime then never hooks it (false if it already has) |
| `int settings_tab_host_open()` | when the host's form opens: snapshot the tabs and send "opened"; returns the tab count |
| `int settings_tab_host_title(int tab, wchar_t* out, int capacity)` | 0 a tab page, 1 rows under Controls, -1 no such tab |
| `int settings_tab_host_rows(int tab, int page, Row* out, int capacity)` | the rows to draw now; pointers stay valid until the next call |
| `void settings_tab_host_change(...)` | forwards a widget change, with `change`'s arguments after `tab` |
| `void settings_tab_host_event(int tab, int event)` | tab -1 for all tabs |
| `uint32_t character_hits_version()` | 2 (0.28.0; v2 adds `u64 weapon` at 0x70 and cut/crush kinds) |
| `int character_hits(uint32_t after, CharacterHitRecord* out, int capacity)` | weapon hits on character parts with serial > `after`, oldest first (a 64-entry ring): `{u32 size, u32 serial, u64 victim, u64 part, float point[3], float direction[3], float pose[12], float health, float balance, u32 kind (0 unknown, 1 cut, 2 crush, 3 thrust), u32 0, u64 weapon (the attacker's weapon body)}`, 0x78 bytes (check `size`); copies only, its own lock |
| `uint32_t character_marks_version()` | 1 (0.28.0) |
| `int character_mark_nodes(const char* name, uintptr_t* nodes, uint32_t* values, int capacity)` | each live character marked `name` (characters.set_mark): its scene node (character `+0x18`) and value; returns how many; any thread, no lock, no game memory read, no allocation; refreshed at each pulse, so a node may be stale by one pulse (compare pointers only) |
| `int mod_settings_help(int id, wchar_t* out, int capacity)` | the help text of a Mod Settings row (the `id` from its rows), for a tooltip; returns its length, 0 when none; copies at most `capacity - 1` units and a terminator (0.28.0) |

`activate` runs on the game's main thread inside the menu's activate; set a
flag there and act later. `serve` runs on whatever thread reads the package,
with no runtime lock held; do not block in it, and do not hold a lock your
`serve` needs while calling `remove_member_provider`. Faults in either are
caught. Nothing is hooked until something registers, so a game without
registered rows or providers carries none of these hooks; until the editor uses
these exports, do not run it together with a mod that registers here.
Priority 50 belongs to the runtime's own `levels.serve`, so an active Lua
level serve wins over the editor's project files.

### Mod Settings

The runtime adds a **Mod Settings** tab to the game's own Settings menu. It
shows one section per mod and a **Profile** row: profiles are named sets of
values (`Default`, `Profile 2`, ...) kept in `profiles/<name>.ini` beside
`ScriptRuntime.dll`; **New profile** copies the values on screen into a new one.
Changes are a draft until **Apply**; **Cancel** drops them and **Defaults**
resets every section to its defaults.

```lua
settings.define("Grindstone", {
  {key = "wear_rate", label = "Wear rate", type = "number", default = 1, min = 0, max = 3, step = 0.25,
   help = "How fast blades dull and armour wears in a fight"},
  {key = "notices", label = "Show notices", type = "toggle", default = true},
  {key = "mode", label = "Mode", type = "choice", default = "Normal", choices = {"Easy", "Normal", "Hard"}},
  {key = "use_key", label = "Use key", type = "key", default = 0},   -- 0 = unbound
  {type = "header", label = "Limits"},                                -- a sub-header line
  {key = "floor", label = "Floor", type = "number", default = 0.08, min = 0, max = 0.5, step = 0.01,
   format = "percent"},                                               -- shown as 8 %
})
local rate = settings.get("wear_rate")
events.on("settings.changed", function(e) if e.key == "wear_rate" then rate = e.value end end)
```

Keep labels to two or three words: the page has a narrow label column. Put the detail in the optional
`help` (1..160 bytes), which a Settings host shows as the row's tooltip.

A `type = "header"` row (label only, no key or default) draws as a dimmed sub-header line. A number row with
`format = "percent"` shows its value x 100 with a % sign; the stored value and `settings.get` are unchanged.

Rules: 1..48 rows; keys use letters, digits, `_ . -` (48 bytes); labels 40
bytes; help 160; titles 32; numbers need `min < max`, `step > 0` and at most 1000 steps;
2..16 choices of 24 bytes. A key row's value is a Windows virtual-key code;
the player sets it by clicking the row and pressing a key (Delete clears it,
Escape cancels). New key rows ship unbound (`default = 0`).

Native EMTK plugins appear too: their `config.toml` `[[setting]]` entries with a
`true`/`false` default are editable and written back to `value =` on Apply
(EMTK reads them at startup, so they apply at the next launch); other types are
shown read-only. The runtime's own section holds the console key.

The tab needs the Settings form's GUI step (0x32180). The runtime hooks it at a
settled title menu, only on the game's own bytes; a mod that already manages
that form (ExCamera) hosts the tab instead through the `settings_tab_host_*`
exports below. Without either, the tab is missing, the log says why, and
`settings.get` still returns profile values and defaults.

### In-game console and the agent log

The runtime keeps one console for every mod. In the game it is an overlay drawn
over the frame: warnings in amber, errors in red. Set its key in
`ScriptRuntime.ini` beside `ScriptRuntime.dll`:

```ini
[console]
key=0          ; a Windows virtual-key code; 0 (the default) leaves it unbound
```

When it is open, Tab or a click on `[All]`, `[Warnings]` or `[Errors]` filters
the list, and PgUp/PgDn scroll it. While it is closed, a corner badge counts
errors that arrived since it was last open. It shows everything the runtime
logs (Lua errors, addon log lines, the game's own script errors, runtime
state), plus warnings and errors from any other mod's log file under `mods\`
or `.emtk\` (files ending `.log`, or `.txt` files with "log" in the name), so
ExCamera's and the editor's logs appear too.

For agents and smoke tests the same lines are written as files in
`mods\com.necrosis.script-runtime\console\` (inside the game folder, never
AppData):

- `console.jsonl`: one JSON object per line, `{"ts", "level", "source",
  "message"}`, `level` being `"info"`, `"warning"` or `"error"`. Each launch
  starts with `{"level": "session", "runtime", "pid", "mods": [...]}`. At
  2 MiB the file moves to `console.jsonl.1` and a new one starts.
- `console-summary.json`: rewritten every two seconds (written to a temporary
  file, then swapped in) with `updated`, `runtime`, `seconds_since_launch`,
  `errors`, `warnings`, `info`, `last_error` and `last_warning`. A smoke test can
  pass or fail on `errors == 0` here.

### Errors and diagnostics

Everything goes to `ScriptRuntime.log` beside `ScriptRuntime.dll` (rotated at
1 MiB), one line per message with the source and level:

- **Load problems**: an unknown capability with a "did you mean" hint, a
  capability the runtime could not enable and why, Lua syntax errors with line
  numbers.
- **Callback errors**: what failed (`timer callback failed`,
  `character.hit handler failed`, ...), Lua's message and traceback, and how to
  reload.
- **The game's own errors** (source `game`): the game reports script mistakes
  such as `Script Error: Function "ChangeRole" does not exist.` or `Invalid
  role identifier.` through a logger no player sees. The runtime copies them
  into this log (each distinct message three times at most) and to
  `game.message` handlers.
- **Script command problems** (source `script`): a malformed `LuaEvent` or
  `LuaFlag` name is a warning (three times per distinct call); `LuaFlag`
  reading a flag no addon has set yet is noted once, as a spelling hint.
  `LuaLog` writes at most 60 lines a minute and reports how many it held back. The game's script language itself reports nothing for a typo:
  an unknown function silently returns 0.
- **Runtime state** (source `runtime`): which features were enabled, and why
  one was not.

### Not exposed, on purpose

- `ChangeRole` in game scripts is a stub that only reports an error; use
  `Actor.AddRole`/`RemoveRole` there.
- The native damage override slot (0x4F0900) is not used: replacing the
  damage applier would take over every hit in the game. `character.hit`
  observes damage instead.
- No live skill, role, armour or weapon writes: those need their own reviews
  before they ship. Skills, roles and item numbers are read-only, and NPC AI
  changes go through `set_ai`/`set_attitude`.
- No raw memory, pointers, threads, files or processes.

## Release 0.28.0: sounds and placed lights

- `ui.sound(name[, volume])` (capability `ui.sound`).
- `lights.place(name, spec)`; 3 lights per addon, 6 in all.
- `item.use`: `event:restore_condition{wear, dirt}` on a weapon or apparel
  target in the inventory, `event:consume()`, and `target.gear/carried/condition`.
- `character.hit`: `blocked`, `shield_id`, `parried`, `parry_weapon_id`. Read
  from the body the weapon struck (not a body part: its item's record);
  static reading, not yet seen in game.
- `gear.use` admits shields (`item.kind = "shield"`, no condition), and its
  target has `heat`/`heat_distance` (a fire within 300 cm).
- `character.hit.armour_id` for the player.
- `gear.use` `event:ignite_target()`.
- `lights.set{glow = {size, color}}`: a particle glow with the light.
- `items.add_wear(id, {wear, dirt})` (capability `items.wear`).
- `ui.dialog(title, text)`: the game's message box.
- `characters.grant_powers(record_id, {power ids})` (capability
  `characters.powers`), and `LuaEvent` documented for dialogue reply actions.
- `[script.records]`: item records and strings merged at load from per-mod
  ranges, and `records_delta.py` to make them.
- `"item.id_changed"` (`items.read`): a detour of the ID conversion 102DF0, made
  when an addon first listens.
- `powers.gesture(kind)`: a cast motion without a power (179E40 with a
  releasing +1230 callback, ended with 179DE0).
- `levels.add_placement(level, item_id, position[, yaw])`: items added to a
  level as the game reads it (the member read edits the bytes after any
  provider or the game filled them), so mods stop shipping whole level files.
- `powers.caster().last_power` and `powers.hold_bindings(n)` (a read-only
  detour of the bindings count 0x1d3a80, made when an addon first holds one).

## Release 0.27.0: Mod Settings

- A Mod Settings tab in the game's Settings menu with profiles: `settings.define`,
  `settings.get`, `"settings.changed"` (capability `ui.settings`), native
  `config.toml` settings, and the console key.
- Settings tabs for native mods (`add_settings_tab`), and a host role for a mod
  that already manages the Settings form (`settings_tabs_claim_host`).

## Release 0.26.0: staged game calls

`player.damage`, `player.push` and `levels.drive` call the game's own damage
applier 0x159700, push-back 0x158870 and device driver 0x14a3e0 (scene =
world + 0x220), each behind its entry-byte guard, after the callback returns.
They are the first Lua calls that change game state outside the reviewed
inventory/world staging, so they keep to its rules: backup made, session pulse,
game thread, living player. Statically reviewed.
Offline evidence:
`test_shared_hooks` (LUA_NATIVE_CALLS). Not run in the game.

## Release 0.25.0: shared title rows and level serving

The runtime becomes the one owner of two sets of game hooks other mods need:
the title menu (an Endless row, the editor's EDITOR row) and the package
member read (served level files, the editor's applied project files). Lua gets
`title.add_row` and `levels.serve`; native mods get the exports above. Each
hook is installed only if its entry bytes are still the game's own; otherwise
the feature stays unavailable and the log says which function another mod
owns. The menu arithmetic is the reviewed editor/Endless method generalised to
eight rows. Statically reviewed.
Offline evidence: `test_shared_hooks`. Not run in the game.

## Release 0.24.0: item stats, level, details, zones, game messages

Added, all read-only or event-only; nothing here writes game state:

- `api.revision`, `api.runtime`, `api.addon`, `api.capabilities()`.
- Load errors name a misspelled capability and suggest the closest one; an
  unknown optional capability is logged instead of silently ignored.
- `items.stats(id)` (`items.stats`) through the same record chunk lookup the
  inventory paths already use (0x1041b0). Formulas match ExaniMate's
  `rdb.weapon_strike`/`armour`.
- `world.level()` (`world.read`) from the world name (+0x140), mode 0x2b7540,
  level id 0x2e8210 and flow state 0x4eff70.
- `characters.snapshot{details = true}` reads the six live skill slots
  (stats +0xc0) and the held roles (stats +0x128/+0x130).
- `character.hit` gains `weapon_id` (the delivering weapon's item record) and
  `part`/`part_name` (struck body part, 1-based in the order above), copied
  at impact entry.
- `zone.enter` (`events.zone`) from a detour of the zone runner 0xaa4d0, which
  copies `{character, ZoneID}` and then runs the zone's own script unchanged.
  Both callers arrive: the any-character zone test (0xaa540) and the
  player-only one (0xaa7c0). An ordinary zone fires on every entry crossing;
  whether a player-only zone re-fires while the player stays inside is not
  traced. Keep "once" state yourself.
- `game.message` (`events.messages`): the game's message slot 0x2ab6b0 is
  chained, so the game's logger still runs with the same arguments. The slot is
  left alone if another mod already replaced it.
- `LuaEvent` evaluates its value with the game's own argument reader 0x1c4090
  (no side effects); `LuaLog` is new; malformed calls are logged.

Statically safety-reviewed. Offline
evidence: `test_api`, `test_characters`, `test_examples` and `test_api_docs`
in `build.py`. None of this has run in the game.

## Item spawning, hit events, world time, save data and script commands (runtime 0.23.0)

Doors also report their progress (with `world.doors`): `command` is
`"opening"` or `"closing"` while the game is driving a leaf toward its target
and nil once it gets there, and `moving` stays true while any hinge is still
swinging (the game's own stop threshold) or a command is active. "Command
finished" and "stopped moving" are separate: a door can arrive and still sway.
Poll them from a timer to react when a door is done.

Every feature sits behind a permission that the addon names in `requires`
(load fails without it) or `optional` (check `api.has_capability`). The runtime
grants a permission only when the game functions it relies on match on the
running executable; otherwise the addon sees it as missing, never as broken.

| Permission | Gives | Evidence so far |
|---|---|---|
| `world.spawn` | `inventory.snapshot():spawn_item(id, position)` | static review, owned-process test |
| `events.combat` | `events.on("character.hit", fn)` | static review, independent review, owned-process test |
| `world.time` | `world.set_time(seconds)` (`world.time()` needs only `world.read`) | static review, owned-process test |
| `storage.save` | `save_data.get()` / `save_data.set(text)` | static review, independent review (3 defects fixed), owned-process test |
| `events.script` | `events.on("script.event", fn)` for `LuaEvent(...)` | static review, independent review, owned-process test |
| `script.flags` | `script_flags.get/set` for `LuaFlag(...)` | static review, independent review, owned-process test |

None of these has run in the game yet. The read-only example `com.example.self-test` logs what
they see while you play; set `enabled = true` in its config to use it.

The runtime adds two commands to the game's role and dialogue scripts. An
action `LuaEvent(quest_started, 2);` reaches addons that declare
`events.script`:

```lua
events.on("script.event", function(event)
    if event.name == "quest_started" then
        save_data.set(event.value)
    end
end)
```

A condition `result = (LuaFlag(door_open) = 1);` reads a number an addon set
with `script_flags.set("door_open", 1)` (`script.flags`); unknown flags read 0.
`script_flags.get(name)` returns the value or nil, and `script_flags.set(name,
nil)` removes it. Names are up to 32 letters, digits or `_`; values are 32-bit
integers; at most 256 flags. Arguments arrive as the text written in the
script (`event.value` is a string or nil). Events arrive at the next session
pulse, at most 32 per pulse. Flags are not saved: keep them in `save_data` and
set them again after a load. Flag names are shared by all addons, so prefix
yours. Without the runtime both commands do nothing and `LuaFlag` reads 0.

With `storage.save`, an addon keeps up to 4096 bytes inside the player's save:

```lua
timers.after(0, function()
    local stage = tonumber(save_data.get() or "0")
    save_data.set(tostring(stage + 1))
end)
```

`save_data.get()` returns the string saved with the loaded game, or nil.
`save_data.set(text)` replaces it and `save_data.set(nil)` removes it; both
return false when the limits are reached (32 addons, 32 KB in all). Values are
written the next time the game saves, read whenever a story game loads,
restored with the checkpoint after death, and empty in a new game. Arena saves
do not carry them. Use `storage.settings` for preferences that are not part of
a playthrough.

The game itself keeps story progress in visited levels, the roles its
characters hold (scripts' `AddRole`/`ChangeRole`), story event bits set by
`GameEvent`, and what NPCs remember; there is no hidden quest table. A quest
addon either follows those (roles through dialogue scripts and `LuaEvent`) or
keeps its own state in `save_data`.

`world.time()` (with `world.read`) returns the world clock in whole seconds, or
nil outside a session. With `world.time`, `world.set_time(seconds)` sets it
from a timer callback (0..4294967295) and returns true when applied. The clock
is saved with the game; no day/night cycle is known to follow it.
Declare `inventory.read` and `world.spawn` to create a fresh item from the item
database next to the player:

```lua
timers.after(0, function()
    local player = inventory.snapshot()
    if player and player.spawn_item then
        local p = player.position
        player:spawn_item(558, {p[1] + 50, p[2], p[3]})
    end
end)
```

The ID must be a database item ID (1 to 0x3FFFFFFF), not a level-local one, and
an ordinary physical item under the same rules as drop. The new item starts in
state 0 and goes through the same native creation, initialization, registration
and release as a dropped row; inventory is unchanged. Placement rules, timing
and refusals match drop. Spawned items are saved with the level like dropped
ones.

Declare `events.combat` and register during loading to hear about damage:

```lua
events.on("character.hit", function(hit)
    if hit.downed and not hit.is_player then
        log.info(hit.name .. " went down")
    end
end)
```

Each event is a copy: `record_id`, `name`, `is_player`, `health_damage`,
`balance_damage` and `downed` (this hit knocked the character down or killed
it). `source` is `"weapon"` for a weapon or unarmed strike and `"impact"` for
falls, thrown objects and body collisions; a weapon hit also carries
`attacker` = {`record_id`, `name`, `is_player`} when the attacker is still in
the level. Both fields are absent if the runtime could not hook the weapon
path. Bleeding is not reported. Hits arrive at the next session pulse, in
order, at most 64 per pulse; hits on characters that left the level's
character list by then are dropped.

## Fire extinction and static world torches (runtime 0.22.0)

With `world.read`, `world.devices` and `timers`, supported fire sources and static
world torches expose copied `lit` state. A lit source exposes `:extinguish()`;
an unlit torch exposes `:ignite()` and the equivalent `:use()`. Methods appear
only when their capability and native path are available. Check method presence.
Fire sources retain `:use()` for ignition when unlit. Existing addons/API 1 remain
compatible; no executable version or hash equality check is added.

```lua
timers.after(0, function()
    local scene = world.snapshot()
    if not scene then return end
    for _, object in ipairs(scene.objects) do
        if object.item_id == configured_record then
            if object.extinguish then object:extinguish()
            elseif object.ignite then object:ignite() end
            return
        end
    end
end)
```

Torch support is the native kind-8/subtype-12 static world operative, not carried
or held inventory torches. Unique private level records, body-free static source,
current sector/scene/player and reach/action gates are required. A bounded 32-node
subtree validates the native flame marker, mesh pointers and ownership. Marker
replacement permits only one leaf with self ownership, no item/controller flags,
no groups or virtual
cleanup. Dynamic sources, ambiguous effects, stale/malformed hierarchies and
unsupported sizes refuse. Native torch size is clamped with engine constants;
this bridge accepts effective sizes at most 2 native units. Scripts cannot supply
positions, flame sizes or duration. The native handler supplies local flame pose,
attachment or marker replacement; native pose propagation and scene registration
complete publication for live use. Registration and source identity are checked.

Extinction targets the validated fire effect, not its source object or mesh. It
uses native deferred scene removal mode 1, retaining engine ownership of removal,
light/audio cleanup and destruction. A full removal ring refuses before a native
call. Queued requests revalidate ownership before execution, check the exact
removal record/dead marker and leave source record/state/definition bytes intact.
A native fault or missing postcondition disables shared writes for the session;
partial native changes are not rolled back. Lua errors discard staged requests.
Pending requests outlive addon reload. `true` means staged, accepted commit means
queued; neither proves visible lighting or completed removal. While native dead
nodes remain attached, recapture refuses until native cleanup is observed.

Inventory Tools adds disabled-by-default `ignite` and `extinguish` actions;
`world_use` also lights a supported unlit torch. Offline fixtures cover these
paths and native faults. Real-game flame placement, audio/light cleanup,
relighting, save/reload and other-mod coexistence still require acceptance.

## Native fire sources (runtime 0.21.0)

`world.read`, `world.devices` and `timers` grant `fire_source:use()` on eligible
nearby native kind-6 subtype-12 sources. Copied metadata is
`interaction_kind = "fire_source"` and `device_kind = "fire"`. Fire support is
discovered independently of scanners and electrical emitters; inventory.read is
not required. Inventory Tools already supports it through `world_use`.

```lua
timers.after(3.5, function()
    local scene = world.snapshot()
    if not scene then return end
    for _, object in ipairs(scene.objects) do
        if object.item_id == intended_fire_source_record
            and object.interaction_kind == "fire_source" and object.use then
            object:use()
            return
        end
    end
end)
```

This is the native ignition action, not an on/off control. It uses the source's
configured unsigned-short size (zero selects the native default); Lua supplies
no coordinates, radius or duration. Supported effective sizes are positive,
finite and no larger than both the native default and 2 native size units.
The native factory owns the selected fire variant, light, particles, damage and
lifetime. No arbitrary particle factory is exposed. The independent torch and extinguishing
controls below extend this contract in runtime 0.22.0.

Only unique private level records qualify. The existing backup, quiet, same-sector,
live-range and protected timer staging gates apply. The shared device queue
rechecks world/scene/player sector, reach/action, source bytes and pose before
execution. Stale requests cancel before any native call. True means staged;
accepted commit means queued. Publication is checked by a newly registered fire
root with native fire-body ancestry, source position and sector/parent relationship,
not the handler's non-Boolean return value. Source bytes must remain unchanged.

The shared pending guard survives Lua reload/disable. After publication a bounded
64-entry native-identity registry suppresses repeat ignition from that source
while its fire remains in the sector tree, even if it later moves away from the
source. Existing native fires within one native unit of the source
also suppress ignition without a registry entry. Identity is checked before
pointer dereference. Slots retire when removal is observed in their sector or
the world/scene changes. Exhausted capacity refuses; the registry is not saved
campaign progress and does not establish a permanent once-only trigger.

Native exceptions, missing/foreign/multiple fire publication, or unexpected source
changes latch staged mutations off. Effects are not rolled back after publication.
Actual rendering, damage, native retirement, save/reload and mod coexistence still
require in-game acceptance.

## Electrical emitters (runtime 0.20.0)

With `world.read`, `world.devices` and `timers`, eligible nearby world rows expose
`interaction_kind = "emitter"`, `device_kind = "electrical"`, copied `active`,
`emitter:set_active(boolean)` and `emitter:use()` (toggle). These are native
kind-6 subtype-13 devices, independent of scanner availability. No inventory
read grant or additional addon DLL is needed.

```lua
timers.after(3.5, function()
    local scene = world.snapshot()
    if not scene then return end
    for _, object in ipairs(scene.objects) do
        if object.item_id == intended_emitter_record and object.set_active then
            object:set_active(true)
            return
        end
    end
end)
```

As with other mutations, true means staged. Successful Lua return queues one
request on the native world-device job queue; it does not prove a visible effect,
damage, completed shutdown or saved persistence. Repeating the settled current
state is an accepted no-op. `active` is the copied native enable bit, not a claim
about visible rendering. Inventory Tools demonstrates `set_active` with its
`active` setting and `world_use` for toggle; it remains disabled by default.

Only unique private level records qualify. Shared templates, malformed emitter
parameters, duplicate ownership, foreign effect classes, retiring effects and
enable/effect disagreement refuse. The existing same-sector/live-range, backup,
quiet and staging gates apply. Effect geometry and allocation budgets are bounded;
this is not a general particle factory or an arbitrary spell interpreter.

The runtime queues the same enable-bit/native effect sequence used by the game's
device dispatcher. A pinned Lua-independent request rechecks world/player/scene,
record bytes, position/direction and attached effect ownership before execution.
Stale requests cancel before writing and report cancellation on the next mutation
readiness check. Native creation, damage behavior, sound and retirement remain
owned by the game. No Lua callback or pointer enters that queue.

Shared staged mutations pause while the request is pending, including after addon
reload/disable. The native payload slot is bounded to one outstanding request and
needs no allocation; the native queue does not free payloads. A stopped effect
must finish native retirement before that emitter accepts another request. Native
exceptions or failed publication postconditions latch mutations off; a queue that
does not execute leaves admission paused. There is no unsafe rollback of native
effects. Real rendering, damage, cleanup timing, save/reload and coexistence need
in-game acceptance.

## Teleporter interfaces and scanner devices (runtime 0.19.0)

| Capability | Method | Native behavior |
| --- | --- | --- |
| `world.teleporters` | `teleporter:use()` | Activate a nearby native teleporter's destination interface |
| `world.devices` | `scanner:use()` | Start a nearby native scanner device/effect |

Both use `world.read` snapshots and successful timer callbacks. Eligible rows
have `interaction_kind = "teleporter"` or `"scanner"` and a callback-scoped `use`
method. These capabilities are independent of `world.use`, `world.doors`,
`world.levers` and `inventory.read`. API 1 and existing addons remain compatible.

```lua
timers.after(3500, function()
    local scene = world.snapshot()
    if not scene then return end
    for _, object in ipairs(scene.objects) do
        if object.item_id == intended_device_record and object.use then
            object:use()
            return
        end
    end
end)
```

Declare `world.read`, `timers` and the intended capability above. Select an
actual scene-local record and probe method presence. As with other commands,
true means staged; check commit logs for native acceptance. Inventory Tools
already supports these targets through `world_use` and `world_item_record`.
The example remains disabled until configured.

Teleporter use calls native device-kind-2 subtype-15 activation. It opens the
game's destination interface; players choose destinations there. Lua does not
supply travel coordinates, select an arbitrary destination or claim arrival.
The native interface/job owns destination selection, animation, travel and
crystal consumption. Activation may create a private world record and change
the device's raw ID, so query fresh snapshots afterward.

The native socket child and first sector object within its small socket radius
must identify an installed eligible crystal. Teleporters accept native raw
crystal IDs 0x1af/0x58f with charge byte below 0x55; scanners require raw ID 500.
These are intrinsic native device requirements, not game version checks. A key
or crystal merely carried in inventory is insufficient. Charge bits are not
treated as generic removed-item flags; activation preserves charge for the
native interaction to consume later. The native player-to-device check is
stricter than the inherited 200 cm world-operation limit.

Scanner use calls native subtype-18 activation. The engine creates and owns the
attached scanner effect, its callbacks and cleanup. Repeated activation while
that effect exists refuses. This exposes scanner activation, not arbitrary
character effects, pedestal spells, story triggers or general device dispatch.

The existing staged inventory owner performs both operations. Before commit,
recheck current world/player/sector, definition bytes, socket and crystal
identity/state/position, native busy state, readable bounded lists and owner
quiet/backup/queue conditions. Teleporter publication must return the matching
native window with source/crystal/private-definition ownership; scanner
publication must produce the matching attached effect and native armed state.
An unchanged native refusal stays a refusal. A fault, failed postcondition or
refusal after record cloning latches mutations off without unsafe rollback.

Native interactions retain pointers after the Lua callback. A shared mutation
guard therefore blocks staged NPC, container and inventory/world writes while
the teleporter window or requested scanner effect remains active, including
after script reload/disable. Timers, settings and copied observation continue.
Normal mutation availability resumes after native close/removal is observed;
unreadable state after an owned activation remains blocked. No Lua-owned native
payload or additional hook/launcher is introduced.

Offline fixtures establish activation/refusal and lifetime guards only. Native
window rendering, chosen-destination travel, scanner visuals/results, crystal
consumption, save/reload and other-mod coexistence remain in-game acceptance.

## Sliding/paired doors and light-control levers (runtime 0.18.0)

Existing `world.doors` and `world.levers` capabilities gain more supported
native mechanisms. No new launcher, hook, version comparison or capability name
is required. Reading snapshots still requires `world.read`; requests still use
one successful timer callback and the existing owner/backup/quiet/range gates.

`door:set_open(boolean)` now supports ordinary sliding doors, mechanical doors
and native paired leaves. Eligible rows expose `door_kind` (`hinged`, `sliding`,
or `mixed`) and `door_leaves` (1 or 2), alongside `closed` and `set_open`.
`closed` means all selected leaves have their native latch engaged; it does not
measure angle/travel. Both physical and record lock states must be clear.

Paired leaves must be sibling door bodies under the same world record with
reciprocal native pair links. Native descriptor ancestry, body/child/parent/raw
record identity, same sector, both lock states, geometry/slider constraints,
definitions and copied control state are checked before either leaf changes.
One locked or stale partner refuses the whole request. Extra physical leaves,
foreign bodies, broken pairs and special fixed constraints refuse. Pair writes
are sequential, not atomic: if the second native call fails, the first leaf can
already have changed. Mutations then latch off without unsafe rollback.

Sliding requests use the engine's own slider constraint/update contract, never
the hinge angle target. Native latch release owns opening initialization and
the correct command duration (80 ticks for ordinary sliding, 300 for mechanical
sliding); native command 8 owns closing. A nonmechanical slider already unlatched
is treated as already open/requested. Reversing its active close request before
it latches is refused, because the reviewed native open path only initializes
a latched slider. Mechanical sliders can be retargeted through their native
driven open command. Fast-close flags retain the native 100-tick close duration;
other closes use 300 ticks. Native physics retains motion and auto-lock behavior.

Subtype-6 light-control levers now use the same `set_position(0 or 1)` and `use()`
methods as ordinary levers. Their rows expose `light_control = true` (false for
ordinary levers). The embedded native light pointer must equal the first light
found in the lever's bounded, cycle-checked child tree and remain attached at
commit. Native lever physics owns light transitions and any linked outputs;
the Lua request does not directly overwrite lamp state or prove it changed.
Endpoint inversion remains native: endpoint 1 is not universally logical on.

Inventory Tools already accepts these targets with `set_open`, `set_position`
and `world_use`. Configure their scene-local `world_item_record`; source and
examples ship together. Native signature checks for sliding, pairing and light
levers are independent; unsupported mechanisms lose their methods without
disabling supported ordinary controls. Teleporters, arbitrary logic dispatch,
floor-plate simulation, continuous dragging and general light edits remain
outside this interface. Compound child-body wake remains refused.

The historical 0.17 limits below describe that version; the reviewed extensions
in this section supersede its sliding/paired/mechanical/light-lever exclusions.
Offline fixture/startup/package passes establish request and refusal contracts,
not physical motion, linked outcomes, rendering, save/reload or coexistence.

## Physical levers and hinged doors (runtime 0.17.0)

Two independent optional capabilities extend the same callback-scoped world
rows. Both require `world.read` for snapshots and a successful timer callback.
Neither requires `inventory.read`, `world.use`, ExaniMate or ExCamera.

| Capability | Method | Native request |
| --- | --- | --- |
| `world.levers` | `lever:set_position(0 or 1)` | Request one of the two physical travel endpoints |
| `world.levers` | `lever:use()` | Toggle the captured requested/nearest endpoint |
| `world.doors` | `door:set_open(boolean)` | Open or close an ordinary unlocked hinged door |

Lever rows have `interaction_kind = "lever"`, `lever_position` (copied normalized
physical travel, 0–1) and `target_position` (0 or 1). Endpoints describe physical
travel; the device's native inversion flags still determine its logical output.
Do not interpret endpoint 1 as universally on. A current override determines
`target_position`; otherwise it is the nearer endpoint. `use()` toggles this
target, including while moving. Repeating the same endpoint refuses cleanly.
Only integer 0/1 arguments are accepted; fractional values and booleans error.

```lua
timers.after(3500, function()
    local scene = world.snapshot()
    if not scene then return end
    for _, object in ipairs(scene.objects) do
        if object.item_id == desired_lever_record and object.set_position then
            object:set_position(1)
            return
        end
    end
end)
```

Declare `world.read`, `world.levers` and `timers` for this example; choose an
actual desired scene record. Records are scene-local, snapshots may be partial,
and methods are only offered in admitted timer callbacks. Door rows offer
`set_open` and `closed` only when eligible. `closed` is the native latch state,
not an angle measurement or completed close animation. A freshly key-unlocked
door may remain ineligible until the engine finishes clearing its lock state.
Closing can engage the door's normal latch/auto-lock behavior.

Lever requests use the engine's endpoint override fields from its native timed
return path, then native body wake. They do not fake a held mouse, move physics
points or call links directly. Native updates retain movement, sounds, linked
devices, inversion and automatic return timing. Held levers, an active return
countdown, forced-return flag 2, subtype-6 light mechanisms, invalid travel and
angles outside the reviewed interval are refused. General dragging, continuous
angles and special lever mechanisms are not exposed.

Door requests reuse native latch release, command publication and wake. Open
uses the ordinary hinge's target/command 4; close uses target/command 8. Require
both root and physical lock state clear: this cannot unlock or force a locked
door. Paired, sliding, mechanical and noninteractive doors are refused. Commands
already requesting the same action are clean no-ops. Native physics may be
blocked; a successful request is not observed motion or completion.

The inherited eight-target, same-sector 200 cm, current identity/definition,
save-backup, UI/drag/combat/queue and one-command-per-callback gates apply. The
native wake graph is bounded/cycle-checked; compound child-body wake requiring
unreviewed virtual dispatch is refused. Commit rechecks captured lever controls
or door state; a stale request cannot overwrite a newly held/timed mechanism.
No new hook, delayed Lua callback, version equality gate or extra executable is
introduced. Partial writes/native faults latch mutations off without unsafe
rollback. Presentation execution does not establish simulation locking.

Inventory Tools adds `set_position` (`world_item_record`, integer `position`),
`set_open` (`world_item_record`, boolean `open`) and supports lever toggle through
`world_use`. Defaults remain disabled with zero IDs. Offline fixtures establish
request/refusal contracts only; physical movement, links, UI, save/reload and
other-mod coexistence still require game acceptance.

## Item use and world interactions (runtime 0.16.0)

Two independent capabilities extend the existing staged inventory owner:

| Capability | Callback-scoped method | Supported behavior |
| --- | --- | --- |
| `items.use` | `inventory.snapshot().carried[i]:use()` | Apply native healing salve/elixir effect to the player, then consume that exact carried row |
| `items.use` | `world_object:use_item(carried_row)` | Unlock a nearby keyed door using its actual matching carried key; retain the key |
| `world.use` | `world_object:use()` | Open an unlocked ordinary world container, or press a manual button |
| `world.use` | `world_object:set_pressed(boolean)` | Request native manual-button press/release state |

Reading carried rows requires `inventory.read`; reading world rows requires
`world.read`. World interactions internally capture inventory ownership without
requiring that addon to hold `inventory.read`. Probe `api.has_capability` and
method presence; operation availability depends on its own native contracts,
not a game/mod version comparison. Lua API remains 1.

Eligible world rows include `interaction_kind` (`door`, `container`, `button`).
Doors expose `locked` and raw `required_key`; containers expose `locked` and
buttons expose `pressed`. There are at most eight interaction targets per
presentation pulse. Nearby means within 200 cm using current live positions,
in the player's sector; no line-of-sight or surface test is supplied. Ordinary
world row `item_id` remains the masked scene record; carried IDs/required keys
are raw IDs. Mutating these copied fields does not change the bound target.

```lua
timers.after(3500, function()
    local inv, scene = inventory.snapshot(), world.snapshot()
    if not inv or not scene then return end
    for _, object in ipairs(scene.objects) do
        if object.use_item and object.locked then
            for _, key in ipairs(inv.carried) do
                if key.item_id == object.required_key then
                    object:use_item(key)
                    return
                end
            end
        end
    end
end)
```

Declare the capabilities above plus `timers` for this example. Select intended
targets in a real addon; world snapshots can be partial. Only actual ordinary
key devices are accepted; knowing `required_key` cannot bypass carrying a key.
This method uses carried keys, not contents nested inside bags/keyrings.

Methods stage one operation per successful timer callback. A true return means
staged, not committed. Retained methods expire with that callback. Lua failure
discards staging; do not mix these with NPC/container mutation batches. Existing
save-backup, settling, quiet UI/drag/combat, queue and stale-ownership gates apply.
World target definitions, parent/body identity, sector, distance and captured
state are checked again before calling native code. Wrong keys, spent salves,
locked/already-open containers and unsupported devices refuse cleanly.

Consumables currently support native device kind 1 subtypes 8 and 14 only.
The native effect helper receives a short-lived context borrowing the player's
real native view, with source/GUI row unset; it applies the engine-owned effect
without GUI source bookkeeping. The existing inventory owner then removes the
exact selected row and recounts/notifies. Duplicate IDs retain distinct rows.
This adapter avoids fabricating GUI state; replace it if a reviewed native
exact-row caller can own both effects and consumption. It is not atomic:
effects can precede a later fault. Exceptions or failed postconditions latch
mutations off; no unsafe effect/database rollback is attempted.

Button calls change the native pressed request; native physics owns travel,
auto-reset and linked-device effects. They do not directly open linked doors or
prove a puzzle completed. Container calls open the native window; acceptance
does not prove rendering. Logs distinguish accepted world interactions from
inventory changes. No general device dispatch, physical lever dragging, floor
plate simulation, teleportation, brushes, repair or crafting is exposed.
Presentation-thread checks do not establish simulation locking. Native motion,
UI rendering, save/reload and other-mod coexistence still require game testing.

Inventory Tools adds `use` (`item_id`), `use_item` (`item_id` and
`world_item_record`), `world_use` (`world_item_record`) and `set_pressed`
(`world_item_record`, `pressed`). Defaults remain disabled and IDs zero.

## Weapon removal, shields and direct bag transfers (runtime 0.15.0)

New capabilities are independent `weapons.unequip` and `shields.equip`. Both use
`inventory.read` and the existing successful-timer staging contract:

```lua
local player = inventory.snapshot()
if player and player.weapons[2].unequip then
    player.weapons[2]:unequip() -- 1 = right, 2 = left; active slots only
end
-- Separate callback: player.carried[index]:equip_shield()
```

Unequip retains the item in carried inventory and clears its hidden/equipped
bit through native inventory bookkeeping. A native simulation-queue action
removes the physical weapon/shield, updates saved slots, and refreshes the actor.
The engine may promote a remaining offhand weapon into the primary hand when
removing the primary weapon. This is native behavior, not a request to empty
both hands. Alternate loadout rows have no unequip method. Empty hands, stale
physical/body ownership, ambiguous IDs, and special held props are refused.

The runtime supplies a pinned, Lua-independent queue callback around the native
removal operation solely for payload cleanup and delayed fault signalling. It
rechecks player/world/scene/stats/selected-body identity before execution, calls
no Lua or logger from the simulation callback, and continues after addon reload
or disable. It never writes hand slots or physical constraints itself. A changed
owner or native callback fault stops later mutations on the presentation thread.
Native completion and save/reload still require game acceptance.

Shield equip uses native shield construction, attachment and queued completion.
It targets the left hand, can replace an existing ordinary offhand item, and
restores displaced inventory visibility. Mutable IDs may be instanced. Native
shield prerequisites are respected; two-handed or special primary weapons,
ambiguous ownership, busy queues, and hidden/removed shield rows are refused.
There is no automatic primary-weapon removal to force a shield combination.
A positive Lua return is staged; commit acceptance is not completed animation.
Inventory access waits for pending native action queues as with weapon equip.

`inventory.bags` additionally supplies direct contents transfer:

```lua
local player = inventory.snapshot()
if player then
    local source = player.carried[source_index]
    local destination = player.carried[destination_index]
    source:transfer_to(contents_index, destination)
end
```

The one-based contents row moves directly between two closed, directly carried
bags, with full item state and stored XY coordinates. Player inventory is never
used as an intermediate container, so its spare capacity is irrelevant. Native
destination bag volume/per-item limits still apply. Both captured records and
the complete carried/worn/weapon snapshot must remain unchanged. Same-bag,
nested-container, special-item, hidden/removed and stale-owner transfers are
refused. Method tables and retained handles cannot redirect native ownership.

Shared source/destination templates are independently cloned on first edit,
even when both carried rows use the same template ID. Only those selected rows
receive private IDs; original templates/other copies remain unchanged. Instance
metadata is retained. Database slots and bytes are re-resolved before each
publication because cloning/replacement can relocate database storage.

Destination publication precedes source removal. This is **not an atomic engine
transaction**: failure of the second publication can leave a duplicated item.
The runtime then latches mutations off and reports quit-without-saving/backup
recovery guidance; it does not attempt unsafe database rollback. Offline fault
fixtures cover that outcome. Native save/reload and grid presentation are still
unverified; automatic grid packing is not provided.

Inventory Tools adds `unequip_weapon` (`hand`), `equip_shield` (`item_id`) and
`bag_transfer` (`bag_item_id`, `destination_bag_item_id`, contents `item_id`).
Defaults remain disabled. Use different raw instance IDs for source/destination
in this example; direct Lua row selection can also distinguish identical shared
template bags. One command per callback and one hand/bag/world publication per
pulse, backup/settling/UI/drag/combat gates, and partial-fault latching still apply.

## Specific weapons and carried bags (runtime 0.14.0)

Declare `inventory.read`, `timers`, and the independent `weapons.equip` or
`inventory.bags` capabilities. Neither needs world/NPC inspection or the editor.

```lua
-- Each example runs in a separate successful timer callback.
local player = inventory.snapshot()
if player and player.carried[1] and player.carried[1].equip_weapon then
    player.carried[1]:equip_weapon("right") -- or "left"
end
```

`equip_weapon` uses the native queued physical weapon equip path, native
allocator and displaced-item callback. The selected inventory row retains
ownership with its native hidden/equipped bit set. Native completion clears
that bit on displaced weapons and owns slots, grips, constraints, refresh and
freeing. Disabling/reloading Lua does not invalidate its callback. Mutable
records can receive a new native item ID. True means staged; the commit log
reports request acceptance, not animation completion. Inspect fresh slots later.

Right requests accept native one-/two-handed weapons. Left requests require a
one-handed weapon, an occupied one-handed primary slot, and the native dual-wield
skill (ID 8). Empty-primary left requests are refused because the engine redirects
them to its primary slot. Shields, bows/special props, hidden/removed rows,
already referenced weapons and ambiguous selected/displaced IDs are refused.
This is not a force-equip or alternate-loadout editor. While `weapons.equip` is
available, inventory access also waits for both native action queues to drain.

```lua
local player = inventory.snapshot()
if player then
    local bag = player.carried[1] -- choose by raw item_id
    local rows = bag.contents and bag:contents()
    if rows and player.carried[2] and player.carried[2].store_in then
        player.carried[2]:store_in(bag)
    end
end
-- Another callback: bag:take_item(one_based_contents_index)
```

`inventory.bags` supplies `contents`, `take_item(index)` and `store_in(bag)` on
carried rows without `inventory.transfer` or `world.read`. Non-bags return nil
from `contents` and refuse transfers. Contents copy raw IDs/state and a `removed`
flag. Four contents queries and eight privately captured bags are allowed per
callback/pulse respectively. Methods expire after the callback; editing Lua
copies does not redirect a native target.

Closed level-local and persistent-instance bag records use the existing native
record publisher. Shared templates are cloned through the native instance path
on first transfer. Only the selected carried bag row receives that private ID;
other copies and the template stay unchanged. Native clone metadata is retained,
and cloned contents must match the captured source before editing. Shared bags
containing nested containers are refused. An existing instance ID must have one
carried owner.

Transfers move existing ordinary items with full state, captured inventory/record
conflict checks, and native volume/per-item limits. Storing retains coordinates;
taking uses native append coordinates (40, 4). Self/nested-container transfers,
open/busy bags, weapon/worn references, special devices and hidden/removed source
rows are refused. This does not physically pick up/drop bags, recursively search them, or
automatically pack inventory grids. Direct transfers follow in 0.15.0.

One command per successful timer callback, staging rollback on Lua errors,
backup/settling/UI/drag/combat/builder gates and partial-fault latching apply.
Hand/bag/world requests share one publication budget per pulse. Native transfers
are not engine transactions or simulation locks. Fixtures do not establish
in-game hand orientation, inventory layout, save/reload or coexistence acceptance.

Inventory Tools adds `equip_weapon`, `bag_store` and `bag_take`; configure `hand`,
raw `bag_item_id` and raw `item_id`. It remains disabled by default. Shared bags
change ID when instanced: inspect fresh inventory before later operations.

## Weapon switching and loose items (runtime 0.13.0)

`inventory.snapshot()` additionally copies `weapons` and `alternate_weapons`,
each two `{item_id, state}` native slot rows (zero ID means empty), and
`position = {x, height, z}`, `space = "player_sector_cm"`. These coordinates
describe the player's body centre in its current sector. They are distinct
from world.snapshot's saved item coordinates.

Declare `weapons.switch` with `inventory.read`:

```lua
local player = inventory.snapshot()
if player and player.switch_weapons then player:switch_weapons() end
```

This requests the native active/alternate weapon-set switch used by the game
input handler. It does not select an arbitrary inventory weapon or overwrite
hand/constraint/slot fields. Native action timing, animation and attachments
remain authoritative. The log `Weapon request accepted; completion is native` confirms a request, not completion. Snapshot access is withheld during
pending/active switch actions; inspect the resulting slots when access resumes.
Equal active/alternate ID pairs are refused. This initial API uses the existing
quiet, outside-combat inventory gate.

Declare `world.read` and `world.pickup` for callback-scoped `object:pickup()`
methods on eligible loose world-item rows. A positive staged result moves that
exact node's item ID/state into carried inventory and removes its scene node
through the native mode-0 removal path. Native cleanup handles physics/model
ownership; the runtime never frees a node itself. Parent membership is checked
after removal without dereferencing a potentially destroyed node.

Declare `inventory.read` and `world.drop` for carried-row drop:

```lua
local player = inventory.snapshot()
if player and player.carried[1] and player.carried[1].drop then
    local p = player.position
    player.carried[1]:drop({p[1] + 50, p[2], p[3]})
end
```

The destination must contain three finite player-sector coordinates within
200 cm of the player's current body centre. Native mutable-record conversion
may assign a level-local ID; item state is retained. Native inventory-to-world
creation, pose/physics initialization, scene registration and queued physics
release run before the selected carried row is removed. This drops an existing
carried row; it is not arbitrary item spawning. Construction returning no node
refuses the operation and retains the carried row.

Pickup also requires the same sector, a current live node position within
200 cm, valid physical item body, reachable scene/parent membership, unchanged
ID/state/body/sector, and room in player inventory. Native scene removal-queue
occupancy is bounded. Character-held items, equipped/alternate-slot references
(for drop), doors, containers, special devices/torches and entry markers are
refused. Ordinary physical items with unsupported models/bodies can be refused
even when a Lua method exists. Bags remain excluded from physical pickup/drop,
and held-weapon throwing remains unsupported. See the carried-item API above
for bag contents and specific weapon requests.

These APIs do not perform a cursor raycast, surface-placement test or wall
visibility test. Choose appropriate targets/destinations in addon policy;
native physics resolves a released item's subsequent motion. Successful enqueue
does not prove it has fallen, settled, switched weapons or saved successfully.

The existing one-operation-per-successful-timer-callback contract, independent
capability grants, backup, UI/drag/combat/job gates, private expiring tickets,
full carried/worn/weapon snapshot comparisons and fault latch remain. One new
weapon/world command may publish per admitted presentation pulse across addons;
another command must wait for a later pulse. No NPC-edit capability is needed.
Any partial publication fault disables mutations and reports quit-without-saving
recovery guidance. These calls provide no simulation lock or engine transaction.

Inventory Tools now supports `switch`, `pickup` and `drop`. `world_item_record`
selects the world snapshot's masked item ID; `item_id` selects the carried raw
ID. `drop_offset` is relative to copied player-sector position. Defaults remain
disabled with zero IDs. It makes one attempt per observed session entry.

## Player transfers and apparel (runtime 0.12.0)

Declare `inventory.read` for `inventory.snapshot()`. It returns independent
`{carried = {...}, worn = {...}}` arrays of `{item_id, state}` rows, or nil
when unavailable/busy. `worn` is apparel, not weapon hands. Four queries are
allowed per callback. IDs/state are native records, not stable instance handles.

Declare `inventory.transfer` and `world.read` to move existing rows between
the player and closed level-local world containers:

```lua
local player = inventory.snapshot()
local chest = world.snapshot().objects[1] -- select the intended container
if player and chest.take_item then
    local rows = chest:contents()
    if rows and rows[1] then chest:take_item(1) end
end
-- In a separate callback: player.carried[1]:store_in(chest)
```

`take_item` selects a one-based index in the captured container contents;
`store_in` selects the exact carried row bound to its method. Duplicate IDs
with different state remain distinct. Item ID and full state move unchanged;
no template copy is granted. Native append assigns coordinates (40, 4), and
stored rows retain carried coordinates. Automatic grid packing and real-game
inventory presentation remain unverified. Transfers/contents do not require
`containers.edit`.

Transfers check native container total-volume/per-item limits and the player's
400-volume limit/current maximum item size. They refuse removed/hidden rows,
nested container items, and storing anything referenced by worn gear or either
weapon/quick-slot pair. Carried bags and loose-item pickup/drop use the separate
capabilities above.
Distance to a chest is addon policy; this API does not enforce proximity.

Declare `equipment.write` alongside `inventory.read` for
`player.carried[i]:equip()` and `player.worn[i]:unequip()`. Only apparel records
with a native 0x1000 chunk are supported. Native fit resolves slot conflicts and
returns displaced apparel, including state, to carried inventory; native
mesh/stat rebuild follows. Ambiguous duplicate apparel IDs are refused.
Weapons, shields, held props and hand/action switching need separate operations.

Methods exist only on timer-callback snapshots and expire after that callback.
Stage **one inventory/equipment operation per callback**. True means staged,
not applied; a caught second-operation error leaves the first staged operation
intact. Callback failure discards staging. Never mix this with NPC/container
edits in one callback. Check the addon-tagged `Inventory operation applied` /
`refused or faulted` log after Lua returns.

Commit rechecks world, scene, player, stats pointer, full captured carried/worn
rows and (for transfers) exact container bytes. Save backup and the existing
settling gate are mandatory. Open windows, foreign UI capture, inventory/world
dragging, combat/down state and pending native builder/completion jobs prevent
access. Private tickets expose no pointers or persistent entity handles.

Container publication uses the native record replacer; carried ownership uses
native append or the native array's reviewed compaction pattern. Equipment uses
native fit/add/remove/rebuild. These are not an engine transaction or simulation
lock. A fault after publication starts can leave partial state; mutations latch
off with a quit-without-saving/backup diagnostic. No partial operation is retried
or blindly rolled back over native state. Successful changes may persist in
native saves. Real inventory layout, worn appearance, save/reload and coexistence
remain gameplay acceptance checks.

Inventory Tools defaults to disabled/zero IDs. Configure one action (`take`,
`store`, `equip`, `unequip`) and item/container IDs. It tries once after each
observed session entry; it is not campaign-once tracking.

## Edit world container contents (runtime 0.11.0)

Declare `world.read` and `containers.edit`. During a timer callback, editable
world.snapshot item rows expose methods:

```lua
local rows = object:contents()
object:add_item(123)       -- objdb item template ID
object:remove_item(123)    -- remove one matching serialized row
```

contents returns independent `{item_id, state, removed}` rows from the captured
live container record, or nil when unavailable. It does not preview staged
changes. Four reads and sixteen edit operations are allowed per callback.
Add/remove accept positive integer objdb IDs below 0x40000000 and require a
resolvable item look chunk at commit. They operate on item rows, not quantities,
equipment slots or an existing carried instance. Adding grants a new row;
removing does not move the item into the player's inventory.

This first inventory mutation domain is closed, level-local world containers
(native record tag 0x80000000). Shared templates, carried bags/player inventories,
open containers and any open game window are refused. A missing method means
the object cannot be edited now. Root/player/scene stability, a successful save
backup and callback-scoped private tickets use the existing mutation gates.
Native container capture is bounded to eight candidates per pulse and inherits
the bounded world snapshot's discoverability limits.

All edits in a callback must target one container. The runtime copies its
current record, applies every requested operation through ExaniMate's canonical
chestEdit helper, then calls the game's record-replacement path once. Names,
descriptions, unrelated chunks and existing row state/coordinates are preserved.
No old snapshot is restored to remove loot that has already been taken: a
missing row refuses the batch. remove_item follows the canonical last-matching
serialized-row rule, so inspect removed/state flags when choosing an item ID.
The supported path does not enforce the drag-and-drop volume limit.

Methods return true for staged requests, not confirmed publication. Callback
failure discards the entire container batch; commit/refusal is logged afterward.
Identity, scene reachability, record bytes, item validity and window state are
rechecked before publication. Changed records refuse rather than silently
overwrite another mod's changes. A native publication fault or failed readback
disables further container/NPC writes, without attempting an unsafe database
snapshot restore. Quit without saving after such a fault and use the backup
if needed. This is not an engine-atomic or save-integrity guarantee.

Container and NPC mutations must use separate callbacks; attempting to mix them
raises an error. Persisted addon preferences remain a separate domain. Container
methods expire after the callback that created them, even if a row is retained.
Successful publication consumes its native tickets. Container edits can persist
in the game's save; disabling/reloading the addon does not undo granted loot.

Reward Chest is a configurable opt-in example: edit its declared config module
with a level-local container record and objdb item ID, then enable it for a
disposable session. It checks current contents before adding a missing reward.
It is not campaign-once tracking: after loot is taken, another session entry
can grant it again. Gameplay, UI placement, persistence and coexistence still
require actual-game acceptance. This API does not yet provide player inventory
transfer, equip/unequip, loose-world spawning or object movement.

## Change NPC attitude toward the player (runtime 0.10.0)

Declare `characters.attitude.write` with characters.read or characters.awareness.
Editable snapshot rows expose a callback-scoped method during timers:

```lua
character:set_attitude {hostility = 0, concern = 0}
```

Supply either field or both. Hostility accepts finite native values 0..20;
concern accepts 0..15. Omitted fields stay unchanged. Empty tables, out-of-range
values and duplicate writes to the same field in one callback raise errors.
Separate hostility and concern calls can merge, as can set_ai with set_attitude
when both capabilities are granted. All requests share the eight-NPC callback
limit and one native batch. An unhandled callback error discards them all.

This reuses ExaniMate's Live NPC attitude path: hostility updates the existing
player-memory value, while concern calls the engine's own setter to preserve
reaction/crossing flags. No memory entries are created. An NPC that does not
currently remember the player rejects the whole batch at commit, even if its
row had an edit method. No actor is forced to stop attacking, flee or become a
companion; native AI can change these values on its next update. Use awareness
to inspect the subsequent state. Encounter Truce is a one-shot opt-in example
that reduces both values for up to eight living NPCs with readable memories.

Attitude capability checks the setter calling contract and relevant constants
independently of the skill-edit contract. It works when characters.ai.write is
unavailable. Readiness, save backup, three-second settling, private tickets,
callback expiry, target identity checks and commit/refusal logging follow
set_ai. True from the method means staged, not committed. Both mutation APIs
preflight the whole batch and revalidate the matching player-memory entry
before stores. On a write/setter fault, rollback also attempts to restore prior
concern and crossing flags; further NPC writes are disabled for the process.

These edits inherit the editor's presentation-thread/simulation concurrency
limitations. They are not engine-atomic and memory replacement by AI remains
an inherited risk. Rollback is best-effort, not a save-integrity guarantee.
Actual encounter behavior, saves and coexistence remain unverified in-game.

## Change NPC AI (runtime 0.9.0)

Declare `characters.ai.write` plus characters.read or characters.awareness.
During a timer callback, editable NPC snapshot rows have a `set_ai` method:

```lua
character:set_ai {skill = 40, attribute2 = 0, attribute3 = 0}
```

This uses the same AI-input validation, constructor arithmetic and six field
offsets as ExaniMate's Live NPC editor. Skill uses actor-file units 0..100;
attributes 2 and 3 use -100..100. All three finite numeric values are required.
Attribute 2 feeds courage and hostility weighting; attribute 3 affects hostility
build-up. The operation does not assign a behavior mode, force an attack, create
a player memory or edit the actor template. Practice AI is an opt-in example
that changes up to eight living NPCs once per observed session entry.

The method appears only after a save backup succeeds at the settled title menu
and the same player/world/scene has remained observed for three seconds, with
no recently changed conflicting travel name. No method means not editable now;
retry from a later timer. Players and unverified AI objects never get a method.
Capability availability alone does not guarantee readiness.

Requests are staged, at most eight distinct NPCs per callback. They commit only
after that timer returns successfully. Errors discard all staged NPC edits.
True from set_ai means staged, not applied: the runtime log records the batch
commit/refusal. Caught argument errors do not discard earlier valid requests.
Retained rows/methods expire at the end of their creating callback; fresh queries
are required. Methods are unavailable in load, phase and item-use callbacks.

Native commit rechecks world/player/scene, current-list membership, AI class and
back-pointer, stats pointer, actor name and record ID, constants, finite derived
values and writable destinations. It prepares the entire batch before its first
store. These are stale-target safeguards, not persistent entity identities.
Successful commits invalidate the runtime's character caches for later queries.
Disable discards tickets; it does not restore already applied NPC changes.

This inherits the editor's presentation-thread field-edit mechanism. It does
not lock the simulation or promise an engine-atomic six-field update. A write
fault attempts rollback, disables further AI writes for the process and reports
failure; rollback is best-effort. Quit without saving after such a fault and use
the backup if needed. No gameplay/coexistence acceptance has been performed.
Treat changes as live session state; this API creates no persistent template,
campaign storage or automatic reapplication on newly spawned NPCs.

## NPC awareness (runtime 0.8.0)

Declare `characters.awareness` and call `characters.awareness()` during an
admitted session timer. This returns the same copied body rows as
characters.snapshot, with an optional `awareness` table for each verified
TMotileAI. It works without characters.read. Body-only snapshot calls never
include awareness and do not trigger AI reads. The player and rows without a
verified AI class/back-pointer omit awareness; absence does not prove death.

Known fields in awareness are:

- `engagement`: none/wary/provoked/fight/flee (native levels 0 through 4).
- `mode`: wander/stand/dormant/follow/lie_down/short_wander.
- `handler`: idle/engage/flee/fetch/dormant/wander/follow/lie_down/short_wander/player_brain/other. `player_brain` is the player character's own explore routine, seen on a character the player has controlled. `lie_down` also follows a knock-down, so read `mode` for the NPC's resting behaviour.
- `attack`: swing/overhead/thrust/other; `opposite_side`, `wants_attack`,
  `casting`, `asleep`, `returning` and `fights_player`: booleans.
- `skill`, `stress`, `shock`, `courage`, `alarm` and derived `fear`
  (`max(0, stress + shock - courage)`): finite native numeric values.
- `toward_player`: an optional table of finite trust, hesitation, interest,
  concern, hostility, danger and closeness values from the NPC's existing
  player-memory entry. These are native units, not percentages or guarantees
  of the NPC's next action.

Unknown/unreadable fields and unrecognized mode/engagement values are omitted.
The canonical reader bounds AI memory lookup to 12 entries. No memory table or
target pointers reach Lua. Engagement/attack state is an observation, not a
damage, attack or perception event. Row IDs still are not stable handles.
The separate lazy awareness cache runs at most once per presentation pulse;
awareness and body snapshot calls share the four-query execution quota.
The existing 128-row cap and partial-membership semantics apply. A missing
toward_player table means no readable matching measurements, not proof that
the NPC has never noticed the player. Awareness Observer demonstrates an addon
that logs encounter state without the keyring or editor service.

This directly uses ExaniMate's NPC brain reader, including class and character
back-pointer checks. It calls no engine operations and changes no AI state.
Gameplay, telemetry interpretation and performance remain unverified in-game.

## Character inspection (runtime 0.7.0)

Declare `characters.read` to call `characters.snapshot()`. Like world.snapshot,
it is available only during an admitted session presentation pulse, normally
from a timer scheduled in a runtime.phase handler. It returns nil if the world
or character-list metadata is unavailable/invalid. A valid empty list returns
an empty snapshot, not nil. Character Observer is a complete example.

The result is `{space="scene_local_cm", truncated=boolean, characters={...}}`.
Rows contain `name`, `record_id` and a three-element `position` array copied
from the character's current body centre. These are scene-local centimetres,
not global coordinates or the saved poses returned by world.snapshot.
Nonprintable/unavailable names are empty; unavailable record IDs are zero.
Record IDs are level data, not unique entity handles or campaign identities.

When readable, `is_player`, `dead`, `down` and `has_health_pool` are booleans.
Unknown fields are nil. Finite body values appear as `health`, `health_max`,
`balance`, `balance_max`, `stamina`, `stamina_max`, `focus` and `focus_max`.
Values are native units, not percentages. Health and balance share the native
maximum; calculate a ratio only when a readable maximum is positive. Health
and health_max are omitted when the body has no health pool or its pool flag
is unreadable. Other unreadable/nonfinite measurements are omitted as well.

This reuses ExaniMate's canonical NPC list/body reader without overlay role or
head-label work. It reads no AI memory, calls no engine function and exposes no
pointers or mutations. At most 4096 list entries are examined and 128 characters
returned; duplicate, invalid or omitted rows mark the snapshot truncated.
Optional unknown measurements do not by themselves mark membership partial.
Never infer death/removal merely because a row or measurement is absent.
The native copy is cached lazily for one presentation pulse across addons;
every Lua call returns independent tables, with four calls allowed per protected
execution. An availability capability is not a world-ready guarantee.

The loader probes only the character-list operation using the editor's existing
signatures, with no version/hash allowlist. Unavailable character inspection
does not disable world inspection or other addons. Real-game acceptance remains
unverified; this API currently supports observation, not actor control or events.

## Level scene inspection (runtime 0.6.0)

Declare `world.read` to call `world.snapshot()`. It returns nil outside an
admitted session presentation pulse or when the level root cannot be read.
Call from a timer scheduled by a session phase handler; the phase notification
itself precedes the scene query window. Capability availability means the
operation contract is supported, not that a world is ready.

The result is `{space="level_saved", truncated=boolean, objects={...}}`.
Each object has `kind` (item/object/light), `name` (placed object model name,
otherwise empty), `item_id` (loose item ID, otherwise zero), and `position`
as a three-element numeric array. Positions are saved level-local translations,
not global distances or live physics poses. Carried items and characters are
not enumerated. An item ID is not an entity handle or durable identity.

This reuses ExaniMate's level-tree classification and shared field definitions.
No pointers, mutation methods or stable handles are exposed. Returned tables
are independent copies; retaining one does not keep an engine object alive.
The native scan is lazy and cached for one presentation pulse across addons.
It visits at most 8192 nodes and returns at most 512 entries. Unreadable nodes,
cycles, invalid children, nonfinite positions or limits mark the result partial
with `truncated=true`. Never treat absence in a partial result as proof of removal.
Each protected script execution may request at most four snapshots.

See the World Survey example for a general scene inspection addon that requires
neither the keyring nor the editor service. Actual game behavior is unverified.

This API is distinct from ExaniMate's existing editor/plugin API v1. It adds
in-process callbacks; it does not replace or modify that frozen API.

An enabled EMTK addon includes a script section in manifest.toml:

```toml
[script]
runtime = "com.necrosis.script-runtime"
entry = "scripts/main.lua"
requires = ["events.item_use", "items.read", "containers.read", "containers.open", "doors.unlock"]
```

`api.version` is 1. Capabilities describe API operations, not game-version
allowlists. Unknown capabilities reject loading. The entry must resolve inside
the addon directory and contain text Lua source, at most 1 MiB.

`events.on("item.use", model_name, handler)` registers during addon load. Model
names are native model identifiers of at most 16 bytes. Handlers run in profile
order and registration order until one returns true. False/nil forwards to the
next handler, then the original game handler. Other return types are errors.

The event contains copied `item.id`, `item.model`, `item.contents`,
`storage_open`, and `target.kind` (door/inventory/other), `target.item_id`,
`target.required_key` and `target.locked`. Contents are direct serialized rows,
with `id`, `is_key` and `removed`; an open storage window gives no stale rows.
Container-like keys are not classified as ordinary keys. Reading contents
requires containers.read and items.read; subscribing requires events.item_use
and items.read.

`event:open_storage()` requires containers.open and an inventory self-target.
`event:unlock()` requires doors.unlock and a currently locked target door with
storage closed. Only one action may be requested per callback. The callback
must return true; an error or unhandled return discards the requested action.
Methods expire when that callback returns. Retaining a table is allowed, but
it cannot be used to mutate a later event.

No native pointers, arbitrary reads/writes, worker-thread game calls, generic
physics callbacks or script-owned save records are available in API 1. Adding
those requires reviewing their execution and lifetime contracts first.

## Modules and optional features

```toml
[script]
runtime = "com.necrosis.script-runtime"
entry = "scripts/main.lua"
requires = []
optional = ["items.read"]

[script.modules]
"util.format" = "scripts/util/format.lua"
```

`require("util.format")` loads only a module declared by this addon. Module
names contain letters, digits, underscores and dots, with no empty segments.
The loader resolves each path inside the addon directory (including symlink
resolution). Entry plus modules are limited to 1 MiB total, with at most 64
modules. Sources are copied before addon execution; Lua never opens files.

Each addon owns its module cache. Modules execute once per state, including
modules returning false; nil becomes true. Cyclic imports and undeclared names
raise errors. A successful reload replaces the entire state and cache. A failed
reload keeps the previous state and subscriptions. Modules share their addon's
globals and budgets, and cannot register handlers after initial addon loading.
No cross-addon imports, package search paths or native Lua modules are exposed.

`api.has_capability(name)` returns true only for an operation both granted to
this addon and available in the host. `requires` must be satisfied before the
addon runs. `optional` grants known available operations and ignores unavailable
or unknown names. Optional operations must be guarded before use. These are
operation names, not executable or game version checks.

```lua
local format = require("util.format")
log.info(format.describe("loaded"))
if api.has_capability("items.read") then
    log.info("Item reading is available")
end
```

## Diagnostics

`log.info(...)`, `log.warn(...)`, `log.error(...)` and `print(...)` accept values
converted by Lua's tostring rules; print uses info level. Each load or callback
can emit up to 32 messages, truncated to 2 KiB each. Messages include addon ID
and severity. Runtime initialization, rejected addons and callback failures also
produce host diagnostics; protected errors include Lua stack traces.

The native provider writes `ScriptRuntime.log` beside its DLL, rotating to
`ScriptRuntime.log.previous` at 1 MiB. Control characters are escaped. Failed
file writes cannot interrupt gameplay; debugger output is also attempted.
A folder without write permission therefore cannot guarantee a persistent log.
Core-only initialization logs its reason: missing native operations, title-menu
context, save backup or hook setup. Runtime 0.3.0 provides the menu-gated lifecycle and retry behavior described below.

## Native loader contract

The existing `load_script_addon(id, source, size, required)` C export remains
supported. Bundles or optional capabilities use `load_script_addon_v2(id,
source, size, required, optional, modules, count)`. Required/optional are
NUL-terminated comma-separated UTF-8 capability names. Each native module record
is `{const char* name; const unsigned char* source; size_t size;}` in that order,
with platform C layout. Sources use explicit byte lengths and text-only Lua
loading. The provider consumes/copies all buffers before returning. This native
transport revision does not change Lua `api.version`, which remains 1.

## Lifecycle and reload (runtime 0.3.0)

Declare `events.lifecycle` as required or optional, then register during loading:

```lua
if api.has_capability("events.lifecycle") then
    events.on("runtime.phase", function(event)
        log.info(event.phase, event.generation)
    end)
end
```

The callback receives copied `phase` and `generation` values, with no action
methods or native handles. Phases are `loading`, `menu`, and `session`.
`menu` means the native title menu was drawn and the player root is absent.
`session` means a player root was observed. Other presentations are `loading`.
Generation increments when a new nonzero player-root identity is observed,
including a directly observed replacement. This is an observation epoch, not
proof that every save/level load was detected or that simulation is ready.

Phase events run after presentation on the native game window's owning thread.
Unchanged observations do not repeat callbacks; a successfully loaded/reloaded
addon receives its current snapshot. Source/module globals survive phase changes;
authors should clear session-local data when generation/phase changes. Item-event
action handles still expire at callback return. Lifecycle errors disable that
addon and do not stop the next addon. Disable clears states and pending commands;
it does not invoke arbitrary Lua from the caller's thread.

On a lifecycle-capable host, submitted source bundles are copied into a bounded
queue (128 addon IDs, 16 MiB aggregate source, at most four loads per pulse).
Acceptance does not mean activated.
Scripts first run at a title menu observed continuously for at least 500 ms.
Known required item capabilities may remain pending until native initialization
and save backup succeed. Missing operations on a host without a recovery path,
or unknown required capability names, reject the submission. Initialization
retries at most once every five seconds while that menu remains settled.
Reloads never mutate Lua states during callbacks or from a worker thread.

To reload edited scripts, write an addon ID (or `all`) to `reload.request` beside
ScriptRuntime.dll. The runtime checks once per second while at a settled title
menu, consumes the request, and asks EMTK to reread the current manifest, entry
and modules. The same profile-selection, capability, path and source-limit
checks apply. Pending requests wait while playing/loading. This does not reload
native DLLs/assets or change the selected profile. No key binding or executable
is required. A failed script replacement preserves its previous state; an `all`
request is atomic per addon, not a transaction across all addons.

Native integration may call `request_script_reload(id)` to enqueue the same
request. Its boolean indicates queue acceptance; consult diagnostics for the
result. The single pending request slot coalesces to the latest request. EMTK
registers `set_script_reload_callback(callback, runtime_id)` where callback is
`bool callback(const char* runtime_id, const char* addon_id_or_all)`. The runtime
copies the ID and retains the framework-owned process-lifetime function pointer.
The framework retains profile snapshots by runtime ID, releases registry locks
before loading, and catches Rust panics at the ABI boundary. It executes no Lua
itself and cannot redirect a reload to another runtime. Disabling/re-enabling
the runtime queues a fresh selected-profile reload at the next settled menu.

No simulation tick, actual world-load completion, campaign persistence,
shutdown callback or in-game reload UI is promised by this interface.

## Presentation timers (runtime 0.4.0)

Declare `timers` in required or optional capabilities. The API is:

- `timers.after(seconds, callback)` schedules one call and returns a numeric ID.
- `timers.every(seconds, callback)` repeats until cancelled and returns an ID.
- `timers.cancel(id)` returns true if this addon owned a pending timer with that ID.
- `timers.now()` returns the current monotonic presentation time in seconds.
- `tasks.run(fn [, {budget = n}])` runs a long job in slices: each pulse it gets `n` Lua instructions (default
  200,000; a lower budget keeps each frame cheaper), pauses where it ran out or at `tasks.yield()`
  (even deep inside nested Lua calls) and resumes there next pulse. A pause cannot happen inside a C callback such
  as a `table.sort` comparator; running out there is an error as in any callback.
- `powers.open_school(school)` (school 1..8; Light is 5) lets the spell-book wheel pick that school before the
  character knows any of its powers, including at character creation. Call it while loading.

```lua
local reminder
reminder = timers.every(1.0, function()
    log.info("Presentation time:", timers.now())
    timers.cancel(reminder)
end)
```

These are wall-clock presentation timers. They run only on verified presentations
on the existing native window thread, in observed menu/session phases. They do
not run on workers, during loading, or when no presentation occurs. They are not
simulation time and can continue while gameplay is paused. Delays are minimums,
not precise delivery guarantees. One-shots accept finite 0..86400 seconds; repeats
accept 0.01..86400. Delays round upward to milliseconds; zero waits until time
advances on a later pulse. A stalled repeat fires once, then schedules its next
interval from the current pulse; it never catches up with a burst.

Each addon may hold 64 timers. A presentation runs at most 16 callbacks globally
and four per addon, with a rotating starting addon for fairness. New timers cannot
run recursively during their scheduling callback. Callbacks receive no arguments,
use the existing instruction/memory/log limits, and have no item-action context.
Retained item-event methods remain expired. A callback error disables the faulty
addon and cancels its remaining timers, while other addons continue.

IDs are addon-owned and not reused across reloads in the same runtime process.
Cancelling a running repeat works; a one-shot is no longer pending when called.
All timers are cancelled on an observed phase or generation transition, before
phase callbacks run; those callbacks may schedule new work for that phase.
Successful reload/unload/disable destroys that addon's timers. Failed replacement
preserves the previous state's timers. Timers are not persisted to saves.
Cancellation follows observed lifecycle changes and cannot detect an unseen
save/level transition that reuses the same player-root identity.

```lua
if api.has_capability("timers") and api.has_capability("events.lifecycle") then
    events.on("runtime.phase", function(event)
        if event.phase == "session" then
            timers.after(0.5, function() log.info("Deferred session work") end)
        end
    end)
end
```

## Addon settings (runtime 0.5.0)

Request `storage.settings` for preferences shared by this user across campaigns:

```lua
local options = storage.get("options", {enabled = true, volume = 0.5})
options.volume = 0.75
storage.set("options", options)
-- storage.set("options", nil) removes the key.
log.info(storage.status())
```

`storage.get(key, default)` returns a fresh copy of the stored value, or the
supplied default when missing. Editing a returned table does not change stored
data until set is called. `storage.set(key, value)` stages a replacement; nil
removes it. Top-level keys are 1..64 UTF-8 bytes without NUL. Values may be
booleans, signed 64-bit integers, finite floating-point numbers, UTF-8 strings,
string-keyed tables or dense positive-integer arrays. Mixed/sparse arrays,
functions, userdata, cycles and non-finite numbers are rejected. Empty tables
encode as objects. Nested object keys are 1..128 UTF-8 bytes. Strings may contain
escaped NUL/control characters. Table nesting is bounded to eight levels;
files hold at most 128 top-level keys, 520 JSON nodes including the envelope,
and 64 KiB total. Each load/callback may perform at most 128 storage operations.

Changes commit to the in-memory namespace only when the protected load/callback
succeeds. A failed reload or callback discards its staged settings. A caught
individual set error preserves earlier valid staged changes. Successful updates
are queued for disk publication: normally at most one namespace per second on
the existing pulse; core-only loading and disable attempt a final synchronous
flush without invoking Lua. An abrupt process exit can lose pending changes.
`storage.status()` reports `saved`, `pending`, `error` or `conflict`; it describes
the cached namespace, not a guarantee that external files are unchanged.

Files live under `%LOCALAPPDATA%/ExanimaLua/settings/`, with addon IDs encoded
into bounded path components. The JSON envelope records format 1, addon identity
and values. Lua cannot choose paths or access another addon's namespace.
Publication uses a flushed temporary file and atomic replacement, retaining
`settings.previous.json`. A per-namespace writer lock and expected-revision
check prevent cooperating runtime instances from silently overwriting each
other. Conflicts keep the external file unchanged and stop automatic overwrites;
other write failures retain committed memory and retry after a backoff. Diagnostics
report the failure. Back up/reconcile files with the game closed, then restart
to refresh the cache. Files are not live-reloaded or merged automatically.

Corrupt data, an unknown format or mismatched addon identity is never silently
reset or recovered from the previous copy. A required settings capability then
rejects that addon; an optional one is withheld so guarded code can continue.
Reparse points/non-regular owned files are refused. This is not a security
boundary against arbitrary native mods or hostile filesystem races.

**This scope is user/addon settings, not save-specific state.** It is shared
across Story, Arena and new campaigns. Do not store quests, keys, character
progress or campaign identifiers here. The researched save slots are reused
and checkpoints can switch packages; the runtime has no verified campaign-ID
and save-commit contract yet. No campaign storage capability is exposed, and
native saves are neither read nor written by this API. The keyring continues
to use the game's existing native container persistence.
