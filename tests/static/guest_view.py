"""pandare2's env view (panda.guest.env) turns field assignments into the right env writes.
No engine, no guest.

    python3 tests/static/guest_view.py

A cffi CPUArchState stand-in with nested structs, arrays, a union, scalar arrays behind pointers
and an array of such pointers; a fake libpanda records each panda_guest_write_env_path call. The
view must give the engine the offsets it would compute itself (offsetof), follow pointers as
path steps rather than host addresses, read through to the live struct, refuse repointing, and
turn the engine's result codes into the matching exceptions.
"""
import importlib.util
import unittest
from pathlib import Path

from cffi import FFI

PANDA_TEST = dict(area="api", tier="static", needs=["pandare2"], nix_check="guest-view")

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('guest', ROOT / 'python/core/pandare2/guest.py')
guest = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guest)

CDEF = '''
typedef struct { uint32_t *drbar; uint32_t *drsr; } PMSAv7;
typedef struct { uint32_t *rbar[2]; uint32_t mair0[2]; } PMSAv8;
typedef struct Inner { uint64_t a; uint16_t b; } Inner;
typedef struct CPUArchState {
    uint32_t regs[16];
    uint64_t xregs[32];
    struct { uint64_t ttbr0_el[4]; uint32_t c1_sys; } cp15;
    union { uint32_t lo; uint64_t wide; } u;
    PMSAv7 pmsav7;
    PMSAv8 pmsav8;
    Inner *inner;
    void *opaque;
} CPUArchState;
'''


class FakeLib:
    def __init__(self, ffi):
        self.ffi = ffi
        self.calls = []
        self.result = 0

    def panda_guest_write_env_path(self, cpu, derefs, n, offset, data, length):
        path = [derefs[i] for i in range(n)] if n else []
        self.calls.append((path, offset, bytes(data)[:length]))
        return self.result

    def panda_guest_write_strerror(self, result):
        return self.ffi.new("char[]", b"refused")


class FakePanda:
    def __init__(self):
        self.ffi = FFI()
        self.ffi.cdef(CDEF)
        self.libpanda = FakeLib(self.ffi)
        self.env = self.ffi.new("CPUArchState *")
        self.drbar = self.ffi.new("uint32_t[8]", list(range(8)))
        self.rbar1 = self.ffi.new("uint32_t[8]")
        self.inner = self.ffi.new("Inner *")
        self.env.pmsav7.drbar = self.drbar
        self.env.pmsav8.rbar[1] = self.rbar1
        self.env.inner = self.inner
        self.env.regs[2] = 0x22
        self.endianness = 'little'

    def cpu_env(self, cpu):
        return self.env

    def _needs_engine(self, what):
        pass

    def _check_guest_mutation(self, what, cpu=None, memory=False):
        pass


def offsetof(ffi, *path):
    return ffi.offsetof("CPUArchState", *path)


class GuestView(unittest.TestCase):
    def setUp(self):
        self.panda = FakePanda()
        self.ffi = self.panda.ffi
        self.env = guest.Guest(self.panda).env("cpu")
        self.calls = self.panda.libpanda.calls

    def test_fields_and_arrays(self):
        self.env.regs[3] = 0x11223344
        self.env.cp15.ttbr0_el[1] = 0x1000
        self.env.cp15.c1_sys = 5
        self.env.u.wide = 2**40
        self.assertEqual(self.calls, [
            ([], offsetof(self.ffi, 'regs', 3), (0x11223344).to_bytes(4, 'little')),
            ([], offsetof(self.ffi, 'cp15', 'ttbr0_el', 1), (0x1000).to_bytes(8, 'little')),
            ([], offsetof(self.ffi, 'cp15', 'c1_sys'), (5).to_bytes(4, 'little')),
            ([], offsetof(self.ffi, 'u', 'wide'), (2**40).to_bytes(8, 'little')),
        ])

    def test_reads_pass_through(self):
        self.assertEqual(self.env.regs[2], 0x22)
        self.assertEqual(self.env.pmsav7.drbar[5], 5)
        self.assertEqual(len(self.env.regs), 16)
        self.assertEqual(self.calls, [])

    def test_pointers_are_path_steps(self):
        self.env.pmsav7.drbar[3] = 0xabc
        self.env.pmsav8.rbar[1][2] = 0xdef
        self.env.inner.b = 7
        self.assertEqual(self.calls, [
            ([offsetof(self.ffi, 'pmsav7', 'drbar')], 12, (0xabc).to_bytes(4, 'little')),
            ([offsetof(self.ffi, 'pmsav8', 'rbar', 1)], 8, (0xdef).to_bytes(4, 'little')),
            ([offsetof(self.ffi, 'inner')], self.ffi.offsetof('Inner', 'b'), (7).to_bytes(2, 'little')),
        ])
        self.assertIsNone(self.env.pmsav7.drsr)       # NULL: nothing to follow

    def test_refusals(self):
        with self.assertRaises(guest.GuestWriteInvalid):
            self.env.pmsav7.drbar = 0
        with self.assertRaises(guest.GuestWriteInvalid):
            self.env.opaque = 0
        with self.assertRaises(guest.GuestWriteInvalid):
            self.env.cp15 = 1
        with self.assertRaises(IndexError):
            self.env.regs[16] = 1
        with self.assertRaises(AttributeError):
            self.env.nosuch = 1
        self.env.cp15 = bytes(self.ffi.sizeof(self.ffi.typeof(self.panda.env.cp15)))
        self.assertEqual(self.calls[-1][1], offsetof(self.ffi, 'cp15'))

    def test_engine_results(self):
        for code, error in ((1, PermissionError), (2, ValueError), (3, ValueError),
                            (4, PermissionError), (5, guest.GuestWriteTooBig)):
            self.panda.libpanda.result = code
            with self.assertRaises(error) as ctx:
                self.env.regs[0] = 1
            self.assertIsInstance(ctx.exception, guest.GuestWriteError)
            self.assertEqual(ctx.exception.result, code)


if __name__ == '__main__':
    unittest.main()
