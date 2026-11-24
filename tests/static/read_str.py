"""Offline tests of pandare2's read_str against a fake guest memory, without loading an engine.

Run: python3 tests/static/read_str.py
read_str must give what the byte-at-a-time read gave: the string up to its NUL or max_length,
ValueError at an unreadable byte before either. Both of its readers are checked: the engine's
panda_virtual_memory_read_str (modelled here with the engine's contract) and the page-by-page
Python read used with osi_linux's read. The guest's page size is 1 KiB in some cases: 4 KiB
reads must not fail where a byte read wouldn't.
"""
import ast
from pathlib import Path
import unittest

import cffi

PANDA_TEST = dict(area="python", tier="static")

ROOT = Path(__file__).resolve().parents[2]
tree = ast.parse((ROOT / 'python/core/pandare2/panda.py').read_text())
panda = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == 'Panda')
body = [n for n in panda.body if isinstance(n, ast.FunctionDef) and n.name in ('read_str', '_read_cstr', '_vmr_fn')]
ns = {}
exec(compile(ast.fix_missing_locations(ast.Module(body=[ast.ClassDef(
    name='P', bases=[], keywords=[], decorator_list=[], body=body)], type_ignores=[])),
    str(ROOT / 'python/core/pandare2/panda.py'), 'exec'), ns)
ffi = cffi.FFI()


class Mem:
    """Guest memory: mapped pages of `page` bytes (start -> bytes); everything else unreadable."""

    def __init__(self, page=4096):
        self.page, self.pages, self.calls = page, {}, 0

    def put(self, addr, data):
        for i, b in enumerate(data):
            a = addr + i
            base = a - a % self.page
            pg = self.pages.setdefault(base, bytearray(self.page))
            pg[a - base] = b

    def byte(self, a):
        base = a - a % self.page
        if base not in self.pages:
            return None
        return self.pages[base][a - base]

    def read(self, cpu, addr, buf, n):      # panda_virtual_memory_read_external: all or nothing
        self.calls += 1
        data = [self.byte(addr + i) for i in range(n)]
        if None in data:
            return -1
        ffi.memmove(buf, bytes(data), n)
        return 0

    def read_str(self, cpu, addr, buf, mx):  # panda_virtual_memory_read_str's contract
        self.calls += 1
        n = 0
        while n < mx:
            l = min(self.page - (addr % self.page), mx - n)
            data = [self.byte(addr + i) for i in range(l)]
            if None in data:
                return -1
            ffi.memmove(buf + n, bytes(data), l)
            if 0 in data:
                return n + data.index(0)
            n += l
            addr += l
        return mx


def old_read_str(mem, ptr, max_length=None):
    """pandare2's read_str before: a byte at a time."""
    r, idx = b"", 0
    while max_length is None or idx < max_length:
        b = mem.byte(ptr)
        if b is None:
            raise ValueError("unreadable")
        if b == 0:
            break
        r += bytes([b])
        ptr += 1
        idx += 1
    return r.decode("utf8", "ignore")


def make(mem, engine_helper):
    p = ns['P']()
    p.ffi, p.bits, p._engine_started, p._osi_linux_loaded = ffi, 64, True, False
    p._needs_engine = lambda what: None
    p._read_str_c = mem.read_str if engine_helper else None
    p.libpanda = type('L', (), {'panda_virtual_memory_read_external': staticmethod(mem.read)})()
    return p


class ReadStr(unittest.TestCase):
    def cases(self, page):
        m = Mem(page)
        P = 0x10000
        m.put(P, b"\0" * (4 * 4096))                        # four mapped 4 KiB
        m.put(P, b"a" * 8 + b"\0")
        m.put(P + 512, b"b" * 300 + b"\0")
        m.put(P + 4096 - 4, b"c" * 8 + b"\0")              # across a 4 KiB boundary
        m.put(P + 4096 + 100, b"d" * 5000 + b"\0")         # longer than a page
        m.put(P + 4 * 4096 - 50, b"e" * 50)                 # runs into the unmapped page
        m.put(P + 2 * 4096, b"\xff\xfeok\0")                # not UTF-8
        if page < 4096:                                     # a hole a 4 KiB read would hit
            m.put(P + 3 * 4096, b"f" * 10 + b"\0")
            del m.pages[P + 3 * 4096 + 2 * page]
        ptrs = [(P, None), (P, 3), (P, 0), (P + 512, None), (P + 512, 300), (P + 512, 301),
                (P + 4096 - 4, None), (P + 4096 + 100, None), (P + 4096 + 100, 4096),
                (P + 4 * 4096 - 50, None), (P + 4 * 4096 - 50, 49), (P + 4 * 4096 - 50, 50),
                (P + 4 * 4096, None), (P + 2 * 4096, None), (0, None), (P + 3 * 4096, None)]
        return m, ptrs

    def check(self, page, helper):
        m, ptrs = self.cases(page)
        p = make(m, helper)
        for ptr, mx in ptrs:
            try:
                want = old_read_str(m, ptr, mx)
            except ValueError:
                with self.assertRaises(ValueError, msg=(hex(ptr), mx)):
                    p.read_str(None, ptr, mx)
                continue
            self.assertEqual(p.read_str(None, ptr, mx), want, (hex(ptr), mx))

    def test_engine_helper(self):
        self.check(4096, True)

    def test_python_pages(self):
        self.check(4096, False)

    def test_python_pages_small_guest_pages(self):
        self.check(1024, False)

    def test_engine_helper_small_guest_pages(self):
        self.check(1024, True)

    def test_few_calls(self):
        m, _ = self.cases(4096)
        p = make(m, True)
        m.calls = 0
        p.read_str(None, 0x10000 + 4096 + 100)              # 5000 bytes: 2 helper calls
        self.assertLessEqual(m.calls, 2)
        p = make(m, False)
        m.calls = 0
        p.read_str(None, 0x10000 + 4096 + 100)              # the rest of a page + 1 page + 1
        self.assertLessEqual(m.calls, 3)


if __name__ == '__main__':
    unittest.main()
