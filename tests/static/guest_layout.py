"""Does the layout map the engine build generated agree with the bindings? No guest.

    python3 tests/static/guest_layout.py <engine>/include/panda/guest-layout

The engine generates panda/src/guest-write.c's layout map for each softmmu target from its own
debug info (engine/src/panda/scripts/guest_layout.py, run by meson.build) and installs it as
include/panda/guest-layout/guest-layout-<target>.h. This reads it back and compares it with
what the committed bindgen bindings give (bindings_layout.py): two independent readings of
CPUArchState and the structs it points to, on the x86_64 host both come from.

Every struct the bindings reach must be in the engine's map with the same size and the same
followable pointers, and CPUArchState must have the same fields. The engine's host-pointer
ranges must cover every range from the bindings. The engine's map may see more than the
bindings do:
- more host-pointer ranges, where bindgen gives a function pointer through a typedef that
  isn't one (QEMUTimer's cb);
- more followable pointers, to structs that are opaque in the bindings (x86's Clock).
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
    engine = {n: i for i, (n, _) in enumerate(types)}
    wanted = {n for n, _, _, _ in want}

    def named(entries, names):
        return [(o, names[t] if t >= 0 else None, e, n, p) for o, t, e, n, p in entries]

    bad, extra, more = [], 0, set()
    for name, size, wh, wf in want:
        i = engine.get(name)
        if i is None or types[i][1] != size:
            bad.append(f'{name}: bindings size {size}, engine '
                       f'{"missing" if i is None else types[i][1]}')
            continue
        theirs = named(follow.get(i, []), [n for n, _ in types])
        ours = named(wf, [n for n, _, _, _ in want])
        # the engine may follow pointers to structs the bindings have only as opaque
        only = [f for f in theirs if f not in ours]
        more |= {f[1] for f in only if f[1] not in wanted}
        if [f for f in theirs if f[1] in wanted or f[1] is None] != ours or \
                any(f[1] in wanted for f in only):
            bad.append(f'{name}: followable pointers differ:\n    bindings {ours}\n'
                       f'    engine   {theirs}')
        missing = [r for r in wh if not covered(host.get(i, []), *r)]
        if missing:
            bad.append(f'{name}: host pointers the engine map misses: {missing}')
        extra += sum(1 for r in host.get(i, []) if not covered(wh, *r))
    if sorted(top) != sorted(fields):
        bad.append(f'CPUArchState fields differ: only in bindings '
                   f'{sorted(set(top) - set(fields))[:6]}, only in engine '
                   f'{sorted(set(fields) - set(top))[:6]}')
    return bad, extra, more


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
        bad, extra, more = compare(arch, header.read_text())
        if bad:
            failed.append(arch)
            print(f'{arch}:\n  ' + '\n  '.join(bad))
        else:
            notes = ([f'{extra} more host-pointer ranges'] if extra else []) + \
                ([f'also follows {", ".join(sorted(more))}'] if more else [])
            print(f'{arch}: the engine map agrees with the bindings'
                  + (f' ({"; ".join(notes)})' if notes else ''))
    if failed:
        sys.exit(f'FAIL: the generated layout map disagrees with the bindings on {" ".join(failed)}')
    print('PASS: the generated layout map agrees with the bindings on every arch')


if __name__ == '__main__':
    main()
