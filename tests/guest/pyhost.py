#!/usr/bin/env python3
"""pyhost.py <libqemu-system-*.so> <profile> <scenario> <log> -- <qemu argv...>

Hosts QEMU in-process through CFFI, the way penguin's qemu_compat does, and
registers syscall hooks through the penguin_syscall_* C ABI
(system/penguin-syscalls.c).  Logs events in plugin/syscalls.c's format so
sctest.py can judge them.

Scenarios: func (guest sc-func), trap (sc-trap), pen (sc-pen), fault
(sc-fault), pmem (sc-fault, the callbacks written against penguin's own
pyplugins/apis/mem.py), reap (sc-reap), evict (sc-execs), bench-<rule set>
(sc-bench).

Callbacks may be generators that yield commands, as Penguin's portal
coroutines do: ('read', addr, size), ('read_str', addr, max), ('write',
addr, bytes), or penguin's PortalCmd objects.  on_event() drives them: QEMU carries a command out (faulting the
memory in when it has to) and the answer is sent back into the generator.
"""
import os
import re
import sys
import threading
import types

import cffi

CDEF = """
typedef _Bool bool;
typedef uint64_t vaddr;
typedef struct CPUState CPUState;

struct penguin_sc_filter {
    bool enabled;
    int32_t type;
    int64_t value;
    int64_t min_value;
    int64_t max_value;
    uint64_t bitmask;
    const char *pattern;
    uint32_t pattern_len;
};
struct penguin_sc_hook {
    bool enabled;
    bool on_enter;
    bool on_return;
    bool on_all;
    bool on_unknown;
    char name[48];
    bool pid_filter_enabled;
    int64_t filter_pid;
    bool comm_filter_enabled;
    char comm_filter[16];
    bool scope_filter_enabled;
    struct penguin_sc_filter arg_filters[6];
    struct penguin_sc_filter retval_filter;
};
struct penguin_sc_event {
    uint64_t call;
    uint64_t args[6];
    uint64_t pc;
    uint64_t hook;
    uint64_t task;
    uint64_t regs;
    int64_t retval;
    uint64_t pid;
    uint64_t create_time;
    uint32_t argc;
    bool skip_syscall;
    char syscall_name[48];
    int32_t abi;
    int64_t num;
    bool is_enter;
    bool restart;
};
struct penguin_sc_cmd {
    int32_t op;
    uint64_t addr;
    uint64_t size;
    const void *data;
    int32_t status;
    const void *buf;
    uint64_t len;
    int64_t num;
    uint64_t args[6];
    int64_t ret;
};
struct penguin_sc_layout {
    uint64_t cur_mask;
    uint64_t cur_offset;
    int32_t ptr_size;
    int32_t task_pid;
    int32_t task_comm;
    int32_t task_start_time;
    int32_t task_nsproxy;
    int32_t nsproxy_uts_ns;
    uint64_t init_uts_ns;
};
struct penguin_sc_stats {
    uint64_t enters, events, commands, faults, refused, kept, pending, full, stale, reaped,
             injected;
};
typedef void (*penguin_syscall_cb_t)(CPUState *cs, struct penguin_sc_event *ev,
                                     struct penguin_sc_cmd *cmd, void *opaque);
typedef void (*penguin_syscall_boot_cb_t)(CPUState *cs, void *opaque);
bool set_penguin_syscall_callback(penguin_syscall_cb_t cb, void *opaque);
void set_penguin_syscall_boot_callback(penguin_syscall_boot_cb_t cb, void *opaque);
bool penguin_syscall_set_name(int32_t abi, int64_t num, const char *name, int32_t nargs);
bool penguin_syscall_set_layout(const struct penguin_sc_layout *layout);
void penguin_syscall_set_scope(bool enabled);
bool penguin_syscall_set_guard(uint64_t force_sig_fault, uint64_t force_sig_info);
bool penguin_syscall_set_reaper(uint64_t release_task);
bool penguin_syscall_set_exec(uint64_t set_task_comm);
uint64_t penguin_syscall_register(const struct penguin_sc_hook *hook);
bool penguin_syscall_unregister(uint64_t id);
bool penguin_syscall_read_memory(CPUState *cs, uint64_t addr, void *buf, size_t len);
bool penguin_syscall_write_memory(CPUState *cs, uint64_t addr, const void *buf, size_t len);
void penguin_syscall_stats(struct penguin_sc_stats *stats);

void qemu_init(int argc, char **argv);
int qemu_main_loop(void);
void qemu_cleanup(int status);
void bql_lock_impl(const char *file, int line);
void bql_unlock(void);
bool bql_locked(void);
void replay_mutex_lock(void);
void replay_mutex_unlock(void);
"""

ABIS = ['x86_64', 'i386', 'aarch64', 'arm', 'arm_oabi', 'mips', 'ppc64', 'ppc',
        'riscv64', 'riscv32', 'loongarch64']
OP_NONE, OP_READ, OP_READ_STR, OP_WRITE, OP_ABORT = range(5)
OK, PARTIAL, FAIL = range(3)
OPS = {'read': OP_READ, 'read_str': OP_READ_STR, 'write': OP_WRITE}
(EXACT, GREATER, GREATER_EQUAL, LESS, LESS_EQUAL, NOT_EQUAL, RANGE, SUCCESS,
 ERROR, BITMASK_SET, BITMASK_CLEAR, STR_EXACT, STR_CONTAINS, STR_STARTSWITH,
 STR_ENDSWITH) = range(15)

lib_path, prof_path, scenario, log_path = sys.argv[1:5]
assert sys.argv[5] == '--'
qargv = sys.argv[6:]

ffi = cffi.FFI()
ffi.cdef(CDEF)
lib = ffi.dlopen(lib_path, ffi.RTLD_GLOBAL | ffi.RTLD_NOW)
log = open(log_path, 'w', buffering=1)
lock = threading.Lock()


def out(line):
    with lock:
        log.write(line + '\n')


# the profile (guest/pk/sysprof.py)
P = {'sys': [], 'csys': []}
for line in open(prof_path):
    w = line.split()
    if not w or w[0].startswith('#'):
        continue
    if w[0] in ('task', 'meta', 'nsproxy'):
        P[w[0]] = {k: int(v, 0) for k, v in (x.split('=') for x in w[1:])}
    elif w[0] == 'sys':
        P['sys'].append((w[1], int(w[3], 16)))
    elif w[0] == 'csys':
        P['csys'].append((w[1], w[2], int(w[3])))
    else:
        P[w[0]] = ' '.join(w[1:])
arch = P['arch']
native = {'x86_64': 0, 'aarch64': 2}[arch]
ptr = int(P['ptr'])
m = re.match(r'm:\w+\+(\w+):(\d)', P['ucurrent'])
layout = ffi.new('struct penguin_sc_layout *', dict(
    cur_mask=(1 << 64) - 1, cur_offset=int(m.group(1), 0), ptr_size=ptr,
    task_pid=P['task']['pid'], task_comm=P['task']['comm'],
    task_start_time=P['task']['start_time'], task_nsproxy=P['task']['nsproxy'],
    nsproxy_uts_ns=P['nsproxy']['uts_ns'], init_uts_ns=P['nsproxy']['init_uts_ns']))
assert lib.penguin_syscall_set_layout(layout)

n_names = 0


@ffi.callback('void(CPUState *, void *)')
def boot(cs, opaque):
    """syscall_metadata.syscall_nr is set by now: the native numbers; the
    compat ones are from the uapi headers, with the native argument counts"""
    global n_names
    buf = ffi.new('uint8_t[8]')
    nargs = {}
    for name, meta in P['sys']:
        vals = []
        for k in ('syscall_nr', 'nb_args'):
            if not lib.penguin_syscall_read_memory(cs, meta + P['meta'][k], buf, 4):
                break
            vals.append(int.from_bytes(bytes(buf[0:4]), 'little', signed=True))
        if len(vals) < 2 or vals[0] < 0:
            continue
        lib.penguin_syscall_set_name(native, vals[0], name.encode(), vals[1])
        nargs[name] = vals[1]
        n_names += 1
    for cabi, name, nr in P['csys']:
        lib.penguin_syscall_set_name(ABIS.index(cabi), nr, name.encode(),
                                     nargs.get(name, -1))
        n_names += 1
    out(f'SC boot syscalls={n_names} hooks={len(hooks)} inline=0')


# hooks
hooks = {}       # id -> (fn, tag)
counts = {}      # tag -> [enter, return]
keep = []        # patterns, kept alive


def flt(spec):
    """a filter: an int (exact), ('op', value...), or a str (starts with)"""
    if isinstance(spec, int):
        return dict(enabled=True, type=EXACT, value=spec)
    if isinstance(spec, str):
        spec = (STR_STARTSWITH, spec)
    t, *v = spec
    d = dict(enabled=True, type=t)
    if t == RANGE:
        d.update(min_value=v[0], max_value=v[1])
    elif t in (BITMASK_SET, BITMASK_CLEAR):
        d.update(bitmask=v[0])
    elif t >= STR_EXACT:
        b = ffi.new('char[]', v[0].encode())
        keep.append(b)
        d.update(pattern=b, pattern_len=len(v[0]))
    elif v:
        d.update(value=v[0])
    return d


def hook(name, fn=None, tag=None, enter=False, ret=False, args=None,
         retval=None, comm='pagetest', pid=None, scope=False, on_all=False):
    h = ffi.new('struct penguin_sc_hook *')
    h.enabled = True
    h.on_enter, h.on_return, h.on_all = enter, ret, on_all
    h.name = (name or '').encode()
    if comm:
        h.comm_filter_enabled = True
        h.comm_filter = comm.encode()
    if pid is not None:
        h.pid_filter_enabled = True
        h.filter_pid = pid
    h.scope_filter_enabled = scope
    for i, spec in (args or {}).items():
        for k, v in flt(spec).items():
            setattr(h.arg_filters[i], k, v)
    if retval is not None:
        for k, v in flt(retval).items():
            setattr(h.retval_filter, k, v)
    hid = lib.penguin_syscall_register(h)
    assert hid
    hooks[hid] = (fn or count, tag)
    counts.setdefault(tag, [0, 0])
    return hid


def ustr(cs, va, n=256):
    buf = ffi.new('char[]', n)
    if not lib.penguin_syscall_read_memory(cs, va, buf, n):
        return '?'
    return ffi.string(buf).decode(errors='replace')


def count(cs, ev, tag):
    counts[tag][0 if ev.is_enter else 1] += 1


def logger(*strargs):
    def fn(cs, ev, tag):
        count(cs, ev, tag)
        a = ' '.join(f'a{i}=' + (ustr(cs, ev.args[i]) if i in strargs else hex(ev.args[i]))
                     for i in range(min(ev.argc, 6)))
        kind = 'enter' if ev.is_enter else 'return'
        extra = '' if ev.is_enter else f' ret={ev.retval}'
        if ev.restart:
            extra += ' restart=1'
        out(f'SC {kind} tag={tag} name={ffi.string(ev.syscall_name).decode()} '
            f'abi={ABIS[ev.abi]} nr={ev.num} pid={ev.pid} {a}{extra}')
    return fn


def setter(**kv):
    def fn(cs, ev, tag):
        logger()(cs, ev, tag)
        for k, v in kv.items():
            if k == 'skip':
                ev.skip_syscall = True
                ev.retval = v
            elif k == 'ret':
                ev.retval = v
            else:
                ev.args[int(k[1:])] = v
    return fn


runs = {}        # call id -> the generator waiting for its answer
wbufs = {}       # call id -> the bytes of its write, alive until answered
cur = threading.local()     # the vCPU of the callback running on this thread
hop = None       # penguin's HYPER_OP, once pmem loads it
unsupported = set()
aborts = [0]


def command(c):
    """a yielded command as (op, addr, size or bytes), or None if QEMU cannot
    carry it out (Penguin's PortalCmd: only its memory operations)"""
    if isinstance(c, tuple):
        return c
    if hop is not None and hasattr(c, 'op'):
        if c.op == hop.HYPER_OP_READ:
            return ('read', c.addr, c.size)
        if c.op == hop.HYPER_OP_READ_STR:
            return ('read_str', c.addr, c.size)
        if c.op == hop.HYPER_OP_WRITE:
            return ('write', c.addr, bytes(c.data))
        if c.op not in unsupported:
            unsupported.add(c.op)
            out(f'SC unsupported op={c.op}')
    return None


def answer(cmd):
    """the value the portal sends into the coroutine (Portal.wrap)"""
    if cmd.op in (OP_READ, OP_READ_STR) and cmd.status != FAIL:
        return bytes(ffi.buffer(cmd.buf, cmd.len)) if cmd.len else b''
    return None


@ffi.callback('void(CPUState *, struct penguin_sc_event *, struct penguin_sc_cmd *, void *)')
def on_event(cs, ev, cmd, opaque):
    """a run of a hook: start it, or send it the answer to its command"""
    if cmd.op == OP_ABORT:
        g = runs.pop(ev.call, None)
        wbufs.pop(ev.call, None)
        if g:
            aborts[0] += 1
            g.close()
        return
    cur.cs = cs
    if cmd.op == OP_NONE:
        fn, tag = hooks.get(ev.hook, (None, None))
        g = fn(cs, ev, tag) if fn else None
        if not isinstance(g, types.GeneratorType):
            return                      # done: cmd.op stays OP_NONE
        val, first = None, True
    else:
        g = runs.pop(ev.call)
        wbufs.pop(ev.call, None)
        val, first = answer(cmd), False
    try:
        c = next(g) if first else g.send(val)
        # what QEMU cannot do gets the portal's failure: None
        while command(c) is None:
            c = g.send(None)
    except StopIteration:
        cmd.op = OP_NONE
        return
    c = command(c)
    runs[ev.call] = g
    cmd.op, cmd.addr = OPS[c[0]], c[1]
    if c[0] == 'write':
        b = ffi.new('char[]', c[2])
        wbufs[ev.call] = b
        cmd.data, cmd.size = b, len(c[2])
    else:
        cmd.size = c[2]


def stats():
    st = ffi.new('struct penguin_sc_stats *')
    lib.penguin_syscall_stats(st)
    return st


def guard():
    """pagein's guard symbols (guest/pk/syms.py): faults may be raised"""
    env = re.sub(r'sysprof-(.*)\.txt$', r'syms-\1.env', prof_path)
    d = dict(x.split('=', 1) for x in open(env).read().split())
    ok = lib.penguin_syscall_set_guard(int(d.get('SIG') or '0', 0),
                                       int(d.get('SIGINFO') or '0', 0))
    assert ok, env
    if os.environ.get('NOREAP') != '1':
        assert lib.penguin_syscall_set_reaper(int(P['reaper'], 0))


def scenario_func():
    """sc-rules.txt, plus igloo's dispatch: skip, chains"""
    hook('openat', logger(1), 'open', enter=True, ret=True,
         args={1: (STR_EXACT, '/data.sqsh')})
    hook('ioctl', tag='ioeq', enter=True, args={1: 0x1234})
    hook('ioctl', tag='iorange', enter=True, args={1: (RANGE, 0x1230, 0x123f)})
    hook('ioctl', tag='ioall', enter=True, args={2: (BITMASK_SET, 0xf0)})
    hook('ioctl', tag='ionone', enter=True, args={2: (BITMASK_CLEAR, 0xf0)})
    hook('ioctl', tag='ioneg', enter=True, args={2: (LESS, 0)})
    hook('read', logger(), 'blockread', enter=True, ret=True, args={2: 777})
    hook('fork', logger(), 'forkret', ret=True)
    hook('clone', logger(), 'forkret', ret=True)
    hook('getpgid', setter(skip=777), 'override', enter=True, args={0: 4242})
    hook('getpgid', tag='afterskip', enter=True, args={0: 4242})
    hook('getpgid', logger(), 'skipret', ret=True, args={0: 4242})
    hook('dup3', setter(a1=101), 'setarg', enter=True, args={1: 100})
    hook('dup3', logger(), 'setarg2', enter=True, args={1: 100})
    hook('getpgid', setter(ret=54), 'setret1', ret=True, args={0: 4343})
    hook('getpgid', setter(ret=55), 'setret', ret=True, args={0: 4343})
    hook('getpid', tag='getpid', enter=True)
    hook('getsid', tag='siderr', ret=True, retval=(ERROR,))
    hook('getsid', tag='sidok', ret=True, retval=(SUCCESS,))
    hook('exit_group', tag='exit', enter=True, ret=True)
    hook(None, tag='all', enter=True, on_all=True)
    hook(None, tag='allneg', enter=True, on_all=True, comm='nomatch')


def scenario_trap():
    """sc-trap-rules.txt"""
    hook('read', logger(), 'eintr', enter=True, ret=True, args={2: 778})
    hook('getpgid', logger(), 'inhandler', ret=True, args={0: 5151})
    hook('read', logger(), 'restart', enter=True, ret=True, args={2: 779})
    hook('vfork', logger(), 'vfork', ret=True)
    hook('clone', logger(), 'vfork', ret=True, args={0: (BITMASK_SET, 0x4000)})
    hook('getpgid', logger(), 'vchild', ret=True, args={0: 5252})
    hook('execve', logger(), 'exec', ret=True)
    hook('read', logger(), 'thr', ret=True, args={2: 780})


def scenario_pen():
    for tag, f in (('sw', '/proc/'), ('ct', (STR_CONTAINS, 'self')),
                   ('ew', (STR_ENDSWITH, 'status')),
                   ('exneg', (STR_EXACT, '/proc/self')),
                   ('swneg', (STR_STARTSWITH, '/proc/selfx'))):
        hook('openat', logger(1), tag, enter=True, args={1: f})

    seen = []

    def unreg(cs, ev, tag):
        count(cs, ev, tag)
        seen.append(1)
        if len(seen) == 3:
            assert lib.penguin_syscall_unregister(ev.hook)
    hook('getppid', unreg, 'unregister', enter=True)

    lib.penguin_syscall_set_scope(True)
    hook('sched_yield', tag='scope_in', enter=True, scope=True)
    hook('sched_yield', tag='scope_all', enter=True)

    def add_pid(cs, ev, tag):
        logger()(cs, ev, tag)
        hook('getsid', logger(), 'pidfilter', enter=True, args={0: 7171}, pid=ev.pid)
    hook('getpgid', add_pid, 'addpid', enter=True, args={0: 6363})

    hook('getpgid', setter(skip=616), 'skip', enter=True, args={0: 6161})
    hook('getpgid', tag='afterskip', enter=True, args={0: 6161})
    hook('getpgid', logger(), 'skipret', ret=True, args={0: 6161})
    hook('getpgid', setter(ret=1), 'ret1', ret=True, args={0: 6262})
    hook('getpgid', setter(ret=2), 'ret2', ret=True, args={0: 6262})
    hook('dup3', setter(a1=201), 'arg1', enter=True, args={1: 200})
    hook('dup3', setter(a1=202), 'arg2', enter=True, args={1: 200})
    hook(None, tag='allneg', enter=True, on_all=True, comm='nomatch')


def direct(cs, va, n):
    """a read that never faults (mem.read_bytes_panda), or None"""
    buf = ffi.new('char[]', n)
    if not lib.penguin_syscall_read_memory(cs, va, buf, n):
        return None
    return bytes(buf)


def cstr(b):
    return 'FAIL' if b is None else b.split(b'\0', 1)[0].decode(errors='replace')


def scenario_fault():
    """sc-fault: memory the task has not touched, at both events"""
    hook('openat', setter(skip=7777), 'fstr', enter=True,
         args={1: (STR_EXACT, '/proc/self/status')})

    def rd(cs, ev, tag):
        # mem.read_bytes: the direct read first, the portal command on a miss
        count(cs, ev, tag)
        f0 = stats().faults
        d = direct(cs, ev.args[1], 32)
        data = d if d is not None else (yield ('read', ev.args[1], 32))
        out(f'SC data tag={tag} direct={int(d is not None)} data={cstr(data)} '
            f'faults={stats().faults - f0}')
    hook('write', rd, 'fread', enter=True, args={2: 1717})

    def wr(cs, ev, tag):
        count(cs, ev, tag)
        f0 = stats().faults
        r = yield ('write', ev.args[1], b'COW-DONE\0')
        s = yield ('read_str', ev.args[1], 64)
        out(f'SC data tag={tag} wrote={r} readback={cstr(s)} faults={stats().faults - f0}')
    hook('write', wr, 'fwrite', enter=True, args={2: 1818})

    def rt(cs, ev, tag):
        count(cs, ev, tag)
        f0 = stats().faults
        got = []
        for i in (0, 1):
            got.append(cstr((yield ('read', ev.args[i], 16))))
        out(f'SC data tag={tag} a0={got[0]} a1={got[1]} faults={stats().faults - f0} '
            f'ret={ev.retval}')
    hook('setpgid', rt, 'fret', ret=True)


def load_mem():
    """penguin's pyplugins/apis/mem.py, unchanged, over the engine: its direct
    reads and writes (never faulting) and its portal commands"""
    global hop
    H = os.path.dirname(os.path.abspath(__file__))
    sys.path[:0] = [os.path.join(H, 'penstub')]
    import importlib.util
    from penguin import Plugin, plugins
    from hyper.consts import HYPER_OP
    hop = HYPER_OP

    class Lib:
        @staticmethod
        def panda_virtual_memory_read_external(cpu, addr, buf, n):
            return 0 if lib.penguin_syscall_read_memory(cpu, addr, buf, int(n)) else -1

        @staticmethod
        def panda_virtual_memory_write_external(cpu, addr, buf, n):
            return 0 if lib.penguin_syscall_write_memory(cpu, addr, buf, int(n)) else -1

    class Panda:
        endianness, bits, ffi, libpanda = 'little', 64, ffi, Lib()
        arch = arch

        @staticmethod
        def get_cpu():
            return cur.cs

    Plugin.panda = Panda()

    class Portal:
        regions_size = 4096 - 24        # a page less region_header
    plugins.portal = Portal()
    spec = importlib.util.spec_from_file_location(
        'apis_mem', os.path.join(os.environ['PENGUIN'], 'pyplugins', 'apis', 'mem.py'))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    plugins.mem = m.Mem()
    return plugins


def scenario_pmem():
    """sc-fault with the callbacks as Penguin plugins write them"""
    plugins = load_mem()
    hook('openat', setter(skip=7777), 'fstr', enter=True,
         args={1: (STR_EXACT, '/proc/self/status')})

    def rd(cs, ev, tag):
        count(cs, ev, tag)
        s0 = stats()
        data = yield from plugins.mem.read_bytes(ev.args[1], 32)
        s = yield from plugins.mem.read_str(ev.args[1])
        s1 = stats()
        # mem.py yields a command only when its direct read failed
        out(f'SC data tag={tag} direct={int(s1.commands == s0.commands)} '
            f'data={cstr(data)} str={s} faults={s1.faults - s0.faults}')
    hook('write', rd, 'fread', enter=True, args={2: 1717})

    def wr(cs, ev, tag):
        count(cs, ev, tag)
        f0 = stats().faults
        n = yield from plugins.mem.write_bytes(ev.args[1], b'COW-DONE\0')
        s = yield from plugins.mem.read_str(ev.args[1])
        out(f'SC data tag={tag} wrote={n} readback={s} faults={stats().faults - f0}')
    hook('write', wr, 'fwrite', enter=True, args={2: 1818})

    def rt(cs, ev, tag):
        count(cs, ev, tag)
        f0 = stats().faults
        got = []
        for i in (0, 1):
            d = yield from plugins.mem.read_bytes(ev.args[i], 16)
            # mem.read_bytes reads a failure as zeros
            got.append('FAIL' if d == bytes(16) else cstr(d))
        n = yield from plugins.mem.read_int(ev.args[1])
        out(f'SC data tag={tag} a0={got[0]} a1={got[1]} int={n:#x} '
            f'faults={stats().faults - f0} ret={ev.retval}')
    hook('setpgid', rt, 'fret', ret=True)


def scenario_reap():
    """sc-reap: tasks killed with a call held for a fault, or pending"""
    def blocked(cs, ev, tag):
        count(cs, ev, tag)
        out(f'SC data tag={tag} start')
        d = yield ('read', ev.args[1], 16)
        out(f'SC data tag={tag} resumed={cstr(d)}')     # never: killed
    hook('write', blocked, 'held', enter=True, args={2: 1919})
    hook('read', tag='waiting', ret=True, args={2: 1920})

    def mark(cs, ev, tag):
        count(cs, ev, tag)
        st = stats()
        out(f'SC data tag={tag} at={ev.args[0]} pending={st.pending} '
            f'reaped={st.reaped} stale={st.stale} aborts={aborts[0]} runs={len(runs)}')
    hook('getpgid', mark, 'mark', enter=True, args={0: (RANGE, 9191, 9192)})


def scenario_evict():
    """sc-evict-rules.txt"""
    hook('execve', tag='exec', ret=True)


BENCH = {
    'empty': lambda: None,
    'one': lambda: hook('getppid', tag='n', enter=True, comm=None),
    'all': lambda: hook(None, tag='n', enter=True, on_all=True, comm=None),
    'fail': lambda: hook('getpgid', tag='n', enter=True, args={0: 4242}, comm=None),
    'pass': lambda: hook('getpgid', tag='n', enter=True, args={0: 0}, comm=None),
    'ret': lambda: hook('getppid', tag='n', enter=True, ret=True, comm=None),
    'comm': lambda: hook('getppid', tag='n', enter=True),
    'allret': lambda: hook(None, tag='n', enter=True, ret=True, on_all=True, comm=None),
    'log': lambda: hook('getppid', logger(), 'n', enter=True, comm=None),
}

assert lib.set_penguin_syscall_callback(on_event, ffi.NULL)
lib.set_penguin_syscall_boot_callback(boot, ffi.NULL)
if scenario.startswith('bench-'):
    BENCH[scenario[6:]]()
else:
    guard()
    {'func': scenario_func, 'trap': scenario_trap, 'pen': scenario_pen,
     'fault': scenario_fault, 'pmem': scenario_pmem, 'reap': scenario_reap,
     'evict': scenario_evict}[scenario]()

argv_keep = [ffi.new('char[]', a.encode()) for a in qargv]
argv = ffi.new('char *[]', argv_keep + [ffi.NULL])
lib.qemu_init(len(qargv), argv)
if lib.bql_locked():
    lib.bql_unlock()
lib.replay_mutex_unlock()
lib.replay_mutex_lock()
lib.bql_lock_impl(b'pyhost.py', 0)
ret = lib.qemu_main_loop()

for tag, (e, r) in sorted(counts.items(), key=lambda x: str(x[0])):
    out(f'SC count tag={tag} enter={e} return={r}')
st = stats()
out(f'SC exit pending={st.pending} stale={st.stale} full={st.full} mode=pen '
    f'enters={st.enters} events={st.events} commands={st.commands} '
    f'faults={st.faults} refused={st.refused} kept={st.kept} reaped={st.reaped} '
    f'aborts={aborts[0]} runs={len(runs)}')
log.close()
lib.qemu_cleanup(ret)
lib.bql_unlock()
lib.replay_mutex_unlock()
sys.exit(ret)
