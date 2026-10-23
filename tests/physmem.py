"""PANDA physical memory reads and writes that cross a memory region boundary.

Run: panda-python tests/physmem.py <dir>/debian-13-x86_64.json

panda_physical_memory_rw used to translate the start address once and then copy the caller's whole
length from that region's host pointer: a range that ran past the end of a RAM region read or wrote
past its RAM block (past the end of RAM: QEMU's guard page, SIGSEGV). Now it goes region by region
like address_space_rw and fails (MEMTX_ERROR) when a piece isn't RAM. The test calls libpanda
directly: pandare2's physical_memory_read/write only raise on a negative return, and MEMTX_ERROR is
positive (1), so pandare2 never reports a failed physical access (separate, pre-existing).

With the VM stopped after the root revert, from the main loop:
- a write and a read across a page boundary inside RAM succeed and read back;
- a write and a read across the end of RAM fail cleanly (no crash), and the in-RAM half of the
  write landed (as address_space_rw would);
- x86_64: a write from RAM into the VGA window at 0xa0000 fails cleanly.
"""
import sys
import threading

from pandare2 import Panda
from pandare2.qcows_internal import Qcows

RAM_BASE = {"x86_64": 0, "aarch64": 0x40000000, "riscv64": 0x80000000}


def size(s):
    units = {"K": 1 << 10, "M": 1 << 20, "G": 1 << 30}
    return int(s[:-1]) * units[s[-1].upper()] if s[-1].upper() in units else int(s)


def main():
    image = sys.argv[1]
    q = Qcows.get_qcow_info(image)
    panda = Panda(generic=image)
    end = RAM_BASE[q.arch] + size(q.default_mem)
    res = {}
    done = threading.Event()

    lib = panda.libpanda

    def write(addr, data):
        buf = panda.ffi.new("char[]", data)
        if lib.panda_physical_memory_write_external(addr, panda.ffi.cast("char*", buf), len(data)) != 0:
            raise ValueError("write failed")

    def read(addr, n):
        buf = panda.ffi.new("char[]", n)
        if lib.panda_physical_memory_read_external(addr, panda.ffi.cast("char*", buf), n) != 0:
            raise ValueError("read failed")
        return bytes(panda.ffi.unpack(buf, n))

    def attempt(name, fn):
        try:
            res[name] = ("ok", fn())
        except ValueError as e:
            res[name] = ("error", str(e))
        print(f"PHYSMEM {name}: {res[name][0]}", flush=True)

    def checks():
        inside = end - 0x1010           # 32 bytes across the last page boundary but one
        pat = bytes(range(32))
        attempt("write across a page inside RAM", lambda: write(inside, pat))
        attempt("read across a page inside RAM", lambda: (read(inside, 32)))
        res["readback"] = res["read across a page inside RAM"] == ("ok", pat)
        before = (read(end - 16, 16))
        attempt("write across the end of RAM", lambda: write(end - 16, b"\x5a" * 32))
        res["in-RAM half written"] = (read(end - 16, 16)) == b"\x5a" * 16
        write(end - 16, before)
        attempt("read across the end of RAM", lambda: (read(end - 16, 32)))
        if q.arch == "x86_64":
            low = (read(0xa0000 - 16, 16))
            attempt("write from RAM into the VGA window", lambda: write(0xa0000 - 16, b"\x5a" * 32))
            write(0xa0000 - 16, low)
        done.set()

    @panda.queue_blocking
    def drive():
        panda.revert_sync("root")
        panda.vm_stop()
        panda.queue_main_loop_wait_fn(checks)
        done.wait(60)
        panda.end_analysis()

    panda.run()
    ok = True

    def check(name, cond):
        nonlocal ok
        ok &= bool(cond)
        print(f"  {'PASS' if cond else 'FAIL'} {name}")

    check("a write across a page inside RAM succeeds", res.get("write across a page inside RAM", ("",))[0] == "ok")
    check("and reads back", res.get("readback"))
    check("a write across the end of RAM fails cleanly", res.get("write across the end of RAM", ("",))[0] == "error")
    check("its in-RAM half was written", res.get("in-RAM half written"))
    check("a read across the end of RAM fails cleanly", res.get("read across the end of RAM", ("",))[0] == "error")
    if q.arch == "x86_64":
        check("a write from RAM into the VGA window fails cleanly",
              res.get("write from RAM into the VGA window", ("",))[0] == "error")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
