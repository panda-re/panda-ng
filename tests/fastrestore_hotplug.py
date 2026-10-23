"""Fast restore around a loadvm that fails partway, and around an unplugged RAM-backed device.

Run: panda-python tests/fastrestore_hotplug.py <dir>/debian-13-x86_64.json   (x86_64: ACPI PCI hotplug)

1. A loadvm that fails after it loaded RAM (review M2). A virtio-rng device is hot-plugged, the VM
   is saved as "frbad", and the device is unplugged again: loading frbad now loads all of RAM and
   then fails on the rng's device section. panda_fast_snap is taken before that loadvm; the guest
   runs; the loadvm fails. The next panda_fast_restore must copy all RAM (last_mode=full,
   last_why=loadvm) and panda_fast_verify must be 0. (Before: the tracking still looked valid, the
   restore took the dirty path and left the half-loaded RAM in place.)
2. An unplugged RAM-backed device (review L2). An e1000 (its option ROM is a RAM block) is
   hot-plugged, a fast snapshot taken, the e1000 unplugged (its RAMBlock freed). panda_fast_verify
   must return -1 and panda_fast_info say blocks=changed, without touching the freed block; and
   panda_fast_restore must refuse. (Before: both read the freed RAMBlock.)
The guest must still answer on the console at the end.
"""
import sys
import threading
import time

from pandare2 import Panda


def main():
    image = sys.argv[1]
    panda = Panda(generic=image)
    lib = panda.libpanda
    st = {"err": None}
    ev = threading.Event()

    def info():
        return dict(kv.split("=", 1) for kv in panda.ffi.string(lib.panda_fast_info()).decode().split())

    def in_main_loop(fn, cont=True):
        ev.clear()

        def run():
            try:
                fn()
            except Exception as e:  # noqa: BLE001 - reported as a failed check
                st["err"] = st["err"] or repr(e)
            if cont:
                lib.panda_cont()
            ev.set()
        panda.vm_stop()
        panda.queue_main_loop_wait_fn(run)
        if not ev.wait(120):
            st["err"] = st["err"] or "main-loop work timed out"

    def wait_pci(dev, present, timeout=60):
        """Wait until `info pci` does (or doesn't) list the vendor:device id."""
        t = time.time()
        while time.time() - t < timeout:
            if (dev in panda.run_monitor_cmd("info pci")) == present:
                return True
            panda.run_serial_cmd("sleep 1", timeout=30)
        return False

    @panda.queue_blocking
    def drive():
        try:
            panda.revert_sync("root")
            # --- 1. loadvm that fails partway ---
            panda.run_monitor_cmd("device_add virtio-rng-pci,id=rng0")
            st["rng_seen"] = "virtio" in panda.run_serial_cmd(
                "sleep 2; ls /sys/bus/virtio/drivers/virtio_rng/ 2>&1", timeout=60)
            in_main_loop(lambda: st.__setitem__("savevm", lib.panda_snap(panda.ffi.new("char[]", b"frbad"))))
            panda.run_monitor_cmd("device_del rng0")
            st["rng_gone"] = wait_pci("1af4:1005", False) or wait_pci("1af4:1044", False)

            in_main_loop(lambda: st.__setitem__("snap1", lib.panda_fast_snap()))
            panda.run_serial_cmd("ls -R /usr/share/doc | wc -l", timeout=120)   # the guest changes RAM

            def bad_loadvm_then_restore():
                st["loadvm_rc"] = lib.panda_revert(panda.ffi.new("char[]", b"frbad"))
                st["r1_rc"] = lib.panda_fast_restore()
                st["r1_verify"] = lib.panda_fast_verify()
                i = info()
                # (an engine before last_mode/last_why: say what last_full says)
                st["r1_mode"] = i.get("last_mode", "full" if i.get("last_full") == "1" else "dirty")
                st["r1_why"] = i.get("last_why")
            in_main_loop(bad_loadvm_then_restore)
            st["alive1"] = panda.run_serial_cmd("echo al''ive1", timeout=60)

            # --- 2. unplugged RAM-backed device ---
            panda.run_monitor_cmd("device_add e1000,id=nic1")
            st["nic_seen"] = wait_pci("8086:100e", True)
            panda.run_serial_cmd("sleep 2", timeout=30)
            in_main_loop(lambda: st.__setitem__("snap2", lib.panda_fast_snap()))
            st["blocks_with_nic"] = info()["blocks"]
            panda.run_monitor_cmd("device_del nic1")
            st["nic_gone"] = wait_pci("8086:100e", False)
            panda.run_serial_cmd("sleep 1", timeout=30)   # RCU frees the RAMBlock

            def after_unplug():
                st["verify2"] = lib.panda_fast_verify()
                st["blocks2"] = info().get("blocks")
                st["r2_rc"] = lib.panda_fast_restore()
            in_main_loop(after_unplug)
            st["alive2"] = panda.run_serial_cmd("echo al''ive2", timeout=60)
            in_main_loop(lib.panda_fast_delete)   # main loop: it stops dirty logging (BQL)
        except Exception as e:  # noqa: BLE001
            st["err"] = st["err"] or repr(e)
        panda.end_analysis()

    panda.run()
    print("HOTPLUG", {k: v for k, v in st.items() if k != "blocks_with_nic"})
    print("HOTPLUG blocks with the e1000:", st.get("blocks_with_nic"))
    ok = True

    def check(name, cond):
        nonlocal ok
        ok &= bool(cond)
        print(f"  {'PASS' if cond else 'FAIL'} {name}")

    check("no error", st["err"] is None)
    check("the guest bound the hot-plugged virtio-rng", st.get("rng_seen"))
    check("savevm frbad with the rng", st.get("savevm") == 0)
    check("the rng was unplugged", st.get("rng_gone"))
    check("panda_fast_snap", st.get("snap1") == 0)
    check("loadvm frbad failed (the rng's section has no device)", st.get("loadvm_rc", 0) != 0)
    check("the next fast restore copied all RAM because of the loadvm (last_mode=full, last_why=loadvm)",
          st.get("r1_rc") == 0 and st.get("r1_mode") == "full" and st.get("r1_why") == "loadvm")
    check("RAM equals the fast snapshot after it (panda_fast_verify == 0)", st.get("r1_verify") == 0)
    check("the guest runs on after it", "alive1" in (st.get("alive1") or ""))
    check("the e1000 was plugged, snapshotted with its ROM block, and unplugged",
          st.get("nic_seen") and st.get("snap2") == 0 and "e1000.rom" in (st.get("blocks_with_nic") or "")
          and st.get("nic_gone"))
    check("panda_fast_verify returns -1 once RAM blocks changed", st.get("verify2") == -1)
    check("panda_fast_info says blocks=changed", st.get("blocks2") == "changed")
    check("panda_fast_restore refuses", st.get("r2_rc") == -1)
    check("the guest runs on", "alive2" in (st.get("alive2") or ""))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
