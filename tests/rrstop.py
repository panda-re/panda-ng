"""Record/replay switched on and off at run time, repeatedly, in one process.

Run: panda-python tests/rrstop.py <dir>/debian-13-x86_64.json [workdir]

x-rr-begin-record and x-rr-end-record switch replay_mode while vCPU threads exist (patches 0039,
0040). One process makes ROUNDS recordings back to back (begin, end, begin, ...); before each
begin it stops and resumes the VM from the main loop, which kicks the vCPU thread into the
replay-mutex window the mode switch used to race with (engine 0120/0121: an abort in
replay_mutex_unlock at begin, or a main loop that waits for ever at the second begin; 0122: the second begin registered the
replay vmstate again and exited on the duplicate). Each
recording runs a command; then every recording is replayed in a fresh process and must reach
its end and print its command's output again. After the last recording, a savevm must work
(before 0122, 0040 left the replay vmstate registered, whose pre_save read the closed log's
offset), and so must a loadvm of that recording's start snapshot (rr-<name>, which carries the
"replay" section: an earlier 0122 unregistered the section at end and broke this load), and the
guest runs on from it.
"""
import json
import os
import subprocess
import sys
import tempfile
import threading

from pandare2 import Panda

ROUNDS = 3
MARK = "rrstop-{}-42"  # the command computes 42, so its echo alone does not match


def child(mode, image, name, out):
    panda = Panda(generic=image, rr=True)
    lib = panda.libpanda
    seen = {"rounds": [], "replay_serial": None}
    ev = threading.Event()

    def stop_cont():
        ev.clear()

        def run():
            lib.panda_cont()
            ev.set()
        panda.vm_stop()
        panda.queue_main_loop_wait_fn(run)
        return ev.wait(120)

    if mode == "record":
        @panda.queue_blocking
        def drive():
            panda.revert_sync("root")
            for k in range(ROUNDS):
                r = {"stop_cont": stop_cont()}
                panda.record(f"{name}{k}")
                panda.wait_rr("record")
                r["output"] = panda.run_serial_cmd(f"echo rrstop-{k}-$((6*7))", timeout=120)
                panda.end_record()
                panda.wait_rr("off")
                seen["rounds"].append(r)
            # a snapshot after the last recording (no log to point into), then the guest still runs
            seen["savevm"] = panda.run_monitor_cmd("savevm rrstop-after")
            panda.run_monitor_cmd("delvm rrstop-after")
            seen["after"] = panda.run_serial_cmd("echo after-$((6*7))", timeout=120)
            # the last recording's start snapshot loads after the end, and the guest runs on from it
            seen["loadvm"] = panda.run_monitor_cmd(f"loadvm rr-{os.path.basename(name)}{ROUNDS - 1}")
            seen["after_load"] = panda.run_serial_cmd("echo loaded-$((6*7))", timeout=120)
            panda.end_analysis()
        panda.run()
    else:
        panda.run_replay(name)
        seen["replay_serial"] = panda.replay_serial.decode(errors="replace")
    with open(out, "w") as f:
        json.dump(seen, f)


def main():
    image = sys.argv[1]
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix="rrstop-")
    name = os.path.join(work, "r")
    runs = {}
    for mode, arg in [("record", name)] + [(f"replay{k}", f"{name}{k}") for k in range(ROUNDS)]:
        out = os.path.join(work, mode + ".json")
        try:
            rc = subprocess.run([sys.executable, "-u", __file__, "--child", mode.rstrip("0123456789"), image, arg, out],
                                timeout=900).returncode
        except subprocess.TimeoutExpired:
            rc = "timeout"
        runs[mode] = json.load(open(out)) if rc == 0 and os.path.exists(out) else None
        print(f"{mode}: rc={rc}" + ("" if runs[mode] else " no result"))
    rec = runs["record"]
    reps = [runs[f"replay{k}"] for k in range(ROUNDS)]
    rounds = rec["rounds"] if rec else []
    logs = [f"{name}{k}-rr-nondet.log" for k in range(ROUNDS)]
    checks = {
        f"{ROUNDS} recordings in one process, each after a stop+cont": len(rounds) == ROUNDS
            and all(r["stop_cont"] for r in rounds),
        "each recording's command ran": len(rounds) == ROUNDS
            and all(MARK.format(k) in (r["output"] or "") for k, r in enumerate(rounds)),
        "savevm after the last recording succeeds, and the guest runs on": rec is not None
            and not (rec.get("savevm") or "").strip() and "after-42" in (rec.get("after") or ""),
        "loadvm of the last recording's start snapshot after its end, and the guest runs on": rec is not None
            and not (rec.get("loadvm") or "").strip() and "loaded-42" in (rec.get("after_load") or ""),
        "each recording wrote a log": all(os.path.exists(p) and os.path.getsize(p) > 0 for p in logs),
        "every recording replays and prints its command's output again": all(
            rep is not None and MARK.format(k) in (rep["replay_serial"] or "") for k, rep in enumerate(reps)),
    }
    for k, ok in checks.items():
        print(f"  {'PASS' if ok else 'FAIL'} {k}")
    sys.exit(0 if all(checks.values()) else 1)


if __name__ == "__main__":
    if sys.argv[1] == "--child":
        child(*sys.argv[2:6])
    else:
        main()
