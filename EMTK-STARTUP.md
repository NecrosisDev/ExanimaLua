# EMTK native and Lua startup update

**Why this exists.** Stock EMTK (github.com/ExanimaModding/Toolkit) loads asset
mods, but its native-plugin start is commented out and it has no script-addon
transport, so it cannot run this runtime or any Lua addon. Until EMTK carries
these changes itself, players need a framework DLL built with this patch; a
release of this repository should attach that DLL with its source fingerprint.
Asset precedence follows the profile priority: a higher number wins, so a mod
listed later overrides earlier mods' files of the same name.

This is a small source patch to the existing EMTK framework, based on upstream
revision `b422845374d579e155dd4a07d85b5ebc0f779650`. It restores the native-plugin
startup path that upstream currently leaves commented out. There is no second
bootstrap, proxy DLL, process injector or keyring activator in this package.

Install the included framework DLL beside the EMTK launcher once, while the
game is closed, after backing up its existing framework DLL. Keep an existing framework only if it already supports the script transport
needed by your addons (including module bundles and optional capabilities). Thereafter,
enable Exanima Lua Runtime and your chosen script addons in the normal profile
and launch normally through EMTK.

The loader uses the same enabled, manifest-validated entries for assets and
native code, ordered by profile priority. An asset-only mod needs no legacy
native config. A native mod supplies `config.toml`, `enable` and `disable`;
disabled profile/config entries stay unloaded. A missing export, missing DLL,
path escape or declined enable is logged without preventing subsequent mods.
Loading and enabling run before the original game entry point, outside DllMain.
This update does not install the old GUI or change the archive hook provider.

Source changes are included in `emtk-startup.patch`, with upstream licenses and
source/build fingerprints. To rebuild, clone the upstream repository, check out
the revision above, initialize its submodules and apply the patch. From a Visual
Studio x64 developer command prompt with Rust nightly installed, run:

```text
cargo +nightly build --locked --release -p emtk_framework
```

On this machine, `legacy_stdio_definitions.lib` is installed only under the
MSVC OneCore x64 library directory. The successful build appended that directory
to `LIB`; no global environment or upstream dependency version was changed:

```bat
set "LIB=%LIB%;%VCToolsInstallDir%lib\onecore\x64"
```

The supplied test runner builds three owned fixture DLLs and exercises the real
profile loader. It checks enabled/disabled entries, missing exports, declined
activation, path confinement, continued loading after failure, and the real
Lua runtime executing core scripts while refusing unavailable game operations. Tests do not launch or
attach to Exanima, and do not prove game startup or gameplay acceptance.

To roll back the framework, restore the backed-up framework DLL while the game
is closed. Save compatibility is separate: retain the keyring item mod for
saves containing it, or restore a pre-mod save.

## Lua startup

After native providers load, enabled profile entries with a `[script]` manifest
section are passed to their enabled runtime. The entry must resolve inside its
addon directory. The loader sends source bytes and required capabilities through
`load_script_addon(id, source, size, capabilities)`. Inputs are consumed/copied
before return; the native host does not retain pointers to Rust buffers.
Disabled entries and unavailable runtimes do not run scripts. Both the runtime
and each script addon must be enabled. The keyring's legacy native config is
disabled intentionally to retire its old hook owner during upgrades.

Declared modules are canonically confined to the addon directory and read with
an aggregate 1 MiB bundle limit before execution. The framework uses the additive
load_script_addon_v2 export when modules or optional capabilities are declared;
existing single-file required-only scripts retain the original export. The owned
integration test executes a real multi-file addon and checks its persistent log,
optional capability behavior and rejected module escapes.

## Menu-gated activation and reload

Runtime 0.3.0 accepts copied bundles pending a settled native title menu. EMTK's
accepted log therefore does not claim that Lua has executed. The runtime owns
phase observation, retries and queue consumption on the native window thread.
The framework registers a process-lifetime reload callback keyed by runtime ID;
it rereads selected-profile entries through the same loader. Provider registry
locks are dropped before calling back into the runtime. Runtime requests cannot
enable disabled profile entries or switch provider ownership. Profile changes
and native/asset reloads still require normal startup; no hot profile switch is
introduced. See API.md for reload.request and request_script_reload semantics.
