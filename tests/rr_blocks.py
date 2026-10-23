"""Replay with block-exec callbacks that leave blocks at their start: the replay must not drift.

Run: panda-python tests/rr_blocks.py <dir>/debian-13-x86_64.json [workdir]

Records `cat /etc/os-release; ls -R /usr/share/doc | wc -l` (as tests/rr.py does), then replays it
in fresh processes:
- "plain": no block-exec callback;
- "invalidate": before/after_block_exec on, and before_block_exec_invalidate_opt returning true at
  every 997th ask (taint2-style retranslation);
- "break": before/after/start_block_exec on, start_block_exec calling panda_do_break_exec() at
  every 997th block (it leaves the block at its start; the same block starts again).
The block-exec callbacks are on for the first BLOCKS blocks of the replay, then turned off (Python
callbacks on every block of a whole replay would take too long); hundreds of exits happen in that
window. Under icount, leaving a block at its start must give back the instructions its prologue
already counted, or every exit counts instructions that never ran and the replay drifts from the
log. Each replay must reach the recording's end and see what the recording saw: the asid_changed
sequence, the openat of /etc/os-release, cat's output on the serial port, the instruction count at
the last exception.
"""
import json
import os
import subprocess
import sys
import tempfile

from pandare2 import Panda

EVERY = 997
BLOCKS = 300_000
CMD = "cat /etc/os-release; ls -R /usr/share/doc | wc -l"


def child(mode, image, name, out):
    panda = Panda(generic=image, rr=True)
    seen = {"asids": [], "openat": 0, "icount": None, "exits": 0, "blocks": 0}

    @panda.cb_asid_changed
    def asid_changed(cpu, old, new):
        if panda.libpanda.rr_on():
            seen["asids"].append(new)
        return False

    @panda.ppp("syscalls2", "on_sys_openat_enter")
    def openat(cpu, pc, dfd, filename, flags, *mode_):
        if not panda.libpanda.rr_on():
            return
        try:
            if panda.read_str(cpu, filename) == "/etc/os-release":
                seen["openat"] += 1
        except ValueError:
            pass

    @panda.cb_before_handle_exception
    def exc(cpu, index):
        if panda.libpanda.rr_on():
            seen["icount"] = panda.rr_get_guest_instr_count()
        return index

    if mode in ("invalidate", "break"):
        names = ["bbe", "abe"] + (["inv"] if mode == "invalidate" else ["sbe"])
        state = {"asks": 0, "open": None, "pairs_bad": 0}

        def window_done():
            for n in names:
                panda.disable_callback(n)

        @panda.cb_before_block_exec(name="bbe")
        def bbe(cpu, tb):
            if state["open"] is not None:
                state["pairs_bad"] += 1
            state["open"] = tb.pc
            seen["blocks"] += 1
            if seen["blocks"] == BLOCKS:
                window_done()

        @panda.cb_after_block_exec(name="abe")
        def abe(cpu, tb, code):
            if state["open"] != tb.pc:
                state["pairs_bad"] += 1
            state["open"] = None

        if mode == "invalidate":
            @panda.cb_before_block_exec_invalidate_opt(name="inv")
            def inv(cpu, tb):
                state["asks"] += 1
                if state["asks"] % EVERY == 0:
                    seen["exits"] += 1
                    return True
                return False
        else:
            @panda.cb_start_block_exec(name="sbe")
            def sbe(cpu, tb):
                state["asks"] += 1
                if state["asks"] % EVERY == 0:
                    seen["exits"] += 1
                    panda.libpanda.panda_do_break_exec()

    if mode == "record":
        @panda.queue_blocking
        def drive():
            panda.revert_sync("root")
            panda.record_cmd(CMD, recording_name=name, snap_name=None, timeout=600)
            panda.end_analysis()
        panda.run()
    else:
        panda.run_replay(name)
        seen["serial"] = b"PRETTY_NAME" in panda.replay_serial
        if mode in ("invalidate", "break"):
            seen["pairs_bad"] = state["pairs_bad"]
    with open(out, "w") as f:
        json.dump(seen, f)


def main():
    image = sys.argv[1]
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix="rrblocks-")
    name = os.path.join(work, "cat")
    runs = {}
    for mode in ["record", "plain", "invalidate", "break"]:
        out = os.path.join(work, mode + ".json")
        try:
            rc = subprocess.run([sys.executable, "-u", __file__, "--child", mode, image, name, out],
                                timeout=1800).returncode
        except subprocess.TimeoutExpired:
            rc = "timeout"
        runs[mode] = json.load(open(out)) if rc == 0 and os.path.exists(out) else None
        r = runs[mode]
        print(f"{mode}: rc={rc} " + (f"asids={len(r['asids'])} openat={r['openat']} icount={r['icount']} "
                                     f"blocks={r['blocks']} exits={r['exits']}" if r else "no result"),
              flush=True)
    rec, plain = runs["record"], runs["plain"]
    checks = {"recording made, saw the openat": rec is not None and rec["openat"] == 1,
              "plain replay ran to the end": plain is not None and plain["icount"] is not None}
    for mode in ["invalidate", "break"]:
        r = runs[mode]
        checks[f"[{mode}] replay ran, with {BLOCKS} blocks' callbacks and exits at block start"] = (
            r is not None and r["blocks"] == BLOCKS and r["exits"] >= 100)
        checks[f"[{mode}] before/after_block_exec pair up"] = r is not None and r.get("pairs_bad") == 0
        checks[f"[{mode}] asid_changed sequence matches the recording"] = (
            r is not None and rec is not None and r["asids"] == rec["asids"])
        checks[f"[{mode}] openat and cat's output seen again"] = (
            r is not None and r["openat"] == 1 and r.get("serial"))
        checks[f"[{mode}] instruction count at the last exception matches the plain replay"] = (
            r is not None and plain is not None and r["icount"] == plain["icount"])
    for k, ok in checks.items():
        print(f"  {'PASS' if ok else 'FAIL'} {k}")
    sys.exit(0 if all(checks.values()) else 1)


if __name__ == "__main__":
    if sys.argv[1] == "--child":
        child(*sys.argv[2:6])
    else:
        main()
