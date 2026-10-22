"""Write a standalone copy of the Lua runtime, ready to become its own repository. Never pushes or installs.

    python -B export.py --out <new folder> [--check]

The copy holds the runtime's own files plus snapshots of the files it shares with the rest of this workspace (the
MinHook library, the save-backup helper, the game layout headers and the Keyring script the tests load) under
third_party/, so its build.py runs from a fresh clone. The workspace keeps owning those files; export again to
refresh the snapshots. --check runs the exported build.py into a scratch folder to prove the copy builds alone.
"""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import build  # noqa: E402  (its WORKSPACE table names the shared files)

OWN = ['README.md', 'API.md', 'DEVELOPER.md', 'GETTING-STARTED.md', 'LICENSE', 'build.py', 'test_api_docs.py', 'export.py',
       'emtk-startup.patch', 'EMTK-STARTUP.md', 'test_startup.py']
TREES = ['src', 'examples', 'test_fixture']
GITIGNORE = '__pycache__/\n*.pyc\n/out/\n/build/\n'
WORKFLOW = """name: build
on: [push, pull_request]
jobs:
  build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.12'
      - name: Build and run the owned-process tests
        run: python -X utf8 -B build.py --out out
      - uses: actions/upload-artifact@v4
        with:
          name: ScriptRuntime
          path: out/mods/com.necrosis.script-runtime
"""


def export(out: Path) -> dict:
    if out.exists():
        raise SystemExit('--out must be a new folder')
    out.mkdir(parents=True)
    for name in OWN:
        if (HERE / name).exists():
            shutil.copyfile(HERE / name, out / name)
    ignore = shutil.ignore_patterns('__pycache__', '*.pyc', '.claude', '.git', '.github', '.gitignore', '.gitattributes')
    for tree in TREES:
        shutil.copytree(HERE / tree, out / tree, ignore=ignore)
    lua = out / 'vendor/lua'
    lua.mkdir(parents=True)
    for f in (HERE / 'vendor/lua').iterdir():
        if f.suffix in ('.c', '.h') or f.name == 'README.md':
            shutil.copyfile(f, lua / f.name)
    third = out / 'third_party'
    for name in build.WORKSPACE:
        source = build.ROOT / build.WORKSPACE[name]
        target = third / name
        target.parent.mkdir(parents=True, exist_ok=True)
        if source.is_dir():
            shutil.copytree(source, target, ignore=ignore)
        else:
            shutil.copyfile(source, target)
    (out / '.gitignore').write_text(GITIGNORE)
    (out / '.gitattributes').write_text('* text=auto eol=lf\n*.patch -text\n')
    workflows = out / '.github/workflows'
    workflows.mkdir(parents=True)
    (workflows / 'build.yml').write_text(WORKFLOW)
    files = {p.relative_to(out).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(out.rglob('*')) if p.is_file()}
    snapshot = {name: build.WORKSPACE[name] for name in build.WORKSPACE}
    (out / 'third_party/SOURCES.json').write_text(json.dumps({'snapshots_of': snapshot, 'runtime': build.VERSION}, indent=2))
    return files


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--check', action='store_true')
    a = p.parse_args()
    files = export(a.out.resolve())
    print(f'EXPORTED {len(files)} files to {a.out.resolve()}')
    if a.check:
        scratch = Path(tempfile.mkdtemp(prefix='Exanima-Lua-export-')) / 'out'
        result = subprocess.run([sys.executable, '-X', 'utf8', '-B', str(a.out.resolve() / 'build.py'), '--out', str(scratch)])
        if result.returncode:
            raise SystemExit('the exported copy did not build on its own')
        print('EXPORT_BUILDS_ALONE=' + str(scratch))


if __name__ == '__main__':
    main()
