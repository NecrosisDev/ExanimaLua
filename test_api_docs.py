"""API.md's reference must name every capability, event and Lua function the runtime defines, and its version."""
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent


def main() -> int:
    host = (HERE / 'src/script_host.h').read_text(encoding='utf-8')
    doc = (HERE / 'API.md').read_text(encoding='utf-8')
    start = doc.find('\n## Reference')
    if start < 0:
        print('API.md has no "## Reference" section')
        return 1
    end = doc.find('\n## ', start + 1)
    reference = doc[start:end if end > 0 else len(doc)]
    capabilities = re.findall(r'\{"([a-z_.]+)",\w+\}', host[host.index('capabilityNames[]'):host.index('inline uint64_t capability(')])
    events = re.findall(r'\{"([a-z_.]+)",Kind::', host) + ['item.use']
    # Every global table built in initialize() and the functions set on it.
    body = host[host.index('static int initialize(lua_State* L){'):host.index('static int invoke(lua_State* L){')]
    functions = []
    for table in re.finditer(r'lua_newtable\(L\);(.*?)lua_setglobal\(L,"(\w+)"\)', body, re.S):
        functions += [f'{table.group(2)}.{name}' for name in re.findall(r'lua_setfield\(L,-2,"(\w+)"\)', table.group(1))]
    problems = []
    for name in capabilities:
        if f'`{name}`' not in reference:
            problems.append(f'capability {name} is not in the reference')
    for name in events:
        if f'"{name}"' not in reference:
            problems.append(f'event "{name}" is not in the reference')
    for name in functions:
        if f'{name}' not in reference:
            problems.append(f'{name} is not in the reference')
    version = re.search(r'runtimeVersion="([^"]+)"', host).group(1)
    if f'runtime {version}' not in doc[:400]:
        problems.append(f'API.md does not open with runtime {version}')
    for p in problems:
        print(p)
    print(f'API_DOCS_{"FAILED" if problems else "PASSED"} capabilities={len(capabilities)} events={len(events)} functions={len(functions)}')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
