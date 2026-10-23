"""panda_fast_snap/panda_fast_restore restore RAM exactly, through the dirty-page path and the full one.

Run: panda-python tests/fastrestore.py <dir>/debian-13-x86_64.json [rounds]

The guest runs `cat /tmp/fr-start; drop caches; md5sum of /usr/share/common-licenses/*; cat /tmp/fr-end`.
At cat's openat of /tmp/fr-start the test takes a fast snapshot and remembers two pages: the one
holding cat's path (user memory) and the first page of RAM (where reset re-copies the arm virt DTB).
At the openat of /tmp/fr-end (after the guest read the licenses back from disk: virtio/IDE DMA into
the page cache) it stops, overwrites both pages from the host, restores, and checks:
- panda_fast_verify() == 0: every RAM page equals the snapshot (CPU, DMA and host writes undone);
- the restore took the dirty-page path and copied at least every page that differed before it;
- both overwritten pages read back their snapshot bytes;
- the guest then runs on from the snapshot again (the next round reaches /tmp/fr-end).
The last round loads the root snapshot (loadvm, untracked RAM writes) before the fast restore: that
restore must take the full path (last_why=loadvm) and still verify. With 3 or more rounds, the one
before it saves a qcow2 snapshot (savevm reads and clears the migration bitmap the fast restore
uses): that restore must take the full path too, told by the precopy notifier (last_why=precopy),
not only by the sentinel pages.
When the guest reaches the end marker after the last round, the test takes a second fast snapshot
there (the incremental path: only the pages written since the last restore), checks it equals RAM,
lets the guest run on, overwrites a page, restores, verifies, and checks the guest runs again.
Reports restore times.
"""
import threading
import sys
from time import perf_counter

from pandare2 import Panda

START, END = "/tmp/fr-start", "/tmp/fr-end"
CMD = (f"cat {START}; echo 3 > /proc/sys/vm/drop_caches; cat /usr/share/common-licenses/* | md5sum; "
       f"cat {END}")
RAM_BASE = {"x86_64": 0, "aarch64": 0x40000000, "riscv64": 0x80000000}


def main():
    image = sys.argv[1]
    rounds = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    panda = Panda(generic=image)
    lib = panda.libpanda
    st = {"snap": False, "pages": {}, "round": 0, "at_end": False, "results": [], "err": None}

    def info():
        return dict(kv.split("=", 1) for kv in panda.ffi.string(lib.panda_fast_info()).decode().split())

    def read_page(addr):
        return bytes(panda.physical_memory_read(addr, 4096))

    def take_snap():
        if lib.panda_fast_snap() != 0:
            st["err"] = "panda_fast_snap failed"
        st["pages"] = {a: read_page(a) for a in st["pages"]}

    def check_and_restore():
        r = {"round": st["round"]}
        try:
            r["differ_before"] = lib.panda_fast_verify()
            for a in st["pages"]:
                panda.physical_memory_write(a, b"\xa5" * 4096)
            last = st["round"] == rounds - 1
            save = rounds >= 3 and st["round"] == rounds - 2
            if last:
                # loadvm writes RAM without dirty bits: the fast restore must notice
                if lib.panda_revert(panda.ffi.new("char[]", b"root")) != 0:
                    st["err"] = "panda_revert root failed"
            elif save:
                # savevm reads and clears the migration bitmap: the fast restore must notice
                if lib.panda_snap(panda.ffi.new("char[]", b"frsave")) != 0:
                    st["err"] = "panda_snap frsave failed"
            t = perf_counter()
            r["rc"] = lib.panda_fast_restore()
            r["ms"] = (perf_counter() - t) * 1000
            i = info()
            r["full"] = i["last_full"] == "1"
            r["why"] = i["last_why"] if r["full"] else None
            r["copied"] = int(i["last_pages"])
            r["differ_after"] = lib.panda_fast_verify()
            r["pages_back"] = all(read_page(a) == v for a, v in st["pages"].items())
            r["want_full"] = last or save
            r["want_why"] = "loadvm" if last else "precopy" if save else None
        except Exception as e:  # noqa: BLE001 - reported as a failed check
            st["err"] = f"round {st['round']}: {e!r}"
        st["results"].append(r)
        print(f"FAST round {r}", flush=True)
        st["round"] += 1
        st["at_end"] = False
        lib.panda_cont()

    def second_snap():
        """At the end marker: an incremental snapshot, then a restore to it."""
        r = st["snap2"] = {}
        try:
            r["rc"] = lib.panda_fast_snap()
            i = info()
            r["mode"], r["pages"] = i["last_snap_mode"], int(i["last_snap_pages"])
            r["verify"] = lib.panda_fast_verify()
            r["page"] = RAM_BASE[panda.arch_name]
            r["want"] = read_page(r["page"])
        except Exception as e:  # noqa: BLE001
            st["err"] = f"second snapshot: {e!r}"
        print(f"FAST snap2 { {k: v for k, v in r.items() if k != 'want'} }", flush=True)
        lib.panda_cont()
        threading.Timer(2.0, lambda: (panda.vm_stop(), panda.queue_main_loop_wait_fn(restore2))).start()

    def restore2():
        r = st["restore2"] = {}
        try:
            r["differ_before"] = lib.panda_fast_verify()
            panda.physical_memory_write(st["snap2"]["page"], b"\xa5" * 4096)
            r["rc"] = lib.panda_fast_restore()
            i = info()
            r["mode"] = i["last_mode"]
            r["differ_after"] = lib.panda_fast_verify()
            r["page_back"] = read_page(st["snap2"]["page"]) == st["snap2"]["want"]
        except Exception as e:  # noqa: BLE001
            st["err"] = f"restore to the second snapshot: {e!r}"
        print(f"FAST restore2 {r}", flush=True)
        st["writes_after"] = 0
        lib.panda_cont()
        threading.Timer(2.0, panda.end_analysis).start()

    @panda.ppp("syscalls2", "on_sys_write_enter")
    def write(cpu, pc, fd, buf, count):
        if "writes_after" in st:
            st["writes_after"] += 1

    @panda.ppp("syscalls2", "on_sys_openat_enter")
    def openat(cpu, pc, dfd, filename, flags, *mode):
        try:
            name = panda.read_str(cpu, filename)
        except ValueError:
            return
        if name == START and not st["snap"]:
            st["snap"] = True
            path = panda.virt_to_phys(cpu, filename) & ~0xfff
            st["pages"] = {path: None, RAM_BASE[panda.arch_name]: None}
            panda.vm_stop()
            panda.queue_main_loop_wait_fn(take_snap)
            panda.queue_main_loop_wait_fn(lib.panda_cont)
        elif name == END and st["snap"] and not st["at_end"]:
            st["at_end"] = True
            if st["round"] == rounds:
                st["reran"] = True
                panda.vm_stop()
                panda.queue_main_loop_wait_fn(second_snap)
                return
            panda.vm_stop()
            panda.queue_main_loop_wait_fn(check_and_restore)

    @panda.queue_blocking
    def drive():
        panda.revert_sync("root")
        try:
            panda.run_serial_cmd(CMD, timeout=300)
        except Exception:
            pass  # the command reruns from the snapshot each round; the console sees the first

    panda.run()

    ok = True
    def check(name, cond):
        nonlocal ok
        ok &= bool(cond)
        print(f"  {'PASS' if cond else 'FAIL'} {name}")

    res = [r for r in st["results"] if "rc" in r]
    for r in res:
        print(f"FAST round={r['round']} full={int(r['full'])} differ_before={r['differ_before']} "
              f"copied={r['copied']} differ_after={r['differ_after']} restore_ms={r['ms']:.1f}")
    print(f"FAST info {panda.ffi.string(lib.panda_fast_info()).decode() if res else ''}")
    check("snapshot taken at the start marker", st["snap"])
    check(f"{rounds} restores ran (the guest reached the end marker each time)", len(res) == rounds)
    check("no error", st["err"] is None)
    if st["err"]:
        print("   ", st["err"])
    check("every restore returned 0", all(r["rc"] == 0 for r in res))
    check("RAM equals the snapshot after every restore (panda_fast_verify == 0)",
          all(r["differ_after"] == 0 for r in res))
    check("the guest changed RAM between restores (differ_before > 0)",
          all(r["differ_before"] > 0 for r in res))
    check("dirty-page path for the plain restores, full path after a loadvm",
          all(r["full"] == r["want_full"] for r in res))
    check("the dirty restores copied at least every page that differed",
          all(r["copied"] >= r["differ_before"] for r in res if not r["full"]))
    check("host-overwritten pages read back their snapshot bytes", all(r["pages_back"] for r in res))
    check("the guest re-ran to the end marker after the last restore", st.get("reran"))
    check("the full restores say why: loadvm after a loadvm, precopy after a savevm",
          all(r["why"] == r["want_why"] for r in res if r["want_full"]))
    s2, r2 = st.get("snap2", {}), st.get("restore2", {})
    check("a second snapshot at the end marker took the incremental path and equals RAM",
          s2.get("rc") == 0 and s2.get("mode") == "incremental" and s2.get("verify") == 0)
    check("the restore to it took the dirty-page path and verifies",
          r2.get("rc") == 0 and r2.get("mode") == "dirty" and r2.get("differ_before", 0) > 0
          and r2.get("differ_after") == 0 and r2.get("page_back"))
    check("the guest ran on after that restore (write syscalls seen)", st.get("writes_after", 0) > 0)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
