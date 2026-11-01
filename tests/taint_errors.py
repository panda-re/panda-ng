"""Taint mode's refusals: each says why, in the engine's log and as a RuntimeError from pandare2.

Run: panda-python tests/taint_errors.py <image>.json [workdir]   (any arch; mkimage or mkbusybox)

Each case runs in its own process:
- early:    taint_enable() before panda.run() has started the engine: "not running yet";
- no-cap:   replaying a recording without the "taint" capability: taint_enable and
            taint_label_ram raise "capability"; taint_enabled() stays False;
- live:     with the capability, from a callback while the guest runs live (not a replay):
            taint_enable raises "replaying";
- conflict: replaying with the capability: label sets, then address mode, raises "both";
            taint itself still enables;
- accel:    the capability without TCG (-accel qtest): pandare2 refuses (taint needs rr, rr needs
            TCG), and qemu-system-<arch>, started directly, exits saying "needs TCG".
"""
import json
import os
import subprocess
import sys
import tempfile

from pandare2 import Panda

CMD = "echo taint-errors"


def attempt(fn):
    """None if fn() returned, else the RuntimeError's message."""
    try:
        fn()
        return None
    except RuntimeError as e:
        return str(e)


def child(case, image, name, out):
    seen = {}
    if case == "accel":
        # pandare2 refuses first (taint implies rr, which needs TCG); the engine's own check is
        # reached from its command line: qemu-system-<arch> next to the PANDA library.
        try:
            Panda(generic=image, rr=True, capabilities=["analysis", "taint"],
                  extra_args=["-accel", "qtest"])
            seen["pandare2"] = None
        except ValueError as e:
            seen["pandare2"] = str(e)
        try:
            Panda(generic=image, capabilities=["analysis", "taint"])
            seen["no-rr"] = None
        except ValueError as e:
            seen["no-rr"] = str(e)
        panda = Panda(generic=image, rr=True, capabilities=["analysis", "taint"])
        lib = os.path.dirname(panda.libpanda_path)
        exe = next((p for p in (os.path.join(lib, f"qemu-system-{panda.arch_name}"),
                                os.path.join(lib, "..", "bin", f"qemu-system-{panda.arch_name}"))
                    if os.path.exists(p)), None)
        if exe:
            p = subprocess.run([exe, "-M", "none", "-accel", "qtest", "-display", "none",
                                "-analysis-capabilities", "taint"],
                               capture_output=True, text=True, timeout=60)
            seen["engine"] = [p.returncode, p.stderr[-500:]]
        with open(out, "w") as f:
            json.dump(seen, f)
        return
    caps = ["analysis", "guest-hypercall", "rr"] + ([] if case in ("record", "no-cap") else ["taint"])
    panda = Panda(generic=image, rr=True, capabilities=caps)

    if case == "early":
        seen["enable"] = attempt(panda.taint_enable)
    elif case == "record":
        @panda.queue_blocking
        def drive():
            panda.revert_sync("root")
            panda.record_cmd(CMD, recording_name=name, snap_name=None)
            panda.end_analysis()
        panda.run()
    elif case == "live":
        @panda.cb_start_block_exec(observe_only=True)
        def first(cpu, tb):
            seen["enable"] = attempt(panda.taint_enable)
            panda.disable_callback("first")
            panda.end_analysis()

        @panda.queue_blocking
        def drive():
            panda.revert_sync("root")
        panda.run()
    else:
        @panda.cb_start_block_exec(observe_only=True)
        def first(cpu, tb):
            if case == "no-cap":
                seen["enable"] = attempt(panda.taint_enable)
                seen["label"] = attempt(lambda: panda.taint_label_ram(0, 1))
                seen["enabled"] = panda.taint_enabled()
            else:
                seen["labels"] = attempt(panda.taint_enable_labels)
                seen["address"] = attempt(panda.taint_address_mode)
                seen["enable"] = attempt(panda.taint_enable)
                seen["enabled"] = panda.taint_enabled()
            panda.disable_callback("first")
        panda.run_replay(name)
    with open(out, "w") as f:
        json.dump(seen, f)


def main():
    image = sys.argv[1]
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix="taint-errors-")
    name = os.path.join(work, "rec")
    res = {}
    for case in ("early", "record", "no-cap", "live", "conflict", "accel"):
        out = os.path.join(work, f"{case}.json")
        p = subprocess.run([sys.executable, __file__, "--child", case, image, name, out],
                           capture_output=True, text=True)
        log = p.stdout + p.stderr
        with open(os.path.join(work, f"{case}.log"), "w") as f:
            f.write(log)
        if case != "record":
            if p.returncode:
                print(log[-3000:])
            res[case] = json.load(open(out)) if os.path.exists(out) else {}
        elif p.returncode:
            print(log[-3000:])
            sys.exit("recording failed")

    def has(msg, word):
        return msg is not None and word in msg

    e, n, l, c, a = (res[k] for k in ("early", "no-cap", "live", "conflict", "accel"))
    checks = [
        ("before panda.run(): taint_enable says the engine is not running", has(e.get("enable"), "not running"), e),
        ("no capability: taint_enable says so", has(n.get("enable"), "capability"), n),
        ("no capability: taint_label_ram says so", has(n.get("label"), "capability"), n),
        ("no capability: taint stays off", n.get("enabled") is False, n),
        ("live, not a replay: taint_enable says it needs a replay", has(l.get("enable"), "replaying"), l),
        ("label sets then address mode: refused, saying both can't be on",
         c.get("labels") is None and has(c.get("address"), "both"), c),
        ("after that refusal, taint still enables", c.get("enable") is None and c.get("enabled") is True, c),
        ("taint with another accelerator: pandare2 refuses", has(a.get("pandare2"), "accel"), a),
        ("taint without rr=True: pandare2 says it runs only in replay", has(a.get("no-rr"), "replay"), a),
    ]
    if "engine" in a:
        checks.append(("without TCG: the engine exits saying taint needs TCG",
                       a["engine"][0] != 0 and "needs TCG" in a["engine"][1], a))
    else:
        print("ok   skipped: no qemu-system binary next to the PANDA library")
    failed = 0
    for what, ok, detail in checks:
        print(f"{'ok  ' if ok else 'FAIL'} {what}" + ("" if ok else f": {detail}"))
        failed += not ok
    print(f"{len(checks) - failed}/{len(checks)} passed")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--child":
        child(*sys.argv[2:6])
    else:
        main()
