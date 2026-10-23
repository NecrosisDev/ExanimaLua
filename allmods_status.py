"""Wait until all package-affecting main changes have fresh published release ZIPs."""
import fnmatch
import json
import os
import subprocess
import sys
from pathlib import Path


OWNER = os.environ.get('GITHUB_REPOSITORY_OWNER', 'NecrosisDev')
MODS = [
    ('ExanimaLua', 'v'),
    ('ExCamera', 'v'),
    ('Keyring', 'v'),
    ('Endless', 'v'),
    ('Grindstone', 'v'),
    ('LightSchool', 'v'),
    ('BodySchool', 'v'),
    ('Wounds', 'injuries-v'),
    ('Fire', 'v'),
    ('EnergySchool', 'v'),
    ('Wounds', 'groundfighters-v'),
]
NON_PACKAGE_PATHS = [
    '.github/**',
    'docs/**',
    'test/**',
    'tests/**',
    'test_*.py',
    '**/test_*.py',
]
NON_PACKAGE_PATHS_BY_REPO = {
    # These are the bundle builder and its inputs, not the runtime ZIP input.
    'ExanimaLua': ['allmods_status.py', 'bundle.py', 'bundle/**', 'emtk-startup.patch'],
}


def run(args: list[str]) -> str:
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout.strip()


def latest_release(repo: str, prefix: str) -> str | None:
    releases = json.loads(run([
        'gh', 'release', 'list', '--repo', f'{OWNER}/{repo}', '--exclude-drafts', '--limit', '100',
        '--json', 'tagName,publishedAt',
    ]))
    releases = [r for r in releases if r['tagName'].startswith(prefix)
                and r['tagName'][len(prefix):len(prefix) + 1].isdigit()]
    if not releases:
        return None
    return max(releases, key=lambda r: (r['publishedAt'] or '', r['tagName']))['tagName']


def remote_refs(repo: str) -> dict[str, str]:
    url = f'https://github.com/{OWNER}/{repo}.git'
    output = run(['git', 'ls-remote', url, 'refs/heads/main', 'refs/tags/*'])
    refs = {}
    for line in output.splitlines():
        sha, ref = line.split('\t', 1)
        refs[ref] = sha
    return refs


def ref_commit(refs: dict[str, str], tag: str) -> str | None:
    return refs.get(f'refs/tags/{tag}^{{}}') or refs.get(f'refs/tags/{tag}')


def package_changes(repo: str, base: str, head: str) -> tuple[bool, str | None]:
    comparison = json.loads(run([
        'gh', 'api', f'repos/{OWNER}/{repo}/compare/{base}...{head}',
    ]))
    if comparison.get('status') == 'identical':
        return False, None
    if comparison.get('status') != 'ahead':
        return True, f'{repo}: release tag is not an ancestor of main ({comparison.get("status", "unknown")})'
    files = comparison.get('files') or []
    if len(files) >= 300:
        return True, f'{repo}: compare reached GitHub\'s 300-file limit; cannot verify package freshness safely'
    excludes = NON_PACKAGE_PATHS + NON_PACKAGE_PATHS_BY_REPO.get(repo, [])
    changed = [item['filename'] for item in files]
    packaged = [
        path for path in changed
        if not (path.endswith('.md') and '/' not in path)
        and not any(fnmatch.fnmatchcase(path, pattern) for pattern in excludes)
    ]
    return bool(packaged), f'{repo}: release ZIP is stale for {", ".join(packaged[:5])}' if packaged else None


def main() -> int:
    output_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('dist/allmods-commits.txt')
    heads: dict[str, str] = {}
    refs_by_repo: dict[str, dict[str, str]] = {}
    waiting = []
    selected = []

    for repo, prefix in MODS:
        if repo not in refs_by_repo:
            refs = remote_refs(repo)
            refs_by_repo[repo] = refs
            if 'refs/heads/main' not in refs:
                waiting.append(f'{repo}: main branch was not found')
            else:
                heads[repo] = refs['refs/heads/main']

        tag = latest_release(repo, prefix)
        if tag is None:
            waiting.append(f'{repo}: no published {prefix}<version> release')
            continue
        selected.append(f'{repo} {tag}')
        tag_sha = ref_commit(refs_by_repo[repo], tag)
        head_sha = heads.get(repo)
        if tag_sha is None:
            waiting.append(f'{repo} {tag}: tag ref was not found')
        elif head_sha and tag_sha != head_sha:
            try:
                stale, reason = package_changes(repo, tag_sha, head_sha)
            except (subprocess.CalledProcessError, json.JSONDecodeError) as error:
                stale, reason = True, f'{repo}: could not verify packaged paths ({error})'
            if stale and reason:
                waiting.append(f'{repo} {tag}: {reason}; ZIP commit {tag_sha[:12]}, main {head_sha[:12]}')

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(''.join(f'{repo} {sha}\n' for repo, sha in sorted(heads.items())), encoding='utf-8')

    ready = not waiting and len(selected) == len(MODS)
    print('Selected releases:')
    for source in selected:
        print(f'  {source}')
    if ready:
        print('Every packaged main change is covered by the selected published release ZIPs.')
    else:
        print('Waiting for fresh release ZIPs where main changed packaged files:')
        for reason in waiting:
            print(f'  {reason}')

    github_output = os.environ.get('GITHUB_OUTPUT')
    if github_output:
        with open(github_output, 'a', encoding='utf-8') as stream:
            stream.write(f'ready={str(ready).lower()}\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
