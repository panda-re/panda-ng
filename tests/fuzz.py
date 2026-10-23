"""Snapshot-and-restore fuzzing through pandare2 on an image built by pandare2.mkimage.

Run: panda-python tests/fuzz.py <dir>/debian-13-x86_64.json [iterations]

The guest runs `cat /etc/os-release`. When cat's openat of /etc/os-release enters the kernel, the
test takes a snapshot (Panda.snap from the callback). Each iteration then reverts to it, writes an
input over the path in cat's memory (physically, translated at the snapshot) and continues; cat
either reads the file (read on fd 3) or reports the error (write on fd 2), and that outcome must be
the input's. Checks snap, revert, cont and memory writes from the main loop, and state restored
on every revert (each iteration sees only its own input); reports iterations per second.
"""
import sys
from time import time

from pandare2 import Panda

ORIG = b"/etc/os-release"
# (input, file exists): each fits in the original path's buffer
INPUTS = [(b"/etc/hostname", True), (b"/nonexistent", False), (b"/etc/passwd", True),
          (b"/etc/nope", False), (b"/etc/os-release", True)]


def main():
    image = sys.argv[1]
    iterations = int(sys.argv[2]) if len(sys.argv) > 2 else 10
    panda = Panda(generic=image)
    st = {"snap": False, "asid": None, "path": None, "i": -1, "outcomes": [], "t0": None,
          "failed": None, "done": False, "loadvm": 0}

    @panda.cb_after_loadvm
    def after_loadvm(cpu):
        st["loadvm"] += 1

    def next_input():
        st["i"] += 1
        if st["i"] >= iterations:
            st["done"] = True
            panda.end_analysis()
            return
        data, _ = INPUTS[st["i"] % len(INPUTS)]
        panda.vm_stop()
        panda.queue_main_loop_wait_fn(panda.libpanda.panda_revert, [panda.ffi.new("char[]", b"fuzz")])
        panda.queue_main_loop_wait_fn(write_input, [data])
        panda.queue_main_loop_wait_fn(panda.libpanda.panda_cont)

    def write_input(data):
        # physical: the vCPU stopped in the kernel, which may not reach user pages (riscv SUM)
        try:
            panda.physical_memory_write(st["path"], data + b"\0")
        except ValueError as e:
            st["failed"] = f"write: {e}"

    @panda.ppp("syscalls2", "on_sys_openat_enter")
    def openat(cpu, pc, dfd, filename, flags, *mode):
        if st["snap"]:
            return
        try:
            if panda.read_str(cpu, filename) != ORIG.decode():
                return
        except ValueError:
            return
        st["snap"] = True
        st["asid"] = panda.current_asid(cpu)
        st["path"] = panda.virt_to_phys(cpu, filename)
        if (filename & 0xfff) + len(ORIG) + 1 > 0x1000:
            st["failed"] = "path crosses a page"
        panda.snap("fuzz")
        # after the snapshot is taken (cont), the first iteration reverts to it
        panda.queue_main_loop_wait_fn(lambda: st.__setitem__("t0", time()))
        panda.queue_main_loop_wait_fn(next_input)

    def outcome(cpu, exists):
        if st["t0"] is None or st["done"] or panda.current_asid(cpu) != st["asid"]:
            return
        if st["i"] >= 0 and len(st["outcomes"]) == st["i"]:
            st["outcomes"].append(exists)
            next_input()

    @panda.ppp("syscalls2", "on_sys_read_enter")
    def read(cpu, pc, fd, buf, count):
        if fd == 3:
            outcome(cpu, True)

    @panda.ppp("syscalls2", "on_sys_write_enter")
    def write(cpu, pc, fd, buf, count):
        if fd == 2:
            outcome(cpu, False)

    @panda.queue_blocking
    def drive():
        panda.revert_sync("root")
        try:
            panda.run_serial_cmd("cat /etc/os-release", timeout=60)
        except Exception:
            pass  # cat runs once per iteration; the console only sees the last

    t_start = time()
    panda.run()
    elapsed = time() - (st["t0"] or t_start)
    want = [INPUTS[i % len(INPUTS)][1] for i in range(iterations)]
    got = st["outcomes"]
    print(f"FUZZ iterations={len(got)} outcomes={got} {len(got) / elapsed:.2f} it/s"
          + (f" failed={st['failed']}" if st["failed"] else ""))
    checks = {
        "snapshot taken from the openat callback": st["snap"],
        f"all {iterations} iterations ran": len(got) == iterations,
        "each iteration saw only its own input": got == want,
        "no memory write failed": st["failed"] is None,
        "after_loadvm fired for the root revert and each iteration": st["loadvm"] == iterations + 1,
    }
    for k, ok in checks.items():
        print(f"  {'PASS' if ok else 'FAIL'} {k}")
    sys.exit(0 if all(checks.values()) else 1)


if __name__ == "__main__":
    main()
