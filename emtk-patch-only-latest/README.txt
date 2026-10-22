EMTK PATCH FOR LUA MODS - for people who already have EMTK  (LATEST EMTK MAIN, 7ceeb3e)
======================================================================================

This copy fits EMTK built from the main branch at commit 7ceeb3ed99362f128a2bf16455833f3c0eaa6b24
(19 October 2026). For the older EMTK 0.1.0-dev build (commit b422845) use emtk-patch-only instead.
EMTK's own feat/lua-rt branch (last changed May 2025) is a separate, unfinished Lua experiment; neither patch
uses or needs it.

Stock EMTK loads asset mods and native plugins from a mods folder scan, but cannot hand Lua script mods to a
runtime. This patch adds that, and makes native plugins follow your profile: only enabled entries load, in
profile priority order. Only one EMTK file changes: emtk_framework.dll. Your launcher, profiles and mods stay.

What is here
  emtk_framework.dll                 the patched framework, ready to drop in
                                     sha256 f7c81bd99cf5071284abf69365abc922547f04bafe3fd3e47efa866895de6ea8
  emtk-startup-main-7ceeb3e.patch    the source change, if you prefer to build it yourself
  LICENSE-MIT, LICENSE-APACHE        EMTK's licences (the patch is under the same terms)

Status: built and compile-checked against that commit (Rust nightly-2026-10-15, release, no errors).
Not yet run in game with this EMTK version.

1. Back up
   Close Exanima. In the folder that holds emtk_launcher.exe, copy emtk_framework.dll to
   emtk_framework.dll.backup.

2. Apply (the quick way)
   Copy this emtk_framework.dll over the one beside emtk_launcher.exe. Use it only with a launcher from the
   same EMTK commit.

   Or build it yourself: clone the EMTK repository, check out the commit above, initialise its submodules,
   run "git apply emtk-startup-main-7ceeb3e.patch", then from a Visual Studio x64 developer prompt:
       cargo build --locked --release -p emtk_framework
   (the repository pins its own Rust nightly) and use target\release\emtk_framework.dll.

3. Check it worked
   Install the Exanima Lua Runtime (mods\com.necrosis.script-runtime) and enable it in your EMTK profile, then
   start the game through EMTK. At the title menu, mods\com.necrosis.script-runtime\ScriptRuntime.log says
   "Lifecycle installed" and then "loaded" for each enabled Lua mod. EMTK's log says "Native plugin enabled"
   for the runtime.

4. Undo
   Close Exanima and copy emtk_framework.dll.backup back over emtk_framework.dll. Your saves are not touched by
   EMTK either way.

Notes
  - Asset precedence follows the profile priority: a higher number wins.
  - Lua mods need both the runtime and the mod enabled in the profile; a disabled entry never runs.
  - A mod's native DLL now loads only when that mod is enabled in the profile (stock main loads every mods
    folder that has a config.toml).
  - The game no longer opens EMTK's log console window; the log file is still written to
    %APPDATA%\exanima-modding-toolkit\log. To see the console, set the environment variable EMTK_CONSOLE=1.
  - Fixes stock EMTK serving mod archives wrongly: a game read that starts partway into a packed file, or
    continues where the last read stopped, now gets the right bytes. Before, any enabled mod could give wrong
    textures and lighting, empty inventories or a freeze.
