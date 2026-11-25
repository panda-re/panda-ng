"""Offline tests of panda.inject's generator driver (python/core/pandare2/inject.py), no engine.

Run: python3 tests/static/inject_driver.py

The engine's next() is driven by hand: each step gives the previous call's value, as
panda_inject_service() would. Checks the calls it produces (numbers by ABI, arguments, nargs),
the values the generator sees (errno, 32-bit values), check=True, the fork refusal, Touch,
ctx.alloc/ctx.read/ctx.write and the session's cleanup, and that the number table is current.
"""
import importlib.util
import subprocess
import sys
import types
import unittest
from pathlib import Path
from types import SimpleNamespace

PANDA_TEST = dict(area="inject", tier="static", nix_check="inject-driver")

ROOT = Path(__file__).resolve().parents[2]
PKG = ROOT / "python/core/pandare2"

# pandare2.inject without pandare2/__init__ (which needs the native bindings)
pkg = types.ModuleType("injectpkg")
pkg.__path__ = [str(PKG)]
sys.modules["injectpkg"] = pkg
spec = importlib.util.spec_from_file_location("injectpkg.inject", PKG / "inject.py")
inject = importlib.util.module_from_spec(spec)
sys.modules["injectpkg.inject"] = inject
spec.loader.exec_module(inject)
N = inject.NUMBERS
CALL, DONE, AGAIN = inject._CALL, inject._DONE, inject._AGAIN


class FakePanda:
    """virtual_memory_read and the user write, over a dict of pages; misses until 'paged'"""

    def __init__(self):
        self.mem = {}
        self.missing = set()        # page addresses that fault until a madvise
        self.libpanda = SimpleNamespace(panda_guest_write_virt_user=self._write)
        self.ffi = SimpleNamespace(from_buffer=lambda t, d: bytes(d))

    def _page(self, a):
        return a & ~0xfff

    def virtual_memory_read(self, cpu, addr, n):
        if any(self._page(a) in self.missing for a in range(addr, addr + n)):
            raise ValueError("unmapped")
        return bytes(self.mem.get(a, 0) for a in range(addr, addr + n))

    def _write(self, cpu, addr, buf, n):
        if any(self._page(a) in self.missing for a in range(addr, addr + n)):
            return 3
        for i, b in enumerate(buf):
            self.mem[addr + i] = b
        return 0


class Driver:
    """Runs a session's _next by hand: step.calls > 0 after a call, its value in step.ret"""

    def __init__(self, gen, abi="x86_64", allow_fork=False, panda=None):
        self.panda = panda or FakePanda()
        self.s = inject.Session(SimpleNamespace(_panda=self.panda), gen, allow_fork)
        self.s.ctx = inject.Context(self.panda, self.s)
        self.abi = inject.ABIS.index(abi)
        self.calls = 0
        self.ret = 0

    def step(self, ret=None):
        """The next answer: (r, call); ret: the previous call's value"""
        if ret is not None:
            self.calls += 1
            self.ret = ret
        step = SimpleNamespace(id=1, abi=self.abi, calls=self.calls, ret=self.ret, touch=2**64 - 1)
        call = SimpleNamespace(nr=0, args=[0] * 6, nargs=-1)
        r = self.s._next("cpu", step, call)
        if r == AGAIN:
            self.calls = 0          # the registers go back; a new round starts at 0
        return r, call, step


class Driving(unittest.TestCase):
    def test_numbers_and_values(self):
        seen = {}

        def gen(ctx):
            seen["pid"] = yield inject.syscall("getpid")
            seen["fd"] = yield inject.syscall("openat", -100, 0x1000, 0)
            seen["err"] = yield inject.syscall("openat", -100, 0x2000, 0)
            return "end"
        for abi in inject.ABIS:
            d = Driver(gen, abi)
            r, c, _ = d.step()
            self.assertEqual((r, c.nr, c.nargs), (CALL, N[abi]["getpid"][0], 0), abi)
            r, c, _ = d.step(41)
            self.assertEqual((r, c.nr, c.nargs), (CALL, N[abi]["openat"][0], 4), abi)
            self.assertEqual(c.args[0], 2**64 - 100)
            r, c, _ = d.step(3)
            r, c, _ = d.step(-2)
            self.assertEqual(r, DONE)
            self.assertEqual((seen["pid"], seen["fd"], seen["err"]), (41, 3, -2), abi)
            self.assertEqual(d.s.value, "end")

    def test_32bit_values_unsigned(self):
        def gen(ctx):
            return (yield inject.syscall("mmap2", 0, 4096, 3, 0x22, -1, 0))
        d = Driver(gen, "arm")
        d.step()
        r, _, _ = d.step(-0x48100000)       # 0xb7f00000, sign-extended by the engine
        self.assertEqual((r, d.s.value), (DONE, 0xb7f00000))

    def test_check_raises_in_generator(self):
        def gen(ctx):
            try:
                yield inject.syscall("openat", -100, 0, 0, check=True)
            except inject.InjectFailed as e:
                return e.errno
        d = Driver(gen)
        d.step()
        r, _, _ = d.step(-2)
        self.assertEqual((r, d.s.value), (DONE, 2))

    def test_fork_refused(self):
        def gen(ctx):
            try:
                yield inject.syscall("fork")
            except inject.InjectRefused:
                return "refused"
            return "forked"
        d = Driver(gen)
        r, _, _ = d.step()
        self.assertEqual((r, d.s.value), (DONE, "refused"))
        d = Driver(gen, allow_fork=True)
        r, c, _ = d.step()
        self.assertEqual((r, c.nr), (CALL, N["x86_64"]["fork"][0]))

    def test_unknown_name(self):
        def gen(ctx):
            yield inject.syscall("no_such_call")
        d = Driver(gen)
        with self.assertRaises(inject.InjectRefused):
            d.step()

    def test_hyde_syscall_object(self):
        sc = inject.Syscall("getuid")

        def gen(ctx):
            yield sc
        d = Driver(gen)
        d.step()
        d.step(1000)
        self.assertEqual(sc.retval, 1000)

    def test_touch_resumes_next_round(self):
        def gen(ctx):
            yield inject.Touch(0x5000)
            return (yield inject.syscall("getpid"))
        d = Driver(gen)
        r, _, step = d.step()
        self.assertEqual((r, step.touch), (AGAIN, 0x5000))
        r, c, _ = d.step()
        self.assertEqual((r, c.nr), (CALL, 39))
        r, _, _ = d.step(7)
        self.assertEqual((r, d.s.value), (DONE, 7))

    def test_alloc_read_write_and_cleanup(self):
        p = FakePanda()
        p.missing.add(0x7000)

        def gen(ctx):
            buf = yield from ctx.alloc(8192)
            yield from ctx.write(buf, b"hello")
            data = yield from ctx.read(buf, 5)
            other = yield from ctx.read(0x7008, 4)
            return buf, data, other
        for abi, flags, name in (("x86_64", 0x22, "mmap"), ("mips_o32", 0x802, "mmap2"),
                                 ("mips_n64", 0x802, "mmap"), ("i386", 0x22, "mmap2")):
            p.missing = {0x7000}
            d = Driver(gen, abi, panda=p)
            r, c, _ = d.step()
            self.assertEqual((r, c.nr, c.args[1], c.args[3]), (CALL, N[abi][name][0], 8192, flags),
                             abi)
            r, c, _ = d.step(0x10000)                      # mmap's address
            # the read of the missing page: madvise(POPULATE_READ), then the read
            self.assertEqual((r, c.nr, c.args[0], c.args[2]),
                             (CALL, N[abi]["madvise"][0], 0x7000, 22), abi)
            p.missing.clear()
            r, c, _ = d.step(0)
            # the cleanup: munmap of what alloc mapped, then done
            self.assertEqual((r, c.nr, c.args[0], c.args[1]), (CALL, N[abi]["munmap"][0], 0x10000,
                                                               8192), abi)
            r, _, _ = d.step(0)
            self.assertEqual(r, DONE, abi)
            self.assertEqual(d.s.value, (0x10000, b"hello", bytes(4)), abi)

    def test_generator_exception_propagates(self):
        def gen(ctx):
            yield inject.syscall("getpid")
            raise KeyError("mine")
        d = Driver(gen)
        d.step()
        with self.assertRaises(KeyError):
            d.step(1)

    def test_rewind_marshalling(self):
        calls = {}

        class FFI:
            NULL = None

            def new(self, t, init=None):
                if t == "PandaInjectRewind *":
                    return SimpleNamespace(nr=0, args=[0] * 6, nargs=0, len=0)
                return [0]

            def cast(self, t, v):
                return v

            def callback(self, sig, fn):
                return fn

            def string(self, v):
                return v

        def rewind(asid, sp, ret_pc, rw, tns, nxt, done, op, sid):
            calls.update(asid=asid, sp=sp, ret_pc=ret_pc, rw=rw, tns=tns)
            sid[0] = 9
            return inject.QUEUED
        p = SimpleNamespace(ffi=FFI(), libpanda=SimpleNamespace(panda_inject_rewind=rewind,
                                                                panda_inject_rewound=lambda a, s, c: 1),
                            _needs_engine=lambda w: None, _check_guest_mutation=lambda w: None)
        inj = inject.Inject(p)
        s = inj.rewind(0x1000, 0x7ff0, 0x400102, 101, [0x7ff8, 0], 4, timeout=2)
        self.assertEqual(s.id, 9)
        rw = calls["rw"]
        self.assertEqual((calls["asid"], calls["sp"], calls["ret_pc"], calls["tns"]),
                         (0x1000, 0x7ff0, 0x400102, 2_000_000_000))
        self.assertEqual((rw.nr, rw.args[:2], rw.nargs, rw.len), (101, [0x7ff8, 0], -1, 4))
        self.assertTrue(inj.rewound(0x1000, 0x7ff0, 0x4000fe))

    def test_rewind_without_calls(self):
        d = Driver(inject._no_calls)
        r, _, _ = d.step()
        self.assertEqual((r, d.s.value), (DONE, None))

    def test_table_current(self):
        r = subprocess.run([sys.executable, str(ROOT / "plugins/syscalls2/scripts/gen_inject_numbers.py"),
                            "--check"], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)


if __name__ == "__main__":
    unittest.main()
