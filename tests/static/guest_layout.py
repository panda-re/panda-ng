"""Is the engine's guest-write layout map current, and does gcc agree with it? No guest.

    python3 tests/static/guest_layout.py [<engine>/include/panda-ng]

engine/src/panda/src/guest-layout.gen.h is generated from rust/panda-bindings/src/autogen by
tools/guest-layout/gen.py. This regenerates it and fails if the checked-in copy differs. With the
engine's per-arch headers it also compiles, for every arch, the map's checks (sizeof
CPUArchState, offsetof each followable env pointer) and the size of every struct the map lists,
against panda_c_<arch>.h, as the engine's own build does against its target. Needs gcc and glib's
pkg-config for that part (the flake check `guest-layout` runs it).
"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/guest-layout'))
import gen  # noqa: E402


def blocks(text):
    """arch -> its #if block of the generated header."""
    return {m.group(2): m.group(1) for m in re.finditer(
        r'^#if .*?\n(.*?)^#endif /\* (\w+) \*/', text, re.M | re.S)}


def probe(arch, block):
    lines = ['#include "c_sys_includes.h"', f'#include "panda_c_{arch}.h"']
    for cond in re.findall(r'^QEMU_BUILD_BUG_ON\((.*)\);$', block, re.M):
        lines.append(f'_Static_assert(!({cond}), "{cond}");')
    for name, size in re.findall(r'^    \{ "(\w+)", (\d+), guest_layout_follow', block, re.M):
        lines.append(f'_Static_assert(sizeof({name}) == {size}, "sizeof {name}");')
    return '\n'.join(lines) + '\n', len(lines) - 2


def main():
    text = gen.generate()
    if not gen.OUT.exists() or gen.OUT.read_text() != text:
        sys.exit('FAIL: engine/src/panda/src/guest-layout.gen.h is stale: '
                 'python3 tools/guest-layout/gen.py')
    print('PASS: guest-layout.gen.h is what gen.py makes from the bindings')
    if len(sys.argv) < 2:
        return
    incdir = sys.argv[1]
    glib = subprocess.check_output(['pkg-config', '--cflags', 'glib-2.0'], text=True).split()
    found = blocks(text)
    failed = []
    with tempfile.TemporaryDirectory() as tmp:
        for arch in list(gen.TARGETS) + list(gen.TWINS):
            source, n = probe(arch, found[gen.TWINS.get(arch, arch)])
            c = Path(tmp) / f'{arch}.c'
            c.write_text(source)
            for _ in range(20):
                r = subprocess.run(['gcc', '-w', '-fsyntax-only', '-D_GNU_SOURCE', f'-I{incdir}',
                                    f'-I{gen.ROOT / "include"}', *glib, str(c)],
                                   capture_output=True, text=True)
                # a struct tag without a typedef: sizeof(struct name)
                tags = set(re.findall(r"error: '(\w+)' undeclared", r.stderr.replace('‘', "'")
                                      .replace('’', "'")))
                if r.returncode == 0 or not tags:
                    break
                for t in tags:
                    source = source.replace(f'sizeof({t})', f'sizeof(struct {t})')
                c.write_text(source)
            if r.returncode:
                failed.append(arch)
                print(f'{arch}: {r.stderr[-3000:]}')
            else:
                print(f'{arch}: {n} layout checks hold')
    if failed:
        sys.exit(f'FAIL: the layout map disagrees with gcc on {" ".join(failed)}')
    print('PASS: the layout map agrees with gcc on every arch')


if __name__ == '__main__':
    main()
