"""Does the layout map the engine build generated agree with the bindings? No guest.

    python3 tests/static/guest_layout.py <engine>/include/panda/guest-layout

The engine generates panda/src/guest-write.c's layout map for each softmmu target from its own
debug info (engine/src/panda/scripts/guest_layout.py, run by meson.build) and installs it as
include/panda/guest-layout/guest-layout-<target>.h. This reads it back and compares it with
what the committed bindgen bindings give (bindings_layout.py): two independent readings of
CPUArchState and the structs it points to, on the x86_64 host both come from.

They must list the same structs, with the same sizes, the same followable pointers and the same
CPUArchState fields. The engine's host-pointer ranges must cover every range from the bindings;
they may cover more, where bindgen gives a function pointer through a typedef that isn't one
(QEMUTimer's cb).
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bindings_layout as bl  # noqa: E402


def parse(text):
    """(types [(name, size)], host {i: [(offset, size)]}, follow {i: [...]}, fields)"""
    types = [(n, int(s)) for n, s in re.findall(
        r'^    \{ "(\w+)", (\d+), guest_layout_follow_\d+,', text, re.M)]
    host, follow = {}, {}
    for i, body in re.findall(r'guest_layout_host_(\d+)\[\] = \{\n(.*?)\};', text, re.S):
        host[int(i)] = [(int(o), int(s)) for o, s in re.findall(r'\{ (\d+), (\d+) \},', body)]
    for i, body in re.findall(r'guest_layout_follow_(\d+)\[\] = \{\n(.*?)\};', text, re.S):
        follow[int(i)] = [(int(o), int(t), int(e), int(n), p) for o, t, e, n, p in re.findall(
            r'\{ (\d+), (-?\d+), (\d+), (-?\d+) \}, /\* (\S+) \*/', body)]
    fields = [(int(o), int(s), n) for o, s, n in re.findall(
        r'^    \{ (\d+), (\d+), "(\w+)" \},', text, re.M)]
    return types, host, follow, fields


def covered(ranges, off, size):
    return any(o <= off and off + size <= o + s for o, s in ranges)


def compare(arch, text):
    types, host, follow, fields = parse(text)
    b = bl.Bindings((bl.AUTOGEN / f'{arch}.rs').read_text())
    want, _, top = bl.layout(arch, b)
    bad = []
    if [(n, s) for n, s, _, _ in want] != types:
        bad.append(f'structs differ:\n    bindings {[(n, s) for n, s, _, _ in want]}\n'
                   f'    engine   {types}')
        return bad, 0
    extra = 0
    for i, (name, size, wh, wf) in enumerate(want):
        if list(wf) != follow.get(i, []):
            bad.append(f'{name}: followable pointers differ:\n    bindings {wf}\n'
                       f'    engine   {follow.get(i)}')
        missing = [r for r in wh if not covered(host.get(i, []), *r)]
        if missing:
            bad.append(f'{name}: host pointers the engine map misses: {missing}')
        extra += sum(1 for r in host.get(i, []) if not covered(wh, *r))
    if sorted(top) != sorted(fields):
        bad.append(f'CPUArchState fields differ: only in bindings '
                   f'{sorted(set(top) - set(fields))[:6]}, only in engine '
                   f'{sorted(set(fields) - set(top))[:6]}')
    return bad, extra


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    incdir = Path(sys.argv[1])
    failed = []
    for arch in bl.ARCHES:
        header = incdir / f'guest-layout-{arch}-softmmu.h'
        if not header.exists():
            failed.append(arch)
            print(f'{arch}: no {header}')
            continue
        bad, extra = compare(arch, header.read_text())
        if bad:
            failed.append(arch)
            print(f'{arch}:\n  ' + '\n  '.join(bad))
        else:
            print(f'{arch}: the engine map agrees with the bindings'
                  + (f' ({extra} more host-pointer ranges)' if extra else ''))
    if failed:
        sys.exit(f'FAIL: the generated layout map disagrees with the bindings on {" ".join(failed)}')
    print('PASS: the generated layout map agrees with the bindings on every arch')


if __name__ == '__main__':
    main()
