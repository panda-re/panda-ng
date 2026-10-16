#!/usr/bin/env python3
"""penhost.py <libqemu-system-*.so> <penguin arch> <isf> <scenario> <log> -- <qemu argv...>

Runs the guest under penguin's own code for syscall hooks:
compat/qemu_compat.py (QEMU in-process), apis/syscalls.py with the trap
backend (compat/syscall_trap.py), apis/mem.py, hyper/portal.py's PortalCmd.
penstub/ and the stand-ins below take the rest of the runtime (the portal,
hypercall and kffi plugins).  The hooks are registered and written as
penguin plugins write them: plugins.syscalls.syscall(...), callbacks
(regs, proto, syscall, *args) that yield from plugins.mem.  Logs in
pyhost.py's format, for sctest.py.

Scenarios: pen (guest sc-pen), fault (sc-fault), reap (sc-reap), osi (osi:
apis/hostosi.py's answers at getpgid(9301) enter and getpgid(9302) return, as
OSI got lines in the guest's OSI truth format, and the cost of each call),
bench-<rule set> (sc-bench), inject (sc-inject: Syscalls.inject).
Every logged event also checks pt_regs against the event (SC regs lines).

PROFILE=1: cProfile of every Syscalls dispatch, in <log>.prof.  MICRO=1: no
guest; time the event path on a synthetic event for the scenario's first
hook, and exit.
"""
import logging
import os
import sys
import threading

H = os.path.dirname(os.path.abspath(__file__))
PEN = os.path.join(os.environ['PENGUIN'], 'pyplugins')
sys.path[:0] = [os.path.join(H, 'penstub'), PEN]

lib_path, parch, isf, scenario, log_path = sys.argv[1:6]
assert sys.argv[6] == '--'
qargv = sys.argv[7:]
logging.basicConfig(level=logging.INFO, stream=sys.stderr,
                    format='penhost %(name)s %(levelname)s: %(message)s')

from penguin import Plugin, plugins  # noqa: E402
from dwarffi.dffi import DFFI  # noqa: E402

log = open(log_path, 'w', buffering=1)
lock = threading.Lock()


def out(line):
    with lock:
        log.write(line + '\n')


class KFFI:
    """plugins.kffi: the kernel's types and symbols (its ISF)"""
    def __init__(self, path):
        self.isf = path
        # "none": a kernel with no ISF (the trap backend's uapi mode)
        self.ffi = None if path == 'none' else DFFI([path])

    def new(self, type_, init=None):
        return self.ffi.new(type_, init)


class Portal:
    """plugins.portal: no portal here; the trap backend serves commands"""
    regions_size = 4096 - 24

    def register_interrupt_handler(self, name, fn):
        pass

    def queue_interrupt(self, name):
        return True

    def wrap(self, f):
        return f


class Hypercall:
    """plugins.hypercall: igloo's hypercalls, through qemu_compat"""
    def __init__(self, panda):
        self.panda, self.handlers = panda, {}

    def hypercall(self, nr):
        def deco(f):
            self.handlers.setdefault(nr, []).append(f)
            self.panda.register_guest_hypercall(nr)
            return f
        return deco

    def dispatch(self, cpu, nr, ret_ptr):
        fs = self.handlers.get(nr)
        if not fs:
            return 1
        for f in fs:
            r = f(cpu)
            if isinstance(r, int):
                self.panda._set_current_retval(r)
        if ret_ptr[0] == 0:
            ret_ptr[0] = self.panda._current_retval
        return 0


plugins.kffi = KFFI(isf)
from compat.qemu_compat import QemuCompat  # noqa: E402

qarch = {'x86_64': 'x86_64', 'aarch64': 'aarch64'}[parch]
panda = QemuCompat(lib_path, qarch, mode='system')
Plugin.panda = panda
Plugin.args = {'outdir': os.path.dirname(os.path.abspath(log_path)), 'backend': 'trap',
               'conf': {'core': {'analysis_scope': 'firmware', 'smp': 1}}}
plugins.hypercall = Hypercall(panda)
panda.bind_hypercall_plugin(plugins.hypercall)
plugins.portal = Portal()

from apis.mem import Mem  # noqa: E402
from apis.syscalls import Syscalls, ValueFilter  # noqa: E402

plugins.mem = Mem()
plugins.syscalls = Syscalls()
trap = plugins.syscalls._trap
assert trap is not None
ABIS = ['x86_64', 'i386', 'aarch64', 'arm']

counts = {}
closed = [0]
protos = {}
M64 = (1 << 64) - 1


def stats():
    return trap.stats()


def count(sc, tag):
    counts.setdefault(tag, [0, 0])[0 if sc.is_enter else 1] += 1


def check_regs(regs, sc, tag):
    """pt_regs, from the vCPU, against the event: the arguments at the entry
    (unless an earlier hook changed them), the number, the pc, the return
    value at the return"""
    bad = []
    if sc.is_enter and tag not in CHAINED:
        for i in range(min(sc.argc, 6)):
            v = regs.get_syscall_arg(i)
            if v is None or (v & M64) != sc.args[i]:
                bad.append(f'a{i}={v}')
    if regs.get_syscall_number() != sc.num:
        bad.append(f'nr={regs.get_syscall_number()}')
    if regs.get_pc() != sc.pc:
        bad.append(f'pc={regs.get_pc()}')
    # a 32-bit ABI's value: arm64 keeps the low 32 bits in regs[0]
    m = 0xffffffff if sc.abi in (1, 3) else M64
    if not sc.is_enter and (regs.get_retval() & m) != (sc.retval & m):
        bad.append(f'ret={regs.get_retval()}')
    out(f'SC regs tag={tag} ok={int(not bad)} {" ".join(bad)}'.rstrip())


CHAINED = {'arg2'}


def logger(tag, strargs=(), then=None, check=True):
    counts.setdefault(tag, [0, 0])

    def cb(regs, proto, sc, *args):
        count(sc, tag)
        parts = []
        for i in range(min(sc.argc, 6)):
            v = sc.args[i]
            if i in strargs:
                s = yield from plugins.mem.read_str(v)
                parts.append(f'a{i}={s}')
            else:
                parts.append(f'a{i}={v:#x}')
        kind = 'enter' if sc.is_enter else 'return'
        extra = '' if sc.is_enter else f' ret={sc.retval}'
        if sc.restart:
            extra += ' restart=1'
        out(f'SC {kind} tag={tag} name={sc.name} abi={ABIS[sc.abi]} nr={sc.num} '
            f'pid={sc.pid} ' + ' '.join(parts) + extra)
        if check:
            check_regs(regs, sc, tag)
        if proto is not None and sc.name not in protos:
            protos[sc.name] = proto
            out(f'SC proto name={sc.name} proto={proto.name} args={",".join(proto.names)}')
        if then:
            then(sc)
    return cb


def counter(tag):
    counts.setdefault(tag, [0, 0])

    def cb(regs, proto, sc, *args):
        count(sc, tag)
    return cb


def hook(name, cb, enter=False, ret=False, comm='pagetest', scope=False, **kw):
    return plugins.syscalls.syscall(name, on_enter=enter, on_return=ret, comm_filter=comm,
                                    scope_filter=scope, **kw)(cb)


def setter(**kv):
    def fn(sc):
        for k, v in kv.items():
            if k == 'skip':
                sc.skip_syscall = True
                sc.retval = v
            elif k == 'ret':
                sc.retval = v
            else:
                sc.args[int(k[1:])] = v
    return fn


def scenario_pen():
    for tag, f in (('sw', ValueFilter.string_startswith('/proc/')),
                   ('ct', ValueFilter.string_contains('self')),
                   ('ew', ValueFilter.string_endswith('status')),
                   ('exneg', ValueFilter.string_exact('/proc/self')),
                   ('swneg', ValueFilter.string_startswith('/proc/selfx'))):
        hook('openat', logger(tag, strargs=(1,)), enter=True, arg_filters=[None, f])

    seen = []
    handle = {}

    def unreg(regs, proto, sc, *args):
        count(sc, 'unregister')
        seen.append(1)
        if len(seen) == 3:
            plugins.syscalls.unregister(handle['h'])
    handle['h'] = hook('getppid', unreg, enter=True)

    hook('sched_yield', counter('scope_in'), enter=True, scope=True)
    hook('sched_yield', counter('scope_all'), enter=True)

    def add_pid(sc):
        hook('getsid', logger('pidfilter'), enter=True, arg_filters=[7171], pid_filter=sc.pid)
    hook('getpgid', logger('addpid', then=add_pid), enter=True, arg_filters=[6363])

    hook('getpgid', logger('skip', then=setter(skip=616)), enter=True, arg_filters=[6161])
    hook('getpgid', counter('afterskip'), enter=True, arg_filters=[6161])
    hook('getpgid', logger('skipret'), ret=True, arg_filters=[6161])
    hook('getpgid', logger('ret1', then=setter(ret=1)), ret=True, arg_filters=[6262])
    hook('getpgid', logger('ret2', then=setter(ret=2)), ret=True, arg_filters=[6262])
    hook('dup3', logger('arg1', then=setter(a1=201)), enter=True, arg_filters=[None, 200])
    hook('dup3', logger('arg2', then=setter(a1=202)), enter=True, arg_filters=[None, 200])
    # penguin's every-syscall hooks: the on_all_sys_* patterns
    hook('on_all_sys_enter', counter('allneg'), comm='nomatch')


def scenario_fault():
    """sc-fault with the callbacks on penguin's mem"""
    hook('openat', logger('fstr', then=setter(skip=7777)), enter=True,
         arg_filters=[None, ValueFilter.string_exact('/proc/self/status')])

    def cstr(b):
        return 'FAIL' if b is None else b.split(b'\0', 1)[0].decode(errors='replace')

    def rd(regs, proto, sc, fd, buf, n):
        count(sc, 'fread')
        s0 = stats()
        data = yield from plugins.mem.read_bytes(buf, 32)
        s = yield from plugins.mem.read_str(buf)
        s1 = stats()
        out(f'SC data tag=fread direct={int(s1["commands"] == s0["commands"])} '
            f'data={cstr(data)} str={s} faults={s1["faults"] - s0["faults"]}')
        check_regs(regs, sc, 'fread')
    hook('write', rd, enter=True, arg_filters=[None, None, 1717])

    def wr(regs, proto, sc, fd, buf, n):
        count(sc, 'fwrite')
        f0 = stats()['faults']
        n = yield from plugins.mem.write_bytes(buf, b'COW-DONE\0')
        s = yield from plugins.mem.read_str(buf)
        out(f'SC data tag=fwrite wrote={n} readback={s} faults={stats()["faults"] - f0}')
    hook('write', wr, enter=True, arg_filters=[None, None, 1818])

    def rt(regs, proto, sc, *args):
        count(sc, 'fret')
        f0 = stats()['faults']
        got = []
        for i in (0, 1):
            d = yield from plugins.mem.read_bytes(sc.args[i], 16)
            got.append('FAIL' if d == bytes(16) else cstr(d))
        out(f'SC data tag=fret a0={got[0]} a1={got[1]} faults={stats()["faults"] - f0} '
            f'ret={sc.retval}')
        check_regs(regs, sc, 'fret')
    hook('setpgid', rt, ret=True)


def scenario_reap():
    def blocked(regs, proto, sc, fd, buf, n):
        count(sc, 'held')
        out('SC data tag=held start')
        try:
            d = yield from plugins.mem.read_bytes(buf, 16)
            out(f'SC data tag=held resumed={d!r}')       # never: killed
        finally:
            closed[0] += 1
    hook('write', blocked, enter=True, arg_filters=[None, None, 1919])
    hook('read', counter('waiting'), ret=True, arg_filters=[None, None, 1920])

    def mark(regs, proto, sc, which):
        count(sc, 'mark')
        st = stats()
        out(f'SC data tag=mark at={which} pending={st["pending"]} reaped={st["reaped"]} '
            f'stale={st["stale"]} aborts={closed[0]} runs={len(trap.runs)}')
        check_regs(regs, sc, 'mark')
    hook('getpgid', mark, enter=True, arg_filters=[ValueFilter.range(9191, 9192)])


def scenario_inject():
    """sc-inject: calls the host makes the task run (Syscalls.inject) at
    getpgid(94xx) entries, answered through the skipped call"""
    import struct
    import time
    inject = plugins.syscalls.inject

    def skip(sc, v):
        sc.skip_syscall = True
        sc.retval = v

    def ids(regs, proto, sc, *args):
        count(sc, 'inj-ids')
        p = yield from inject('getpid')
        t = yield from inject('gettid')
        out(f'SC data tag=inj-ids pid={p} tid={t}')
        check_regs(regs, sc, 'inj-ids')     # the task's own registers, back
        skip(sc, p if p == t else -1)
    hook('getpgid', ids, enter=True, arg_filters=[9401])

    def enosys(regs, proto, sc, *args):
        count(sc, 'inj-enosys')
        e = yield from inject(4000)
        x = yield from inject('execve', 0, 0, 0)     # fails (-EFAULT), comes back
        k = yield from inject('clone', 0x11, 0, 0, 0, 0)   # refused: the child
        out(f'SC data tag=inj-enosys ret={e} execve={x} clone={k}')
        skip(sc, e if x == -14 and k is None else -1)
    hook('getpgid', enosys, enter=True, arg_filters=[9402])

    def procread(regs, proto, sc, fd, buf, n):
        count(sc, 'inj-proc')
        m = yield from inject('mmap', 0, 4096, 3, 0x22, -1, 0)      # RW, private anon
        w = yield from plugins.mem.write_bytes(m, b'/proc/self/stat\0')
        f = yield from inject('openat', -100, m, 0, 0)               # AT_FDCWD, O_RDONLY
        k = yield from inject('read', f, m, 4096)
        data = (yield from plugins.mem.read_bytes(m, k)) if k and k > 0 else b''
        c = yield from inject('close', f)
        u = yield from inject('munmap', m, 4096)
        out(f'SC data tag=inj-proc mmap={m:#x} write={w} fd={f} read={k} close={c} '
            f'munmap={u} data={data[:40]!r}')
        yield from plugins.mem.write_bytes(buf, struct.pack('<qqqq', f, k, c, u) +
                                           data[:255] + b'\0')
        skip(sc, k)
    hook('write', procread, enter=True, arg_filters=[None, None, 9403])

    def signal(regs, proto, sc, *args):
        count(sc, 'inj-signal')
        p = yield from inject('getpid')
        k = yield from inject('kill', p, 10)                         # SIGUSR1
        out(f'SC data tag=inj-signal kill={k}')
        skip(sc, k)
    hook('getpgid', signal, enter=True, arg_filters=[9404])

    def eintr(regs, proto, sc, *args):
        count(sc, 'inj-eintr')
        r = yield from inject('ppoll', 0, 0, 0, 0, 8)               # until a signal
        out(f'SC data tag=inj-eintr ret={r}')
        skip(sc, r)
    hook('getpgid', eintr, enter=True, arg_filters=[9405])

    def restart(regs, proto, sc, *args):
        count(sc, 'inj-restart')
        # getpgid(9406, fd, buf): the registers past its one argument
        r = yield from inject('read', sc.args[1], sc.args[2], 1)     # an empty pipe
        out(f'SC data tag=inj-restart ret={r}')
        skip(sc, r)
    hook('getpgid', restart, enter=True, arg_filters=[9406])

    def atreturn(regs, proto, sc, *args):
        """at a return: the injected call runs, then the task's own call
        returns what the callback says, its other registers its own"""
        count(sc, 'inj-ret')
        r = yield from inject('getpid')
        sc.retval = -1 if r is None else r
    hook('getpgid', atreturn, ret=True, arg_filters=[9407])
    hook('getpgid', atreturn, ret=True, arg_filters=[9410])

    def many(regs, proto, sc, *args):
        count(sc, 'inj-many')
        n, t0 = 1000, time.perf_counter()
        for _ in range(n):
            p = yield from inject('getpid')
        out(f'SC data tag=inj-many n={n} us_per_call={(time.perf_counter() - t0) / n * 1e6:.1f}')
        skip(sc, p)
    hook('getpgid', many, enter=True, arg_filters=[9408])

    def compat(regs, proto, sc, *args):
        """a 32-bit task: an AArch32 one's EABI getpid runs (its pid), an
        i386 one's is refused (1)"""
        count(sc, 'inj-compat')
        r = yield from inject('getpid')
        skip(sc, 1 if r is None else r)
    hook('getpgid', compat, enter=True, arg_filters=[9409])


def osi_answers(osi):
    """every call's answers, as the guest's OSI truth lines"""
    got = []
    p = yield from osi.get_proc()
    got += [f'pid {p.pid}', f'ppid {p.ppid}', f'ids {p.uid} {p.gid} {p.euid} {p.egid}',
            f'comm {p.name}', f'start {p.create_time // 10_000_000}',
            'mm ' + ' '.join(f'{x:x}' for x in (
                p.start_code, p.end_code, p.start_stack, p.start_data, p.end_data,
                p.start_brk, p.arg_start, p.arg_end, p.env_start, p.env_end))]
    a = yield from osi.get_args()
    got.append('args ' + ('|'.join(a) if a is not None else 'FAIL'))
    e = yield from osi.get_env()
    got.append('env ' + ('|'.join(f'{k}={v}' for k, v in e.items()) if e is not None else 'FAIL'))
    got.append(f'cwd {(yield from osi.get_cwd())}')
    got.append(f'exe {(yield from osi.get_proc_exe())}')
    for f in (yield from osi.get_fds()) or []:
        got.append(f'fd {f.fd} {f.name}')
    for m in (yield from osi.get_mappings()) or []:
        got.append(f'map {m.base:x} {m.base + m.size:x} {m.perms} {m.offset:x} {m.name}')
    procs = (yield from osi.get_all_procs()) or []
    got.append('procs ' + ' '.join(str(q.pid) for q in sorted(procs, key=lambda q: q.pid)))
    kids = [q for q in procs if q.ppid == p.pid]
    for c in kids:
        got.append(f'child {c.pid}')
        cp = yield from osi.get_proc(c.pid)
        got.append(f'childcomm {cp.name if cp else "FAIL"}')
        ca = yield from osi.get_args(c.pid)
        got.append('childargs ' + ('|'.join(ca) if ca is not None else 'FAIL'))
    return got, (kids[0].pid if kids else None)


def osi_costs(osi, label, pid, n):
    """the cost of each call"""
    import time
    for name, call in (('get_proc', lambda: osi.get_proc()),
                       ('get_proc_name', lambda: osi.get_proc_name()),
                       ('get_args', lambda: osi.get_args()),
                       ('get_env', lambda: osi.get_env()),
                       ('get_fd_name', lambda: osi.get_fd_name(7)),
                       ('get_cwd', lambda: osi.get_cwd()),
                       ('get_fds', lambda: osi.get_fds()),
                       ('get_proc_exe', lambda: osi.get_proc_exe()),
                       ('get_mappings', lambda: osi.get_mappings()),
                       ('get_all_procs', lambda: osi.get_all_procs()),
                       ('get_proc(pid)', lambda: osi.get_proc(pid)),
                       ('get_args(pid)', lambda: osi.get_args(pid))):
        t0 = time.perf_counter()
        for _ in range(n):
            yield from call()
        us = (time.perf_counter() - t0) / n * 1e6
        out(f'OSI cost {label} op={name} us={us:.2f}')


def osi_raw(osi, pid):
    """the C walks alone: one call each, timed in C's terms (cffi included)"""
    import time
    lib, ffi, cpu, task = osi.lib, osi.ffi, osi._cpu(), osi._current()
    p, strs, fds, vmas, tasks = osi._proc, osi._strs, osi._fds, osi._vmas, osi._tasks
    other = lib.penguin_osi_task(cpu, pid) if pid else 0
    nop = lib.penguin_syscall_stats
    st = ffi.new('struct penguin_sc_stats *')
    for name, call in (('cffi_call', lambda: nop(st)),
                       ('proc', lambda: lib.penguin_osi_proc(cpu, task, p)),
                       ('fd_path', lambda: lib.penguin_osi_fd_path(cpu, task, 7, strs, 4096)),
                       ('fd_path_socket', lambda: lib.penguin_osi_fd_path(cpu, task, 8, strs, 4096)),
                       ('exe_path', lambda: lib.penguin_osi_exe_path(cpu, task, strs, 4096)),
                       ('fd_match_miss', lambda: lib.penguin_osi_fd_path_match(cpu, task, 7, 3, b'/nomatch', 8)),
                       ('fd_match_exact', lambda: lib.penguin_osi_fd_path_match(cpu, task, 7, 0, b'/mnt/data.bin', 13)),
                       ('exe_match_miss', lambda: lib.penguin_osi_exe_match(cpu, task, 3, b'/busybox', 8)),
                       ('fds', lambda: lib.penguin_osi_fds(cpu, task, 0, fds, 4096, strs, 1 << 18)),
                       ('mappings', lambda: lib.penguin_osi_mappings(cpu, task, vmas, 8192, strs, 1 << 18)),
                       ('procs', lambda: lib.penguin_osi_procs(cpu, tasks, 16384)),
                       ('task_by_pid', lambda: lib.penguin_osi_task(cpu, pid or 1)),
                       ('user_read_other', lambda: lib.penguin_osi_user_read(cpu, other, 0x7fffffffe000, strs, 64))):
        n = 20000
        c0 = ffi.new('uint64_t[7]')
        c1 = ffi.new('uint64_t[7]')
        lib.penguin_osi_counters(c0)
        t0 = time.perf_counter()
        for _ in range(n):
            call()
        us = (time.perf_counter() - t0) / n * 1e6
        lib.penguin_osi_counters(c1)
        pages = ' '.join(f'{k}={(c1[i] - c0[i]) / n:.1f}'
                         for i, k in enumerate(('kmap', 'cached', 'tlb', 'walk')))
        out(f'OSI cost raw op={name} us={us:.3f} {pages}')


def osi_match_patterns(name):
    """patterns around a true name: itself, its tails and heads (at its
    component boundaries and between), near misses, the root"""
    cuts = sorted({0, 1, len(name) // 2, len(name) - 1, len(name)} |
                  {i for i, c in enumerate(name) if c == '/'} |
                  {i + 1 for i, c in enumerate(name) if c == '/'})
    pats = {name, name + 'x', 'x' + name, '/', ' (deleted)', '/' + name}
    for k in cuts:
        if 0 <= k <= len(name):
            pats |= {name[k:], name[:k]}
    for k in (0, len(name) // 2, len(name) - 1):
        if 0 <= k < len(name):
            pats.add(name[:k] + chr(ord(name[k]) ^ 1) + name[k + 1:])
    return sorted(p for p in pats if p)


def osi_match_check(osi):
    """the engine's matcher (penguin_osi_fd_path_match, _exe_match) against
    Python's string tests on the names the walks gave: every match type, the
    patterns around each name"""
    lib, cpu, task = osi.lib, osi._cpu(), osi._current()
    tests = {0: lambda s, p: s == p, 1: lambda s, p: p in s,
             2: lambda s, p: s.startswith(p), 3: lambda s, p: s.endswith(p)}
    subjects = [(f.fd, f.name) for f in (yield from osi.get_fds()) or []]
    exe = yield from osi.get_proc_exe()
    subjects.append((None, exe))
    n = bad = 0
    for fd, name in subjects:
        for pat in osi_match_patterns(name):
            b = pat.encode('latin-1')
            for how, want in tests.items():
                got = (lib.penguin_osi_exe_match(cpu, task, how, b, len(b)) if fd is None else
                       lib.penguin_osi_fd_path_match(cpu, task, fd, how, b, len(b)))
                n += 1
                if got != int(want(name, pat)):
                    bad += 1
                    if bad <= 20:
                        out(f'OSI matchbad fd={fd} how={how} name={name!r} pat={pat!r} got={got}')
    out(f'OSI matchcheck n={n} bad={bad}')


def scenario_first():
    """the first syscalls in the system: who makes them, and a file written into
    the root by injected calls at the first one, before init has done anything;
    pagetest's first call reads it back the same way"""
    inject = plugins.syscalls.inject
    seen = []

    def comm():
        m = yield from inject('mmap', 0, 4096, 3, 0x22, -1, 0)
        yield from inject('prctl', 16, m)                            # PR_GET_NAME
        name = (yield from plugins.mem.read_bytes(m, 16)).split(b'\0')[0]
        yield from inject('munmap', m, 4096)
        return m, name.decode(errors='replace')

    def first(regs, proto, sc, *args):
        if len(seen) >= 3:
            return
        seen.append(sc.syscall_name)
        n = len(seen)
        pid = yield from inject('getpid')
        m, name = yield from comm()
        out(f'SC data tag=first n={n} call={sc.syscall_name} pid={pid} comm={name} '
            f'pc={regs.get_pc() or 0:#x} scratch={m:#x}')
        if n != 1:
            return
        m = yield from inject('mmap', 0, 4096, 3, 0x22, -1, 0)
        body = f'written before init, at its {sc.syscall_name}\n'.encode()
        yield from plugins.mem.write_bytes(m, b'/preinit.txt\0' + body)
        f = yield from inject('openat', -100, m, 0x241, 0o644)       # O_WRONLY|O_CREAT|O_TRUNC
        w = yield from inject('write', f, m + 13, len(body))
        c = yield from inject('close', f)
        u = yield from inject('munmap', m, 4096)
        out(f'SC data tag=first-write fd={f} write={w} close={c} munmap={u}')
    hook('on_all_sys_enter', first, comm=None)

    done = []

    def readback(regs, proto, sc, *args):
        if done:
            return
        done.append(1)
        m = yield from inject('mmap', 0, 4096, 3, 0x22, -1, 0)
        yield from plugins.mem.write_bytes(m, b'/preinit.txt\0')
        f = yield from inject('openat', -100, m, 0, 0)
        k = yield from inject('read', f, m, 4096)
        data = (yield from plugins.mem.read_bytes(m, k)) if k and k > 0 else b''
        yield from inject('close', f)
        yield from inject('munmap', m, 4096)
        out(f'SC data tag=first-readback call={sc.syscall_name} fd={f} read={k} data={data!r}')
    hook('on_all_sys_enter', readback, comm='pagetest')


def scenario_takeover():
    """the first task, ours: at init's first syscall, an injected execveat
    makes pid 1 run gen/ours-<arch> from a memfd; it does its part and execs
    the real init with init's own argv and environment.  An execve of a
    missing file first: it fails, and the callback goes on"""
    import struct
    from apis.hostosi import HostOSI
    inject = plugins.syscalls.inject
    osi = HostOSI()
    prog = open(os.path.join(os.environ['GEN'], f'ours-{parch}'),
                'rb').read()
    state = {}

    def first(regs, proto, sc, *args):
        if state:
            return
        state['seen'] = sc.syscall_name
        pid = yield from inject('getpid')
        if pid != 1:
            out(f'SC data tag=takeover-skip pid={pid} call={sc.syscall_name}')
            state.clear()
            return
        argv = yield from osi.get_args()
        env = yield from osi.get_env()
        envl = [f'{k}={v}' for k, v in (env or {}).items()]
        out(f'SC data tag=takeover-init call={sc.syscall_name} argv={argv} env={envl}')
        size = (len(prog) + 4095) & ~4095
        m = yield from inject('mmap', 0, size + 4096, 3, 0x22, -1, 0)
        w = yield from plugins.mem.write_bytes(m, prog)
        # strings, then argv and envp (64-bit pointers) in the last page
        a = m + size
        blob, ptrs = b'', []
        for item in [b'ours\0', b'/no/such/init\0'] + \
                [x.encode('latin-1') + b'\0' for x in ['ours', argv[0]] + argv + envl]:
            ptrs.append(a + len(blob))
            blob += item
        # ptrs: [0] the memfd name ("" at +4), [1] the missing file, then
        # ours, init, init's argv..., env...
        strs = ptrs[2:]
        nargs = 2 + len(argv)
        vec = strs[:nargs] + [0] + strs[nargs:] + [0]
        vaddr = a + ((len(blob) + 7) & ~7)
        yield from plugins.mem.write_bytes(a, blob)
        yield from plugins.mem.write_bytes(vaddr, struct.pack(f'<{len(vec)}Q', *vec))
        argvp, envp = vaddr, vaddr + 8 * (nargs + 1)
        bad = yield from inject('execve', ptrs[1], argvp, envp)
        out(f'SC data tag=takeover-missing execve={bad}')
        fd = yield from inject('memfd_create', ptrs[0], 1)              # MFD_CLOEXEC
        n = yield from inject('write', fd, m, len(prog))
        out(f'SC data tag=takeover-memfd write_bytes={w} fd={fd} write={n} size={len(prog)}')
        try:
            r = yield from inject('execveat', fd, ptrs[0] + 4, argvp, envp, 0x1000)  # AT_EMPTY_PATH
        except GeneratorExit:
            out('SC data tag=takeover-exec gone (the exec succeeded)')
            raise
        out(f'SC data tag=takeover-exec failed={r}')
    hook('on_all_sys_enter', first, comm=None)

    done = []

    def readback(regs, proto, sc, *args):
        if done:
            return
        done.append(1)
        m = yield from inject('mmap', 0, 4096, 3, 0x22, -1, 0)
        yield from plugins.mem.write_bytes(m, b'/ours.txt\0')
        f = yield from inject('openat', -100, m, 0, 0)
        k = yield from inject('read', f, m, 4096)
        data = (yield from plugins.mem.read_bytes(m, k)) if k and k > 0 else b''
        yield from inject('close', f)
        yield from inject('munmap', m, 4096)
        ppid = yield from inject('getppid')
        out(f'SC data tag=takeover-readback ppid={ppid} read={k} data={data!r}')
    hook('on_all_sys_enter', readback, comm='pagetest')


def scenario_kmod():
    """a kernel module from the hypervisor (L2 by injection): at pid 1's first
    syscall, mmap a buffer, copy $KMOD in, init_module it, munmap; later, the
    test's first syscall reads /proc/modules back.  No kernel symbol is needed:
    the trap is the syscall instruction, the calls are pid 1's own"""
    inject = plugins.syscalls.inject
    ko = open(os.environ['KMOD'], 'rb').read()
    params = os.environ.get('KMOD_PARAMS', '').encode() + b'\0'
    # KMOD_COMM: the first call of a task with that comm instead of pid 1's
    # (the c32 tests' 32-bit pagetest is an AArch32 task on arm64)
    task = os.environ.get('KMOD_COMM')
    state = {}

    def first(regs, proto, sc, *args):
        if state:
            return
        state['seen'] = sc.syscall_name
        pid = yield from inject('getpid')
        if pid != 1 and not task:
            state.clear()
            return
        # an AArch32 task (EABI): mmap2, whose offset is in pages (0 here)
        mmap = 'mmap2' if sc.abi == 3 else 'mmap'
        size = (len(ko) + len(params) + 4095) & ~4095
        # KMOD_POPULATE: MAP_POPULATE, for a kernel with no fault guard
        flags = 0x22 | (0x8000 if os.environ.get('KMOD_POPULATE') else 0)
        m = yield from inject(mmap, 0, size + 4096, 3, flags, -1, 0)
        out(f'SC data tag=kmod-task pid={pid} abi={sc.abi} {mmap}={m if m is None else hex(m)}')
        w = yield from plugins.mem.write_bytes(m, ko)
        yield from plugins.mem.write_bytes(m + size, params)
        back = yield from plugins.mem.read_bytes(m, len(ko))
        bad = [i for i in range(0, len(ko), 4096) if back[i:i + 4096] != ko[i:i + 4096]]
        head = (len(back), len(bad), bad[:4])
        r = yield from inject('init_module', m, len(ko), m + size)
        out(f'SC data tag=kmod-check nr={trap.number("init_module")} head={head!r}')
        yield from inject('munmap', m, size + 4096)
        out(f'SC data tag=kmod-load call={sc.syscall_name} size={len(ko)} write_bytes={w} '
            f'init_module={r}')
    hook('on_all_sys_enter', first, comm=task)
    done = []

    def modules(regs, proto, sc, *args):
        """later, with /proc mounted: the module list, as the test sees it"""
        if done:
            return
        done.append(1)
        m = yield from inject('mmap2' if sc.abi == 3 else 'mmap', 0, 8192, 3, 0x22, -1, 0)
        yield from plugins.mem.write_bytes(m, b'/proc/modules\0')
        f = yield from inject('openat', -100, m, 0, 0)
        k = f
        if f is not None and f >= 0:
            k = yield from inject('read', f, m, 8192)
        data = (yield from plugins.mem.read_bytes(m, k)) if k and k > 0 else b''
        yield from inject('close', f)
        yield from inject('munmap', m, 8192)
        out(f'SC data tag=kmod-modules fd={f} read={k} data={data!r}')
    hook('on_all_sys_enter', modules, comm='pagetest')


def scenario_hcwrite():
    """a hypercall's write into the guest (send_hypercall's shape): QEMU's
    permission-checked write and the debug write, at the hypercall"""
    ffi, lib = panda.ffi, panda.lib

    def handler(cpu):
        addr = panda.arch.get_arg(cpu, 1, convention="syscall")
        data = b'written\0'
        buf = ffi.new('char[]', data)
        ok = lib.penguin_syscall_write_memory(cpu, addr, buf, len(data))
        st = ffi.new('uint64_t[40]')
        n = lib.penguin_syscall_regs(cpu, 0, 40, st)
        out(f'HC data tag=hc-write addr={addr:#x} checked={int(ok)} nregs={n} '
            f'cs={st[18]:#x} eflags={st[17]:#x}')
        if not ok:
            r = lib.cpu_memory_rw_debug(cpu, addr, buf, len(data), True)
            out(f'HC data tag=hc-write debug={r}')
        return 0
    plugins.hypercall.hypercall(0xb335a535)(handler)


def scenario_osi():
    """apis/hostosi.py (C walks) at an entry and at a return, and osiref.py's
    Python walks at the entry"""
    from apis.hostosi import HostOSI
    from osiref import PyOSI
    osi, ref = HostOSI(), PyOSI()

    def report(regs, proto, sc, *args):
        count(sc, 'osi')
        at = 'enter' if sc.is_enter else 'return'
        got, pid = yield from osi_answers(osi)
        if sc.is_enter:
            lib, cpu, task = osi.lib, osi._cpu(), osi._current()
            out(f'OSI match fd7_end={lib.penguin_osi_fd_path_match(cpu, task, 7, 3, b"data.bin", 8)} '
                f'fd7_exact={lib.penguin_osi_fd_path_match(cpu, task, 7, 0, b"/mnt/data.bin", 13)} '
                f'fd7_miss={lib.penguin_osi_fd_path_match(cpu, task, 7, 3, b"nomatch", 7)} '
                f'fd8_sock={lib.penguin_osi_fd_path_match(cpu, task, 8, 2, b"socket:", 7)} '
                f'exe_end={lib.penguin_osi_exe_match(cpu, task, 3, b"/pagetest", 9)}')
        for line in got:
            out(f'OSI got {at} {line}')
        if sc.is_enter:
            yield from osi_match_check(osi)
            got, _ = yield from osi_answers(ref)
            for line in got:
                out(f'OSI ref {line}')
            return
        yield from osi_costs(osi, 'c', pid, 2000)
        osi_raw(osi, pid)
        yield from osi_costs(ref, 'py', pid, 10)
    hook('getpgid', report, enter=True, arg_filters=[9301])
    hook('getpgid', report, ret=True, arg_filters=[9302])
    # the engine's OSI filters, decided in C: an fd argument's name, the exe
    VF = ValueFilter
    for tag, f in (('fd_null', VF.string_endswith('/dev/null')),
                   ('fd_exact', VF.string_exact('/dev/null')),
                   ('fd_pipe', VF.string_startswith('pipe:')),
                   ('fd_miss', VF.string_endswith('/nomatch'))):
        hook('write', counter(tag), enter=True, arg_filters=[VF.fd_path(f)])
    for tag, f in (('exe_yes', '/pagetest'), ('exe_no', '/busybox'),
                   ('exe_exact', VF.string_exact('/pagetest'))):
        hook('getpgid', counter(tag), enter=True, arg_filters=[9303], exe_filter=f)


def count_only(regs, proto, sc, *args):
    pass


BENCH = {   # sctest.sh's rule sets, as penguin hooks (no comm filter but comm's)
    'empty': lambda: None,
    'one': lambda: hook('getppid', count_only, enter=True, comm=None),
    'all': lambda: hook('on_all_sys_enter', count_only, comm=None),
    'fail': lambda: hook('getpgid', count_only, enter=True, comm=None, arg_filters=[4242]),
    'pass': lambda: hook('getpgid', count_only, enter=True, comm=None, arg_filters=[0]),
    'ret': lambda: hook('getppid', count_only, enter=True, ret=True, comm=None),
    'comm': lambda: hook('getppid', count_only, enter=True),
    'allret': lambda: (hook('on_all_sys_enter', count_only, comm=None),
                       hook('on_all_sys_return', count_only, comm=None)),
    'log': lambda: hook('getppid', logger('n', check=False), enter=True, comm=None),
    # sc-fdbench: write(2) to /dev/null under filters that reject it, and one
    # that passes it to a callback
    'wnum': lambda: hook('write', count_only, enter=True, comm=None, arg_filters=[9999]),
    'wfd': lambda: hook('write', count_only, enter=True, comm=None, arg_filters=[
        ValueFilter.fd_path(ValueFilter.string_endswith('/nomatch'))]),
    'wfdx': lambda: hook('write', count_only, enter=True, comm=None, arg_filters=[
        ValueFilter.fd_path(ValueFilter.string_exact('/dev/nomatch'))]),
    'wfdnear': lambda: hook('write', count_only, enter=True, comm=None, arg_filters=[
        ValueFilter.fd_path(ValueFilter.string_endswith('/nodir/null'))]),
    'wfdstart': lambda: hook('write', count_only, enter=True, comm=None, arg_filters=[
        ValueFilter.fd_path(ValueFilter.string_startswith('/nomatch'))]),
    'wexe': lambda: hook('write', count_only, enter=True, comm=None, exe_filter='/nomatch'),
    'wfdpass': lambda: hook('write', count_only, enter=True, comm=None, arg_filters=[
        ValueFilter.fd_path(ValueFilter.string_endswith('/dev/null'))]),
}

if os.environ.get('DUMPNUMS'):
    # the native numbers and argument counts the trap learnt at boot (json)
    import atexit
    import json

    def dumpnums():
        with open(os.environ['DUMPNUMS'], 'w') as f:
            json.dump({'numbers': trap.numbers, 'nargs': getattr(trap, 'nargs', {})}, f,
                      sort_keys=True)
    atexit.register(dumpnums)

if os.environ.get('PROFILE'):
    # cProfile around each Syscalls dispatch (the vCPU thread's calls)
    import cProfile
    import pstats
    import time
    prof, spent = cProfile.Profile(), [0.0, 0]
    real = plugins.syscalls._trap_event

    def profiled(sce):
        t0 = time.perf_counter()
        prof.enable()
        try:
            return real(sce)
        finally:
            prof.disable()
            spent[0] += time.perf_counter() - t0
            spent[1] += 1
    plugins.syscalls._trap_event = profiled

    def dump():
        with open(log_path + '.prof', 'w') as f:
            f.write(f'dispatches={spent[1]} mean_us={spent[0] / max(spent[1], 1) * 1e6:.2f}\n')
            pstats.Stats(prof, stream=f).sort_stats('cumulative').print_stats(25)
    import atexit
    atexit.register(dump)

osi_ticks = None
if scenario.startswith('bench-'):
    BENCH[scenario[6:]]()
    if os.environ.get('OSICOUNT'):
        # the matchers' own time: counting from here, host ticks per ns by
        # the wall clock across the run
        import time  # noqa: F811
        osi_ticks = (panda.ffi.new('uint64_t[7]'), time.perf_counter())
        panda.lib.penguin_osi_counters(osi_ticks[0])
else:
    {'pen': scenario_pen, 'fault': scenario_fault, 'reap': scenario_reap,
     'osi': scenario_osi, 'inject': scenario_inject,
     'first': scenario_first, 'takeover': scenario_takeover,
     'kmod': scenario_kmod, 'hcwrite': scenario_hcwrite}[scenario]()

if os.environ.get('MICRO'):
    # the event path alone, on a synthetic event for the first hook: no guest
    import cProfile  # noqa: F811
    import pstats  # noqa: F811
    import time  # noqa: F811
    ffi = panda.ffi
    ev = ffi.new('struct penguin_sc_event *')
    cmd = ffi.new('struct penguin_sc_cmd *')
    ev.hook = min(plugins.syscalls._hooks)
    ev.syscall_name = b'getppid'
    ev.is_enter = True
    n = 20000
    t0 = time.perf_counter()
    for i in range(n):
        ev.call = i + 1
        cmd.op = 0
        trap._cb(ffi.NULL, ev, cmd, ffi.NULL)
    print(f'MICRO via cffi callback: {(time.perf_counter() - t0) / n * 1e6:.2f} us')
    t0 = time.perf_counter()
    for i in range(n):
        cmd.op = 0
        trap._on_event(ffi.NULL, ev, cmd, ffi.NULL)
    print(f'MICRO _on_event: {(time.perf_counter() - t0) / n * 1e6:.2f} us')
    import timeit
    from collections.abc import Iterator
    from compat.syscall_trap import TrapEvent
    sce = TrapEvent(ffi, ffi.NULL, ev)
    st = panda._thread_state
    for label, fn in (
            ('TrapEvent', lambda: TrapEvent(ffi, ffi.NULL, ev)),
            ('pt_regs wrapper', lambda: trap.pt_regs(sce)),
            ('_get_proto', lambda: plugins.syscalls._get_proto(None, sce, False)),
            ('sce.hook.address', lambda: sce.hook.address),
            ('isinstance Iterator', lambda: isinstance(None, Iterator)),
            ('_current_cpu set', lambda: setattr(panda, '_current_cpu', ffi.NULL)),
            ('thread flag', lambda: setattr(st, 'syscall_trap', True)),
            ('_trap_event', lambda: plugins.syscalls._trap_event(sce)),
            ('ev.call read', lambda: ev.call)):
        print(f'MICRO {label}: {timeit.timeit(fn, number=n) / n * 1e6:.2f} us')
    pr = cProfile.Profile()
    pr.enable()
    for i in range(n):
        cmd.op = 0
        trap._on_event(ffi.NULL, ev, cmd, ffi.NULL)
    pr.disable()
    pstats.Stats(pr).sort_stats('tottime').print_stats(14)
    sys.exit(0)

panda.panda_args = qargv
rc = 0
try:
    rc = panda.run()
except Exception as e:
    out(f'SC error {type(e).__name__}: {e}')
    rc = 1
for tag, (e, r) in sorted(counts.items()):
    out(f'SC count tag={tag} enter={e} return={r}')
if osi_ticks:
    import time  # noqa: F811
    c0, t0 = osi_ticks
    c1 = panda.ffi.new('uint64_t[7]')
    panda.lib.penguin_osi_counters(c1)
    per_ns = (c1[6] - c0[6]) / ((time.perf_counter() - t0) * 1e9)
    calls = c1[4] - c0[4]
    out(f'OSI matchers calls={calls} ns_per_call={(c1[5] - c0[5]) / per_ns / max(calls, 1):.1f} '
        f'pages_per_call=' + '/'.join(f'{(c1[i] - c0[i]) / max(calls, 1):.1f}' for i in range(4)))
st = stats()
out(f'SC exit pending={st["pending"]} stale={st["stale"]} full={st["full"]} mode=penguin '
    f'enters={st["enters"]} events={st["events"]} commands={st["commands"]} '
    f'faults={st["faults"]} refused={st["refused"]} kept={st["kept"]} reaped={st["reaped"]} '
    f'injected={st["injected"]} aborts={closed[0]} runs={len(trap.runs)} protos={len(plugins.syscalls._syscall_info_table)}')
log.close()
sys.exit(rc)
