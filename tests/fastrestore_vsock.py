"""panda_fast_restore stays exact with a vhost-user vsock device (IGLOO's guest channel).

Run: panda-python tests/fastrestore_vsock.py <dir>/debian-13-x86_64.json [rounds] [--kernel]
Needs vhost-device-vsock (rust-vmm) on PATH, or VHOST_DEVICE_VSOCK=<binary>.

--kernel uses the kernel's vhost-vsock (/dev/vhost-vsock; the host connects with AF_VSOCK) instead.
That backend logs (VHOST_F_LOG_ALL): the restores must take the dirty-page path (the dirty log is
synced before the bitmap is read) and still verify.

A vhost-user backend is another process: it writes guest RAM (the RX buffers and the used rings)
through shared memory, outside TCG. vhost-device-vsock has no VHOST_USER_PROTOCOL_F_LOG_SHMFD (and
no VHOST_F_LOG_ALL), so none of those writes reach QEMU's dirty bitmap: a dirty-page restore would
leave them in RAM while it restores the device state from before them.

The image's root snapshot has no vsock device, so the guest is given RAM QEMU can share
(memory-backend-memfd under the machine's own RAM block name, so loadvm still finds its RAM) and a
vhost-user-vsock-pci device it discovers with a PCI rescan after the root revert. Then, per round:
fast snapshot (VM stopped) -> run; the host opens connections to the guest through the backend's
unix socket (the backend writes connection requests into the guest's RX buffers; the guest answers
with resets on TX) -> stop -> panda_fast_restore -> panda_fast_verify. Checks:
- every restore returns 0 and panda_fast_verify() == 0 afterwards;
- the guest changed RAM between snapshot and restore;
- the engine reports the device as unloggable and the restores compared every page (compare);
- the guest still answers on the console after the last restore, and the backend still reaches it.

Before the fix (engine without 0060 and the compare fallback) arming the snapshot's dirty logging
asked the backend to log: it refused VHOST_F_LOG_ALL, dropped the connection, and the next restore
aborted QEMU in vhost_dev_start.
"""
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time

from pandare2 import Panda
from pandare2.qcows_internal import Qcows

RAM_ID = {"x86_64": "pc.ram", "aarch64": "mach-virt.ram", "riscv64": "riscv_virt_board.ram"}
CID = 3
PORT = 1234


def host_connects(uds, n):
    """Open n connections to the guest's PORT through the backend; return the replies seen."""
    replies = []
    for _ in range(n):
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(3)
        try:
            s.connect(uds)
            s.sendall(f"CONNECT {PORT}\n".encode())
            try:
                replies.append(s.recv(64))
            except socket.timeout:
                replies.append(b"<timeout>")
        except OSError as e:
            replies.append(f"<{e}>".encode())
        finally:
            s.close()
    return replies


def host_connects_kernel(cid, n):
    """The same through the kernel's vhost-vsock: AF_VSOCK to the guest's CID."""
    replies = []
    for _ in range(n):
        s = socket.socket(socket.AF_VSOCK, socket.SOCK_STREAM)
        s.settimeout(3)
        try:
            s.connect((cid, PORT))
            replies.append(b"<connected>")
        except ConnectionResetError:
            replies.append(b"")     # the guest answered with a reset: no listener
        except OSError as e:
            replies.append(f"<{e}>".encode())
        finally:
            s.close()
    return replies


def main():
    kernel = "--kernel" in sys.argv
    args = [a for a in sys.argv[1:] if a != "--kernel"]
    image = args[0]
    rounds = int(args[1]) if len(args) > 1 else 3
    tmp = tempfile.mkdtemp(prefix="frvsock", dir=os.environ.get("TMPDIR", "/tmp"))
    q = Qcows.get_qcow_info(image)
    arch, mem = q.arch, q.default_mem
    backend = None
    if kernel:
        try:
            os.close(os.open("/dev/vhost-vsock", os.O_RDWR))
        except OSError as e:
            print(f"SKIP: /dev/vhost-vsock: {e}")
            sys.exit(2)
        cid = 1000 + os.getpid() % 100000
        connects = lambda n: host_connects_kernel(cid, n)  # noqa: E731
        extra = ["-device", f"vhost-vsock-pci,guest-cid={cid},addr=0x10"]
    else:
        vdv = os.environ.get("VHOST_DEVICE_VSOCK") or shutil.which("vhost-device-vsock")
        if not vdv:
            print("SKIP: no vhost-device-vsock")
            sys.exit(2)
        vhu, uds = f"{tmp}/vhu.sock", f"{tmp}/v.sock"
        backend = subprocess.Popen([vdv, "--guest-cid", str(CID), "--socket", vhu, "--uds-path", uds],
                                   stdout=open(f"{tmp}/backend.log", "w"), stderr=subprocess.STDOUT)
        for _ in range(100):
            if os.path.exists(vhu):
                break
            time.sleep(0.05)
        connects = lambda n: host_connects(uds, n)  # noqa: E731
        ram = RAM_ID[arch]
        # vhost-user needs RAM the backend can map; same RAM block name, so loadvm finds it
        extra = ["-object", f"memory-backend-memfd,id={ram},size={mem},share=on",
                 "-machine", f"memory-backend={ram}",
                 "-chardev", f"socket,id=vhu,path={vhu}",
                 "-device", "vhost-user-vsock-pci,chardev=vhu,addr=0x10"]
    # (a fixed slot: the image's own devices keep theirs, so the root snapshot still loads)
    panda = Panda(generic=image, extra_args=extra)
    lib = panda.libpanda
    st = {"results": [], "err": None, "console": None, "driver": None}
    ev = threading.Event()

    def info():
        return dict(kv.split("=", 1) for kv in panda.ffi.string(lib.panda_fast_info()).decode().split())

    def in_main_loop(fn):
        """Stop the VM, run fn in the main loop, continue; wait for it."""
        ev.clear()

        def run():
            try:
                fn()
            except Exception as e:  # noqa: BLE001 - reported as a failed check
                st["err"] = repr(e)
            lib.panda_cont()
            ev.set()
        panda.vm_stop()
        panda.queue_main_loop_wait_fn(run)
        ev.wait(120)

    def snap():
        if lib.panda_fast_snap() != 0:
            st["err"] = "panda_fast_snap failed"

    def restore():
        r = {"differ_before": lib.panda_fast_verify()}
        t = time.perf_counter()
        r["rc"] = lib.panda_fast_restore()
        r["ms"] = (time.perf_counter() - t) * 1000
        i = info()
        r["full"], r["copied"] = i.get("last_full"), int(i.get("last_pages", -1))
        r["mode"], r["unlogged"] = i.get("last_mode"), i.get("unlogged")
        r["differ_after"] = lib.panda_fast_verify()
        st["results"].append(r)
        print(f"FAST round {r}", flush=True)

    @panda.queue_blocking
    def drive():
        try:
            panda.revert_sync("root")
            st["driver"] = panda.run_serial_cmd(
                "echo 1 > /sys/bus/pci/rescan; sleep 2; ls /sys/bus/virtio/drivers/vmw_vsock_virtio_transport/",
                timeout=120)
            print(f"VSOCK driver: {st['driver']!r}", flush=True)
            in_main_loop(snap)
            for _ in range(rounds):
                st.setdefault("replies", []).append(connects(8))
                panda.run_serial_cmd("sleep 1", timeout=60)
                in_main_loop(restore)
            st["console"] = panda.run_serial_cmd("echo al''ive", timeout=60)
            st["replies_after"] = connects(2)
        except Exception as e:  # noqa: BLE001
            st["err"] = st["err"] or repr(e)
        panda.end_analysis()

    try:
        panda.run()
    finally:
        if backend:
            backend.terminate()
    print(f"VSOCK replies {st.get('replies')} after={st.get('replies_after')}")
    if backend:
        print("BACKEND LOG:", open(f"{tmp}/backend.log").read()[-2000:])
    res = [r for r in st["results"] if "rc" in r]
    ok = True

    def check(name, cond):
        nonlocal ok
        ok &= bool(cond)
        print(f"  {'PASS' if cond else 'FAIL'} {name}")

    for r in res:
        print(f"FAST round full={r['full']} differ_before={r['differ_before']} copied={r['copied']} "
              f"differ_after={r['differ_after']} restore_ms={r['ms']:.1f}")
    print(f"FAST info {panda.ffi.string(lib.panda_fast_info()).decode()}")
    check("the guest bound the vsock driver", st["driver"] and "virtio" in st["driver"])
    check("no error", st["err"] is None)
    if st["err"]:
        print("   ", st["err"])
    check(f"{rounds} restores ran", len(res) == rounds)
    check("every restore returned 0", res and all(r["rc"] == 0 for r in res))
    check("the guest changed RAM between snapshot and restore", res and all(r["differ_before"] > 0 for r in res))
    check("RAM equals the snapshot after every restore (panda_fast_verify == 0)",
          res and all(r["differ_after"] == 0 for r in res))
    if kernel:
        check("the kernel's vhost-vsock logs (unlogged=0)", res and all(r["unlogged"] == "0" for r in res))
        check("the restores took the dirty-page path (last_mode=dirty)",
              res and all(r["mode"] == "dirty" for r in res))
    else:
        check("the engine knows the vsock device can't log (unlogged=1)",
              res and all(r["unlogged"] == "1" for r in res))
        check("the restores compared every page (last_mode=compare)",
              res and all(r["mode"] == "compare" for r in res))
    check("the guest answers on the console after the last restore", st["console"] and "alive" in st["console"])
    check("the backend still reaches the guest after the restores (connections reset, no socket error)",
          st.get("replies_after") and all(not x.startswith(b"<") for x in st["replies_after"]))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
