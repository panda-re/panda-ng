'''
System calls injected into a guest task: ``panda.inject``.

A generator chooses the calls; the engine has the task make them, one at a time, at its next
user-mode boundary (or, from a syscalls2 entry callback, before the call it is making), and then
puts the task's registers back (``panda/inject.h``)::

    from pandare2.inject import syscall

    def demo(ctx):
        pid = yield syscall("getpid")
        buf = yield from ctx.alloc(4096)
        yield from ctx.write(buf, b"/etc/hostname\\0")
        fd = yield syscall("openat", -100, buf, 0, check=True)
        n = yield syscall("read", fd, buf, 256)
        data = yield from ctx.read(buf, n)
        yield syscall("close", fd)
        return pid, data

    s = panda.inject(demo, asid=asid)        # any callback, the main loop or another thread
    s.done(lambda result, err: print(result, err))
    pid, data = s.result(timeout=30)         # off the vCPU threads (a queue_blocking function)

``yield syscall(...)`` evaluates to the call's value: a negative errno on failure, otherwise the
value as an unsigned number of the task's word size; with ``check=True`` a failure raises
:class:`InjectFailed` in the generator instead. HyDE's ``Syscall`` objects work too: yield one and
its ``retval`` is set.

Between calls the generator runs on the vCPU's thread with the task current: ``ctx.read`` and
``ctx.write`` reach its memory with the task's own permissions, paging it in with an injected
``madvise`` when needed (``yield from``). The registers the generator sees through ``ctx.cpu``
are the injected call's, not the task's: read what you need before ``panda.inject``.

Refused: record/replay (:class:`InjectRefused`; starting a recording is refused while a call is
out), KVM, targets without an injector, and ``fork``/``vfork``/``clone``/``clone3`` (the child would
come back with the injected registers).
'''

import threading

from .inject_numbers import ABIS, NUMBERS

# PandaInjectStatus
OK, QUEUED, ETIMEDOUT, ECANCELED, ERECORD, EACCEL, EARCH, EINVAL, EBUSY, EABANDONED, ENOTHERE, \
    EFAILED = 0, 1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11
# next()'s answers
_DONE, _CALL, _AGAIN, _FAIL = 0, 1, 2, 3
TARGET_ASID, TARGET_TASK = 0, 1

# 32-bit ABIs: values come back as their low 32 bits
_BITS = {"i386": 32, "arm": 32, "mips_o32": 32, "ppc": 32}
# fork-like calls: refused (the child returns with the injected registers)
_FORKS = {"fork", "vfork", "clone", "clone3"}

PROT_READ, PROT_WRITE = 1, 2
MADV_POPULATE_READ, MADV_POPULATE_WRITE = 22, 23


class InjectError(RuntimeError):
    '''A session that did not complete'''

    def __init__(self, what, status=None, reason=None):
        self.status = status
        self.reason = reason
        super().__init__(f"{what}: {reason}" if reason else what)


class InjectRefused(InjectError, PermissionError):
    '''Refused: record/replay, KVM, no injector, a fork-like call, a hold outside an entry'''


class InjectTimeout(InjectError, TimeoutError):
    '''The target did not run in user mode in time'''


class InjectCancelled(InjectError):
    '''Cancelled, or a snapshot load or reset came first'''


class InjectAbandoned(InjectError):
    '''The thread came back from a call somewhere else (a signal handler longjmp'd, rseq)'''


class InjectFailed(InjectError, OSError):
    '''A call made with ``check=True`` failed: ``errno`` is the guest's'''

    def __init__(self, call, errno):
        RuntimeError.__init__(self, f"{call} failed: errno {errno}")
        self.errno = errno
        self.status = None
        self.reason = None
        self.call = call


_CLASS = {ETIMEDOUT: InjectTimeout, ECANCELED: InjectCancelled, EABANDONED: InjectAbandoned}


class Syscall:
    '''A call to inject: ``yield Syscall("getpid")``; HyDE's class, ``retval`` set when it is back'''

    def __init__(self, name, args=None, signed=False, check=False):
        self.name = name
        self.args = list(args or [])
        self.signed = signed
        self.check = check
        self.retval = None

    def __repr__(self):
        r = f"{self.name}({', '.join(hex(a) if isinstance(a, int) else repr(a) for a in self.args)})"
        return r if self.retval is None else f"{r} = {self.retval}"


def syscall(name, *args, check=False):
    '''``yield syscall("openat", -100, path, 0)``: the call's value (``check``: raise on failure)'''
    return Syscall(name, args, check=check)


class Touch:
    '''``yield Touch(addr)``: the task accesses ``addr`` itself (and may fault) with its registers
    back, and the generator resumes at a later boundary. What ``ctx.read`` uses when the guest's
    TLB is refilled in software (MIPS, BookE PowerPC).'''

    def __init__(self, addr, write=False):
        self.addr = addr
        self.write = write


class Context:
    '''What a session's generator gets: the task's vCPU, ABI and memory helpers'''

    def __init__(self, panda, session):
        self.panda = panda
        self.session = session
        self.cpu = None
        self.abi = None             # "x86_64", "i386", "aarch64", "arm", "mips_o32", ...
        self._allocs = []

    @property
    def bits(self):
        return _BITS.get(self.abi, 64)

    def number(self, name):
        '''A call's number for the task's ABI, or None'''
        e = NUMBERS[self.abi].get(name)
        return e[0] if e else None

    def _direct_read(self, addr, n):
        try:
            return self.panda.virtual_memory_read(self.cpu, addr, n)
        except ValueError:
            return None

    def _direct_write(self, addr, data):
        ffi, C = self.panda.ffi, self.panda.libpanda
        buf = ffi.from_buffer("uint8_t[]", data)
        return C.panda_guest_write_virt_user(self.cpu, addr, buf, len(data)) == 0

    def _populate(self, addr, n, advice):
        '''madvise(MADV_POPULATE_*) over [addr, addr + n), at the guest's page alignment'''
        for shift in (12, 14, 16):
            start = addr & ~((1 << shift) - 1)
            end = (addr + n + 4095) & ~4095
            r = yield Syscall("madvise", [start, end - start, advice])
            if r != -22:            # EINVAL: maybe a larger page: align to it
                return r
        return r

    def read(self, addr, n):
        '''``data = yield from ctx.read(addr, n)``: the task's memory, paged in if needed'''
        if n <= 0:
            return b""
        data = self._direct_read(addr, n)
        if data is not None:
            return data
        r = yield from self._populate(addr, n, MADV_POPULATE_READ)
        if r < 0:
            raise InjectFailed(f"read({addr:#x}, {n})", -r)
        for _ in range(2 * (n // 4096) + 8):
            data = self._direct_read(addr, n)
            if data is not None:
                return data
            yield Touch(addr)       # a TLB the kernel refills: take the miss, then again
        raise InjectFailed(f"read({addr:#x}, {n})", 14)

    def write(self, addr, data):
        '''``yield from ctx.write(addr, data)``: with the task's permissions, paged in if needed'''
        data = bytes(data)
        if not data or self._direct_write(addr, data):
            return
        r = yield from self._populate(addr, len(data), MADV_POPULATE_WRITE)
        if r < 0:
            raise InjectFailed(f"write({addr:#x}, {len(data)})", -r)
        for _ in range(2 * (len(data) // 4096) + 8):
            if self._direct_write(addr, data):
                return
            yield Touch(addr, write=True)
        raise InjectFailed(f"write({addr:#x}, {len(data)})", 14)

    def alloc(self, size):
        '''``addr = yield from ctx.alloc(size)``: anonymous memory in the task, unmapped at the
        session's end (or by ``ctx.free``)'''
        mips = self.abi.startswith("mips")
        anon = 0x802 if mips else 0x22          # MAP_PRIVATE | MAP_ANONYMOUS
        name = "mmap" if self.bits == 64 or "mmap2" not in NUMBERS[self.abi] else "mmap2"
        r = yield Syscall(name, [0, size, PROT_READ | PROT_WRITE, anon,
                                 (1 << self.bits) - 1, 0])
        if -4096 < _signed(r, self.bits) < 0:
            raise InjectFailed(f"alloc({size})", -_signed(r, self.bits))
        self._allocs.append((r, size))
        return r

    def free(self, addr):
        for i, (a, size) in enumerate(self._allocs):
            if a == addr:
                del self._allocs[i]
                yield Syscall("munmap", [a, size])
                return


def _signed(v, bits):
    v &= (1 << bits) - 1
    return v - (1 << bits) if v >> (bits - 1) else v


class Session:
    '''A queued injection: ``done(cb)``, ``result(timeout)``, ``cancel()``'''

    def __init__(self, inject, factory, allow_fork):
        self._inject = inject
        self._factory = factory
        self._allow_fork = allow_fork
        self._gen = None
        self._pending = None        # what the generator last yielded
        self._value = None          # what to send it next
        self._exc = None            # or throw
        self._event = threading.Event()
        self._callbacks = []
        self.id = None
        self.status = None
        self.value = None           # the generator's return value
        self.error = None
        self.calls = []             # the Syscall objects that went out, with their values
        self.ctx = None
        self._over = False          # the generator returned: only the cleanup's calls remain

    def done(self, cb):
        '''``cb(result, error)`` once the session is over (at once if it is)'''
        with self._inject._lock:
            if self.status is None:
                self._callbacks.append(cb)
                return self
        cb(self.value, self.error)
        return self

    def result(self, timeout=30):
        '''The generator's return value; raises its exception or the session's error. Waits,
        off the vCPU threads only.'''
        if self.status is None and not self._inject._panda.pagein.can_wait():
            raise BlockingIOError("panda.inject: a callback can't wait for its session: "
                                  "use .done(cb)")
        if not self._event.wait(timeout):
            raise InjectTimeout("panda.inject", ETIMEDOUT, "no completion within the host timeout")
        if self.error is not None:
            raise self.error
        return self.value

    def cancel(self):
        return bool(self._inject._panda.libpanda.panda_inject_cancel(self.id))

    # --- the engine's next() ------------------------------------------------------------

    def _next(self, cpu, step, call):
        p = self._inject._panda
        ctx = self.ctx
        ctx.cpu = cpu
        ctx.abi = ABIS[step.abi] if 0 <= step.abi < len(ABIS) else None
        if self._over:
            return self._cleanup(call)
        if self._gen is None:
            g = self._factory(ctx) if callable(self._factory) else self._factory
            if not hasattr(g, "send"):
                raise TypeError("panda.inject: give a generator, or a function returning one")
            self._gen = g
            send, throw = None, None
        elif step.calls > 0 and isinstance(self._pending, Syscall):
            sc = self._pending
            bits = ctx.bits
            v = _signed(step.ret, bits)
            v = v if -4096 < v < 0 else v & ((1 << bits) - 1)
            sc.retval = v
            send, throw = v, None
            if sc.check and v < 0:
                send, throw = None, InjectFailed(repr(sc), -v)
        else:
            send, throw = self._value, self._exc       # back after a Touch, or a refusal
        self._value = self._exc = None
        while True:
            try:
                y = self._gen.throw(throw) if throw is not None else self._gen.send(send)
            except StopIteration as e:
                self.value = e.value
                return self._cleanup(call)
            if isinstance(y, Touch):
                self._pending = y
                step.touch = y.addr
                return _AGAIN
            if not isinstance(y, Syscall):
                send, throw = None, TypeError(f"panda.inject: yield syscall(...), not {y!r}")
                continue
            name = y.name
            if isinstance(name, str) and name in _FORKS and not self._allow_fork:
                send, throw = None, InjectRefused(f"{name}()", None, "a fork-like call can't be "
                                                  "injected: the child would come back with "
                                                  "the injected registers")
                continue
            if isinstance(name, int):
                nr, nargs = name, -1
            else:
                e = NUMBERS[ctx.abi].get(name) if ctx.abi else None
                if e is None:
                    send, throw = None, InjectRefused(f"{name}()", None,
                                                      f"no such call for the {ctx.abi} ABI")
                    continue
                nr, nargs = e
            args = [int(a) for a in y.args]
            if len(args) > 6:
                send, throw = None, TypeError(f"{name}: at most 6 arguments")
                continue
            nargs = max(nargs, len(args)) if nargs >= 0 else -1
            call.nr = nr
            mask = (1 << 64) - 1
            for i, a in enumerate(args):
                call.args[i] = a & mask
            call.nargs = nargs
            self._pending = y
            self.calls.append(y)
            return _CALL

    def _cleanup(self, call):
        '''After the generator: unmap what ctx.alloc mapped, a call at a time, then finish'''
        self._over = True
        if self.ctx._allocs and self.ctx.abi:
            a, size = self.ctx._allocs.pop()
            call.nr = NUMBERS[self.ctx.abi]["munmap"][0]
            call.args[0], call.args[1], call.nargs = a, size, 2
            self._pending = None
            return _CALL
        return _DONE

    def _finish(self, status):
        p = self._inject._panda
        if status != OK and self.error is None:
            reason = p.ffi.string(p.libpanda.panda_inject_strerror(status)).decode()
            self.error = _CLASS.get(status, InjectRefused)("panda.inject", status, reason)
        with self._inject._lock:
            self.status = status
            cbs, self._callbacks = self._callbacks, []
        self._event.set()
        for cb in cbs:
            cb(self.value, self.error)


def _no_calls(ctx):
    '''A session with no calls (a rewind alone)'''
    return None
    yield


class Inject:
    '''``panda.inject``'''

    def __init__(self, panda):
        self._panda = panda
        self._lock = threading.Lock()
        self._sessions = {}
        self._token = 0
        self._next_cb = None
        self._done_cb = None

    def _callbacks(self):
        ffi = self._panda.ffi
        if self._next_cb is None:
            self._next_cb = ffi.callback(
                "int(void *, CPUState *, PandaInjectStep *, PandaInjectCall *)", self._c_next)
            self._done_cb = ffi.callback("void(void *, uint64_t, int)", self._c_done)

    def _session(self, opaque):
        token = int(self._panda.ffi.cast("uintptr_t", opaque))
        with self._lock:
            return self._sessions.get(token)

    def _c_next(self, opaque, cpu, step, call):
        s = self._session(opaque)
        if s is None:
            return _FAIL
        try:
            return s._next(cpu, step, call)
        except BaseException as e:          # the generator's own exception: the session's error
            s.error = e
            return _FAIL

    def _c_done(self, opaque, sid, status):
        token = int(self._panda.ffi.cast("uintptr_t", opaque))
        with self._lock:
            s = self._sessions.pop(token, None)
        if s is None:
            return
        try:
            s._finish(status)
        except Exception as e:      # a done callback's exception ends the run, as a callback's
            p = self._panda
            if getattr(p, "catch_exceptions", False):
                p.exit_exception = e
                p.end_analysis()
            else:
                raise

    def __call__(self, gen, asid=None, task=None, cpu=None, before_syscall=False, timeout=None,
                 allow_fork=False):
        '''Queue a session: ``gen`` is a generator, or a function ``f(ctx)`` returning one.

        Target: ``asid`` (any thread of that address space), ``task`` (the engine task key of
        one thread: syscalls2 must be loaded), ``cpu`` (the address space current on that
        vCPU), or ``cpu`` with ``before_syscall=True`` from a syscalls2 entry callback (the call
        waits until the session is over, then runs; its entry is delivered again, flagged
        resumed: :meth:`resumed`). ``timeout``: guest seconds until it has to have finished
        (default 5; ``float("inf")``: none).
        '''
        p = self._panda
        ffi, C = p.ffi, p.libpanda
        p._needs_engine("panda.inject")
        p._check_guest_mutation("panda.inject")
        self._callbacks()
        s = Session(self, gen, allow_fork)
        s.ctx = Context(p, s)
        if timeout is None:
            tns = 0
        elif timeout == float("inf"):
            tns = 2**64 - 1
        else:
            tns = max(1, int(timeout * 1e9))
        with self._lock:
            self._token += 1
            token = self._token
            self._sessions[token] = s
        sid = ffi.new("uint64_t *")
        op = ffi.cast("void *", token)
        if before_syscall:
            if cpu is None:
                raise ValueError("panda.inject: before_syscall needs the entry callback's cpu")
            flags = ffi.new("uint32_t *")
            call = p.plugins["syscalls2"].syscalls2_current_call_ptr(cpu, flags)
            if call == ffi.NULL:
                st = ENOTHERE
            else:
                st = C.panda_inject_hold(cpu, ffi.cast("struct PandaSyscallCall *", call), tns,
                                         self._next_cb, self._done_cb, op, sid)
        else:
            if task is not None:
                kind, target = TARGET_TASK, task
            elif asid is not None:
                kind, target = TARGET_ASID, asid
            elif cpu is not None:
                kind, target = TARGET_ASID, p.current_asid(cpu)
            else:
                raise ValueError("panda.inject: give asid=, task= or cpu=")
            st = C.panda_inject_submit(kind, target & (2**64 - 1), tns, self._next_cb,
                                       self._done_cb, op, sid)
        if st != QUEUED:
            with self._lock:
                self._sessions.pop(token, None)
            reason = ffi.string(C.panda_inject_strerror(st)).decode()
            raise _CLASS.get(st, InjectRefused)("panda.inject", st, reason)
        s.id = int(sid[0])
        return s

    def rewind(self, asid, sp, ret_pc, nr, args, length, gen=None, timeout=None, nargs=-1):
        '''A system call the guest kernel skipped, made again after a session.

        For a kernel-side system-call hook that held a call (it did not run, and the kernel is
        returning to user mode): when the thread of ``asid`` with user stack pointer ``sp`` is
        back in user mode at ``ret_pc`` (after its trap instruction of ``length`` bytes), ``gen``
        runs there (as for ``panda.inject``; ``None``: no calls), then the thread goes back to its
        trap with ``nr`` and ``args`` in their registers and makes the call again. Returns a
        ``Session`` (its result is ``None`` without ``gen``).
        '''
        p = self._panda
        ffi, C = p.ffi, p.libpanda
        p._needs_engine("panda.inject.rewind")
        p._check_guest_mutation("panda.inject.rewind")
        self._callbacks()
        s = Session(self, gen if gen is not None else _no_calls, False)
        s.ctx = Context(p, s)
        rw = ffi.new("PandaInjectRewind *")
        rw.nr = nr
        for i, a in enumerate(list(args)[:6]):
            rw.args[i] = int(a) & (2**64 - 1)
        rw.nargs = nargs
        rw.len = length
        tns = 0 if timeout is None else (2**64 - 1 if timeout == float("inf")
                                         else max(1, int(timeout * 1e9)))
        with self._lock:
            self._token += 1
            token = self._token
            self._sessions[token] = s
        sid = ffi.new("uint64_t *")
        st = C.panda_inject_rewind(asid & (2**64 - 1), sp & (2**64 - 1), ret_pc & (2**64 - 1), rw,
                                   tns, self._next_cb, self._done_cb, ffi.cast("void *", token), sid)
        if st != QUEUED:
            with self._lock:
                self._sessions.pop(token, None)
            reason = ffi.string(C.panda_inject_strerror(st)).decode()
            raise _CLASS.get(st, InjectRefused)("panda.inject.rewind", st, reason)
        s.id = int(sid[0])
        return s

    def rewound(self, asid, sp, pc):
        '''At a kernel-side hook: is this call one ``rewind`` put back? (consumes the answer)'''
        return bool(self._panda.libpanda.panda_inject_rewound(asid & (2**64 - 1), sp & (2**64 - 1),
                                                              pc & (2**64 - 1)))

    def resumed(self, cpu):
        '''In a syscalls2 entry callback: is this a held call coming back (after its session)?'''
        p = self._panda
        flags = p.ffi.new("uint32_t *")
        call = p.plugins["syscalls2"].syscalls2_current_call_ptr(cpu, flags)
        return call != p.ffi.NULL and bool(flags[0] & (1 << 6))

    def abi(self, cpu):
        '''The ABI name an injection into the task on ``cpu`` would use now, or None'''
        a = self._panda.libpanda.panda_inject_abi(cpu)
        return ABIS[a] if 0 <= a < len(ABIS) else None

    def stats(self):
        st = self._panda.ffi.new("PandaInjectStats *")
        self._panda.libpanda.panda_inject_stats(st)
        return {k: getattr(st, k) for k in ("submitted", "calls", "restarts", "completed",
                                             "abandoned", "stale", "overframes", "holds", "rewinds",
                                             "resumed", "held", "queued")}
