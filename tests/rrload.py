"""Snapshot loads and a recording: refused while recording, and the recording replays both ways.

Run: panda-python tests/rrload.py <dir>/debian-13-x86_64.json [workdir]

A recording is the log plus its start snapshot; a load during the recording is not in the log, so
its replay could not repeat it (engine 0073). The recording process reverts before
x-rr-begin-record (allowed), then, while recording, tries a loadvm (Panda.revert_sync), a
panda_revert from the main loop and a savevm that would replace the start snapshot rr-<name>: each
must be refused with the VM still running (a command after them runs), and the recording must
replay with x-rr-begin-replay (Panda.run_replay) and print both commands' output again.

The same recording is then replayed in a fresh process through QEMU's startup path, -icount
rr=replay,rrfile=,rrsnapshot=rr-<name>, which cannot replay a recording begun at run time (its log
has nothing of machine init, nor of the snapshot load's block requests: before 0141 the x86 load
hung for ever in blkreplay's flush). It must stop at once with an error that says to use
x-rr-begin-replay: x86 when it loads the start snapshot (marked as x-rr-begin-record's),
aarch64/riscv64 virt already at machine init (the fdt rng-seed's random draw).
"""
import json
import os
import subprocess
import sys
import tempfile
import threading

from pandare2 import Panda

LOAD_REFUSED = "does not allow loading a snapshot while recording"
SAVE_REFUSED = "does not allow replacing snapshot"
STARTUP_HINT = "x-rr-begin-replay"


def child(mode, image, name, out):
    base = os.path.basename(name)
    extra = None
    if mode == "startup":
        extra = ["-icount", f"shift=auto,rr=replay,rrfile={name}-rr-nondet.log,rrsnapshot=rr-{base}"]
    panda = Panda(generic=image, rr=True, extra_args=extra)
    lib = panda.libpanda
    seen = {}

    if mode == "record":
        @panda.queue_blocking
        def drive():
            seen["revert_before"] = panda.revert_sync("root")
            panda.record(name)
            panda.wait_rr("record")
            seen["a"] = panda.run_serial_cmd("echo rrload-a-$((6*7))", timeout=120)
            seen["loadvm"] = panda.revert_sync("root")
            ev = threading.Event()

            def revert():
                seen["panda_revert"] = lib.panda_revert(panda.ffi.new("char[]", b"root"))
                ev.set()
            panda.queue_main_loop_wait_fn(revert)
            seen["revert_ran"] = ev.wait(120)
            seen["savevm_start"] = panda.run_monitor_cmd(f"savevm rr-{base}")
            seen["b"] = panda.run_serial_cmd("echo rrload-b-$((6*7))", timeout=120)
            seen["in_record"] = bool(lib.rr_in_record())
            panda.end_record()
            panda.wait_rr("off")
            panda.end_analysis()
        panda.run()
    elif mode == "replay":
        panda.run_replay(name)
        seen["serial"] = panda.replay_serial.decode(errors="replace")
    else:
        # startup replay: expected to exit during panda_init with the error; if it ran, record that
        panda.run()
        seen["ran"] = True
    with open(out, "w") as f:
        json.dump(seen, f)


def run_child(mode, image, name, out, log):
    with open(log, "w") as f:
        try:
            rc = subprocess.run([sys.executable, "-u", __file__, "--child", mode, image, name, out],
                                stdout=f, stderr=subprocess.STDOUT,
                                timeout=120 if mode == "startup" else 900).returncode
        except subprocess.TimeoutExpired:
            rc = "timeout"
    res = json.load(open(out)) if rc == 0 and os.path.exists(out) else None
    print(f"{mode}: rc={rc}" + ("" if res else " no result") + f" (log {log})")
    return rc, res, open(log, errors="replace").read()


def main():
    image = sys.argv[1]
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix="rrload-")
    name = os.path.join(work, "r")
    arch = json.load(open(image)).get("arch", os.path.basename(image))
    _, rec, _ = run_child("record", image, name, os.path.join(work, "record.json"),
                          os.path.join(work, "record.log"))
    _, rep, _ = run_child("replay", image, name, os.path.join(work, "replay.json"),
                          os.path.join(work, "replay.log"))
    src, start, slog = run_child("startup", image, name, os.path.join(work, "startup.json"),
                                 os.path.join(work, "startup.log"))
    rec = rec or {}
    marks = ("rrload-a-42", "rrload-b-42")
    checks = {
        "a revert before x-rr-begin-record succeeds": rec.get("revert_before", "x").strip() == "",
        "loadvm while recording is refused": LOAD_REFUSED in (rec.get("loadvm") or ""),
        "panda_revert while recording is refused (-1)": rec.get("panda_revert") == -1 and rec.get("revert_ran"),
        "savevm over the start snapshot while recording is refused": SAVE_REFUSED in (rec.get("savevm_start") or ""),
        "the guest runs on after the refusals, still recording":
            marks[0] in (rec.get("a") or "") and marks[1] in (rec.get("b") or "") and rec.get("in_record"),
        "the recording replays (x-rr-begin-replay) and prints both outputs again":
            rep is not None and all(m in rep["serial"] for m in marks),
    }
    checks[f"startup replay (-icount rr=replay,rrsnapshot=) is refused with the x-rr-begin-replay hint ({arch})"] = \
        src not in (0, "timeout") and STARTUP_HINT in slog
    for k, ok in checks.items():
        print(f"  {'PASS' if ok else 'FAIL'} {k}")
    sys.exit(0 if all(checks.values()) else 1)


if __name__ == "__main__":
    if sys.argv[1] == "--child":
        child(*sys.argv[2:6])
    else:
        main()
