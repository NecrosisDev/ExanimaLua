# Exanima Lua Runtime

Write Exanima mods in Lua. The runtime is one native EMTK plugin
(`ScriptRuntime.dll`, Lua 5.4.8 embedded) that loads script addons: a mod is a
folder with a `manifest.toml` and `.lua` files, no DLL of its own.

Runtime 0.28.0, Lua API generation 1, revision 28. For Exanima 0.9.5.2g (Steam
build 25803178, Windows x64) and its Hellmode build.

**Status: offline-tested, not yet played.** Every call is exercised by
owned-process tests that run real Lua against the real bridges and fake game
memory, and each game operation has a static review. None of it has been run
in the game yet; treat a feature as working once you have seen it there.

## What a mod can do

- **Events:** item use, weapon and armour use on the world (`gear.use`), hits
  (attacker, weapon, body part, blocked/parried, armour), zone entry, game
  scripts (`LuaEvent`), power casts, level phases, item ID changes, settings.
- **Read:** the world around the player, characters and their awareness,
  inventory, item stats, the level, world time, the player's thaumaturgy.
- **Change:** inventory and equipment, containers, doors, levers, devices,
  fires and torches, NPC AI and attitude, item condition and wear, world time,
  per-save data, the player (damage, push, place), level travel.
- **Add:** title-menu rows, a section in Settings > Mod Settings with profiles,
  on-screen notices and the game's sounds, point lights with a particle glow,
  loose items in levels, item records and strings without shipping whole
  databases, served or overridden package files.
- **Diagnose:** an in-game console of errors and warnings, mirrored to a JSON
  log an agent or test can read.

The full list, with every limit, is [API.md](API.md). Start with
[GETTING-STARTED.md](GETTING-STARTED.md).

## Install

1. EMTK with the script-runtime patch (stock EMTK does not load script
   addons yet): see [EMTK-STARTUP.md](EMTK-STARTUP.md).
2. Copy `mods/com.necrosis.script-runtime` from a build into Exanima's `mods`
   folder and enable it in your EMTK profile, together with your addons.
3. The runtime backs up your saves at the title menu before it changes
   anything. If a mod misbehaves, open the console (bind its key in Settings >
   Mod Settings) or read `ScriptRuntime.log` beside the DLL.

## Build

Windows x64, Python 3.12 and the Visual Studio 2022 C++ build tools:

    python -X utf8 -B build.py --out <new folder>

This compiles Lua, MinHook, the runtime and its test programs, runs them, and
writes `mods/com.necrosis.script-runtime` with a hash receipt. Nothing launches
the game.

## Layout

| Path | What |
|---|---|
| `src/` | the runtime (C++17): `script_host.h` is the Lua API, `*_bridge.h` the game operations, `test_*.cpp` the tests |
| `examples/` | small complete addons |
| `vendor/lua/` | Lua 5.4.8 (MIT) |
| `third_party/` | MinHook (BSD-2) and shared game-layout headers |
| `emtk-startup.patch`, `EMTK-STARTUP.md` | the EMTK startup patch and its notes |
| `API.md` | the reference and the per-release contracts |

## Licence

MIT (see [LICENSE](LICENSE)). Lua is MIT (copyright notice in `vendor/lua/lua.h`,
shipped as `LICENSE-Lua.txt`); MinHook is BSD-2 (`LICENSE-MinHook.txt`). No
game files are part of this repository.
