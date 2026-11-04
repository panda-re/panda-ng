"""syscall.enter and syscall.return from a C plugin, on a busybox guest (x86_64).

Run: panda-python tests/syscall_events.py <dir>/busybox-x86_64.json <plugin dir> [skip|idle]
  <plugin dir> holds libpanda-scevents_x86_64-softmmu.so (tests/plugins/scevents/scevents.c:
  cc -shared -fPIC -I engine/src/include).

Default: the plugin's observer watches uname (63) at entry and return. The guest runs `uname` N
times (busybox runs it in the shell's own process: one call each). Checks that the observer saw
exactly N entries and N returns of that number and no other, that each return came with the
instance data its entry stored (the call id), and that its own skip was refused with a reason.

skip: a mutator also skips getppid (110) with 4242; the guest's `sh -c 'echo $PPID'` prints
4242, and the observer (after the mutator) sees the skipped entry and its return with that value.

bridge: as the default, with a QEMU plugin on the syscall-event bridge (tests/plugins/scbridge,
libscbridge.so in <plugin dir>) watching the same number: both consumers see every call.

idle: nothing subscribed; prints the host time of a syscall-heavy guest loop (dd with bs=1), for
comparing engines (PANDA_LIB) by hand. idle1: the same with the observer on uname loaded (the
loop's calls are not watched).
"""
import ctypes
import os
import sys
import time

from pandare2 import Panda

image, plugdir = sys.argv[1], sys.argv[2]
mode = sys.argv[3] if len(sys.argv) > 3 else "observe"
N = 40
if mode == "skip":
    os.environ["SCEVENTS_NUM"] = "110"
    os.environ["SCEVENTS_SKIP"] = "4242"

BRIDGE = os.path.join(plugdir, "libscbridge.so")
extra = ["-plugin", BRIDGE] if mode == "bridge" else []
# the busybox images are snapshotted with rr=True (an rr-ready launch), and restore with it
panda = (Panda(generic=image, rr=True, plugin_path=plugdir, extra_args=extra) if mode != "idle"
         else Panda(generic=image, rr=True))
lib = None
if mode != "idle":  # idle1: the observer on uname, timing calls it does not watch
    panda.load_plugin("scevents")
    lib = ctypes.CDLL(os.path.join(plugdir, "libpanda-scevents_x86_64-softmmu.so"))
    lib.scevents_get.restype = ctypes.c_uint64
    lib.scevents_refusal.restype = ctypes.c_char_p
ENTERS, RETURNS, PAIRED, OTHER, SKIPPED_SEEN, SKIP_RET_OK, OBS_REFUSED, MUT_SKIPS, RESTARTS = range(9)
out = {}


@panda.queue_blocking
def run():
    panda.revert_sync("root")
    if mode in ("observe", "bridge"):
        br = None
        if mode == "bridge":
            br = ctypes.CDLL(BRIDGE)
            br.scbridge_get.restype = ctypes.c_uint64
            bbase = [br.scbridge_get(i) for i in range(2)]
        base = [lib.scevents_get(i) for i in range(9)]
        out["guest"] = panda.run_serial_cmd(f"for i in $(seq {N}); do uname >/dev/null; done; echo done")
        out["counts"] = [lib.scevents_get(i) - base[i] for i in range(9)]
        if br:
            out["bridge"] = [br.scbridge_get(i) - bbase[i] for i in range(2)]
    elif mode == "skip":
        out["guest"] = panda.run_serial_cmd("sh -c 'echo PPID=$PPID'")
        out["counts"] = [lib.scevents_get(i) for i in range(9)]
    else:
        panda.run_serial_cmd("true")
        times = []
        for _ in range(3):
            t = time.time()
            panda.run_serial_cmd("dd if=/dev/zero of=/dev/null bs=1 count=100000 2>/dev/null; echo ok")
            times.append(time.time() - t)
        out["times"] = times
    panda.end_analysis()


panda.run()
fails = 0


def check(name, ok, detail=""):
    global fails
    print(f"{'PASS' if ok else 'FAIL'}: {name} {detail}")
    fails += not ok


if mode.startswith("idle"):
    print(mode.upper(), os.environ.get("PANDA_LIB", "?"), " ".join(f"{t:.3f}" for t in out["times"]))
    sys.exit(0)
c = out["counts"]
print("counts", c, "refusal:", lib.scevents_refusal())
if mode in ("observe", "bridge"):
    if mode == "bridge":
        check("the QEMU-plugin bridge beside the events: entries and returns",
              out["bridge"] == [N, N], out["bridge"])
    check("entries", c[ENTERS] == N, c[ENTERS])
    check("returns", c[RETURNS] == N, c[RETURNS])
    check("instance data spans entry and return", c[PAIRED] == N, c[PAIRED])
    check("only the watched number", c[OTHER] == 0, c[OTHER])
    check("an observer's skip is refused with a reason", c[OBS_REFUSED] == N and
          b"observer" in (lib.scevents_refusal() or b""))
else:
    check("the guest got the skipped value", "PPID=4242" in out["guest"], repr(out["guest"]))
    check("mutator skipped", c[MUT_SKIPS] >= 1, c[MUT_SKIPS])
    check("observer saw the skipped entry", c[SKIPPED_SEEN] == c[MUT_SKIPS], c[SKIPPED_SEEN])
    check("observer saw the skipped return with its value", c[SKIP_RET_OK] == c[MUT_SKIPS],
          c[SKIP_RET_OK])
    check("instance data spans the skipped call", c[PAIRED] == c[ENTERS] == c[RETURNS], c[PAIRED])
sys.exit(1 if fails else 0)
