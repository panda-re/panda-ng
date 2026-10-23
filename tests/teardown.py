"""Callback lists change while vCPUs walk them: unregistering and unloading plugins mid-run.

Run: panda-python tests/teardown.py <dir>/debian-13-x86_64.json [seconds]

The engine's callback lists are walked by the vCPU on every callback while other threads add and
remove entries. This test changes them every way pandare2 can while a guest loop runs with
callbacks on every block:
- a callback that deletes itself on its first call, from the vCPU (others follow it in the list);
- a churn thread that registers 8 block callbacks, lets them run, and deletes them: half directly
  from its own thread, half queued to the main loop (vCPUs running);
- at the end, the C plugins (syscalls2, from a ppp callback) and the Python callbacks are unloaded
  from the main loop with the vCPU still running (Panda.unload_plugins, no stop first), then the
  run ends. That is what end_analysis did before pandare2 stopped the VM first.
Freed callback-list memory is poisoned (MALLOC_PERTURB_, set by re-exec), so a walk through a freed
entry crashes instead of reading stale bytes. Passes when the process exits normally with every
phase done.
"""
import json
import os
import random
import sys
import threading
import time

if os.environ.get("MALLOC_PERTURB_") is None:
    os.environ["MALLOC_PERTURB_"] = "165"
    os.execv(sys.executable, [sys.executable] + sys.argv)

from pandare2 import Panda  # noqa: E402

image = sys.argv[1]
seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 20
with open(image) as f:
    image_arch = json.load(f)["arch"]
# Python callbacks on every block: guest time from instructions so the loop isn't starved.
icount = os.environ.get("TEARDOWN_ICOUNT", "1") == "1"
panda = Panda(generic=image, extra_args=["-icount", "shift=6,sleep=off"] if icount else [])
st = {"self_deleted": 0, "after_self": 0, "churn_rounds": 0, "churn_calls": 0, "direct": 0,
      "queued": 0, "openat": 0, "unloaded": False, "err": None}
lock = threading.Lock()


@panda.ppp("syscalls2", "on_sys_openat_enter")
def openat(cpu, pc, dfd, filename, flags, *mode):
    st["openat"] += 1


def armed_self_delete():
    @panda.cb_start_block_exec(name="selfdel")
    def selfdel(cpu, tb):
        st["self_deleted"] += 1
        panda.delete_callback("selfdel")

    @panda.cb_start_block_exec(name="after_self")
    def after_self(cpu, tb):
        st["after_self"] += 1
        if st["after_self"] == 100000:
            panda.disable_callback("after_self")    # a list write from the vCPU too


def churn(stop_at):
    rnd = random.Random(1)
    n = 0
    while time.time() < stop_at:
        names = []
        for k in range(8):
            name = f"churn{n}_{k}"
            if k % 2:
                @panda.cb_start_block_exec(name=name)
                def f(cpu, tb):
                    st["churn_calls"] += 1
            else:
                @panda.cb_before_block_exec(name=name)
                def f(cpu, tb):
                    st["churn_calls"] += 1
            names.append(name)
        time.sleep(0.005)
        rnd.shuffle(names)
        for i, name in enumerate(names):
            if i % 2:
                panda.delete_callback(name)
                st["direct"] += 1
            else:
                panda.queue_main_loop_wait_fn(panda.delete_callback, [name])
                st["queued"] += 1
        n += 1
        st["churn_rounds"] = n


@panda.queue_blocking
def drive():
    try:
        panda.revert_sync("root")
        armed_self_delete()
        t = threading.Thread(target=churn, args=(time.time() + seconds,))
        t.start()
        loops = 0
        while t.is_alive():     # keep the guest busy (guest-time bounded loops) while the churn runs
            out = panda.run_serial_cmd("timeout 2 sh -c 'while :; do :; done'; echo loopdone",
                                       timeout=600)
            loops += 1
        t.join()
        st["loops"] = loops
        st["loop_out"] = out.strip().splitlines()[-1] if out.strip() else ""
        # keep the guest busy while the plugins go
        panda.run_serial_cmd("cat /etc/os-release > /dev/null", timeout=120)
        st["busy"] = True
        bg = threading.Thread(target=lambda: panda.run_serial_cmd(
            "timeout 30 sh -c 'while :; do :; done'", timeout=600))
        bg.daemon = True
        bg.start()
        time.sleep(2)
        @panda.cb_start_block_exec(name="late_hot")
        def late_hot(cpu, tb):
            pass
        time.sleep(1)
        panda.unload_plugins()       # main loop: Python callbacks, then every C plugin; vCPU running
        panda.queue_main_loop_wait_fn(lambda: st.__setitem__("unloaded", True))
        time.sleep(3)
    except Exception as e:  # noqa: BLE001
        st["err"] = repr(e)
    panda.stop_run()


panda.run()
print(f"TEARDOWN arch={panda.arch_name} {st}", flush=True)
checks = {
    "a callback deleted itself from the vCPU, once": st["self_deleted"] == 1,
    "the callback after it kept running": st["after_self"] > 1,
    "churn ran (registered/deleted while running)": st["churn_rounds"] > 10,
    "churned callbacks ran": st["churn_calls"] > 0,
    "the guest loops finished": st.get("loop_out") == "loopdone",
    "C plugin (syscalls2 ppp) callbacks ran": st["openat"] > 0,
    "plugins unloaded while the vCPU ran": st["unloaded"],
    "no error": st["err"] is None,
}
for k, ok in checks.items():
    print(f"  {'PASS' if ok else 'FAIL'} {k}")
sys.exit(0 if all(checks.values()) else 1)
