'''
Page-in reads and writes (``panda.pagein``): guest virtual memory of a process whose pages may
not be present yet (never touched, swapped out, a file page not in the page cache).

A request is served the next time one of the process's threads runs in user mode: the engine has
that thread call ``madvise(MADV_POPULATE_READ)`` (``_WRITE`` for writes), puts its registers back,
then reads or writes the range. The guest kernel pages the range in under its own rules, so a bad
address is an error here (:class:`PageInFailed`, with the guest's errno), never a signal to the
process or a kernel oops. The code that made the request never runs again because of it.

Two forms:

* ``panda.pagein.read(asid, addr, length, done)`` from anywhere (any callback, the
  ``queue_blocking`` thread, another thread): ``done(data, error)`` runs later on a vCPU thread,
  ``data`` the bytes or None, ``error`` None or a :class:`PageInError`.
* ``panda.virtual_memory_read(cpu, addr, length, fault=True)`` (and ``read_str``,
  ``virtual_memory_write``): returns at once when the range is already present; otherwise it
  waits for the page-in, which is allowed only off the vCPU threads and the main loop (from a
  ``queue_blocking`` function). Inside a callback it raises :class:`PageInWouldBlock`: use the
  completion form there.

Refused with :class:`PageInRefused` (and a reason): during record/replay, under KVM, for targets
without a system-call injector, for 32-bit compatibility tasks, bad arguments, too many requests
outstanding. See docs/guides/python.md, "Memory that isn't paged in".
'''
import threading


class PageInError(RuntimeError):
    '''A page-in that did not happen. ``status`` is the engine's PandaPageinStatus.'''

    def __init__(self, what, status, reason, guest_errno=0):
        msg = f"{what}: {reason}"
        if guest_errno:
            import errno as _errno
            msg += f" (guest errno {guest_errno}: {_errno.errorcode.get(guest_errno, '?')})"
        super().__init__(msg)
        self.status = status
        self.guest_errno = guest_errno


class PageInRefused(PageInError, PermissionError):
    '''Not attempted: record/replay, KVM, an unsupported target or task, bad arguments, busy.'''


class PageInFailed(PageInError, ValueError):
    '''The guest kernel refused the range (``guest_errno``: ENOMEM for no mapping, EFAULT, ...),
    or it was reclaimed again each time, or it isn't RAM.'''


class PageInTimeout(PageInError, TimeoutError):
    '''No thread of the process ran in user mode in time: it exited, is blocked, or never ran.'''


class PageInCancelled(PageInError):
    '''A snapshot load or reset came first.'''


class PageInWouldBlock(PageInError, BlockingIOError):
    '''A synchronous page-in asked for where it can't wait (a callback, the main loop).'''


# PandaPageinStatus
OK, QUEUED, EGUEST, ETIMEDOUT, ECANCELED, ERECORD, EACCEL, EARCH, EINVAL, EBUSY, ERECLAIMED, EIO = \
    0, 1, -1, -2, -3, -4, -5, -6, -7, -8, -9, -10
_CLASS = {EGUEST: PageInFailed, ERECLAIMED: PageInFailed, EIO: PageInFailed,
          ETIMEDOUT: PageInTimeout, ECANCELED: PageInCancelled}


class PageIn:
    '''``panda.pagein``'''

    def __init__(self, panda):
        self._panda = panda
        self._lock = threading.Lock()
        self._pending = {}
        self._token = 0
        self._cb = None             # the cffi completion, made at the first request

    def error(self, what, status, guest_errno=0):
        reason = self._panda.ffi.string(self._panda.libpanda.panda_pagein_strerror(status)).decode()
        cls = _CLASS.get(status, PageInRefused)
        return cls(what, status, reason, guest_errno)

    def _done(self, rid, status, guest_errno, data, length, opaque):
        token = int(self._panda.ffi.cast("uintptr_t", opaque))
        with self._lock:
            what, cb = self._pending.pop(token, (None, None))
        if cb is None:
            return
        result = bytes(self._panda.ffi.buffer(data, length)) if status == OK else None
        err = None if status == OK else self.error(what, status, guest_errno)
        try:
            cb(result, err)
        except Exception as e:      # a completion's exception ends the run, as a callback's
            p = self._panda
            if getattr(p, "catch_exceptions", False):
                p.exit_exception = e
                p.end_analysis()
            else:
                raise

    def _submit(self, what, asid, addr, length, wdata, done, timeout_ns):
        ffi, C = self._panda.ffi, self._panda.libpanda
        self._panda._needs_engine(what)
        if self._cb is None:
            self._cb = ffi.callback("void(uint64_t, int, int, const uint8_t *, size_t, void *)",
                                    self._done)
        with self._lock:
            self._token += 1
            token = self._token
            self._pending[token] = (what, done)
        rid = ffi.new("uint64_t *")
        w = ffi.NULL if wdata is None else ffi.from_buffer("uint8_t[]", wdata)
        st = C.panda_pagein_submit(asid & (2**64 - 1), addr & (2**64 - 1), length, w,
                                   int(timeout_ns or 0), self._cb, ffi.cast("void *", token), rid)
        if st != QUEUED:
            with self._lock:
                self._pending.pop(token, None)
            raise self.error(what, st)
        return int(rid[0])

    def read(self, asid, addr, length, done, timeout_ns=None):
        '''Queue a read; ``done(data, error)`` runs when it is served. Returns the request id.'''
        return self._submit(f"pagein.read({addr:#x}, {length})", asid, addr, length, None, done,
                            timeout_ns)

    def write(self, asid, addr, data, done=None, timeout_ns=None):
        '''Queue a write of ``data``; ``done(data, error)`` runs when it is served.'''
        self._panda._check_guest_mutation("pagein.write")
        data = bytes(data)
        return self._submit(f"pagein.write({addr:#x}, {len(data)})", asid, addr, len(data), data,
                            done or (lambda d, e: None), timeout_ns)

    def can_wait(self):
        '''Whether this thread may wait for a page-in (not a vCPU thread, not the main loop).'''
        return bool(self._panda.libpanda.panda_pagein_can_wait())

    def wait(self, what, submit, timeout=30):
        '''Submit through ``submit(done)`` and wait for its completion (off the vCPU threads).'''
        if not self.can_wait():
            raise PageInWouldBlock(what, EINVAL, "the page isn't present and this thread can't "
                                   "wait for it (a callback or the main loop): use "
                                   "panda.pagein.read() with a completion")
        ev, box = threading.Event(), {}

        def done(data, err):
            box["data"], box["err"] = data, err
            ev.set()
        submit(done)
        if not ev.wait(timeout):
            raise PageInTimeout(what, ETIMEDOUT, "no completion within the host timeout")
        if box["err"] is not None:
            raise box["err"]
        return box["data"]

    def read_sync(self, asid, addr, length, timeout=30):
        what = f"pagein.read({addr:#x}, {length})"
        return self.wait(what, lambda done: self.read(asid, addr, length, done), timeout)

    def write_sync(self, asid, addr, data, timeout=30):
        what = f"pagein.write({addr:#x}, {len(data)})"
        self.wait(what, lambda done: self.write(asid, addr, data, done), timeout)

    def stats(self):
        ffi, C = self._panda.ffi, self._panda.libpanda
        s, i, c, a, st, ov = (ffi.new("uint64_t *") for _ in range(6))
        by = ffi.new("uint64_t[11]")
        C.panda_pagein_stats(s, i, c, a, st, ov, by)
        return dict(submitted=s[0], injected=i[0], completed=c[0], abandoned=a[0], stale=st[0],
                    overframes=ov[0],
                    by_status={-k: by[k] for k in range(11) if by[k]})
