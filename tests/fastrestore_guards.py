"""panda_fast_snap/panda_fast_restore refuse where they would be wrong: under record/replay, and
without TCG.

Run: panda-python tests/fastrestore_guards.py <dir>/debian-13-x86_64.json [rr|notcg ...]

rr (review M3): with rr=True, a fast snapshot is taken before recording (allowed: nothing is
recorded yet), then a recording starts. While it records, panda_fast_snap and panda_fast_restore
must return -1 (a restore would rewrite RAM and device state outside the log; on x86 its device
reload would run on 0028's replay-mutex release path). After the recording ends, a fast restore
works again and verifies (x-rr-begin-record saved and reloaded a snapshot, so it copies all RAM:
last_why=precopy or loadvm). Then the recording is replayed in a fresh process, where both must return -1.

notcg (review M4): the same image under KVM (host-arch images only; needs /dev/kvm): both
must return -1. Skipped when /dev/kvm can't be opened or the image is not the host's arch.
"""
import os
import platform
import subprocess
import sys
import tempfile
import threading

from pandare2 import Panda


def child_replay(image, name, out):
    panda = Panda(generic=image, rr=True)
    res = {}

    def try_both():
        res["snap"] = panda.libpanda.panda_fast_snap()
        res["restore"] = panda.libpanda.panda_fast_restore()

    @panda.cb_asid_changed(observe_only=True)
    def asid_changed(cpu, old, new):
        if "snap" not in res and panda.libpanda.rr_on():
            panda.queue_main_loop_wait_fn(try_both)
        return False

    panda.run_replay(name)
    with open(out, "w") as f:
        f.write(repr(res))


def child_notcg(image, out):
    # --8<-- [start:kvm]
    panda = Panda(generic=image, accel="kvm")
    res = {}
    done = threading.Event()

    def try_both():
        res["snap"] = panda.libpanda.panda_fast_snap()
        res["restore"] = panda.libpanda.panda_fast_restore()
        done.set()
    # --8<-- [end:kvm]

    @panda.queue_blocking
    def drive():
        panda.queue_main_loop_wait_fn(try_both)
        done.wait(60)
        panda.end_analysis()

    panda.run()
    with open(out, "w") as f:
        f.write(repr(res))


def run_child(args, timeout=900):
    out = tempfile.mktemp(prefix="frguard", dir=os.environ.get("TMPDIR", "/tmp"))
    try:
        rc = subprocess.run([sys.executable, "-u", __file__, "--child", *args, out], timeout=timeout).returncode
    except subprocess.TimeoutExpired:
        rc = "timeout"
    res = eval(open(out).read()) if os.path.exists(out) else None  # noqa: S307 - our own repr
    return rc, res


def test_rr(image, check):
    work = tempfile.mkdtemp(prefix="frguard-rr", dir=os.environ.get("TMPDIR", "/tmp"))
    name = os.path.join(work, "rec")
    out = os.path.join(work, "record.txt")
    rc = subprocess.run([sys.executable, "-u", __file__, "--child", "record", image, name, out],
                        timeout=900).returncode
    res = eval(open(out).read()) if os.path.exists(out) else {}  # noqa: S307 - our own repr
    print(f"GUARD rr record rc={rc} {res}")
    check("a fast snapshot before recording", res.get("snap_before") == 0)
    check("panda_fast_snap refuses while recording", res.get("snap_rec") == -1)
    check("panda_fast_restore refuses while recording", res.get("restore_rec") == -1)
    check("after the recording a fast restore works, copies all RAM and verifies (recording began with "
          "a savevm and a loadvm: last_why=precopy or loadvm)",
          res.get("restore_after") == 0 and res.get("mode_after") == "full"
          and res.get("why_after") in ("precopy", "loadvm") and res.get("verify_after") == 0)
    check("the guest runs on after it", res.get("alive"))
    rc, res = run_child(["replay", image, name])
    print(f"GUARD rr replay rc={rc} {res}")
    check("panda_fast_snap and panda_fast_restore refuse while replaying",
          res and res.get("snap") == -1 and res.get("restore") == -1)


def child_record(image, name, out):
    panda = Panda(generic=image, rr=True)
    lib = panda.libpanda
    res = {}
    ev = threading.Event()

    def in_main_loop(fn):
        print(f"GUARD step {getattr(fn, '__name__', fn)}", flush=True)
        ev.clear()

        def run():
            try:
                fn()
            except Exception as e:  # noqa: BLE001 - reported
                res["err"] = res.get("err") or repr(e)
            lib.panda_cont()
            ev.set()
        panda.vm_stop()
        panda.queue_main_loop_wait_fn(run)
        ev.wait(120)

    def info():
        return dict(kv.split("=", 1) for kv in panda.ffi.string(lib.panda_fast_info()).decode().split())

    def during():
        res["snap_rec"] = lib.panda_fast_snap()
        res["restore_rec"] = lib.panda_fast_restore()

    def after():
        res["restore_after"] = lib.panda_fast_restore()
        i = info()
        res["mode_after"] = i.get("last_mode", "full" if i.get("last_full") == "1" else "dirty")
        res["why_after"] = i.get("last_why")
        res["verify_after"] = lib.panda_fast_verify()

    @panda.queue_blocking
    def drive():
        try:
            panda.revert_sync("root")
            def before():
                res["snap_before"] = lib.panda_fast_snap()
            in_main_loop(before)
            panda.record(name)
            panda.wait_rr("record")
            panda.run_serial_cmd("cat /etc/os-release", timeout=120)
            in_main_loop(during)
            panda.run_serial_cmd("uname -a", timeout=120)
            panda.end_record()
            panda.wait_rr("off")
            in_main_loop(after)
            res["alive"] = "alive" in panda.run_serial_cmd("echo al''ive", timeout=60)
        except Exception as e:  # noqa: BLE001
            res["err"] = repr(e)
        panda.end_analysis()

    panda.run()
    with open(out, "w") as f:
        f.write(repr(res))


def test_notcg(image, check):
    arch = {"x86_64": "x86_64", "aarch64": "aarch64", "arm64": "aarch64"}.get(platform.machine())
    from pandare2.qcows_internal import Qcows
    if Qcows.get_qcow_info(image).arch != arch:
        print("GUARD notcg: SKIP (image is not the host's arch)")
        return
    try:
        os.close(os.open("/dev/kvm", os.O_RDWR))
    except OSError as e:
        print(f"GUARD notcg: SKIP (/dev/kvm: {e})")
        return
    rc, res = run_child(["notcg", image])
    print(f"GUARD notcg rc={rc} {res}")
    check("without TCG (accel=\"kvm\") panda_fast_snap and panda_fast_restore refuse",
          res and res.get("snap") == -1 and res.get("restore") == -1)


def main():
    if sys.argv[1] == "--child":
        kind = sys.argv[2]
        if kind == "record":
            child_record(*sys.argv[3:6])
        elif kind == "replay":
            child_replay(*sys.argv[3:6])
        else:
            child_notcg(*sys.argv[3:5])
        return
    image = sys.argv[1]
    which = sys.argv[2:] or ["rr", "notcg"]
    ok = True

    def check(name, cond):
        nonlocal ok
        ok &= bool(cond)
        print(f"  {'PASS' if cond else 'FAIL'} {name}")

    if "rr" in which:
        test_rr(image, check)
    if "notcg" in which:
        test_notcg(image, check)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
