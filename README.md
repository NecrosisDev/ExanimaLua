# Exanima Lua Runtime

Lets Exanima load mods written in Lua. The runtime is one native EMTK plugin
(`ScriptRuntime.dll`, with Lua 5.4.8 embedded) that loads script addons: a Lua
mod is a folder with a `manifest.toml` and `.lua` files, with no DLL of its own.

Players install it once so that Lua mods work. Modders use it to write them.

Runtime 0.28.0, Lua API generation 1, revision 28. For Exanima 0.9.5.2g (Steam
build 25803178, Windows x64) and its Hellmode build.

## What a Lua mod can do

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
- **Diagnose:** an in-game console of errors and warnings, also written to a
  log file.

The full list, with every limit, is in [API.md](API.md).

## Requirements

- Exanima (Steam), version 0.9.5.2g, or its Hellmode build.
- EMTK (the Exanima Modding Toolkit) with the script-runtime startup patch.
  Stock EMTK does not load Lua mods or native plugins. See
  [EMTK-STARTUP.md](EMTK-STARTUP.md) to build the patched `emtk_framework.dll`.

The patch comes in two versions, for two EMTK builds:

| Your EMTK | Patch |
|---|---|
| EMTK 0.1.0-dev, commit `b422845` | `emtk-startup.patch` (see `emtk-patch-only/README.txt`) |
| EMTK main branch, commit `7ceeb3e` | `emtk-patch-only-latest/emtk-startup-main-7ceeb3e.patch` |

Use the framework DLL that matches your EMTK launcher.

## Install

1. Close Exanima.
2. Install the patched EMTK framework once: in the folder that holds
   `emtk_launcher.exe`, copy `emtk_framework.dll` to
   `emtk_framework.dll.backup`, then copy the patched `emtk_framework.dll`
   over it.
3. Download `ExanimaLua-<version>.zip` from the Releases page.
4. Copy the ZIP's `mods` folder into your Exanima game folder. You should end
   up with `Exanima\mods\com.necrosis.script-runtime`.
5. In your EMTK profile, enable **Exanima Lua Runtime** together with the Lua
   mods you want. Each Lua mod needs both itself and the runtime enabled.
6. Launch the game through EMTK.

To check it worked: at the title menu,
`mods\com.necrosis.script-runtime\ScriptRuntime.log` says "Lifecycle
installed", then "loaded" for each enabled Lua mod.

Do not replace any DLL while the game is running.

## Settings and the console

The runtime adds a Mod Settings tab to the game's Settings menu. Lua mods that
have options show their own section there, with profiles.

The runtime's own option is the console key. It is unbound until you pick one.
The console lists mod messages, warnings and errors. If a Lua mod has an error,
only that mod is switched off; the console and `ScriptRuntime.log` (beside
`ScriptRuntime.dll`) show what went wrong.

## Saves and uninstalling

At the title menu, before it changes anything in the game, the runtime backs up
your saves into a `saves-backup-<date>` folder beside the runtime. It keeps the
newest two backups. It never restores a backup by itself. Keep your own backup
of your saves as well before trying new mods.

Lua mods that add items put those items into your saves. Read each mod's own
notes before removing it.

To uninstall, close the game and disable **Exanima Lua Runtime** and its Lua
mods in your EMTK profile, or delete the `mods\com.necrosis.script-runtime`
folder. To go back to stock EMTK, copy `emtk_framework.dll.backup` back over
`emtk_framework.dll`. Asset-only mods keep working with stock EMTK; Lua mods and
native plugins stop loading.

## For modders

Start with [GETTING-STARTED.md](GETTING-STARTED.md): a first mod in five
minutes. Then read the Reference section of [API.md](API.md) for every
capability, call and event. [DEVELOPER.md](DEVELOPER.md) has further notes for
mod authors. The `examples/` folder holds small complete addons (armour check,
zone journal, reward chest, practice AI and more); the release ZIP carries them
too, inside the runtime's folder.

While working on a mod, return to the title menu and write its ID (or `all`)
into a file named `reload.request` beside `ScriptRuntime.dll` to reload it
without restarting the game.

## Building from source

Needs Windows x64, Python 3.12 and the Visual Studio 2022 C++ build tools. The
build does not need the game installed and never starts it.

    python -X utf8 -B release.py

This compiles Lua, MinHook, the runtime and its test programs, runs the tests,
and writes:

- `dist/mods/com.necrosis.script-runtime/`
- `dist/ExanimaLua-<version>.zip` with a `.sha256` beside it
- `dist/build.log` and `dist/verification.json`

A previous `dist/mods/com.necrosis.script-runtime/` is moved to `dist/previous/`
first. Options:

- `--zip-only` packages the mod folder already in `dist/` without building.
- `--tag v<version>` stops unless the tag matches the version in the manifest.

`python -X utf8 -B build.py --out <new folder>` runs only the build and tests,
into a new folder.

## Layout

| Path | What |
|---|---|
| `src/` | the runtime (C++17): `script_host.h` is the Lua API, `*_bridge.h` the game operations, `test_*.cpp` the tests |
| `examples/` | small complete addons |
| `vendor/lua/` | Lua 5.4.8 (MIT) |
| `third_party/` | MinHook (BSD-2) and shared game-layout headers |
| `emtk-startup.patch`, `EMTK-STARTUP.md`, `emtk-patch-only*/` | the EMTK startup patch and its notes |
| `API.md` | the reference and the per-release changes |

## License

MIT (see [LICENSE](LICENSE)). Lua is MIT (copyright notice in `vendor/lua/lua.h`,
shipped as `LICENSE-Lua.txt`); MinHook is BSD-2 (`LICENSE-MinHook.txt`). No
game files are part of this repository.
