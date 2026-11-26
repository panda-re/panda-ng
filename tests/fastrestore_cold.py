"""panda_fast_snap/panda_fast_restore on a machine that was booted cold: no loadvm, savevm or
migration before the first fast snapshot.

Run: panda-python tests/fastrestore_cold.py <arch> [restores]

The machine runs its firmware (no disk, no image, no snapshot). After some blocks the test stops
it, takes a fast snapshot, lets it run, and restores it several times, letting it run between
restores. Each restore must succeed (the device state the snapshot saved must load: its
"globalstate" section holds the run state the snapshot was taken in, which a cold-booted machine
has stored nowhere else) and leave RAM equal to the snapshot (panda_fast_verify() == 0), and the
machine must run again after it. The other fast restore tests start from an image's root snapshot,
whose loadvm already filled that section.
"""
import sys
import threading

from pandare2 import Panda
from testlib import Checks

PANDA_TEST = dict(area="engine", tier="harness", args=["{arch}"], timeout=300,
                  arches=["x86_64", "aarch64", "riscv64"])

arch = sys.argv[1] if len(sys.argv) > 1 else "x86_64"
restores = int(sys.argv[2]) if len(sys.argv) > 2 else 5
machine = {"aarch64": ["-M", "virt", "-cpu", "max"], "riscv64": ["-M", "virt"]}.get(arch, [])
panda = Panda(arch=arch, mem="256M", os_version="linux-64-generic",
              extra_args=machine + ["-display", "none", "-nodefaults"])
lib = panda.libpanda
checks = Checks(arch)
st = {"blocks": 0, "snap": None, "rcs": [], "verify": [], "ran": [], "mark": 0, "err": None}
done = threading.Event()
SNAP_AT = 20000


def stop_then(fn):
    panda.vm_stop()
    panda.queue_main_loop_wait_fn(fn)


def snap():
    try:
        st["snap"] = lib.panda_fast_snap()
        st["snap_verify"] = lib.panda_fast_verify()
    except Exception as e:  # noqa: BLE001
        st["err"] = f"snap: {e!r}"
    print(f"FAST snap rc={st['snap']}", flush=True)
    st["mark"] = st["blocks"]
    lib.panda_cont()
    threading.Timer(0.5, stop_then, (restore,)).start()


def restore():
    try:
        st["ran"].append(st["blocks"] > st["mark"])
        rc = lib.panda_fast_restore()
        st["rcs"].append(rc)
        st["verify"].append(lib.panda_fast_verify() if rc == 0 else None)
    except Exception as e:  # noqa: BLE001
        st["err"] = f"restore {len(st['rcs'])}: {e!r}"
        rc = -1
    print(f"FAST restore {len(st['rcs'])} rc={rc}", flush=True)
    st["mark"] = st["blocks"]
    if rc != 0 or st["err"] or len(st["rcs"]) >= restores:
        lib.panda_cont()
        threading.Timer(0.5, finish).start()
        return
    lib.panda_cont()
    threading.Timer(0.5, stop_then, (restore,)).start()


def finish():
    st["ran"].append(st["blocks"] > st["mark"])
    done.set()
    panda.end_analysis()


@panda.cb_before_block_exec(observe_only=True)
def bbe(cpu, tb):
    st["blocks"] += 1
    if st["blocks"] == SNAP_AT:
        stop_then(snap)


# The run must end even if a step hangs.
watchdog = threading.Timer(240, lambda: done.is_set() or (st.__setitem__("err", "timed out"),
                                                         panda.end_analysis()))
watchdog.daemon = True
watchdog.start()
panda.run()

checks.check("no error", st["err"] is None, st["err"])
checks.check("snapshot taken", st["snap"] == 0 and st.get("snap_verify") == 0,
             f"rc={st['snap']} verify={st.get('snap_verify')}")
checks.check(f"{restores} restores succeed", st["rcs"] == [0] * restores, f"rcs={st['rcs']}")
checks.check("RAM equals the snapshot after each restore", st["verify"] == [0] * restores,
             f"verify={st['verify']}")
checks.check("runs between and after restores", len(st["ran"]) == restores + 1 and all(st["ran"]),
             f"ran={st['ran']}")
sys.exit(checks.report(f"fastrestore_cold {arch}"))
