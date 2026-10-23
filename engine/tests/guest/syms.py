#!/usr/bin/env python3
"""syms.py <ver> <arch>: guard symbols from the kernel's cosi (dwarf2json) file,
as kernel-lift would supply them.  Writes syms-<ver>-<arch>.env."""
import json, lzma, os, sys
K, G = os.environ['KERNEL_DIR'], os.environ['GEN']
v, a = sys.argv[1], sys.argv[2]
h = os.path.dirname(os.path.abspath(__file__))
ka = {'aarch64': 'arm64'}.get(a, a)
d = json.load(lzma.open(f'{K}/{v}/cosi.{ka}.json.xz'))
S = d['symbols']
def addr(*names):
    for n in names:
        if n in S and S[n].get('address'):
            return hex(S[n]['address']), n
    return '', ''
out = {}
out['KWRITE'], kw = addr('ksys_write', 'SyS_write', 'sys_write')
out['SEARCH'], _ = addr('search_exception_tables')
# powerpc has no out-of-line fixup_exception(): bad_page_fault() applies the extable
out['FIXUP'], fx = addr('fixup_exception', 'bad_page_fault')
out['SIG'], _ = addr('force_sig_fault')
out['SIGINFO'], _ = ('', '') if out['SIG'] else addr('force_sig_info')
out['EXTABLE'], _ = addr('__start___ex_table')
# a cheap syscall, for probe benchmarks
out['GETPPID'], _ = addr('__x64_sys_getppid', '__arm64_sys_getppid', '__riscv_sys_getppid', 'sys_getppid', 'SyS_getppid')

def ppc64_local_entry(path, big):
    """ELFv2: functions that set up the TOC (addis r2,r12 / addi r2,r2) are
    entered 8 bytes in by local callers, i.e. by the rest of the kernel."""
    import struct
    e = '>' if big else '<'
    b = open(path, 'rb').read()
    phoff, = struct.unpack_from(e + 'Q', b, 0x20)
    phentsize, phnum = struct.unpack_from(e + 'HH', b, 0x36)
    segs = []
    for i in range(phnum):
        o = phoff + i * phentsize
        typ, = struct.unpack_from(e + 'I', b, o)
        off, vaddr, _, filesz = struct.unpack_from(e + 'QQQQ', b, o + 8)
        if typ == 1:
            segs.append((vaddr, off, filesz))
    def fix(x):
        if not x:
            return x
        va = int(x, 16)
        for vaddr, off, sz in segs:
            if vaddr <= va < vaddr + sz:
                i0, i1 = struct.unpack_from(e + 'II', b, off + va - vaddr)
                if i0 >> 16 == 0x3c4c and i1 >> 16 == 0x3842:
                    return hex(va + 8)
        return x
    return fix

if a.startswith('powerpc64'):
    fix = ppc64_local_entry(f'{K}/{v}/vmlinux.{a}', a == 'powerpc64')
    for k in ('KWRITE', 'SEARCH', 'FIXUP', 'SIG', 'SIGINFO', 'GETPPID'):
        out[k] = fix(out[k])

with open(f'{G}/syms-{v}-{a}.env', 'w') as f:
    f.write(' '.join(f'{k}={x}' for k, x in out.items()) + '\n')
print(v, a, kw, fx, out)
