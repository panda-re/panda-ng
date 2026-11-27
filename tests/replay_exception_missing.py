"""A replay whose recorded exception does not happen stops with a divergence error.

Run: panda-python tests/replay_exception_missing.py <x86_64 image json> [workdir]

Records a short command, then replays three copies of its log, each in a fresh process:
- unchanged: the replay reaches the end of the recording;
- the first "N instructions, then an exception" in the log moved one instruction earlier (N-1,
  and one more instruction after the exception, so that the total is unchanged): at N-1 the
  replay runs the next instruction on its own to raise the recorded exception, and that
  instruction (the one before the faulting one in the recording) completes without one. The
  replay must stop there with a divergence error naming the instruction count and the pc,
  rather than run the instruction uncounted and continue one instruction off the log;
- the same edit, again: the error is the same (instruction count and pc).
The original log is not changed. Copies keep the recording's basename, so the replays use the
recording's snapshot.
"""
import json
import os
from pathlib import Path
import re
import resource
import struct
import subprocess
import sys
import tempfile

from testlib import Checks

INSTR, EXCEPTION = 0, 2
DIAGNOSTIC = re.compile(r"replay: diverged at instruction (\d+): the recording has an exception "
                        r"here, but the instruction at pc (0x[0-9a-f]+) completed without one")


def child(mode, image, name, out):
    from pandare2 import Panda
    panda = Panda(generic=image, capabilities=["record"] if mode == "record" else None)
    seen = {"icount": None}
    if mode == "record":
        @panda.queue_blocking
        def drive():
            panda.revert_sync("root")
            panda.record_cmd("cat /etc/os-release > /dev/null; echo done", recording_name=name,
                             snap_name=None, timeout=300)
            panda.end_analysis()
        panda.run()
    else:
        panda.run_replay(name)
        seen["icount"] = panda.rr_get_guest_instr_count()
    Path(out).write_text(json.dumps(seen))


def first_exception(log):
    """(offset of the INSTR count before the first EXCEPTION, that count, offset of the INSTR
    count after it): the first INSTR n>1, EXCEPTION, INSTR sequence. Decodes the event kinds a
    short recording has (replay/replay-internal.h); anything else ends the search."""
    p = 12  # REPLAY_VERSION (4 bytes) and the header's 8
    prev = None  # (kind, payload offset, value) of the event before
    prev2 = None
    while p < len(log):
        kind = log[p]
        p += 1
        at = p
        if kind == INSTR:
            value = struct.unpack_from(">I", log, p)[0]
            p += 4
        elif kind in (1, EXCEPTION) or 11 <= kind <= 22 or 32 <= kind <= 40:
            value = None  # interrupt, exception, shutdown, checkpoint: no payload
        elif kind in (3, 4, 8, 10, 30, 31):
            value = None  # bh, bh oneshot, block, analysis effect (id), clock (value): 8 bytes
            p += 8
        elif kind == 28:
            value = None  # random: result, then a sized buffer
            p += 8 + struct.unpack_from(">I", log, p + 4)[0]
        elif kind == 23:
            value = None  # char write: two dwords
            p += 8
        elif kind == 7:
            value = None  # char read: device byte, then a sized buffer
            p += 5 + struct.unpack_from(">I", log, p + 1)[0]
        elif kind == 9:
            value = None  # network packet: device byte, flags, then a sized buffer
            p += 9 + struct.unpack_from(">I", log, p + 5)[0]
        elif kind == 29:
            value = None  # guest write: header, then its operations
            p += 1 + 2 + 4 + 1 + 8
            ops = struct.unpack_from(">I", log, p)[0]
            p += 4
            for _ in range(ops):
                p += 1 + 1 + 4 + 4 + 8
                p += 1 + 4 * log[p]
                p += 4 + struct.unpack_from(">I", log, p)[0]
        else:
            return None
        if (kind == INSTR and prev and prev[0] == EXCEPTION and prev2 and prev2[0] == INSTR
                and prev2[2] > 1):
            return prev2[1], prev2[2], at
        prev2, prev = prev, (kind, at, value)
    return None


def replay(image, copy, out):
    process = subprocess.run([sys.executable, "-u", __file__, "--child", "replay", image, str(copy),
                              str(out)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=600)
    return process.returncode, process.stdout.decode(errors="replace")


def main():
    image = str(Path(sys.argv[1]).resolve())
    work = Path(sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix="rr-excmiss-")).resolve()
    work.mkdir(parents=True, exist_ok=True)
    checks = Checks("x86_64")
    name = work / "excmiss"
    rc = subprocess.run([sys.executable, "-u", __file__, "--child", "record", image, str(name),
                         str(work / "record.json")], timeout=600).returncode
    log_path = Path(str(name) + "-rr-nondet.log")
    checks.check("recording made", rc == 0 and log_path.exists(), rc)
    if not log_path.exists():
        sys.exit(checks.report("REPLAY_EXCEPTION_MISSING work=" + str(work)))
    original = log_path.read_bytes()
    found = first_exception(original)
    checks.check("the log has an exception after more than one instruction", found is not None)
    if found is None:
        sys.exit(checks.report("REPLAY_EXCEPTION_MISSING work=" + str(work)))
    before, count, after = found
    edited = bytearray(original)
    struct.pack_into(">I", edited, before, count - 1)
    struct.pack_into(">I", edited, after, struct.unpack_from(">I", original, after)[0] + 1)

    # The divergence aborts through the replay's sync-error path: no core dumps.
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    results = {}
    for variant, data in (("unchanged", original), ("moved", bytes(edited)), ("moved-again", bytes(edited))):
        directory = work / variant
        directory.mkdir(exist_ok=True)
        copy = directory / name.name
        Path(str(copy) + "-rr-nondet.log").write_bytes(data)
        try:
            rc, output = replay(image, copy, directory / "result.json")
        except subprocess.TimeoutExpired as error:
            rc, output = "timeout", (error.stdout or b"").decode(errors="replace")
        (directory / "run.log").write_text(output)
        results[variant] = (rc, output, DIAGNOSTIC.search(output))
        print(f"{variant}: rc={rc}")

    rc, output, diag = results["unchanged"]
    result = work / "unchanged" / "result.json"
    checks.check("unchanged log: the replay reaches the recording's end",
                 rc == 0 and diag is None and result.exists()
                 and json.loads(result.read_text())["icount"] is not None,
                 {"rc": rc, "output": output[-1500:]})
    rc, output, diag = results["moved"]
    checks.check("moved exception: the replay stops with the divergence error",
                 rc not in (0, "timeout") and diag is not None,
                 {"rc": rc, "output": output[-1500:]})
    checks.check("moved exception: no replay result", not (work / "moved" / "result.json").exists())
    again = results["moved-again"][2]
    checks.check("moved exception: the same instruction count and pc again",
                 diag is not None and again is not None and diag.groups() == again.groups(),
                 [d.groups() if d else None for d in (diag, again)])
    checks.check("original log unchanged", log_path.read_bytes() == original)
    sys.exit(checks.report("REPLAY_EXCEPTION_MISSING work=" + str(work)))


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--child":
        child(*sys.argv[2:6])
    else:
        main()
