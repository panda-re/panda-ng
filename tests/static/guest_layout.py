"""Is the engine's guest-write layout map current, and does gcc agree with it? No guest.

    python3 tests/static/guest_layout.py [<engine>/include/panda-ng [<patched engine source>]]

engine/src/panda/src/guest-layout.gen.h is generated from rust/panda-bindings/src/autogen by
tools/guest-layout/gen.py. This regenerates it and fails if the checked-in copy differs. With the
engine's per-arch headers it also compiles, for every arch, the map's checks (the compiler's
offsetof and sizeof against the bindings' numbers, for every entry) against panda_c_<arch>.h,
as the engine's own build does against its target on an x86_64 host. Needs gcc and glib's
pkg-config for that part (the flake check `guest-layout` runs it). With the patched engine source
it also checks gen.HOST_CONDITIONAL: every CPUArchState field the bindings have that QEMU's
target/*/cpu.h puts under a host accelerator's #if must be listed there with that condition,
because a host without that accelerator doesn't have the field.
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
    lines = ['#include "c_sys_includes.h"', f'#include "panda_c_{arch}.h"', '#include <stddef.h>']
    for cond in re.findall(r'^QEMU_BUILD_BUG_ON\((.*)\);$', block, re.M):
        lines.append(f'_Static_assert(!({cond}), "{cond}");')
    return '\n'.join(lines) + '\n', len(lines) - 3


# The preprocessor conditions that differ between the hosts the engine is built on
HOST_MACROS = re.compile(r'CONFIG_(KVM|XEN|MSHV|HVF|WHPX|NVMM)\b|HOST_')
TARGET_DIRS = {'aarch64': 'arm', 'arm': 'arm', 'x86_64': 'i386', 'i386': 'i386',
               'loongarch64': 'loongarch', 'mips64': 'mips', 'mips': 'mips', 'ppc64': 'ppc',
               'ppc': 'ppc', 'riscv64': 'riscv', 'riscv32': 'riscv'}


def host_conditional(cpu_h):
    """CPUArchState's fields under a host-dependent #if: field -> the #if's condition."""
    m = re.search(r'^(typedef )?struct CPUArchState \{\n(.*?)^\}', cpu_h, re.M | re.S)
    out, stack = {}, []
    for line in m.group(2).splitlines():
        d = re.match(r'\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)', line)
        if d:
            kind, rest = d.group(1), d.group(2).strip()
            if kind in ('if', 'ifdef', 'ifndef'):
                stack.append(rest if kind != 'ifdef' else f'defined({rest})')
            elif kind == 'endif':
                stack.pop()
            else:
                stack[-1] = None    # #else/#elif: not one condition; flagged if host-dependent
            continue
        f = re.search(r'(\w+)\s*(\[[^]]*\])*\s*;', line)
        conds = [c for c in stack if c is None or HOST_MACROS.search(c)]
        if f and conds:
            out[f.group(1)] = ' && '.join(c or '?' for c in conds)
    return out


def check_conditional(source):
    bad = []
    for arch, sub in TARGET_DIRS.items():
        text = (Path(source) / f'target/{sub}/cpu.h').read_text()
        fields = {gen.c_name(f) for f, _ in gen.Bindings((gen.AUTOGEN / f'{arch}.rs').read_text())
                  .structs[gen.ROOT_TYPE][1]}
        listed = gen.HOST_CONDITIONAL.get(arch, {})
        for field, cond in host_conditional(text).items():
            if field in fields and listed.get(field) != cond:
                bad.append(f'{arch}: {field} is under #if {cond}; '
                           f'HOST_CONDITIONAL has {listed.get(field)!r}')
    if bad:
        sys.exit('FAIL: gen.HOST_CONDITIONAL is out of date:\n  ' + '\n  '.join(bad))
    print('PASS: every host-conditional CPUArchState field the bindings have is in '
          'gen.HOST_CONDITIONAL')


def main():
    text = gen.generate()
    if not gen.OUT.exists() or gen.OUT.read_text() != text:
        sys.exit('FAIL: engine/src/panda/src/guest-layout.gen.h is stale: '
                 'python3 tools/guest-layout/gen.py')
    print('PASS: guest-layout.gen.h is what gen.py makes from the bindings')
    if len(sys.argv) < 2:
        return
    incdir = sys.argv[1]
    if len(sys.argv) > 2:
        check_conditional(sys.argv[2])
    glib = subprocess.check_output(['pkg-config', '--cflags', 'glib-2.0'], text=True).split()
    found = blocks(text)
    failed = []
    with tempfile.TemporaryDirectory() as tmp:
        for arch in list(gen.TARGETS) + list(gen.TWINS):
            source, n = probe(arch, found[gen.TWINS.get(arch, arch)])
            c = Path(tmp) / f'{arch}.c'
            c.write_text(source)
            r = subprocess.run(['gcc', '-w', '-fsyntax-only', '-D_GNU_SOURCE', f'-I{incdir}',
                                f'-I{gen.ROOT / "include"}', *glib, str(c)],
                               capture_output=True, text=True)
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
