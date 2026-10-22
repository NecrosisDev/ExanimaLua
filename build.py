"""Build the in-process Lua host and exercise it in owned processes only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import atexit

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1] if len(HERE.parents)>1 else HERE   # a repository at a drive root has no workspace above it
ID='com.necrosis.script-runtime'
# The one place the release number is written; src/script_host.h runtimeVersion must match (checked below).
VERSION='0.28.0'

# Files this runtime shares with the rest of the workspace (their owners live there). A standalone copy made by
# export.py carries snapshots of them in third_party/, which are used when present.
WORKSPACE={'vendor/minhook':'continuation/vendor/minhook','saves.h':'continuation/saves.h','game_structs.h':'continuation/game_structs.h',
    'game_layout.h':'continuation/game_layout.h','game_signatures.h':'continuation/game_signatures.h','npc/npc_live.h':'continuation/npc/npc_live.h',
    'hotload.h':'continuation/hotload.h','keyring.lua':'mods/keyring/scripts/main.lua','LICENSE':'LICENSE'}
def dependency(name):
    local=HERE/'third_party'/name
    return local if local.exists() else ROOT/WORKSPACE[name]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    out=args.out.resolve()
    if out.exists() or (ROOT/'release').exists() and out.is_relative_to(ROOT/'release'):
        parser.error('use a new candidate folder outside release/')
    if f'runtimeVersion="{VERSION}"' not in (HERE/'src/script_host.h').read_text():
        parser.error(f'src/script_host.h runtimeVersion does not say {VERSION}')
    docs=subprocess.run([sys.executable,'-X','utf8','-B',str(HERE/'test_api_docs.py')],capture_output=True,text=True)
    print(docs.stdout+docs.stderr)
    if docs.returncode:raise SystemExit('API.md does not match the runtime; see above')
    out.mkdir(parents=True)
    stage=Path(tempfile.mkdtemp(prefix='Exanima-Lua-'))
    # The stage (about 100 MB of objects and test programs) is removed when the build ends, passed or failed;
    # build.log in --out keeps the compiler and test output.
    atexit.register(shutil.rmtree,stage,True)
    source_files=sorted(p for p in (HERE/'src').glob('*') if p.is_file())
    for file in source_files:
        shutil.copyfile(file,stage/file.name)
    vendor=stage/'vendor/lua';vendor.mkdir(parents=True)
    for file in (HERE/'vendor/lua').iterdir():
        if file.suffix in ('.c','.h') or file.name=='README.md':shutil.copyfile(file,vendor/file.name)
    shutil.copytree(dependency('vendor/minhook'),stage/'vendor/minhook')
    shutil.copyfile(dependency('saves.h'),stage/'saves.h')
    (stage/'shared').mkdir()
    for name in ('game_structs.h','game_layout.h','game_signatures.h','npc/npc_live.h','hotload.h'):
        shutil.copyfile(dependency(name),stage/'shared'/Path(name).name)
    shutil.copyfile(dependency('keyring.lua'),stage/'keyring.lua')
    shutil.copyfile(HERE/'examples/com.example.character-observer/scripts/main.lua',stage/'character-observer.lua')
    shutil.copyfile(HERE/'examples/com.example.awareness-observer/scripts/main.lua',stage/'awareness-observer.lua')
    shutil.copyfile(HERE/'examples/com.example.practice-ai/scripts/main.lua',stage/'practice-ai.lua')
    shutil.copyfile(HERE/'examples/com.example.encounter-truce/scripts/main.lua',stage/'encounter-truce.lua')
    shutil.copyfile(HERE/'examples/com.example.reward-chest/scripts/main.lua',stage/'reward-chest.lua')
    shutil.copyfile(HERE/'examples/com.example.inventory-tools/scripts/main.lua',stage/'inventory-tools.lua')
    shutil.copyfile(HERE/'examples/com.example.self-test/scripts/main.lua',stage/'self-test.lua')
    shutil.copytree(HERE/'examples',stage/'examples')
    vswhere=Path(os.environ['ProgramFiles(x86)'])/'Microsoft Visual Studio/Installer/vswhere.exe'
    vs=subprocess.check_output([str(vswhere),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
    vcvars=Path(vs)/'VC/Auxiliary/Build/vcvars64.bat'
    commands=[f'call "{vcvars}"','if %errorlevel% neq 0 exit /b 1','mkdir luaobj','mkdir obj']
    lua_sources=' '.join('vendor\\lua\\'+p.name for p in sorted(vendor.glob('*.c')) if p.name not in ('lua.c','ltests.c','onelua.c'))
    common='cl /nologo /MT /O2 /std:c++17 /EHsc /W4 /Brepro /Ivendor\\lua /Fo:obj\\ '
    minhook='vendor\\minhook\\src\\buffer.c vendor\\minhook\\src\\hook.c vendor\\minhook\\src\\trampoline.c vendor\\minhook\\src\\hde\\hde64.c'
    for command in [
        'cl /nologo /TP /EHsc /MT /O2 /W2 /D_CRT_SECURE_NO_WARNINGS /Fo:luaobj\\ /c '+lua_sources,
        'lib /nologo /OUT:lua.lib luaobj\\*.obj',
        common+'/LD plugin.cpp '+minhook+' lua.lib user32.lib /Fe:ScriptRuntime.dll /link /INCREMENTAL:NO',
        common+'test_host.cpp lua.lib /Fe:test_host.exe','test_host.exe',
        common+'test_tools.cpp lua.lib /Fe:test_tools.exe','test_tools.exe',
        common+'test_session.cpp lua.lib /Fe:test_session.exe','test_session.exe',
        common+'test_timers.cpp lua.lib /Fe:test_timers.exe','test_timers.exe',
        common+'test_settings.cpp lua.lib /Fe:test_settings.exe','test_settings.exe',
        common+'test_world.cpp lua.lib /Fe:test_world.exe','test_world.exe',
        common+'test_characters.cpp lua.lib /Fe:test_characters.exe','test_characters.exe',
        common+'test_awareness.cpp lua.lib /Fe:test_awareness.exe','test_awareness.exe',
        common+'test_ai_write.cpp '+minhook+' lua.lib user32.lib /Fe:test_ai_write.exe','test_ai_write.exe',
        common+'test_attitude.cpp '+minhook+' lua.lib user32.lib /Fe:test_attitude.exe','test_attitude.exe',
        common+'test_containers.cpp '+minhook+' lua.lib user32.lib /Fe:test_containers.exe','test_containers.exe',
        common+'test_inventory.cpp '+minhook+' lua.lib user32.lib /Fe:test_inventory.exe','test_inventory.exe',
        common+'test_world_items.cpp '+minhook+' lua.lib user32.lib /Fe:test_world_items.exe','test_world_items.exe','test_world_items.exe remove-fault',
        common+'test_carried_items.cpp '+minhook+' lua.lib user32.lib /Fe:test_carried_items.exe','test_carried_items.exe','test_carried_items.exe publication-fault','test_carried_items.exe clone-fault','test_carried_items.exe equip-fault','test_carried_items.exe remove-fault','test_carried_items.exe remove-transition','test_carried_items.exe shield-fault','test_carried_items.exe bag-second-fault',
        common+'test_interactions.cpp '+minhook+' lua.lib user32.lib /Fe:test_interactions.exe','test_interactions.exe','test_interactions.exe effect-fault','test_interactions.exe effect-noop','test_interactions.exe unlock-fault','test_interactions.exe button-fault','test_interactions.exe lever-wake-fault','test_interactions.exe door-unlatch-fault','test_interactions.exe door-command-fault','test_interactions.exe door-wake-fault','test_interactions.exe door-command-noop','test_interactions.exe pair-second-fault','test_interactions.exe slider-unlatch-fault','test_interactions.exe light-wake-fault','test_interactions.exe teleporter-fault','test_interactions.exe teleporter-bad-window','test_interactions.exe teleporter-decline-after-clone','test_interactions.exe scanner-fault','test_interactions.exe scanner-no-effect','test_interactions.exe emitter-queue-fault','test_interactions.exe emitter-native-fault','test_interactions.exe emitter-no-effect','test_interactions.exe emitter-stop-fault','test_interactions.exe emitter-stop-noop','test_interactions.exe fire-queue-fault','test_interactions.exe fire-native-fault','test_interactions.exe fire-no-effect','test_interactions.exe fire-foreign-effect','test_interactions.exe fire-source-mutated','test_interactions.exe fire-wrong-position','test_interactions.exe extinguish-native-fault','test_interactions.exe extinguish-noop','test_interactions.exe torch-native-fault','test_interactions.exe torch-no-effect','test_interactions.exe torch-foreign','test_interactions.exe torch-publish-fault','test_interactions.exe torch-unregistered','test_interactions.exe torch-queue-fault',
        common+'test_presentation.cpp '+minhook+' lua.lib user32.lib /Fe:test_presentation.exe','test_presentation.exe',
        common+'test_bridge.cpp '+minhook+' lua.lib user32.lib /Fe:test_bridge.exe','test_bridge.exe',
        common+'test_compatibility.cpp '+minhook+' lua.lib user32.lib /Fe:test_compatibility.exe','test_compatibility.exe',
        common+'test_combat.cpp '+minhook+' lua.lib user32.lib /Fe:test_combat.exe','test_combat.exe',
        common+'test_powers.cpp '+minhook+' lua.lib user32.lib /Fe:test_powers.exe','test_powers.exe',
        common+'test_saves.cpp /Fe:test_saves.exe','test_saves.exe',
        common+'test_save_data.cpp '+minhook+' lua.lib user32.lib /Fe:test_save_data.exe','test_save_data.exe',
        common+'test_script_commands.cpp '+minhook+' lua.lib user32.lib /Fe:test_script_commands.exe','test_script_commands.exe',
        common+'test_api.cpp '+minhook+' lua.lib user32.lib /Fe:test_api.exe','test_api.exe',
        common+'test_examples.cpp '+minhook+' lua.lib user32.lib /Fe:test_examples.exe','test_examples.exe',
        common+'test_shared_hooks.cpp '+minhook+' lua.lib user32.lib /Fe:test_shared_hooks.exe','test_shared_hooks.exe',
        'dumpbin /nologo /exports ScriptRuntime.dll >exports.txt',
        'findstr /r /c:" enable$" exports.txt >nul','findstr /r /c:" disable$" exports.txt >nul',
        'findstr /r /c:" load_script_addon$" exports.txt >nul',
        'findstr /r /c:" load_script_addon_v2$" exports.txt >nul',
        'findstr /r /c:" add_zone_listener$" exports.txt >nul','findstr /r /c:" remove_zone_listener$" exports.txt >nul',
        'findstr /r /c:" zone_listener_ready$" exports.txt >nul']+[f'findstr /r /c:" {name}$" exports.txt >nul' for name in (
        'shared_hooks_version','add_title_row','remove_title_row','set_title_row_enabled','title_rows_ready',
        'add_member_provider','remove_member_provider','resize_member_buffer','member_providers_ready',
 'settings_tabs_version','add_settings_tab','remove_settings_tab','settings_tabs_ready','settings_tabs_claim_host',
 'settings_tab_host_open','settings_tab_host_title','settings_tab_host_rows','settings_tab_host_change','settings_tab_host_event')]:
        commands += [command,'if %errorlevel% neq 0 exit /b 1']
    (stage/'build.cmd').write_text('@echo off\nsetlocal\n'+'\n'.join(commands)+'\n')
    # os.environ keys are upper-case on Windows; with this variable set cmd.exe will not run programs from the
    # current directory, so drop it whatever its case and name build.cmd by its full path.
    env={k:v for k,v in os.environ.items() if k.upper()!='NODEFAULTCURRENTDIRECTORYINEXEPATH'}
    result=subprocess.run(['cmd.exe','/d','/c',str(stage/'build.cmd')],cwd=stage,env=env,capture_output=True)
    (out/'build.log').write_bytes(result.stdout+result.stderr)
    print((result.stdout+result.stderr).decode(errors='replace')[-4000:])
    if result.returncode:raise SystemExit(result.returncode)
    mod=out/'mods'/ID;mod.mkdir(parents=True)
    shutil.copyfile(stage/'ScriptRuntime.dll',mod/'ScriptRuntime.dll')
    (mod/'manifest.toml').write_text('[plugin]\nname = "Exanima Lua Runtime"\nversion = "'+VERSION+'"\ncategory = "Utility"\n')
    (mod/'config.toml').write_text('[plugin]\nid = "'+ID+'"\nname = "Exanima Lua Runtime"\nversion = "'+VERSION+'"\nsupported_versions = []\nurl = ""\nenabled = true\nexecutable = "ScriptRuntime.dll"\n[plugin.author]\nname = "NecrosisDev"\n')
    (mod/'source').mkdir()
    for file in source_files:shutil.copyfile(stage/file.name,mod/'source'/file.name)
    shutil.copytree(stage/'shared',mod/'source/shared')
    shutil.copytree(HERE/'examples',mod/'examples')
    shutil.copyfile(HERE/'README.md',mod/'README.md')
    shutil.copyfile(HERE/'API.md',mod/'API.md')
    shutil.copyfile(dependency('LICENSE'),mod/'LICENSE')
    shutil.copyfile(dependency('vendor/minhook')/'LICENSE.txt',mod/'LICENSE-MinHook.txt')
    lua_header=(vendor/'lua.h').read_text()
    (mod/'LICENSE-Lua.txt').write_text(lua_header[lua_header.index('Copyright (C) 1994'):lua_header.rindex('******************************************************************************/')])
    (out/'verification.json').write_text(json.dumps({'native_tests':'passed','exports':(stage/'exports.txt').read_text(),
        'lua_revision':'6e22fedb74cf0c9b6656e9fce8b7331db847c605','lua_version':'5.4.8',
        'keyring_script_sha256':hashlib.sha256((stage/'keyring.lua').read_bytes()).hexdigest(),
        'files':{p.relative_to(out).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(mod.rglob('*')) if p.is_file()},
        'acceptance':'owned-process Lua/bridge/hooks; gameplay unverified'},indent=2))
    print('LUA_RUNTIME_BUILD_PASSED='+str(mod))

if __name__=='__main__':main()

