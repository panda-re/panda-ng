#!/usr/bin/env python3
"""hktest.py <out dir>: verdicts for hktest.sh runs; exit 1 if any case failed."""
import os
import re
import sys

O = sys.argv[1]
ARCHS = ['x86_64', 'aarch64', 'armel', 'mipsel', 'mipseb', 'mips64el', 'mips64eb',
         'powerpc64', 'powerpc64le', 'riscv64', 'loongarch64']
CASES = ['count', 'direct', 'many', 'alias', 'dyn', 'multi', 'kern', 'cond', 'smp', 'rp']


def kv(line):
    return dict(re.findall(r'(\w+)=([^\s]+)', line))


def read(p):
    return open(p, errors='replace').read() if os.path.exists(p) else None


class Case:
    def __init__(self, a, n):
        self.a, self.n = a, n
        self.con = read(f'{O}/{a}-{n}.con')
        self.log = read(f'{O}/{a}-{n}.log') or ''
        self.args = kv(read(f'{O}/{a}-{n}.args') or '')
        self.fails = []

    def check(self, ok, what):
        if not ok:
            self.fails.append(what)

    def lines(self, prefix, text=None):
        return [kv(x) for x in (self.log if text is None else text).splitlines()
                if x.startswith(prefix)]

    def windows(self, extra=()):
        """Each guest 'HK expect' window against the plugin's phase counters.
        extra: counters that must move exactly like target in every window."""
        exp = self.lines('HK expect', self.con)
        ph = [p for p in self.lines('HOOKTEST phase=') if p['phase'] not in ('8', '9')]
        self.check(exp, 'no expectations from the guest')
        for e in exp:
            w = int(e.pop('window'))
            if w + 1 >= len(ph):
                self.check(False, f'window {w}: missing phase log')
                return
            d = {k: int(ph[w + 1][k]) - int(ph[w][k]) for k in ph[w] if k != 'phase'}
            for k, v in e.items():
                self.check(d.get(k) == int(v), f'window {w}: {k} {d.get(k)} want {v}')
            for k in extra:
                self.check(d.get(k) == d.get('target'),
                           f'window {w}: {k} {d.get(k)} want target {d.get("target")}')

    def count_extras(self):
        x = ['mid'] if 'mid' in self.args else []
        return x + (['ds'] if 'ds' in self.args else [])

    def regs_redir_fault(self, fault=True):
        for t in ('regs', 'redir'):
            m = re.search(rf'HK {t} got=(\d+) want=(\d+)', self.con)
            self.check(m and m.group(1) == m.group(2), f'{t}: {m and m.group(0)}')
        if fault:
            m1 = re.search(r'HK fault resident_before=(\d) want=(.{32})', self.con)
            m2 = re.search(r'HOOKTEST fault addr=\S+ result=0 data=(.{32})', self.log)
            self.check(m1 and m2 and m1.group(2) == m2.group(1), 'fault from a hook')

    def run(self):
        skip = read(f'{O}/{self.a}-{self.n}.skip')
        if skip:
            return 'SKIP: ' + skip.strip()
        if self.con is None:
            return 'not run'
        rc = re.search(r'^RC (\d+)', self.con, re.M)
        self.check(rc and rc.group(1) == '0', f'qemu exit {rc and rc.group(1)}')
        getattr(self, 'c_' + self.n)()
        return 'PASS' if not self.fails else 'FAIL: ' + '; '.join(self.fails)

    def c_count(self):
        self.windows(self.count_extras())
        self.regs_redir_fault()

    def c_direct(self):
        self.c_count()
        found = self.lines('HOOKTEST direct vcpu=')
        self.check(found and all(f['found'] == '1' for f in found), 'register_ptr not found')
        d = self.lines('HOOKTEST direct checked=')
        self.check(d and int(d[-1]['checked']) > 0 and d[-1]['mismatch'] == '0',
                   f'direct vs gdbstub: {d and d[-1]}')

    def c_many(self):
        self.c_count()

    def c_alias(self):
        self.windows()

    def c_dyn(self):
        self.windows()

    def c_multi(self):
        self.windows(self.count_extras() + ['dup'])
        ex = self.lines('HOOKTEST exit')
        self.check(ex and ex[-1].get('selfrm') == self.args.get('selfrm'),
                   f"self-removing hook ran {ex and ex[-1].get('selfrm')} times, "
                   f"want {self.args.get('selfrm')}")
        self.regs_redir_fault()

    def c_kern(self):
        self.windows()

    def c_cond(self):
        # hk-dyn without dyn=on: 4 windows of hk_target(0..k-1), all hooked
        ph = [p for p in self.lines('HOOKTEST phase=') if p['phase'] not in ('8', '9')]
        self.check(len(ph) >= 5, f'{len(ph)} phase logs')
        for w in range(min(4, len(ph) - 1)):
            d = {k: int(ph[w + 1][k]) - int(ph[w][k]) for k in ph[w] if k != 'phase'}
            k = d['target']
            want = {'ceq': int(k > 7), 'cltu': min(10, k), 'cmask': k // 4,
                    'crange': max(0, min(k, 30) - 20), 'cgt': k,
                    'chost': k if w % 2 == 0 else 0}
            self.check(k == 100, f'window {w}: target {k} want 100')
            for c, v in want.items():
                self.check(d.get(c) == v, f'window {w}: {c} {d.get(c)} want {v}')

    def c_smp(self):
        m = re.search(r'HK smp cpus=(\d+)', self.con or '')
        if m and int(m.group(1)) < 2:
            self.skip = f'SKIP (guest sees {m.group(1)} CPU)'
        self.windows()
        found = self.lines('HOOKTEST direct vcpu=')
        self.check(found and all(f['found'] == '1' for f in found), 'register_ptr not found')
        if not getattr(self, 'skip', None):
            self.check(len({f['vcpu'] for f in found}) == 2,
                       f"register_ptr looked up on vCPUs {[f['vcpu'] for f in found]}")
        d = self.lines('HOOKTEST direct checked=')
        self.check(d and d[-1]['mismatch'] == '0', f'direct vs gdbstub: {d and d[-1]}')

    def c_rp(self):
        self.c_count()
        rps = self.lines('HOOKTEST rp name=')
        self.check(rps, 'no register_ptr results')
        for r in rps:
            nm = r['name']
            self.check(r['found'] != '-1', f'{nm}: no such register')
            if r['found'] == '-1':
                continue
            want = '1' if r['expect'] == '2' else r['expect']
            self.check(r['found'] == want, f"{nm}: found={r['found']} want {want}")
            if r['expect'] == '2':
                self.check(r.get('nonzero') == '1',
                           f'{nm}: return address reads 0 at a function entry')
            self.check(r['same_state'] == '1', f'{nm}: lookup changed a register')
            self.check(r['eq'] == '1', f'{nm}: value differs from the gdbstub')
        d = self.lines('HOOKTEST rp checked=')
        self.check(d and int(d[-1]['checked']) > 0 and d[-1]['mismatch'] == '0',
                   f'per-hit check: {d and d[-1]}')


rows, bad = [], 0
for a in ARCHS:
    cells = []
    for n in CASES:
        c = Case(a, n)
        v = c.run()
        if v.startswith('PASS') and getattr(c, 'skip', None):
            v = c.skip
        bad += v.startswith('FAIL')
        cells.append(v)
    if any(v != 'not run' for v in cells):
        rows.append((a, cells))

print('| arch | ' + ' | '.join(CASES) + ' |')
print('|---' * (len(CASES) + 1) + '|')
for a, cells in rows:
    print(f'| {a} | ' + ' | '.join(cells) + ' |')
n = sum(v.startswith(('PASS', 'SKIP')) for _, cs in rows for v in cs)
print(f'\n{n} passed or skipped, {bad} failed')
sys.exit(1 if bad else 0)
