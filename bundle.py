"""Builds the all-mods bundle: dist/ExanimaMods-<runtime version>.zip with a .sha256 beside it.

    python -X utf8 -B bundle.py --download [--owner NecrosisDev]   (CI: each mod's newest GitHub release)
    python -X utf8 -B bundle.py --mods <folder of release ZIPs>    (local: ZIPs already at hand)

The bundle holds this runtime (dist/mods/com.necrosis.script-runtime, from release.py), every mod in MODS from
its own release ZIP, the patched EMTK framework and launcher with the patch they were built from, the
"Necrosis Mods" EMTK profile, Install.cmd and a README (all under bundle/). No game files: each mod's release ZIP
already leaves out anything built from the game's data. --download needs the GitHub CLI and the mod repositories'
releases to be readable with GH_TOKEN.
"""
import argparse
import fnmatch
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BUNDLE = ROOT / 'bundle'
RUNTIME = ROOT / 'dist' / 'mods' / 'com.necrosis.script-runtime'
# (repository, tag prefix, release asset, mod id, title). The profile in bundle/ lists the same ids.
MODS = [
    ('ExCamera', 'v', 'ExCamera-*-minimal.zip', 'com.necrosis.excamera', 'ExCamera'),
    ('Keyring', 'v', 'Keyring-*.zip', 'com.necrosis.keyring', 'Keyring'),
    ('Endless', 'v', 'Endless-*.zip', 'com.necrosis.endless', 'Endless'),
    ('Grindstone', 'v', 'Grindstone-*.zip', 'com.necrosis.grindstone', 'Grindstone'),
    ('LightSchool', 'v', 'LightSchool-*.zip', 'com.necrosis.light-orb', 'Light School'),
    ('BodySchool', 'v', 'BodySchool-*.zip', 'com.necrosis.stoneskin', 'Body School'),
    ('Wounds', 'injuries-v', 'Injuries-*.zip', 'com.necrosis.injuries', 'Injuries'),
    ('Fire', 'v', 'Fire-*.zip', 'com.necrosis.fire', 'Fire'),
    ('EnergySchool', 'v', 'EnergySchool-*.zip', 'com.necrosis.energy-school', 'Energy School'),
    ('Wounds', 'groundfighters-v', 'GroundFighters-*.zip', 'com.necrosis.groundfighters', 'Ground Fighters'),
]


def download(owner: str, into: Path) -> None:
    """The newest published release of each mod whose tag starts with its prefix."""
    for repo, prefix, asset, _mod_id, _title in MODS:
        listed = subprocess.run(['gh', 'release', 'list', '--repo', f'{owner}/{repo}', '--exclude-drafts', '--limit', '100',
                                 '--json', 'tagName,publishedAt'], check=True, capture_output=True, text=True).stdout
        tags = [r for r in json.loads(listed) if r['tagName'].startswith(prefix) and r['tagName'][len(prefix):][:1].isdigit()]
        if not tags:
            raise SystemExit(f'{owner}/{repo} has no {prefix}<version> release')
        tag = max(tags, key=lambda r: r['publishedAt'])['tagName']
        subprocess.run(['gh', 'release', 'download', tag, '--repo', f'{owner}/{repo}', '--pattern', asset,
                        '--dir', str(into), '--clobber'], check=True)
        print(f'{repo} {tag}')


def unpack(archive: Path, mod_id: str, into: Path) -> None:
    """Release ZIPs hold mods/<id>/...; ExCamera's holds its files at the top."""
    with zipfile.ZipFile(archive) as z:
        for name in z.namelist():
            if name.endswith('/'):
                continue
            relative = name[len(f'mods/{mod_id}/'):] if name.startswith(f'mods/{mod_id}/') else name
            if relative == name and name.startswith('mods/'):
                raise SystemExit(f'{archive.name} holds {name}, not {mod_id}')
            target = into / 'mods' / mod_id / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(z.read(name))


def version(manifest: Path) -> str:
    match = re.search(r'^version = "([^"]+)"', manifest.read_text(encoding='utf-8'), re.M)
    return match.group(1) if match else '?'


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--download', action='store_true', help="fetch each mod's newest GitHub release")
    source.add_argument('--mods', type=Path, help='folder holding the mods\' release ZIPs')
    parser.add_argument('--owner', default=os.environ.get('GITHUB_REPOSITORY_OWNER', 'NecrosisDev'))
    args = parser.parse_args()
    if not (RUNTIME / 'manifest.toml').is_file():
        raise SystemExit(f'no runtime build in {RUNTIME}; run release.py first')

    with tempfile.TemporaryDirectory(prefix='exanima-bundle-') as staging:
        stage = Path(staging)
        zips = args.mods
        if args.download:
            zips = stage / 'downloads'
            zips.mkdir()
            download(args.owner, zips)
        name = f'ExanimaMods-{version(RUNTIME / "manifest.toml")}'
        out = stage / name
        shutil.copytree(RUNTIME, out / 'mods' / RUNTIME.name, ignore=shutil.ignore_patterns('ScriptRuntime.ini', 'ScriptRuntime.log'))
        lines = [f'  {"Exanima Lua Runtime":<20} {version(RUNTIME / "manifest.toml")}']
        for _repo, _prefix, asset, mod_id, title in MODS:
            found = sorted(p for p in zips.iterdir() if fnmatch.fnmatch(p.name, asset))
            if not found:
                raise SystemExit(f'no {asset} in {zips}')
            unpack(found[-1], mod_id, out)
            lines.append(f'  {title:<20} {version(out / "mods" / mod_id / "manifest.toml")}')
        # EMTK needs the plugin config beside ExCamera's DLL; its release ZIP carries only the manifest.
        shutil.copyfile(BUNDLE / 'excamera' / 'config.toml', out / 'mods' / 'com.necrosis.excamera' / 'config.toml')
        shutil.copytree(BUNDLE / 'EMTK', out / 'EMTK')
        shutil.copyfile(ROOT / 'emtk-startup.patch', out / 'EMTK' / 'emtk-startup.patch')
        shutil.copytree(BUNDLE / 'profile', out / 'profile')
        for file in ('Install.ps1', 'Install.cmd'):
            shutil.copyfile(BUNDLE / file, out / file)
        readme = (BUNDLE / 'README.txt').read_text(encoding='utf-8').replace('{MODS}', '\n'.join(lines))
        (out / 'README.txt').write_text(readme, encoding='utf-8')

        files = sorted(p for p in out.rglob('*') if p.is_file())
        sums = ''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest().upper()}  {p.relative_to(out).as_posix()}\n' for p in files)
        (out / 'SHA256SUMS.txt').write_text(sums, encoding='ascii')
        archive = ROOT / 'dist' / f'{name}.zip'
        archive.parent.mkdir(exist_ok=True)
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for path in sorted(out.rglob('*')):
                if path.is_file():
                    z.write(path, f'{name}/{path.relative_to(out).as_posix()}')
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_name(archive.name + '.sha256').write_text(f'{digest}  {archive.name}\n', encoding='ascii')
    print('\n'.join(lines))
    print(f'BUNDLE={archive} sha256={digest}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
