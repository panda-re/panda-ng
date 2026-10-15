#!/usr/bin/env python3
"""sctest.py <out dir>: verdicts and tables for sctest.sh; exit 1 if a func
run failed."""
import os
import re
import sys

O = sys.argv[1]
ARCHS = ['x86_64', 'aarch64', 'armel', 'mipsel', 'mipseb', 'mips64el', 'mips64eb',
         'powerpc64', 'powerpc64le', 'riscv64', 'loongarch64']


def kv(line):
    """key=value pairs; a quoted value may hold spaces."""
    return {k: (q if q else v) for k, q, v in
            re.findall(r'(\w+)=(?:"([^"]*)"|(\S+))', line)}


def read(p):
    return open(p, errors='replace').read() if os.path.exists(p) else None


def func(a, n):
    con, log = read(f'{O}/{a}-{n}.con'), read(f'{O}/{a}-{n}.log') or ''
    if con is None:
        return None
    fails = []

    def check(ok, what):
        if not ok:
            fails.append(what)

    rc = re.search(r'^RC (\d+)', con, re.M)
    check(rc and rc.group(1) == '0', f'qemu exit {rc and rc.group(1)}')
    exp = {}
    for line in con.splitlines():
        m = re.match(r'SC expect (\w+) (.*)', line)
        if m:
            exp.setdefault(m.group(1), []).append(kv(m.group(2)))
    ev = [dict(kv(x), ev=x.split()[1]) for x in log.splitlines()
          if x.startswith('SC ') and x.split()[1] in ('enter', 'return', 'child')]
    cnt = {d['tag']: d for d in (kv(x) for x in log.splitlines() if x.startswith('SC count'))}
    boot = re.search(r'SC boot syscalls=(\d+) hooks=(\d+) inline=(\d+)', log)
    check(boot and int(boot.group(1)) > 300, f'boot: {boot and boot.group(0)}')
    for e in re.findall(r'SC error: .*', log):
        if 'no syscall fork' not in e and 'no syscall clone' not in e:
            fails.append(e)
    self_pid = exp.get('self', [{}])[0].get('pid')
    check(self_pid, 'no guest expectations')

    def tagged(tag, what):
        return [e for e in ev if e.get('tag') == tag and e['ev'] == what]

    def c(tag, k):
        return int(cnt.get(tag, {}).get(k, -1))

    # openat: resident path at entry, then one only readable at return
    en, rt = tagged('open', 'enter'), tagged('open', 'return')
    want = exp.get('open', [])
    check(len(en) == len(want) == 2 and len(rt) == 2,
          f'open: {len(en)} enters, {len(rt)} returns, want 2')
    pen = n.startswith('pfunc')
    for i, w in enumerate(want[:len(en)]):
        check(en[i].get('a1') == '/data.sqsh', f"open {i}: path {en[i].get('a1')}")
        # the engine faults the path in at the entry: never late
        late = en[i].get('late', '0')
        check(late == ('0' if pen else w['late']), f"open {i}: late={late} want {w['late']}")
        check(en[i].get('pid') == self_pid, f"open {i}: pid {en[i].get('pid')}")
        if i < len(rt):
            check(rt[i].get('ret') == w['fd'], f"open {i}: ret {rt[i].get('ret')} want {w['fd']}")

    # ioctl filters
    for tag, k in (('ioeq', 'eq'), ('iorange', 'range'), ('ioall', None),
                   ('ionone', None), ('ioneg', 'neg')):
        want_n = {'ioall': 4, 'ionone': 2}.get(tag) or int(exp.get('ioctl', [{}])[0].get(k, -1))
        check(c(tag, 'enter') == want_n, f'{tag}: {c(tag, "enter")} want {want_n}')

    # a blocked read, returned after a child wrote
    w = exp.get('blockread', [{}])[0]
    rt = tagged('blockread', 'return')
    check(len(rt) == 1 and rt[0].get('ret') == w.get('ret') and rt[0].get('pid') == w.get('pid'),
          f'blockread: {rt} want ret={w.get("ret")}')

    # fork: the child's first return and the parent's
    w = exp.get('fork', [{}])[0]
    ch = [e for e in tagged('fork', 'child') if e.get('pid') == w.get('child')]
    check(pen or (len(ch) == 1 and ch[0].get('ret') == '0'), f'fork child: {ch}')
    pr = [e for e in tagged('forkret', 'return') if e.get('ret') == w.get('child')]
    check(len(pr) == 1 and pr[0].get('pid') == w.get('parent'), f'fork parent return: {pr}')

    for k in ('override', 'notoverridden', 'setarg', 'setret'):
        w = exp.get(k, [{}])[0]
        check(w.get('got') is not None and w.get('got') == w.get('want'), f'{k}: {w}')

    w = exp.get('getpid', [{}])[0].get('n')
    check(str(c('getpid', 'enter')) == w, f'getpid count {c("getpid", "enter")} want {w}')
    check(c('siderr', 'return') == 1 and c('sidok', 'return') == 1,
          f'getsid: err={c("siderr", "return")} ok={c("sidok", "return")}')
    check(c('exit', 'enter') >= 3 and c('exit', 'return') == 0,
          f'exit_group: enter={c("exit", "enter")} return={c("exit", "return")}')
    check(c('all', 'enter') >= 30, f'all: {c("all", "enter")}')
    pend = re.search(r'SC exit pending=(\d+) stale=(\d+)', log)
    check(pend and pend.group(1) == '0' and pend.group(2) == '0', f'leftover: {pend and pend.group(0)}')
    if n.startswith('tfunc'):
        check('mode=trap' in log, 'not in trap mode')
    if pen:
        # igloo's dispatch: the first skip ends the entry, the return hooks see
        # its value; arguments and return values chain from hook to hook
        check('mode=pen' in log, 'not under pyhost')
        check(c('override', 'enter') == 1 and c('afterskip', 'enter') == 0,
              f'skip: later hook ran {c("afterskip", "enter")} times')
        sr = tagged('skipret', 'return')
        check(len(sr) == 1 and sr[0].get('ret') == '777', f'skip: return hooks saw {sr}')
        a2 = tagged('setarg2', 'enter')
        check(len(a2) == 1 and a2[0].get('a1') == '0x65', f'argument chain: {a2}')
        r2 = tagged('setret', 'return')
        check(len(r2) == 1 and r2[0].get('ret') == '54', f'return chain: {r2}')
        check(c('allneg', 'enter') == 0, f'on_all comm filter: {c("allneg", "enter")}')

    extra = ''
    if boot:
        extra = f' (hooks {boot.group(2)}, inline {boot.group(3)})'
    br = tagged('blockread', 'enter'), tagged('blockread', 'return')
    if br[0] and br[1] and br[0][0].get('vcpu') != br[1][0].get('vcpu'):
        extra += ' read returned on another vCPU'
    return ('PASS' + extra) if not fails else 'FAIL: ' + '; '.join(fails)


def sctrap(a, n):
    """sc-trap (trap mode): calls whose return is not simply the next return
    to user mode."""
    con, log = read(f'{O}/{a}-{n}.con'), read(f'{O}/{a}-{n}.log') or ''
    if con is None:
        return None
    fails = []

    def check(ok, what):
        if not ok:
            fails.append(what)

    rc = re.search(r'^RC (\d+)', con, re.M)
    check(rc and rc.group(1) == '0', f'qemu exit {rc and rc.group(1)}')
    exp = {}
    for line in con.splitlines():
        m = re.match(r'SC expect (\w+) (.*)', line)
        if m:
            exp[m.group(1)] = kv(m.group(2))
    ev = [dict(kv(x), ev=x.split()[1]) for x in log.splitlines()
          if x.startswith('SC ') and x.split()[1] in ('enter', 'return')]
    restarts = [e for e in ev if e.get('restart') == '1']
    if n.startswith('ptrap'):
        # the engine reports the restarted call's return; the plugin does not
        check(len(restarts) == 1 and restarts[0].get('tag') == 'restart' and
              restarts[0].get('ret') == '-512', f'restart event: {restarts}')
        ev = [e for e in ev if e not in restarts]
    me = exp.get('self', {}).get('pid')
    check(me, 'no guest expectations')

    def tagged(tag, what=None):
        return [e for e in ev if e.get('tag') == tag and (what is None or e['ev'] == what)]

    want32 = n.endswith('32')
    abis = {e.get('abi') for e in ev}
    check(abis == ({'i386'} if a == 'x86_64' else {'arm'}) if want32 else
          abis == {a}, f'abi {abis}')

    # interrupted read: entered once, returns -EINTR after the handler's calls
    w = exp.get('eintr', {})
    en, rt = tagged('eintr', 'enter'), tagged('eintr', 'return')
    check(len(en) == 1 and len(rt) == 1 and rt[0].get('ret') == w.get('ret') == '-4' and
          rt[0].get('pid') == me, f'eintr: {len(en)} enters, returns {[r.get("ret") for r in rt]}')
    if rt:
        i = ev.index(rt[0])
        check(len([e for e in ev[:i] if e.get('tag') == 'inhandler']) == 2,
              'eintr: not after the handler\'s 2 calls')
    ih = tagged('inhandler', 'return')
    check(len(ih) == 4 and all(e.get('ret') == '-3' for e in ih),
          f'inhandler: {[e.get("ret") for e in ih]} want 4 x -3')
    # SA_RESTART: entered twice (the restart traps again), one return
    w = exp.get('restart', {})
    en, rt = tagged('restart', 'enter'), tagged('restart', 'return')
    check(len(en) == 2 and len(rt) == 1 and rt[0].get('ret') == w.get('ret') == '5',
          f'restart: {len(en)} enters, returns {[r.get("ret") for r in rt]}')
    check(re.search(r'restart=1\b', log), 'restart: not seen as a restart')
    # vfork: the parent's return has the child's pid
    w = exp.get('vfork', {})
    rt = tagged('vfork', 'return')
    check(len(rt) == 1 and rt[0].get('ret') == w.get('child') and rt[0].get('pid') == w.get('parent'),
          f'vfork: {[(r.get("pid"), r.get("ret")) for r in rt]} want {w}')
    rt = tagged('vchild', 'return')
    check(len(rt) == 1 and rt[0].get('pid') == w.get('child') and rt[0].get('ret') == '-3',
          f'vfork child: {[(r.get("pid"), r.get("ret")) for r in rt]}')
    # execve: the failure returns, the success does not
    rt = tagged('exec', 'return')
    check(len(rt) == 1 and rt[0].get('ret') == exp.get('exec', {}).get('fail') == '-2',
          f'exec: {[r.get("ret") for r in rt]}')
    # threads blocked in read at once
    w = exp.get('thr', {})
    got = sorted(f'{e.get("pid")}:{e.get("ret")}' for e in tagged('thr', 'return'))
    check(got == sorted([w.get('t0'), w.get('t1')]), f'threads: {got} want {w}')
    # the successful execve's call: pending, or dropped with its task (the reaper)
    pend = re.search(r'SC exit pending=(\d+) stale=(\d+)', log)
    reaped = re.search(r'SC exit .* reaped=(\d+)', log)
    check(pend and int(pend.group(1)) + int(reaped.group(1) if reaped else 0) == 1 and
          pend.group(2) == '0',
          f'leftover (want the successful execve only): {pend and pend.group(0)} '
          f'{reaped and reaped.group(0)[-12:]}')
    return 'PASS' if not fails else 'FAIL: ' + '; '.join(fails)


def pen(a, n):
    """sc-pen (pyhost.py): string filters, unregister from a callback, scope,
    a pid filter registered at run time, skip, chains."""
    con, log = read(f'{O}/{a}-{n}.con'), read(f'{O}/{a}-{n}.log') or ''
    if con is None:
        return None
    fails = []

    def check(ok, what):
        if not ok:
            fails.append(what)
    rc = re.search(r'^RC (\d+)', con, re.M)
    check(rc and rc.group(1) == '0', f'qemu exit {rc and rc.group(1)}')
    exp = {}
    for line in con.splitlines():
        m = re.match(r'SC expect (\w+) (.*)', line)
        if m:
            exp[m.group(1)] = kv(m.group(2))
    ev = [dict(kv(x), ev=x.split()[1]) for x in log.splitlines()
          if x.startswith('SC ') and x.split()[1] in ('enter', 'return')]
    cnt = {d['tag']: d for d in (kv(x) for x in log.splitlines() if x.startswith('SC count'))}

    def c(tag, k='enter'):
        return int(cnt.get(tag, {}).get(k, -1))
    me = exp.get('self', {}).get('pid')
    check(me, 'no guest expectations')
    want32 = n.endswith('32')
    abis = {e.get('abi') for e in ev}
    check(abis == ({'i386'} if a == 'x86_64' else {'arm'}) if want32 else abis == {a},
          f'abi {abis}')
    for tag in ('sw', 'ct', 'ew'):
        e = [x for x in ev if x.get('tag') == tag]
        check(len(e) == 1 and e[0].get('a1') == '/proc/self/status' and e[0].get('pid') == me,
              f'string filter {tag}: {e}')
    check(c('exneg') == 0 and c('swneg') == 0,
          f'string filters that must fail: exact {c("exneg")}, prefix {c("swneg")}')
    check(c('unregister') == 3, f'unregister: {c("unregister")} events, want 3')
    w = exp.get('scope', {})
    check(w.get('status') == '0', f'scope: unshare failed {w}')
    check(c('scope_in') == int(w.get('in', -1)) and
          c('scope_all') == int(w.get('in', 0)) + int(w.get('out', 0)),
          f'scope: in {c("scope_in")} all {c("scope_all")} want {w}')
    pf = [x for x in ev if x.get('tag') == 'pidfilter']
    check(len(pf) == 4 and all(x.get('pid') == me for x in pf), f'pid filter: {len(pf)} events')
    for k in ('skip', 'retchain', 'argchain'):
        w = exp.get(k, {})
        check(w.get('got') is not None and w.get('got') == w.get('want'), f'{k}: {w}')
    check(c('afterskip') == 0, f'skip: the later hook ran {c("afterskip")} times')
    sr = [x for x in ev if x.get('tag') == 'skipret']
    check(len(sr) == 1 and sr[0].get('ret') == '616', f'skip: return hooks saw {sr}')
    r2 = [x for x in ev if x.get('tag') == 'ret2']
    check(len(r2) == 1 and r2[0].get('ret') == '1', f'return chain: {r2}')
    a2 = [x for x in ev if x.get('tag') == 'arg2']
    check(len(a2) == 1 and a2[0].get('a1') == '0xc9', f'argument chain: {a2}')
    check(c('allneg') == 0, f'on_all comm filter: {c("allneg")}')
    pend = re.search(r'SC exit pending=(\d+) stale=(\d+)', log)
    check(pend and pend.group(1) == '0' and pend.group(2) == '0', f'leftover: {pend and pend.group(0)}')
    return 'PASS' if not fails else 'FAIL: ' + '; '.join(fails)


def fault(a, n):
    """sc-fault (pyhost.py "fault"): string filters and callbacks' commands on
    pages the task has not touched, at both events; an address with no
    mapping (refused, no signal); a write that has to copy the page."""
    con, log = read(f'{O}/{a}-{n}.con'), read(f'{O}/{a}-{n}.log') or ''
    if con is None:
        return None
    fails = []

    def check(ok, what):
        if not ok:
            fails.append(what)
    rc = re.search(r'^RC (\d+)', con, re.M)
    check(rc and rc.group(1) == '0', f'qemu exit {rc and rc.group(1)}')
    check('segfault' not in con, 'the guest reported a segfault')
    exp = {}
    for line in con.splitlines():
        m = re.match(r'SC expect (\w+) (.*)', line)
        if m:
            exp[m.group(1)] = kv(m.group(2))
    check(exp.get('self'), 'no guest expectations')
    for k in ('fstr', 'fbad', 'fread'):
        w = exp.get(k, {})
        check(w.get('got') is not None and w.get('got') == w.get('want'), f'{k}: {w}')
    w = exp.get('fwrite', {})
    check(w.get('pipe') == 'COW-DONE' and w.get('mem') == 'COW-DONE' and
          w.get('file') == 'cow-me-original', f'write: {w}')
    data = {}
    for x in log.splitlines():
        if x.startswith('SC data'):
            d = kv(x)
            data.setdefault(d['tag'], []).append(d)
    cnt = {d['tag']: d for d in (kv(x) for x in log.splitlines() if x.startswith('SC count'))}

    def c(tag, k='enter'):
        return int(cnt.get(tag, {}).get(k, -1))
    check(c('fstr') == 1, f'string filter: {c("fstr")} events, want 1')
    r = data.get('fread', [{}])
    check(len(r) == 1 and r[0].get('direct') == '0' and r[0].get('data') == 'hello-from-page1'
          and int(r[0].get('faults', 0)) >= 1, f'read at the entry: {r}')
    r = data.get('fwrite', [{}])
    check(len(r) == 1 and r[0].get('readback') == 'COW-DONE' and int(r[0].get('faults', 0)) >= 1,
          f'write at the entry: {r}')
    r = data.get('fret', [])
    check(len(r) == 2 and r[0].get('a0') == 'page3-at-return' and
          r[0].get('a1') == 'page4-at-return' and int(r[0].get('faults', 0)) >= 2 and
          r[1].get('a0') == 'FAIL' and r[1].get('a1') == 'page3-at-return',
          f'reads at the return: {r}')
    # one run per callback, however many faults
    check(c('fread') == 1 and c('fwrite') == 1 and c('fret', 'return') == 2,
          f'runs: read {c("fread")} write {c("fwrite")} return {c("fret", "return")}')
    ex = kv(re.search(r'SC exit .*', log).group(0)) if 'SC exit' in log else {}
    check(int(ex.get('refused', 0)) >= 2 and ex.get('runs') == '0' and ex.get('pending') == '0',
          f'exit: {ex}')
    return 'PASS' if not fails else 'FAIL: ' + '; '.join(fails)


def reap(a, n):
    """sc-reap (pyhost.py "reap"): two tasks killed with their calls in the
    engine, one held for a fault that never completes (FUSE), one waiting
    for its return; the reaper (release_task) drops both, and the held
    run's callback is told (ABORT)."""
    con, log = read(f'{O}/{a}-{n}.con'), read(f'{O}/{a}-{n}.log') or ''
    if con is None:
        return None
    fails = []

    def check(ok, what):
        if not ok:
            fails.append(what)
    rc = re.search(r'^RC (\d+)', con, re.M)
    check(rc and rc.group(1) == '0', f'qemu exit {rc and rc.group(1)}')
    w = kv(re.search(r'SC expect reap (.*)', con).group(1)) if 'SC expect reap' in con else {}
    check(w.get('c1') == '9' and w.get('c2') == '9', f'children not killed: {w}')
    check('SC stuck' not in con, 'a child did not go')
    m = {d['at']: d for d in (kv(x) for x in log.splitlines()
                              if x.startswith('SC data tag=mark'))}
    b, e = m.get('9191', {}), m.get('9192', {})
    held = [x for x in log.splitlines() if x.startswith('SC data tag=held')]
    check(b.get('pending') == '2' and b.get('runs') == '1',
          f'before the kills: {b}, want 2 calls in the table, one run suspended')
    check(e.get('pending') == '0' and int(e.get('reaped', 0)) - int(b.get('reaped', 0)) == 2
          and e.get('aborts') == '1' and e.get('runs') == '0' and e.get('stale') == '0',
          f'after: {e}, want both reaped, the run aborted')
    check(held == ['SC data tag=held start'], f'held callback: {held}')
    return 'PASS' if not fails else 'FAIL: ' + '; '.join(fails)


def penguin(judge, a, n):
    """a run under penhost.py (penguin's Syscalls, trap backend): the judge's
    verdict, every pt_regs consistent with its event, the prototypes from
    igloo's syscall reports"""
    v = judge(a, n)
    log = read(f'{O}/{a}-{n}.log')
    if v is None or log is None:
        return v
    fails = [] if v == 'PASS' else [v[len('FAIL: '):]]
    regs = [x for x in log.splitlines() if x.startswith('SC regs')]
    badr = [x for x in regs if 'ok=1' not in x]
    if not regs or badr:
        fails.append(f'pt_regs: {len(badr)} of {len(regs)} inconsistent {badr[:2]}')
    ex = kv(re.search(r'SC exit .*', log).group(0)) if 'SC exit' in log else {}
    if int(ex.get('protos', 0)) < 100:
        fails.append(f'prototypes: {ex.get("protos")}')
    err = re.search(r'SC error .*', log)
    if err:
        fails.append(err.group(0))
    return 'PASS' if not fails else 'FAIL: ' + '; '.join(fails)


def evict(a, n='evict'):
    """3500 successful execs, each leaving its call pending: the oldest are
    dropped when the table (4096) is 3/4 full."""
    con, log = read(f'{O}/{a}-{n}.con'), read(f'{O}/{a}-{n}.log') or ''
    if con is None:
        return None
    m = re.search(r'SC expect execs n=(\d+)', con)
    e = re.search(r'SC exit pending=(\d+) stale=(\d+) full=(\d+)', log)
    c = re.search(r'SC count tag=exec enter=\d+ return=(\d+)', log)
    if not (m and e and c):
        return f'FAIL: missing output {bool(m)} {bool(e)} {bool(c)}'
    n, pend, stale, full = int(m.group(1)), *map(int, e.groups())
    ok = pend + stale == n and pend <= 3072 and stale > 0 and full == 0 and c.group(1) == '0'
    return ('PASS' if ok else 'FAIL') + f' (pending {pend}, dropped {stale}, full {full})'


def bench_runs(a, n, tag='sc'):
    """[(getppid, getpgid, getsid)] over the repetitions of <a>-<n>-<k>."""
    out = []
    for k in range(1, 10):
        con = read(f'{O}/{a}-{n}-{k}.con')
        if con is None:
            break
        pat = (r'BENCH sc n=\d+ ' if tag == 'sc' else rf'BENCH sct what={tag} n=\d+ ') + \
            r'getppid_ns=([\d.]+) getpgid_ns=([\d.]+) getsid_ns=([\d.]+)'
        m = re.search(pat, con)
        if m:
            out.append(tuple(float(x) for x in m.groups()))
    return out


def med(xs):
    xs = sorted(xs)
    return xs[len(xs) // 2] if xs else None


def cell(runs, i, base=None, spread=False):
    v = med([r[i] for r in runs])
    if v is None:
        return '—'
    s = f'{v:.0f}' if base is None else f'{v - base:+.0f}'
    if spread and len(runs) > 1:
        s += f' (±{(max(r[i] for r in runs) - min(r[i] for r in runs)) / 2:.0f})'
    return s


bad = 0
rows = []
for a in ARCHS:
    r = [func(a, 'func'), func(a, 'func-smp')]
    if r[0] is None and r[1] is None:
        continue
    r[1] = r[1] if r[1] is not None else ('SKIP (malta: 1 CPU)' if a.startswith('mips') else 'not run')
    bad += sum(1 for x in r if x and x.startswith('FAIL'))
    rows.append((a, r))
if rows:
    print('## sc-func\n')
    print('| arch | 1 vCPU | 2 vCPUs |\n|---|---|---|')
    for a, r in rows:
        print(f'| {a} | {r[0]} | {r[1]} |')
trows = []
for a in ARCHS:
    r = [func(a, 'tfunc'), func(a, 'tfunc-smp'), sctrap(a, 'trap'), sctrap(a, 'trap32'),
         evict(a)]
    if any(x is not None for x in r):
        r = [x if x is not None else 'not run' for x in r]
        bad += sum(1 for x in r if x.startswith('FAIL'))
        trows.append((a, r))
if trows:
    print('\n## trap mode (QEMU syscall events)\n')
    print('| arch | sc-func, 1 vCPU | sc-func, 2 vCPUs | sc-trap | sc-trap, 32-bit build '
          '| 3500 execs (eviction) |')
    print('|---|---|---|---|---|---|')
    for a, r in trows:
        print(f'| {a} | ' + ' | '.join(r) + ' |')

prows = []
for a in ARCHS:
    r = [func(a, 'pfunc'), func(a, 'pfunc-smp'), sctrap(a, 'ptrap'), sctrap(a, 'ptrap32'),
         pen(a, 'pen'), pen(a, 'pen32'), fault(a, 'pfault'), fault(a, 'pfault32'),
         fault(a, 'pmem'), reap(a, 'preap'), reap(a, 'preap32'), evict(a, 'pevict')]
    if any(x is not None for x in r):
        r = [x if x is not None else 'not run' for x in r]
        bad += sum(1 for x in r if x.startswith('FAIL'))
        prows.append((a, r))
ns = []
for a in ARCHS:
    r = [penguin(pen, a, 'npen'), penguin(pen, a, 'npen32'), penguin(fault, a, 'nfault'),
         penguin(fault, a, 'nfault32'), penguin(reap, a, 'nreap'), penguin(reap, a, 'nreap32')]
    if any(x is not None for x in r):
        r = [x if x is not None else 'not run' for x in r]
        bad += sum(1 for x in r if x.startswith('FAIL'))
        ns.append((a, r))
if prows:
    print('\n## penguin_syscall_* hooks (system/penguin-syscalls.c, hosted by pyhost.py)\n')
    print('| arch | sc-func, 1 vCPU | sc-func, 2 vCPUs | sc-trap | sc-trap, 32-bit build '
          '| sc-pen | sc-pen, 32-bit build | sc-fault | sc-fault, 32-bit build '
          '| sc-fault, penguin\'s mem.py | sc-reap | sc-reap, 32-bit build '
          '| 3500 execs (eviction) |')
    print('|---|---|---|---|---|---|---|---|---|---|---|---|---|')
    for a, r in prows:
        print(f'| {a} | ' + ' | '.join(r) + ' |')

if ns:
    print('\n## penguin\'s Syscalls on the trap (backend=trap; penhost.py: qemu_compat, '
          'apis/syscalls.py, apis/mem.py)\n')
    print('| arch | sc-pen | sc-pen, 32-bit build | sc-fault | sc-fault, 32-bit build '
          '| sc-reap | sc-reap, 32-bit build |')
    print('|---|---|---|---|---|---|---|')
    for a, r in ns:
        print(f'| {a} | ' + ' | '.join(r) + ' |')

SETS = ['none', 'empty', 'one', 'ret', 'comm', 'all', 'allret', 'log', 'fail', 'pass']
R = {a: {n: bench_runs(a, 'bench-' + n) for n in SETS} for a in ARCHS}
for a in ARCHS:
    R[a]['ftrace1'] = bench_runs(a, 'tbench', 'getppid')
    R[a]['ftraceall'] = bench_runs(a, 'tbench', 'all')
have = [a for a in ARCHS if R[a]['none']]


def diff(runs, i, j):
    """Median over boots of (column i - column j), measured in one boot."""
    return med([r[i] - r[j] for r in runs]) if runs else None


def within(a, n, i):
    """ns added to syscall i by rule set n: (i - getsid) in that set's boots,
    minus the same difference in no-plugin boots; cancels boot-to-boot noise."""
    d, d0 = diff(R[a][n], i, 2), diff(R[a]['none'], i, 2)
    return None if d is None or d0 is None else d - d0


def fmt(x):
    return '—' if x is None else f'{x:+.0f}'


if have:
    reps = max(len(R[a]['none']) for a in have)
    print(f'\n## One syscall hooked: ns added to getppid (median of {reps} boots, '
          'measured against getsid in the same boot)\n')
    cols = [('one', 'getppid count'), ('ret', 'count + return'),
            ('comm', 'count, comm filter'), ('log', 'log every call'),
            ('ftrace1', 'ftrace: sys_enter/exit_getppid')]
    print('| arch | getppid, no plugin (ns) | ' + ' | '.join(c for _, c in cols) + ' |')
    print('|---' * (len(cols) + 2) + '|')
    for a in have:
        print(f'| {a} | {cell(R[a]["none"], 0)} | ' +
              ' | '.join(fmt(within(a, n, 0)) for n, _ in cols) + ' |')

    print('\n## Every syscall hooked: ns added per syscall (fastest boot of each set: '
          'boot-to-boot noise only ever slows a boot down)\n')
    cols = [('all', '* count'), ('allret', '* count return'),
            ('ftraceall', 'ftrace: raw_syscalls enter+exit')]
    print('| arch | boot-to-boot spread, no plugin | ' +
          ' | '.join(f'{c}: getppid / getsid' for _, c in cols) + ' |')
    print('|---' * (len(cols) + 2) + '|')
    for a in have:
        b0, b2 = min(r[0] for r in R[a]['none']), min(r[2] for r in R[a]['none'])
        spr = max(r[2] for r in R[a]['none']) - min(r[2] for r in R[a]['none'])
        cells = []
        for n, _ in cols:
            x0 = min([r[0] for r in R[a][n]], default=None)
            x2 = min([r[2] for r in R[a][n]], default=None)
            cells.append('—' if x0 is None else f'{x0 - b0:+.0f} / {x2 - b2:+.0f}')
        print(f'| {a} | {spr:.0f} | ' + ' | '.join(cells) + ' |')

    print('\n## A syscall no rule names (getsid): ns added (fastest boot of each set)\n')
    print('| arch | boot-to-boot spread | plugin, no rules | getppid count (other syscall hooked) |')
    print('|---|---|---|---|')
    for a in have:
        b2 = min(r[2] for r in R[a]['none'])
        spr = max(r[2] for r in R[a]['none']) - b2
        x = [min([r[2] for r in R[a][n]], default=None) for n in ('empty', 'one')]
        print(f'| {a} | {spr:.0f} | ' + ' | '.join(fmt(v - b2 if v is not None else None) for v in x) + ' |')

    print('\n## Argument filter: ns added to getpgid(0) (against getsid in the same boot)\n')
    print('| arch | filter runs in | a0=4242: fails, no callback | a0=0: holds, callback runs |')
    print('|---|---|---|---|')
    for a in have:
        where = 'C, after reading pt_regs' if a in ('x86_64', 'aarch64', 'riscv64') \
            else 'the translated code'
        print(f'| {a} | {where} | {fmt(within(a, "fail", 1))} | {fmt(within(a, "pass", 1))} |')

    T = {a: {n: bench_runs(a, 'trapbench-' + n) for n in SETS if n != 'none'} for a in have}
    PN = {a: {n: bench_runs(a, 'penbench-' + n) for n in SETS if n != 'none'} for a in have}
    PS = {a: {n: bench_runs(a, 'pybench-' + n) for n in SETS if n != 'none'} for a in have}
    tv = [a for a in have if any(T[a].values()) or any(PN[a].values()) or any(PS[a].values())]
    if tv:
        def within_src(src):
            def w(a, n, i):
                d, d0 = diff(src[a][n], i, 2), diff(R[a]['none'], i, 2)
                return None if d is None or d0 is None else d - d0
            return w
        within_t = within_src(T)

        def fastest(runs, i, base):
            x = min([r[i] for r in runs], default=None)
            return None if x is None else x - base

        print('\n## Handler hooks against QEMU syscall events (trap mode, and the '
              'penguin_syscall_* engine with Python callbacks): ns added\n')
        print('getppid columns: against getsid in the same boot; getsid and "*" columns: '
              'fastest boot against the fastest no-plugin boot\n')
        cols = [('one', 0, 'getppid count'), ('ret', 0, 'count + return'),
                ('comm', 0, 'comm filter'), ('fail', 1, 'getpgid a0=4242 (fails)'),
                ('log', 0, 'log every call')]
        print('| arch | mode | ' + ' | '.join(c for _, _, c in cols) +
              ' | getsid, no rule on it | * count: getppid | * count return: getppid |')
        print('|---|---|' + '---|' * (len(cols) + 3))
        for a in tv:
            b0, b2 = min(r[0] for r in R[a]['none']), min(r[2] for r in R[a]['none'])
            for mode, src, w in (('handler', R[a], within), ('trap', T[a], within_t),
                                 ('penguin', PN[a], within_src(PN)),
                                 ("penguin's Syscalls", PS[a], within_src(PS))):
                if not any(src.values()):
                    continue
                cells = [fmt(w(a, n, i)) for n, i, _ in cols]
                cells.append(fmt(fastest(src['one'], 2, b2)))
                cells.append(fmt(fastest(src['all'], 0, b0)))
                cells.append(fmt(fastest(src['allret'], 0, b0)))
                print(f'| {a} | {mode} | ' + ' | '.join(cells) + ' |')

    print('\n## Bootstrap (at start_kernel): read syscall metadata, insert hooks for "* count"\n')
    print('| arch | syscalls | hooks | metadata read | hook insertion |')
    print('|---|---|---|---|---|')
    for a in have:
        log = read(f'{O}/{a}-bench-all-1.log') or ''
        m = re.search(r'SC boot syscalls=(\d+) hooks=(\d+) .*meta_us=(\d+) insert_us=(\d+)', log)
        if m:
            print(f'| {a} | {m.group(1)} | {m.group(2)} | {int(m.group(3)) / 1e3:.1f} ms | '
                  f'{int(m.group(4)) / 1e3:.1f} ms |')

print(f'\n{bad} failed')
sys.exit(1 if bad else 0)
