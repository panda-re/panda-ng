#!/usr/bin/env python3
"""The syscall trap's consumers and the PANDA syscall events, host-only: the real code against a
model and directed cases.

No engine and no guest. Compiled under ASan and UBSan against tests/static/trapstubs.py:

1. The trap core (system/syscall-trap.c, without its guest-memory helpers) in
   syscall_trap_harness.c, driven by a Python model of syscall-trap.h's contract: consumers added
   and removed in any order class and priority, per-consumer watches, calls entering, returning,
   restarting, skipped (with and without the skipped-return option), kept pending (vfork),
   forgotten by task and cancelled. After every command the two agree on the order consumers ran
   in, which consumers' data was dropped (each token exactly once, outside the lock), the return
   value, whether the trap is on (the OR of the watches), and what is pending. A failing sequence
   is shrunk and printed.
2. The same harness's directed cases: holds at entry and return resume at the holder and then the
   consumers after it; an injected call; removing a holder (released) or the holder of an
   injection (refused, nothing changed); cancel and the last consumer's removal; forget_task; the
   target's word (syscall_trap_set_arch); a dispatch abandoned by longjmp; a consumer removing
   itself from its callback; the skipped-return option.
3. The PANDA syscall events (panda/src/syscall-events.c with the registry, events.c, and the trap
   core) in syscall_events_harness.c: no consumer without a subscription, one per kind with,
   watches, instance data from entry to return, dispatch order across kinds and priorities, holds
   and their release on unsubscribe, refusal reasons (observer, record/replay, KVM, phase),
   exit calls, skipped calls' returns, and the QEMU-plugin and Penguin consumers beside them.

    python3 tests/static/syscall_trap.py [--seqs N] [--seed S]
    (nix build .#checks.x86_64-linux.syscall-trap)

Needs a C compiler with ASan and UBSan, pkg-config and GLib.
"""
import argparse
import os
import random
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import trapstubs  # noqa: E402

HERE = Path(__file__).resolve().parent
ENTER, RETURN = 1, 2
SKIP, RET = 2, 4
IS32 = {1}          # ABI 1 (i386) keeps 32-bit values
M64 = (1 << 64) - 1


def sext(v, abi):
    v &= M64
    if abi in IS32:
        v &= 0xffffffff
        return v - (1 << 32) if v >> 31 else v
    return v - (1 << 64) if v >> 63 else v


def width(v, abi):
    return v & (0xffffffff if abi in IS32 else M64)


class Cons:
    def __init__(self, cid, order, prio, seq, skipret):
        self.id, self.order, self.prio, self.seq, self.skipret = cid, order, prio, seq, skipret
        self.live = True
        self.all = {}
        self.nums = {}
        self.cfg = dict(want=0, data=0, task=0, skip=0, skipv=0, consume=0, keep=0, setret=0,
                        retv=0)

    def key(self):
        return (self.order, self.prio, self.seq)

    def w(self, abi, num):
        return self.all.get(abi, 0) | self.nums.get((abi, num), 0)


class Model:
    """syscall-trap.h's contract, for calls nobody holds."""

    def __init__(self):
        self.cons = {}
        self.seq = 0
        self.slots = {}         # (task, pc) -> {"abi", "states": {cid: st}}

    def order(self):
        return sorted((c for c in self.cons.values() if c.live), key=Cons.key)

    def on(self):
        return any(c.all.get(a) or any(c.nums.values()) for c in self.order() for a in (0, 1))

    def tokens(self):
        return sum(1 for s in self.slots.values() for st in s["states"].values() if st["data"])

    def run(self, cmd):
        op, a = cmd[0], cmd[1:]
        out, drops = [], []

        def drop(cid, st):
            if st["data"]:
                drops.append(cid)
                st["data"] = False

        if op == "add":
            cid, order, prio, skipret = a
            self.seq += 1
            self.cons[cid] = Cons(cid, order, prio, self.seq, skipret)
            res = "= 1"
        elif op == "cfg":
            c = self.cons[a[0]]
            c.cfg = dict(zip(["want", "data", "task", "skip", "skipv", "consume", "keep",
                              "setret", "retv"], a[1:]))
            res = None
        elif op == "rm":
            c = self.cons.get(a[0])
            if not c or not c.live:
                res = "= 0"
            else:
                c.live = False
                for key in list(self.slots):
                    states = self.slots[key]["states"]
                    if c.id in states:
                        drop(c.id, states.pop(c.id))
                    if not any(st["want"] for st in states.values()):
                        for cid, st in states.items():
                            drop(cid, st)
                        del self.slots[key]
                if not self.order():
                    for s in self.slots.values():
                        for cid, st in s["states"].items():
                            drop(cid, st)
                    self.slots.clear()
                res = "= 1"
        elif op == "watch":
            cid, abi, num, flags = a
            c = self.cons[cid]
            if c.live:
                if num < 0:
                    c.all[abi] = flags
                else:
                    c.nums[(abi, num)] = flags
            res = None
        elif op == "enter":
            res = self.enter(*a, out=out, drop=drop)
        elif op == "ret":
            res = self.ret(*a, out=out, drop=drop)
        elif op == "forget":
            task, n = a[0], 0
            if task and self.slots:
                for key in list(self.slots):
                    states = self.slots[key]["states"]
                    if any(st["task"] == task for st in states.values()):
                        for cid, st in states.items():
                            drop(cid, st)
                        del self.slots[key]
                        n += 1
            res = f"= {n}"
        elif op == "cancel":
            n = len(self.slots)
            for s in self.slots.values():
                for cid, st in s["states"].items():
                    drop(cid, st)
            self.slots.clear()
            res = f"= {n}"
        elif op == "state":
            res = f"S {int(self.on())} {len(self.slots)} {self.tokens()} {len(self.order())}"
        else:
            res = None
        return out, sorted(drops), res

    def enter(self, task, abi, num, pc, out, drop):
        cons = self.order()
        if not cons or not any(c.w(abi, num) for c in cons):
            return "= 0 0"
        sc = {"skip": False, "ret": 0}
        scr = {}
        for c in cons:
            st = scr[c.id] = {"data": False, "task": 0, "want": False, "keep": False}
            cw = c.w(abi, num)
            st["want"] = bool(cw & RETURN)
            if not cw & ENTER:
                continue
            out.append(f"E {c.id}")
            f = c.cfg
            if f["want"] == 1:
                st["want"] = True
            elif f["want"] == 2:
                st["want"] = False
            if f["data"]:
                st["data"] = True
            if f["task"]:
                st["task"] = f["task"]
            if f["skip"]:
                sc["skip"], sc["ret"] = True, f["skipv"]
        if sc["skip"]:
            for c in cons:
                st = scr[c.id]
                if st["want"] and c.skipret:
                    out.append(f"R {c.id} s")
                    f = c.cfg
                    if f["setret"]:
                        sc["ret"] = f["retv"]
                    if f["consume"]:
                        st["data"] = False
            for cid, st in scr.items():
                drop(cid, st)
            return f"= {SKIP} {width(sc['ret'], abi)}"
        if any(st["want"] for st in scr.values()):
            old = self.slots.pop((task, pc), None)
            if old:
                for cid, st in old["states"].items():
                    drop(cid, st)
            states = {cid: st for cid, st in scr.items() if st["want"]}
            for cid, st in scr.items():
                if not st["want"]:
                    drop(cid, st)
            self.slots[(task, pc)] = {"abi": abi, "states": states}
        else:
            for cid, st in scr.items():
                drop(cid, st)
        return "= 0 0"

    def ret(self, task, pc, val, out, drop):
        restart = False
        s = self.slots.get((task, pc))
        key = (task, pc)
        if not s:
            s = self.slots.get((task, pc + 2))
            key = (task, pc + 2)
            restart = True
            if not s:
                return f"= 0 {val & M64}"
        del self.slots[key]
        abi = s["abi"]
        v = sext(val, abi)
        ret = v
        for c in self.order():
            st = s["states"].get(c.id)
            if not st or not st["want"]:
                continue
            out.append(f"R {c.id}" + (" x" if restart else ""))
            f = c.cfg
            if f["keep"]:
                st["keep"] = True
            if f["setret"]:
                ret = f["retv"]
            if f["consume"]:
                st["data"] = False
        r, rv = 0, val & M64
        if not restart and ret != v:
            r, rv = RET, width(ret, abi)
        states = s["states"]
        if not restart:
            kept = {}
            for c in self.order():
                st = states.get(c.id)
                if st and st["keep"]:
                    st["keep"] = False
                    st["want"] = True
                    kept[c.id] = st
            for cid, st in states.items():
                if cid not in kept:
                    drop(cid, st)
            if kept:
                self.slots[key] = {"abi": abi, "states": kept}
        else:
            for cid, st in states.items():
                drop(cid, st)
        return f"= {r} {rv}"


def gen(rng, n):
    cmds, live, free = [], set(), list(range(1, 6))
    for _ in range(n):
        k = rng.random()
        if (k < 0.12 or not live) and free:
            cid = free.pop(rng.randrange(len(free)))
            live.add(cid)
            cmds.append(("add", cid, rng.randrange(3), rng.randrange(-1, 2), rng.randrange(2)))
            cmds.append(("cfg", cid, rng.choice([0, 0, 1, 2]), rng.randrange(2), rng.choice([0, 0, 7, 8]),
                         int(rng.random() < 0.15), rng.choice([-4, 3, 0x1_0000_0001]),
                         rng.randrange(2), int(rng.random() < 0.2), int(rng.random() < 0.3),
                         rng.choice([-1, 99, 0x1_0000_0002])))
        elif k < 0.17 and live:
            cid = rng.choice(sorted(live))
            cmds.append(("rm", cid))
            live.discard(cid)
            free.append(cid)
        elif k < 0.32 and live:
            cmds.append(("watch", rng.choice(sorted(live)), rng.randrange(2), rng.choice([-1, 0, 1, 2]),
                         rng.randrange(4)))
        elif k < 0.62:
            cmds.append(("enter", rng.randrange(1, 5), rng.randrange(2), rng.randrange(3),
                         rng.choice([0x1000, 0x1008, 0x1010])))
        elif k < 0.88:
            cmds.append(("ret", rng.randrange(1, 5), rng.choice([0x1000, 0x1008, 0x1010, 0xffe, 0x1006]),
                         rng.choice([0, 5, (1 << 64) - 2, 0x8000_0001])))
        elif k < 0.92:
            cmds.append(("forget", rng.choice([0, 7, 8])))
        elif k < 0.94:
            cmds.append(("cancel",))
        elif k < 0.96:
            cmds.append(("flush",))
        cmds.append(("state",))
    return cmds


def build(out):
    trapstubs.write_stubs(out)
    (out / "trap_core.inc").write_text(trapstubs.core_source())
    exe = out / "trap"
    subprocess.run([trapstubs.cc(), str(HERE / "syscall_trap_harness.c"), "-I", str(out), "-o",
                    str(exe), *trapstubs.cflags(out)], check=True)
    exe2 = out / "events"
    subprocess.run([trapstubs.cc(), str(HERE / "syscall_events_harness.c"),
                    str(trapstubs.SRC / "panda/src/events.c"),
                    str(trapstubs.SRC / "panda/src/syscall-events.c"), "-I", str(out), "-o",
                    str(exe2), *trapstubs.cflags(out)], check=True)
    return exe, exe2


def run_c(exe, cmds):
    def s64(x):         # the harness reads signed 64-bit numbers
        return x - (1 << 64) if isinstance(x, int) and x >= 1 << 63 else x
    text = "".join(" ".join(str(s64(x)) for x in c) + "\n" for c in cmds)
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1")
    p = subprocess.run([str(exe)], input=text, capture_output=True, text=True, env=env)
    if p.returncode:
        return None, p.stdout + p.stderr
    blocks, cur = [], []
    for line in p.stdout.splitlines():
        if line == ".":
            blocks.append(cur)
            cur = []
        else:
            cur.append(line)
    return blocks, ""


def split(block):
    calls = [x for x in block if x[:1] in "ER"]
    drops = sorted(int(x.split()[1]) for x in block if x.startswith("D "))
    res = [x for x in block if x[:1] in "=S"]
    return calls, drops, res[0] if res else None


def check(exe, cmds):
    blocks, err = run_c(exe, cmds)
    if blocks is None:
        return f"the harness failed: {err[-3000:]}"
    m = Model()
    for i, (cmd, block) in enumerate(zip(cmds, blocks)):
        calls, drops, res = m.run(cmd)
        got = split(block)
        if got != (calls, drops, res):
            return (f"command {i} {cmd}:\n  trap:  calls {got[0]} drops {got[1]} {got[2]}\n"
                    f"  model: calls {calls} drops {drops} {res}")
    return None


def shrink(exe, cmds):
    changed = True
    while changed:
        changed = False
        for i in range(len(cmds)):
            if cmds[i][0] in ("add", "cfg", "rm"):
                continue        # later commands name the consumer
            trial = cmds[:i] + cmds[i + 1:]
            if check(exe, trial):
                cmds = trial
                changed = True
                break
    return cmds


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seqs", type=int, default=400)
    ap.add_argument("--len", type=int, default=120)
    ap.add_argument("--seed", type=int, default=None)
    args = ap.parse_args()
    seed = args.seed if args.seed is not None else random.randrange(1 << 30)
    with tempfile.TemporaryDirectory(prefix="syscall-trap-") as tmp:
        exe, exe2 = build(Path(tmp))
        rc = 0
        for n in range(args.seqs):
            rng = random.Random(seed + n)
            cmds = gen(rng, args.len)
            why = check(exe, cmds)
            if why:
                small = shrink(exe, cmds)
                print(f"FAIL: sequence {seed + n} disagrees: {check(exe, small)}")
                print("  shrunk:", *small, sep="\n    ")
                rc = 1
                break
        if not rc:
            print(f"trap model: {args.seqs} sequences agree (seed {seed})")
        env = dict(os.environ, ASAN_OPTIONS="detect_leaks=1")
        for exe_, what in ((exe, "directed"), (exe2, "")):
            p = subprocess.run([str(exe_)] + ([what] if what else []), capture_output=True,
                               text=True, env=env)
            sys.stdout.write(p.stdout)
            sys.stderr.write(p.stderr[-6000:])
            rc |= p.returncode != 0
        return rc


if __name__ == "__main__":
    sys.exit(main())
