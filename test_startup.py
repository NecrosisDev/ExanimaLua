"""Build owned native fixtures and exercise EMTK profile startup without a game."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--toolkit',type=Path,required=True)
parser.add_argument('--native-plugin','--keyring',dest='keyring',type=Path,required=True)
parser.add_argument('--out',type=Path,required=True)
args=parser.parse_args()
toolkit,keyring,out=args.toolkit.resolve(),args.keyring.resolve(),args.out.resolve()
out.mkdir(parents=True,exist_ok=False)
shutil.copyfile(Path(__file__).parent/'test_fixture/startup_plugin.cpp',out/'startup_plugin.cpp')
vswhere=Path(os.environ['ProgramFiles(x86)'])/'Microsoft Visual Studio/Installer/vswhere.exe'
vs=subprocess.check_output([str(vswhere),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
commands=[f'call "{Path(vs)/"VC/Auxiliary/Build/vcvars64.bat"}"','if %errorlevel% neq 0 exit /b 1']
for name,define in [('fixture',''),('missing','/DMISSING_DISABLE'),('decline','/DDECLINE_ENABLE')]:
    commands += [f'cl /nologo /LD /MT /O2 {define} startup_plugin.cpp /Fe:{name}.dll /link /INCREMENTAL:NO','if %errorlevel% neq 0 exit /b 1']
(out/'build.cmd').write_text('@echo off\nsetlocal\n'+'\n'.join(commands)+'\n')
built=subprocess.run(['cmd.exe','/d','/c','build.cmd'],cwd=out,capture_output=True)
(out/'fixtures-build.log').write_bytes(built.stdout+built.stderr)
assert built.returncode==0,'fixture compile failed; see fixtures-build.log'
env=dict(os.environ,EMTK_TEST_PLUGIN_DLL=str(out/'fixture.dll'),EMTK_TEST_MISSING_EXPORT_DLL=str(out/'missing.dll'),EMTK_TEST_DECLINE_DLL=str(out/'decline.dll'),EMTK_TEST_KEYRING_DLL=str(keyring),LOCALAPPDATA=str(out/"appdata"))
command=['cargo','+nightly','test','--locked','--release','-p','emtk_framework','--lib','plugins::startup_tests::profile_startup','--','--ignored','--nocapture','--test-threads=1']
(out/'test.cmd').write_text('@echo off\ncall "'+str(Path(vs)/'VC/Auxiliary/Build/vcvars64.bat')+'"\nif %errorlevel% neq 0 exit /b 1\nset "LIB=%LIB%;%VCToolsInstallDir%lib\\onecore\\x64"\n'+' '.join(command)+'\nexit /b %errorlevel%\n')
result=subprocess.run(['cmd.exe','/d','/c',str(out/'test.cmd')],cwd=toolkit,env=env,capture_output=True)
(out/'startup-tests.log').write_bytes(result.stdout+result.stderr)
print((result.stdout+result.stderr).decode(errors='replace')[-3000:])
assert result.returncode==0,'EMTK startup test failed; see startup-tests.log'
evidence={'command':command,'result':'passed','game_launched':False,'framework_sha256':hashlib.sha256((toolkit/'target/release/emtk_framework.dll').read_bytes()).hexdigest(),'native_plugin_sha256':hashlib.sha256(keyring.read_bytes()).hexdigest(),'scope':'owned-process fixtures, real Lua runtime executes core Lua while rejecting unavailable game APIs, profile/config disable, asset-only, missing export, path escape, failed activation, continuation, script entry selection, real declared-module bundle, optional capabilities, module escape rejection, persistent tagged runtime diagnostics, selected-profile fresh-source reload, failed reload recovery, disabled and unknown reload rejection, settings writes/previous copy/rollback in isolated child LOCALAPPDATA'}
(out/'verification.json').write_text(json.dumps(evidence,indent=2))
(toolkit/'startup-verification.json').write_text(json.dumps(evidence,indent=2))
print('LUA_EMTK_STARTUP_PASSED')
