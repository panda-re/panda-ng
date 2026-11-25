"""Page-in requests racing signal delivery: a signal frame built over an injected call.

Run: panda-python tests/pagein_race.py <dir>/busybox-<arch>.json [--smp N] [--rounds R]
(the machine is tests/testmachine.py's)

Each round reverts, runs tests/programs/pagein_target in its wall-clock signal mode (a timer
every 2 ms whose handler siglongjmps to the top of its loop) and floods it with page-in requests
from two threads for as long as it runs, tens of thousands of injected calls per round. Signals
then land on the way out of injected calls: the kernel builds the handler's frame from the
call's registers, and the handler longjmps away, so the thread never comes back to the call;
later it runs the same instruction at the same stack depth again. The engine must put registers
back only for the call's own return (its token in the marker register), never there.

Checks, every round: the program exits 0, its register/counter check finds nothing (registers
from another moment would show as a counter behind its memory copy), no oops or signal in the
guest. And over the run: the race happened (calls with a signal frame over them, and returns to
an abandoned call's instruction and stack pointer without its token).
"""
import argparse
import os
import base64
import gzip
import json
import random
import re
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from testlib import Checks, progress  # noqa: E402
from testmachine import Agent, Machine  # noqa: E402
from programs.pagein_programs import PROGRAMS  # noqa: E402

PANDA_TEST = dict(area="pagein", tier="guest", fixture="testmachine",
                  arches=["x86_64", "aarch64", "riscv64"], plugins=["hypercaller"],
                  variants=dict(smp=[1, 4]), timeout=900)

ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--smp", type=int, default=1)
ap.add_argument("--rounds", type=int, default=4)
ap.add_argument("--secs", type=int, default=5)
ap.add_argument("--signals", default="real")
ap.add_argument("--out")
opt = ap.parse_args()

MAGIC = 0x46525431
PG, NP = 4096, 64
m = Machine(opt.image, smp=opt.smp)
panda = m.panda()
agent = Agent(panda)
arch = m.arch
prog = "ppc64le" if arch == "ppc64" and "64le" in str(m.image.get("extra_args")) else arch
st = dict(ready=threading.Event(), done=threading.Event())
BAD = re.compile(r"(Oops|BUG:|Kernel panic|segfault|PT SIGNAL)")


@panda.hypercall(MAGIC, mutates=True)
def hc(cpu):
    op, a, b = (panda.arch.get_arg(cpu, i, convention="syscall") for i in range(2, 5))
    if op == 1:
        st["asid"], st["base"], st["step"] = panda.current_asid(cpu), a, b
        st["ready"].set()
    elif op == 2:
        st["done"].set()
    panda.guest.set_retval(cpu, 0)


def flood(stop, seed, counts):
    rng = random.Random(seed)
    lock = threading.Lock()
    while not stop.is_set():
        base, step = st["base"], st["step"]
        region = rng.choice([1, 2, 9, 4, 6, 5])          # file, anon, swap, hole, none, trunc
        addr = base + region * step + rng.randrange(NP) * PG + rng.randrange(0, PG, 8)
        ev = threading.Event()

        def done(d, e):
            with lock:
                k = "ok" if e is None else type(e).__name__
                counts[k] = counts.get(k, 0) + 1
            ev.set()
        try:
            panda.pagein.read(st["asid"], addr, rng.choice([8, 64, 4096]), done, timeout_ns=3_000_000_000)
            ev.wait(10)
        except Exception as e:
            counts[type(e).__name__] = counts.get(type(e).__name__, 0) + 1
            time.sleep(0.01)


rows = []


@panda.queue_blocking
def drive():
    for rnd in range(opt.rounds):
        st["ready"].clear()
        st["done"].clear()
        m.revert(panda, agent)
        before = panda.pagein.stats()
        agent.put("/tmp/pt", gzip.decompress(base64.b64decode(PROGRAMS[prog])))
        agent.run("chmod +x /tmp/pt; mkdir -p /mnt; dd if=/dev/zero of=/tmp/swap.img bs=1M count=16 2>/dev/null; "
                  "losetup /dev/loop1 /tmp/swap.img && mkswap /dev/loop1 >/dev/null && swapon /dev/loop1; true")
        agent.run(f"(/tmp/pt /tmp {opt.secs} spin {opt.signals} > /tmp/pt.out 2>&1; echo exit=$? >> /tmp/pt.out) &")
        row = dict(round=rnd, counts={})
        if not st["ready"].wait(60):
            row["error"] = "no READY"
        else:
            stop = threading.Event()
            ts = [threading.Thread(target=flood, args=(stop, rnd * 2 + k, row["counts"])) for k in (0, 1)]
            for t in ts:
                t.start()
            st["done"].wait(opt.secs * 8 + 30)
            stop.set()
            for t in ts:
                t.join(30)
            time.sleep(0.3)
        _, out = agent.run("cat /tmp/pt.out; dmesg | tail -n 20", timeout=120)
        txt = out.decode(errors="replace")
        after = panda.pagein.stats()
        row["exit"] = (re.findall(r"exit=(\d+)", txt) or ["?"])[-1]
        row["pt"] = (re.findall(r"PT [A-Z]+.*", txt) or [""])[0][:120]
        row["bad"] = [l for l in (txt + m.console_tail(n=6000)).splitlines() if BAD.search(l)][:4]
        for k in ("injected", "abandoned", "stale", "overframes"):
            row[k] = after[k] - before[k]
        ok = row["exit"] == "0" and " mismatches 0 " in row["pt"] and " bad 0 " in row["pt"] and not row["bad"]
        row["ok"] = ok
        rows.append(row)
        progress("pagein_race: " + json.dumps(row))
    panda.end_analysis()


panda.run()
n = len(rows)
bad = [r for r in rows if not r["ok"]]
C = Checks(arch)
C.check("every round ran", n == opt.rounds, str(n))
C.check("no round hurt the program or the guest", not bad, json.dumps(bad[:2]))
C.check("signal frames were built over injected calls", sum(r["overframes"] for r in rows) > 0)
print(f"HUNT {arch} smp{opt.smp} {opt.signals}: {n} rounds, {len(bad)} failed, "
      f"stale {sum(r['stale'] for r in rows)}, overframes {sum(r['overframes'] for r in rows)}, "
      f"abandoned {sum(r['abandoned'] for r in rows)}, injected {sum(r['injected'] for r in rows)}")
if opt.out:
    with open(opt.out, "a") as f:
        for r in rows:
            f.write(json.dumps(dict(r, arch=arch, smp=opt.smp, signals=opt.signals)) + "\n")
sys.exit(C.report(f"pagein_race {arch} smp{opt.smp}"))
