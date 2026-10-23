"""Record/replay through pandare2 on an image built by pandare2.mkimage.

Run: panda-python tests/rr.py <dir>/debian-13-x86_64.json [workdir]

Records `cat /etc/os-release; ls -R /usr/share/doc | wc -l` (disk reads) with Panda.record_cmd in one process, then replays it twice, each in
a fresh process with the same command line. Each process reports what PANDA saw while recording or
replaying (asid_changed values, the openat of /etc/os-release, the instruction count at the end);
the replays must match the recording and each other.
"""
import json
import os
import subprocess
import sys
import tempfile

from pandare2 import Panda


def child(mode, image, name, out):
    panda = Panda(generic=image, rr=True)
    seen = {"asids": [], "openat": 0, "icount": None}

    @panda.cb_asid_changed
    def asid_changed(cpu, old, new):
        if panda.libpanda.rr_on():
            seen["asids"].append(new)
        return False

    @panda.ppp("syscalls2", "on_sys_openat_enter")
    def openat(cpu, pc, dfd, filename, flags, *mode):
        if not panda.libpanda.rr_on():
            return
        try:
            if panda.read_str(cpu, filename) == "/etc/os-release":
                seen["openat"] += 1
        except ValueError:
            pass

    # The log ends at the recording's end: count instructions as the replay reaches it.
    @panda.cb_before_handle_exception
    def exc(cpu, index):
        if panda.libpanda.rr_on():
            seen["icount"] = panda.rr_get_guest_instr_count()
        return index

    if mode == "record":
        @panda.queue_blocking
        def drive():
            panda.revert_sync("root")
            # ls -R reads directories the snapshot has not cached: disk I/O while recording.
            panda.record_cmd("cat /etc/os-release; ls -R /usr/share/doc | wc -l", recording_name=name,
                             snap_name=None, timeout=600)
            seen["output"] = "recorded"
            panda.end_analysis()
        panda.run()
    else:
        panda.run_replay(name)
        seen["serial"] = b"PRETTY_NAME" in panda.replay_serial
    with open(out, "w") as f:
        json.dump(seen, f)


def main():
    image = sys.argv[1]
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix="rrtest-")
    name = os.path.join(work, "cat")
    runs = {}
    for mode in ["record", "replay1", "replay2"]:
        out = os.path.join(work, mode + ".json")
        try:
            rc = subprocess.run([sys.executable, "-u", __file__, "--child", mode.rstrip("12"), image, name, out],
                                timeout=600).returncode
        except subprocess.TimeoutExpired:
            rc = "timeout"
        runs[mode] = json.load(open(out)) if rc == 0 and os.path.exists(out) else None
        print(f"{mode}: rc={rc} "
              + (f"asids={len(runs[mode]['asids'])} openat={runs[mode]['openat']} "
                 f"icount={runs[mode]['icount']}" if runs[mode] else "no result"))
    rec, r1, r2 = runs["record"], runs["replay1"], runs["replay2"]
    log = name + "-rr-nondet.log"
    checks = {
        "recording made (log written)": rec is not None and os.path.getsize(log) > 0 if os.path.exists(log) else False,
        "recording saw cat open /etc/os-release": rec is not None and rec["openat"] == 1,
        "replays ran (to the recording's end)": r1 is not None and r2 is not None
        and r1["icount"] is not None and r2["icount"] is not None,
        "replay sees the openat": r1 is not None and r1["openat"] == 1,
        "replay writes cat's output to the serial port again": r1 is not None and r1.get("serial"),
        "replay asid_changed sequence matches the recording": r1 is not None and rec is not None
            and r1["asids"] == rec["asids"],
        "replays agree (asids, openat, instruction count)": r1 is not None and r2 is not None
            and r1["asids"] == r2["asids"] and r1["openat"] == r2["openat"] and r1["icount"] == r2["icount"],
    }
    for k, ok in checks.items():
        print(f"  {'PASS' if ok else 'FAIL'} {k}")
    sys.exit(0 if all(checks.values()) else 1)


if __name__ == "__main__":
    if sys.argv[1] == "--child":
        child(*sys.argv[2:6])
    else:
        main()
