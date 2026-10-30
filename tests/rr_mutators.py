"""Mutating callbacks across repeated transitions into and out of recording.

Run: panda-python tests/rr_mutators.py <dir>/debian-13-x86_64.json [workdir]

Recording and replay never refuse to start over callbacks: a mutator, or a callback without
rr support, stays registered and enabled but is skipped while a recording or replay is active,
and runs again once it ends. One process alternates live analysis and ROUNDS recordings with a
mutator and an observer registered throughout; during the first recording it also registers a
second mutator and re-enables a disabled one. Live, the mutators run; recording, they never run
(each counts any call made while rr_on()), the observer keeps running and begin warns; after
each recording they run again. Each recording then replays in a fresh process with the same
mutator registered: the replay starts, the mutator never runs, and the command's output returns.
"""
import json
import os
import subprocess
import sys
import tempfile

from pandare2 import Panda

from testlib import Checks

ROUNDS = 2
MARK = "rrmut-{}-42"  # the command computes 42, so its echo alone does not match


def counters(panda, counts, name, **opts):
    """A before_block_exec callback that counts its calls, and separately any made during record/replay.

    Every executed block: rarer events (asid_changed) may not fire within one short command."""
    @panda.cb_before_block_exec(name=name, **opts)
    def cb(cpu, tb):
        counts[name] = counts.get(name, 0) + 1
        if panda.libpanda.rr_on():
            counts[name + "_in_rr"] = counts.get(name + "_in_rr", 0) + 1
    return cb


def native_counter(panda, counts, name):
    """The same, registered in the engine with no pandare2 wrapper: tests the engine's own gate."""
    def fn(cpu, tb):
        counts[name] = counts.get(name, 0) + 1
        if panda.libpanda.rr_on():
            counts[name + "_in_rr"] = counts.get(name + "_in_rr", 0) + 1
    raw = panda.callback.before_block_exec(fn)
    cb = panda.ffi.new("panda_cb *", {"before_block_exec": raw})
    owner = panda.ffi.cast("void *", 0xfeed0002)
    assert panda.libpanda.panda_register_callback_modes_helper(
        owner, panda.libpanda.PANDA_CB_BEFORE_BLOCK_EXEC, cb, 7, False)
    panda._rr_mutators_refs = (raw, cb)  # retain cffi objects until the engine stops


def child(mode, image, name, out):
    # As rrcb: RRCB_HANDOFF=1 for fixtures whose snapshots contain handoff extensions.
    capabilities = None
    if os.environ.get("RRCB_HANDOFF") == "1":
        capabilities = ("analysis", "rr", "guest-hypercall", "handoff")
    panda = Panda(generic=image, rr=True, capabilities=capabilities)
    counts = {}
    seen = {"rounds": [], "counts": counts}
    counters(panda, counts, "mutator")
    counters(panda, counts, "observer", observe_only=True)
    native_counter(panda, counts, "native")

    if mode == "record":
        counters(panda, counts, "reenabled", enabled=False)

        def calls():
            return dict(counts)

        @panda.queue_blocking
        def drive():
            panda.revert_sync("root")
            for k in range(ROUNDS):
                r = {}
                before = calls()
                panda.run_serial_cmd("true", timeout=120)
                r["live"] = (before, calls())
                panda.record(f"{name}{k}")
                panda.wait_rr("record")
                start = calls()
                if k == 0:
                    counters(panda, counts, "late")
                    panda.enable_callback("reenabled")
                r["output"] = panda.run_serial_cmd(f"echo rrmut-{k}-$((6*7))", timeout=120)
                r["recording"] = (start, calls())
                panda.end_record()
                panda.wait_rr("off")
                after = calls()
                panda.run_serial_cmd("true", timeout=120)
                r["after"] = (after, calls())
                seen["rounds"].append(r)
            panda.end_analysis()
        panda.run()
    else:
        seen["skipped"] = panda._check_rr_observers()
        panda.run_replay(name)
        seen["replay_serial"] = panda.replay_serial.decode(errors="replace")
    with open(out, "w") as f:
        json.dump(seen, f)


def grew(pair, key):
    return pair[1].get(key, 0) > pair[0].get(key, 0)


def same(pair, key):
    return pair[1].get(key, 0) == pair[0].get(key, 0)


def main():
    image = sys.argv[1]
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix="rr-mutators-")
    os.makedirs(work, exist_ok=True)
    name = os.path.join(work, "m")
    checks = Checks(json.load(open(image))["arch"])
    runs, logs = {}, {}
    for mode, arg in [("record", name)] + [(f"replay{k}", f"{name}{k}") for k in range(ROUNDS)]:
        out = os.path.join(work, mode + ".json")
        try:
            proc = subprocess.run([sys.executable, "-u", __file__, "--child", mode.rstrip("0123456789"),
                                   image, arg, out], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                  timeout=900)
            rc, logs[mode] = proc.returncode, proc.stdout.decode(errors="replace")
        except subprocess.TimeoutExpired:
            rc, logs[mode] = "timeout", ""
        with open(os.path.join(work, mode + ".log"), "w") as f:
            f.write(logs[mode])
        checks.check(mode + " completes", rc == 0, rc)
        runs[mode] = json.load(open(out)) if rc == 0 and os.path.exists(out) else None
    rec = runs["record"] or {"rounds": [], "counts": {}}
    rounds = rec["rounds"]
    checks.check(f"{ROUNDS} recordings in one process", len(rounds) == ROUNDS, len(rounds))
    for k, r in enumerate(rounds):
        checks.check(f"round {k}: live mutator runs", grew(r["live"], "mutator"))
        checks.check(f"round {k}: mutator skipped while recording", same(r["recording"], "mutator"))
        checks.check(f"round {k}: native mutator runs live, skipped while recording, runs after",
                     grew(r["live"], "native") and same(r["recording"], "native") and grew(r["after"], "native"))
        checks.check(f"round {k}: observer runs while recording", grew(r["recording"], "observer"))
        checks.check(f"round {k}: recording's command ran", MARK.format(k) in (r["output"] or ""))
        checks.check(f"round {k}: mutator runs again after the recording", grew(r["after"], "mutator"))
    if rounds:
        checks.check("mutator registered during a recording waits for its end",
                     same(rounds[0]["recording"], "late") and grew(rounds[0]["after"], "late"))
        checks.check("mutator re-enabled during a recording waits for its end",
                     same(rounds[0]["recording"], "reenabled") and grew(rounds[0]["after"], "reenabled"))
    checks.check("no mutator call while recording",
                 not any(rec["counts"].get(c + "_in_rr", 0) for c in ("mutator", "native", "late", "reenabled")),
                 rec["counts"])
    record_log = logs.get("record", "")
    checks.check("begin warns about the skipped mutator",
                 "is skipped during recording" in record_log, record_log[-2000:])
    checks.check("late registration warns it waits for the end",
                 "will not run until the recording ends" in record_log)
    for k in range(ROUNDS):
        rep = runs[f"replay{k}"] or {}
        checks.check(f"replay {k}: starts with the mutator registered and names it",
                     "callback 'mutator'" in rep.get("skipped", []))
        checks.check(f"replay {k}: mutators never run",
                     not any(rep.get("counts", {}).get(c, 0) for c in ("mutator", "native")), rep.get("counts"))
        checks.check(f"replay {k}: observer runs", rep.get("counts", {}).get("observer", 0) > 0)
        checks.check(f"replay {k}: prints the command's output again",
                     MARK.format(k) in (rep.get("replay_serial") or ""))
    sys.exit(checks.report("RR_MUTATORS"))


if __name__ == "__main__":
    if sys.argv[1] == "--child":
        child(*sys.argv[2:6])
    else:
        main()
