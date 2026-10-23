#!/usr/bin/env python3
"""hksummary.py <out dir>: markdown tables from hkbench.sh runs."""
import glob
import os
import re
import statistics as st
import sys

O = sys.argv[1]
ARCHS = ['x86_64', 'aarch64', 'armel', 'mipsel', 'mipseb', 'mips64el', 'mips64eb',
         'powerpc64', 'powerpc64le', 'riscv64', 'loongarch64']


def kv(line):
    return {k: v for k, v in re.findall(r'(\w+)=([^\s]+)', line)}


def bench(a, n, tag):
    """key=value dict of the first BENCH <tag> line of <a>-<n>.con, or {}."""
    p = f'{O}/{a}-{n}.con'
    if not os.path.exists(p):
        return {}
    for line in open(p, errors='replace'):
        if line.startswith(f'BENCH {tag} '):
            return {k: float(v) if re.match(r'^-?[\d.]+$', v) else v
                    for k, v in kv(line).items()}
    return {}


def wall(a, n):
    p = f'{O}/{a}-{n}.con'
    if not os.path.exists(p):
        return None
    for line in open(p, errors='replace'):
        m = re.match(r'WALL ([\d.]+) rc=(\d+)', line)
        if m and m.group(2) == '0':
            return float(m.group(1))
    return None


def trace(a, n):
    p = f'{O}/{a}-{n}.log'
    if not os.path.exists(p):
        return []
    return [kv(x) for x in open(p, errors='replace') if x.startswith('tcg_hooks_commit')]


def windows(a, n):
    """[(expected, got)] per window: guest HK expect lines vs plugin phase deltas."""
    con, log = f'{O}/{a}-{n}.con', f'{O}/{a}-{n}.log'
    if not (os.path.exists(con) and os.path.exists(log)):
        return None
    exp = [kv(x) for x in open(con, errors='replace') if x.startswith('HK expect')]
    ph = [kv(x) for x in open(log, errors='replace') if x.startswith('HOOKTEST phase=')]
    ph = [p for p in ph if p['phase'] not in ('8', '9')]
    res = []
    for e in exp:
        wi = int(e['window'])
        if wi + 1 >= len(ph):
            return None
        got = {k: int(ph[wi + 1][k]) - int(ph[wi][k]) for k in ('target', 'target2')}
        res.append(({k: int(e[k]) for k in ('target', 'target2')}, got))
    return res


def verdict(a, n):
    r = windows(a, n)
    if r is None:
        return '—'
    bad = [(e, g) for e, g in r if e != g]
    if not bad:
        return f'exact ({len(r)} windows)'
    return '; '.join(f"want {e['target']}/{e['target2']} got {g['target']}/{g['target2']}" for e, g in bad)


def f(x, d=1):
    return f'{x:.{d}f}' if isinstance(x, (int, float)) and not isinstance(x, bool) else '—'


out = []
w = out.append

w('## Per-hit cost (ns per call of a small function, 2M calls)\n')
w('| arch | call, no plugin | insn callbacks: hooked − unhooked | hooks: hooked − unhooked | hook removed − unhooked |')
w('|---|---|---|---|---|')
for a in ARCHS:
    n = bench(a, 'hit-none', 'hk')
    i = bench(a, 'hit-insn', 'hk')
    h = bench(a, 'hit-hook', 'hk')
    d = bench(a, 'hit-hookdyn', 'hk')
    if not n:
        continue
    w(f"| {a} | {f(n['hooked_ns'])} | {f(i and i['hooked_ns'] - i['unhooked_ns'])} | "
      f"{f(h and h['hooked_ns'] - h['unhooked_ns'])} | "
      f"{f(d and d['after_remove_ns'] - d['unhooked_ns'])} |")

w('\n## Register reads from a plugin hook (ns per hit)\n')
w('| arch | 0 reads | 1 read | 10 reads | per read |')
w('|---|---|---|---|---|')
for a in ARCHS:
    r = [bench(a, f'reads{k}', 'hk') for k in (0, 1, 10)]
    if not all(r):
        continue
    x = [v['hooked_ns'] for v in r]
    w(f'| {a} | {f(x[0])} | {f(x[1])} | {f(x[2])} | {f((x[2] - x[0]) / 10)} |')

w('\n## Register reads: gdbstub vs qemu_plugin_register_ptr() (ns per read, 10 reads per hit, minus 0 reads)\n')
w('| arch | hook, 0 reads (ns/hit) | gdbstub, new buffer | gdbstub, reused buffer | direct pointer | register size | direct = gdbstub (first 1000) | checks with direct=on |')
w('|---|---|---|---|---|---|---|---|')
for a in ARCHS:
    r = [bench(a, k, 'hk') for k in ('rd-0', 'rd-gdb', 'rd-reuse', 'rd-direct')]
    if not r[0]:
        continue
    base = r[0]['hooked_ns'] - r[0]['unhooked_ns']
    per = [f(x and (x['hooked_ns'] - r[0]['hooked_ns']) / 10) for x in r[1:]]
    lg = f'{O}/{a}-rd-direct.log'
    txt = open(lg, errors='replace').read() if os.path.exists(lg) else ''
    m = re.search(r'found=(\d) size=(\d+)', txt)
    size = (m.group(2) + ' B' if m.group(1) == '1' else 'not found') if m else '—'
    m = re.search(r'checked=(\d+) mismatch=(\d+)', txt)
    chk = f'{int(m.group(1)) - int(m.group(2))}/{m.group(1)}' if m else '—'
    con = f'{O}/{a}-check-direct.con'
    ctxt = open(con, errors='replace').read() if os.path.exists(con) else ''
    ok = [verdict(a, 'check-direct').startswith('exact')]
    for t in ('regs', 'redir'):
        m = re.search(rf'HK {t} got=(\d+) want=(\d+)', ctxt)
        ok.append(bool(m) and m.group(1) == m.group(2))
    cd = 'all pass' if all(ok) else 'FAIL ' + ''.join('.' if x else 'x' for x in ok)
    w(f'| {a} | {f(base)} | ' + ' | '.join(per) + f' | {size} | {chk} | {cd} |')

w('\n## Conditional hooks (ns per call of a small function, 2M calls, hooked − unhooked)\n')
w('| arch | plain hook | condition fails (no call) | condition holds (call) |')
w('|---|---|---|---|')
for a in ARCHS:
    r = [bench(a, k, 'hk') for k in ('cond-plain', 'cond-fail', 'cond-pass')]
    if not any(r):
        continue
    w(f'| {a} | ' + ' | '.join(f(x and x['hooked_ns'] - x['unhooked_ns']) for x in r) + ' |')

w('\n## Insert / remove from a callback (guest-measured, minus a no-op callback), after a broad warm-up\n')
w('| arch | insert, new address | remove | insert, address hooked before | blocks examined | blocks invalidated | commit (QEMU) |')
w('|---|---|---|---|---|---|---|')
for a in ARCHS:
    b = bench(a, 'ins', 'ins')
    if not b:
        continue
    t = [x for x in trace(a, 'ins') if x.get('flush') == '0']
    t = t[-1] if t else {}
    base = b['phase_ns']
    us = lambda k: f((b[k] - base) / 1e3, 0) + ' µs'
    w(f"| {a} | {us('insert_new_ns')} | {us('remove_ns')} | {us('reinsert_ns')} | "
      f"{t.get('tbs_scanned', '—')} | {t.get('tbs_invalidated', '—')} | "
      f"{f(int(t['ns']) / 1e3, 0) + ' µs' if t else '—'} |")

w('\n## Whole run (boot + short test + shutdown), seconds, mean ± sd\n')
w('| arch | no plugin | insn callbacks | hooks | hooks + 10,000 extra sites |')
w('|---|---|---|---|---|')
for a in ARCHS:
    cells = []
    for c in ('none', 'insn', 'hook', 'many'):
        xs = [x for x in (wall(a, os.path.basename(p)[len(a) + 1:-4])
                          for p in glob.glob(f'{O}/{a}-boot-{c}-*.con')) if x]
        cells.append(f'{st.mean(xs):.2f} ± {st.stdev(xs):.2f} (n={len(xs)})' if len(xs) > 1 else '—')
    if any(c != '—' for c in cells):
        w(f'| {a} | ' + ' | '.join(cells) + ' |')

w('\n## CF_PCREL: exec a 2,000-function PIE 50 times (ms per exec, after the first; one value per boot)\n')
w('| arch | ASLR off, PCREL on | ASLR off, PCREL off (hooks) | ASLR on, PCREL on | ASLR on, PCREL off (hooks) |')
w('|---|---|---|---|---|')
for a in ARCHS:
    cells = []
    for s_ in (0, 2):
        for p in ('pcrel', 'nopcrel'):
            xs = [bench(a, os.path.basename(c)[len(a) + 1:-4], 'exec') for c in
                  glob.glob(f'{O}/{a}-exec-{p}-aslr{s_}*.con')]
            xs = [x['rest_avg_us'] / 1e3 for x in xs if x]
            cells.append(' / '.join(f'{x:.2f}' for x in sorted(xs)) or '—')
    if all(c == '—' for c in cells):
        continue
    w(f'| {a} | ' + ' | '.join(cells) + ' |')

w('\n## Probes: in-guest (tracefs events) vs hypervisor hooks, ns added per hit\n')
w('| arch | uprobe (in guest) | user hook (hypervisor) | kprobe (in guest) | kernel hook (hypervisor; difference between two boots, noisy) | getppid, no probe |')
w('|---|---|---|---|---|---|')
for a in ARCHS:
    u = bench(a, 'probe-guest', 'uprobe')
    k = bench(a, 'probe-guest', 'kprobe')
    hv = bench(a, 'probe-hvk', 'kprobe')
    hu = bench(a, 'hit-hook', 'hk')
    if not (u or k or hv):
        continue
    w(f"| {a} | {f(u and u['hooked_ns'] - u['base_ns'])} | "
      f"{f(hu and hu['hooked_ns'] - hu['unhooked_ns'])} | "
      f"{f(k and k.get('hooked_ns') and k['hooked_ns'] - k['base_ns'])} | "
      f"{f(hv and k and hv['base_ns'] - k['base_ns'])} | {f(k and k['base_ns'])} |")


w('\n## Correctness (hit counts per window: target/target2)\n')
w('| arch | count + regs + redirect + fault | alias, hooks | alias, insn callbacks | dynamic insert/remove |')
w('|---|---|---|---|---|')
for a in ARCHS:
    con = f'{O}/{a}-check-count.con'
    if not os.path.exists(con):
        continue
    txt = open(con, errors='replace').read()
    log = open(f'{O}/{a}-check-count.log', errors='replace').read() if os.path.exists(f'{O}/{a}-check-count.log') else ''
    ok = []
    ok.append(verdict(a, 'check-count').startswith('exact'))
    m = re.search(r'HK regs got=(\d+) want=(\d+)', txt); ok.append(bool(m) and m.group(1) == m.group(2))
    m = re.search(r'HK redir got=(\d+) want=(\d+)', txt); ok.append(bool(m) and m.group(1) == m.group(2))
    m1 = re.search(r'HK fault resident_before=(\d) want=(.{32})', txt)
    m2 = re.search(r'HOOKTEST fault addr=\S+ result=0 data=(.{32})', log)
    ok.append(bool(m1 and m2) and m1.group(2) == m2.group(1))
    cnt = 'all pass' if all(ok) else 'FAIL ' + ''.join('.' if x else 'x' for x in ok)
    if m1:
        cnt += f' (page resident before: {m1.group(1)})'
    w(f"| {a} | {cnt} | {verdict(a, 'check-alias-hook')} | {verdict(a, 'check-alias-insn')} | {verdict(a, 'check-dyn')} |")

print('\n'.join(out))
