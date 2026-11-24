"""Engine address hooks under change: hooks added mid-block, inserted and removed from every
thread at once, churned by the thousand, and at addresses they cannot work at.

Run: panda-python tests/hookeng.py <dir>/busybox-<arch>.json [--smp N] [--case NAME]
(an image from tests/mkbusybox.py; the machine is tests/testmachine.py's)

The guest program is tests/programs/hookeng.c (see there for its labels). Each case runs in a
child process:
- late: a hook at a0 adds, on its first hit, hooks at a2 (later in the same block), b0 (the
  block's chained successor on the same page) and e0 (the next block, on a page that had no hook).
  The program runs R rounds of blk with the round number in memory; each hit records it. The
  inserting vCPU leaves its block at the next block boundary, so in the process that added them:
  a2 fires from round 2, b0 and e0 from round 1, and every round after. With --smp 4 (four
  copies at once) every other process must see each from some round on, with none missed after.
- storm: a permanent C hook at c0 whose callback inserts or removes a hook at c1 on every hit,
  and three host threads inserting and removing hooks at c0 and c1, while every vCPU runs c0 c1
  in a loop (tests/hookeng_c.c, the engine's panda/address-hook.h API). No crash, and no
  transient hook called more than 200 ms after its removal returned (the engine allows a call
  from a pass that was already under way when it returned).
- churn: a permanent C hook at c0, hit once per process, removes the 8 hooks it inserted last
  time and inserts 8 at new addresses, one page each, over 700 processes: about 5,600 transient
  addresses. The engine reclaims the dead ones at TB flushes (it asks for one past 4,096) and
  rebuilds its page bitmap: after a final flush, 9 sites are left and at most 9 page bits set,
  so code on the other pages translates with CF_PCREL again where the target has it.
- inside: a hook in the middle of an instruction (ins0 + 2) never fires, and the engine says so
  when it translates that instruction; a hook at ins0 fires every time.
- delay (MIPS): a hook on a branch delay slot (ds0) fires once per round.
- thumb (arm): a hook at a Thumb instruction's address (th0) fires once per round; one at the
  address with the Thumb bit set (th1 | 1) is inside an instruction: it never fires, and the
  engine says so.
- kvm (x86_64, smp 1): under KVM, panda_address_hook_insert() refuses a hook (NULL) instead of
  accepting one that would never be called (pandare2's panda.hook raises already).
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "programs"))
from testlib import Checks, progress  # noqa: E402

PANDA_TEST = dict(area="hooks", tier="guest", fixture="testmachine",
                  arches=["x86_64", "aarch64", "mips", "ppc64", "arm"], plugins=["hooks"],
                  variants=dict(smp=[1, 4]), needs=["cc"], timeout=1800)

R = 100
CASES = {"x86_64": ["late", "storm", "churn", "inside", "kvm"],
         "aarch64": ["late", "storm", "churn", "inside"],
         "mips": ["late", "storm", "churn", "inside", "delay"],
         "ppc64": ["late", "storm", "churn", "inside"],
         "arm": ["late", "storm", "churn", "inside", "thumb"]}
SMP_CASES = ["late", "storm"]
# what the engine prints for a hook inside an instruction (accel/tcg/hooks.c)
INSIDE_MSG = "is inside the instruction at"

ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--smp", type=int, default=1)
ap.add_argument("--case")
opt = ap.parse_args()

from hookeng_programs import PROGRAMS  # noqa: E402

image_arch = json.load(open(opt.image)).get("arch")
if image_arch not in PROGRAMS:
    print("SKIP hookeng: no tests/programs/hookeng.c build for this image's arch")
    sys.exit(0)

if opt.case is None:
    failed = []
    cases = CASES[image_arch] if opt.smp == 1 else [c for c in CASES[image_arch] if c in SMP_CASES]
    for case in cases:
        progress(f"case {case}")
        cmd = [sys.executable, os.path.abspath(__file__), opt.image, "--smp", str(opt.smp),
               "--case", case]
        try:
            r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                               timeout=900)
            out, rc = r.stdout, r.returncode
        except subprocess.TimeoutExpired as e:
            out, rc = (e.stdout or b"").decode(errors="replace") + "\nTIMEOUT", -1
        if case in ("inside", "thumb") and rc == 0:
            # the engine's warning goes to the child's stderr, which is in out
            n = out.count(INSIDE_MSG)
            ok = n == 1
            out += f"\n  {'PASS' if ok else 'FAIL'} the engine warned once about the hook inside " \
                   f"an instruction ({n} times)"
            rc = 0 if ok else 1
        print("\n".join(f"  [{case}] {line}" for line in out.splitlines()), flush=True)
        if rc:
            failed.append(f"{case}(rc={rc})" if rc < 0 or rc > 1 else case)
    print(f"HOOKENG smp{opt.smp} {'FAIL ' + ' '.join(failed) if failed else 'PASS'}")
    sys.exit(1 if failed else 0)

import cffi  # noqa: E402

from testmachine import Agent, Machine  # noqa: E402

case = opt.case
m = Machine(opt.image, smp=opt.smp)
arch = m.arch
C = Checks(arch)
kw = {"accel": "kvm"} if case == "kvm" else {}
panda = m.panda(**kw)
agent = Agent(panda) if case != "kvm" else None
WORD = 8 if arch in ("x86_64", "aarch64", "ppc64") else 4
ENDIAN = "big" if arch in ("mips", "ppc64") else "little"
ffi = cffi.FFI()
ffi.cdef("""
typedef struct { uint64_t sites, dead_sites, graveyard, reclaims; } TCGHooksStats;
void tcg_hooks_stats(TCGHooksStats *st);
extern unsigned long tcg_hooks_pages[];
void *panda_address_hook_insert(uint64_t pc, void *fn, void *opaque);
void panda_address_hook_remove(void *hook);
""")
engine = ffi.dlopen(panda.libpanda_path)
he = None


def load_c():
    global he
    tmp = tempfile.mkdtemp(prefix="hookeng-")
    so = os.path.join(tmp, "he.so")
    subprocess.run(["cc", "-shared", "-fPIC", "-O2", "-pthread", "-o", so,
                    os.path.join(HERE, "hookeng_c.c")], check=True)
    f = cffi.FFI()
    f.cdef("void he_setup(void *insert, void *remove); uint64_t he_get(int i);"
           "void he_storm_start(uint64_t c0, uint64_t c1, int threads); void he_storm_stop(void);"
           "void he_churn_start(uint64_t hot, uint64_t base, int per);"
           "uint64_t he_churn_inserted(void);")
    he = f.dlopen(so)
    shutil.rmtree(tmp)
    he.he_setup(f.cast("void *", ffi.cast("uintptr_t", engine.panda_address_hook_insert)),
                f.cast("void *", ffi.cast("uintptr_t", engine.panda_address_hook_remove)))


def stats():
    s = ffi.new("TCGHooksStats *")
    engine.tcg_hooks_stats(s)
    return dict(sites=s.sites, dead=s.dead_sites, graveyard=s.graveyard, reclaims=s.reclaims)


def page_bits():
    words = ffi.cast("unsigned long *", engine.tcg_hooks_pages)
    nbits = 1 << 20
    return sum(bin(words[i]).count("1") for i in range(nbits // (8 * ffi.sizeof("unsigned long"))))


def put_program():
    import base64
    agent.put("/tmp/he", base64.b64decode(PROGRAMS[arch]))
    rc, out = agent.run("/tmp/he labels")
    labels = {}
    for line in out.decode().splitlines():
        k, v = line.split()
        labels[k] = int(v, 16)
    return labels


def parallel(cmd):
    if opt.smp == 1:
        return cmd + "; echo rc=$?"
    return "(" + " & ".join([f"({cmd})"] * opt.smp) + " & wait); echo rc=$?"


def read_iter(cpu, addr):
    return int.from_bytes(bytes(panda.virtual_memory_read(cpu, addr, WORD)), ENDIAN)


def contiguous_to(xs, last):
    return bool(xs) and xs == list(range(xs[0], last + 1))


def case_late(L):
    rec = defaultdict(lambda: defaultdict(list))        # name -> asid -> [round]
    st = {"adder": None}

    def recorder(name):
        def fn(cpu, tb, hook):
            rec[name][panda.current_asid(cpu)].append(read_iter(cpu, L["iter"]))
        panda.hook(L[name], kernel=False, observe_only=True)(fn)

    def on_a0(cpu, tb, hook):
        asid = panda.current_asid(cpu)
        rec["a0"][asid].append(read_iter(cpu, L["iter"]))
        if st["adder"] is None:
            st["adder"] = asid
            for name in ("a2", "b0", "e0"):
                recorder(name)
    panda.hook(L["a0"], kernel=False, observe_only=True)(on_a0)
    rc, out = agent.run(parallel(f"/tmp/he late {R}"), timeout=600)
    C.check("the program runs", b"rc=0" in out, out[-200:])
    asids = list(rec["a0"])
    C.check(f"{opt.smp} processes ran blk", len(asids) == opt.smp, asids)
    for a in asids:
        C.check(f"a0 every round ({a:#x})", rec["a0"][a] == list(range(1, R + 1)), rec["a0"][a][:5])
    A = st["adder"]
    C.check("a2 (same block, later) from round 2 in the adding process",
            rec["a2"][A] == list(range(2, R + 1)), rec["a2"][A][:5])
    C.check("b0 (chained successor) from round 1 in the adding process",
            rec["b0"][A] == list(range(1, R + 1)), rec["b0"][A][:5])
    C.check("e0 (successor on an unhooked page) from round 1 in the adding process",
            rec["e0"][A] == list(range(1, R + 1)), rec["e0"][A][:5])
    for a in asids:
        if a == A:
            continue
        for name in ("a2", "b0", "e0"):
            C.check(f"{name} in process {a:#x}: from some round on, none missed",
                    contiguous_to(rec[name][a], R), rec[name][a][:5])


def case_storm(L):
    load_c()
    he.he_storm_start(L["c0"], L["c1"], 3)
    rc, out = agent.run(parallel("/tmp/he hot 300000"), timeout=900)
    he.he_storm_stop()
    C.check("the program runs under the storm", b"rc=0" in out, out[-200:])
    C.check("the vCPU callbacks ran", he.he_get(0) > 1000, he.he_get(0))
    C.check("the host threads inserted and removed hooks", he.he_get(2) > 100, he.he_get(2))
    C.check("transient hooks were called", he.he_get(1) > 0, he.he_get(1))
    C.check("no hook called long after its removal returned", he.he_get(-1) == 0,
            f"violations {he.he_get(-1)}, latest call {he.he_get(-2) / 1e6:.1f} ms after")
    print(f"  storm: vcpu callbacks {he.he_get(0)}, transient calls {he.he_get(1)}, host cycles "
          f"{he.he_get(2)}, latest call after removal {he.he_get(-2) / 1e6:.2f} ms, {stats()}")


def case_churn(L):
    load_c()
    per = 8
    he.he_churn_start(L["c0"], 0x30000000, per)
    rc, out = agent.run("for i in $(seq 700); do /tmp/he hot 1; done; echo rc=$?", timeout=900)
    C.check("the program runs 700 times", b"rc=0" in out, out[-200:])
    n = he.he_churn_inserted()
    before = stats()
    C.check("thousands of transient hooks", n >= 5000, n)
    C.check("the engine reclaimed dead sites on its own", before["reclaims"] >= 1, before)
    C.check("sites stayed bounded", before["sites"] <= 4096 * 2 + per + 1, before)
    panda.flush_tb()
    agent.run("true")                   # the flush runs on the vCPU
    after = stats()
    bits = page_bits()
    C.check("after a flush: only the live sites are left", after["sites"] == per + 1 and
            after["dead"] == 0, after)
    C.check("after a flush: only their pages are marked", bits <= per + 1, bits)
    print(f"  churn: {n} transient hooks; before the flush {before}; after {after}, {bits} page bits")


def counter(L, addr):
    hits = [0]

    def fn(cpu, tb, hook):
        hits[0] += 1
    panda.hook(addr, kernel=False, observe_only=True)(fn)
    return hits


def case_inside(L):
    good = counter(L, L["ins0"])
    bad = counter(L, L["ins0"] + 2)
    rc, out = agent.run(f"/tmp/he ins {R}; echo rc=$?")
    C.check("the program runs", b"rc=0" in out, out[-200:])
    C.check("the hook at the instruction fires every time", good[0] == R, good[0])
    C.check("the hook inside the instruction never fires", bad[0] == 0, bad[0])


def case_delay(L):
    ds = counter(L, L["ds0"])
    rc, out = agent.run(f"/tmp/he late {R}; echo rc=$?")
    C.check("the program runs", b"rc=0" in out, out[-200:])
    C.check("the hook on the delay slot fires once per round", ds[0] == R, ds[0])


def case_thumb(L):
    th0 = counter(L, L["th0"])
    odd = counter(L, L["th1"] | 1)
    rc, out = agent.run(f"/tmp/he late {R}; echo rc=$?")
    C.check("the program runs", b"rc=0" in out, out[-200:])
    C.check("the hook at the Thumb instruction fires once per round", th0[0] == R, th0[0])
    C.check("the hook at the address with the Thumb bit never fires", odd[0] == 0, odd[0])


def case_kvm():
    h = engine.panda_address_hook_insert(0x1000, ffi.cast("void *", 1), ffi.NULL)
    C.check("panda_address_hook_insert refuses under KVM", h == ffi.NULL, h)
    if h != ffi.NULL:
        engine.panda_address_hook_remove(h)


@panda.queue_blocking
def drive():
    try:
        if case == "kvm":
            case_kvm()                  # the machine is up, under KVM: no need to revert
            return
        m.revert(panda, agent)
        L = put_program()
        globals()["case_" + case](L)
    finally:
        panda.end_analysis()


panda.run()
sys.exit(C.report(f"HOOKENG case={case} smp={opt.smp}"))
