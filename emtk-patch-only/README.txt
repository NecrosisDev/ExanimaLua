EMTK PATCH FOR LUA MODS - for people who already have EMTK  (EMTK b422845 ONLY)
==============================================================================

This copy fits EMTK built from commit b422845 only. For EMTK's newer main branch (commit 7ceeb3e, October 2026)
use emtk-patch-only-latest instead; its files moved, so this patch does not apply there.
EMTK's own feat/lua-rt branch (last changed May 2025) is a separate, unfinished Lua experiment; neither patch
uses or needs it.

Stock EMTK (github.com/ExanimaModding/Toolkit) loads asset mods but cannot start native plugins or Lua
script mods. This patch adds both. Only one EMTK file changes: emtk_framework.dll. Your launcher, profiles and
mods stay as they are.

Applies to
  EMTK 0.1.0-dev, source commit b422845374d579e155dd4a07d85b5ebc0f779650 (6 September 2025), Windows x64.
  The DLL here was built from that commit plus emtk-startup.patch.
  emtk_framework.dll  sha256 6d517b48a2288513f93ad40c423d075521ae1ee13ca2d2c4d3abbf77a92be169

What is here
  emtk_framework.dll   the patched framework, ready to drop in
  emtk-startup.patch   the source change, if you prefer to build it yourself
  LICENSE-MIT, LICENSE-APACHE   EMTK's licences (the patch is under the same terms)

1. Back up
   Close Exanima. In the folder that holds emtk_launcher.exe, copy emtk_framework.dll to
   emtk_framework.dll.backup.

2. Apply (the quick way)
   Copy this emtk_framework.dll over the one beside emtk_launcher.exe.

   Or build it yourself: clone the EMTK repository, check out the commit above, initialise its submodules,
   run "git apply emtk-startup.patch", then from a Visual Studio x64 developer prompt with Rust nightly:
       cargo +nightly build --locked --release -p emtk_framework
   and use target\release\emtk_framework.dll.

3. Check it worked
   Install the Exanima Lua Runtime (mods\com.necrosis.script-runtime) and enable it in your EMTK profile, then
   start the game through EMTK. At the title menu, mods\com.necrosis.script-runtime\ScriptRuntime.log says
   "Lifecycle installed" and then "loaded" for each enabled Lua mod. EMTK's own log lists the runtime plugin as
   enabled. With stock EMTK neither line appears.

4. Undo
   Close Exanima and copy emtk_framework.dll.backup back over emtk_framework.dll. Asset-only mods keep working;
   Lua mods and native plugins stop loading. Your saves are not touched by EMTK either way.

Notes
  - Asset precedence follows the profile priority: a higher number wins, so a mod listed later overrides
    earlier mods' files of the same name.
  - Lua mods need both the runtime and the mod enabled in the profile; a disabled entry never runs.
  - The game no longer opens EMTK's log console window; the log file is still written to
    %APPDATA%\exanima-modding-toolkit\log. To see the console, set the environment variable EMTK_CONSOLE=1.
  - Fixes stock EMTK serving mod archives wrongly: a game read that starts partway into a packed file, or
    continues where the last read stopped, now gets the right bytes. Before, any enabled mod could give wrong
    textures and lighting, empty inventories or a freeze.
  - Ordinary file reads are no longer slowed: archives are recognised when they are opened, so the framework no
    longer asks Windows for the name of every file the game reads.
