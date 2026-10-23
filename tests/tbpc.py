"""panda_tb_pc on blocks QEMU shares between virtual addresses (CF_PCREL), on an image built by
pandare2.mkimage.

Run: panda-python tests/tbpc.py <dir>/debian-13-x86_64.json

Only block callbacks are registered (no insn_translate, memory callbacks or hooks, which bind a TB
to one address), so blocks stay unbound: tb.pc is 0 and the same TB runs wherever its code is
mapped. The guest runs a few short processes; each maps libc and ld.so at its own (ASLR) address,
so their code runs from shared TBs at several virtual addresses. For every block each callback
sees, panda.tb_pc must be where the block is executing:
  - its physical address (a page walk of the guest's tables, panda.virt_to_phys) is the TB's own
    (tb.page_addr[0], a RAM offset: a constant distance from the physical address);
  - after_block_exec and end_block_exec, which run after the block, report the address that
    before_block_exec / start_block_exec saw for it;
  - after_block_translate reports before_block_translate's pc;
  - before_block_exec and start_block_exec report the CPU's pc at entry.
"""
import sys
from collections import Counter, defaultdict

from pandare2 import Panda

CF_PCREL = 0x00020000
panda = Panda(generic=sys.argv[1])
arch = panda.arch_name
TOP = 1 << 63                  # kernel addresses have the top bit set on all three arches
BAD = (-1, 2**64 - 1)
WORK = "head -1 /etc/os-release > /tmp/o1; cat /etc/os-release > /dev/null; head -1 /etc/os-release > /tmp/o2"

st = {"checked": Counter(), "bad": defaultdict(list), "delta": Counter(), "user_delta": Counter(),
      "tbs": {}, "where": defaultdict(set), "unmapped": 0}
# The guest's serial driver busy-waits, so a command runs millions of kernel blocks: every block
# gets the cheap checks (tb_pc, the after/end pairing), and each (tb, address, page) the page walk
# once, kernel ones up to KERNEL_SAMPLE.
KERNEL_SAMPLE = 20000
checked = set()
n = Counter()
last = {"bbe": (None, None), "sbe": (None, None), "translating": None}
tb_pc = panda.libpanda.panda_tb_pc


def bad(kind, detail):
    if len(st["bad"][kind]) < 5:
        st["bad"][kind].append(detail)
    st["checked"][kind + " bad"] += 1


def key(tb):
    return int(panda.ffi.cast("uintptr_t", tb))


def at_entry(kind, cpu, tb):
    """A block starts: tb_pc must be the CPU's pc and map to the TB's physical page."""
    pc = tb_pc(cpu, tb)
    n[kind] += 1
    if pc & TOP and n[kind + " kernel"] >= KERNEL_SAMPLE:
        return pc
    k = key(tb)
    t = (kind, k, pc, tb.page_addr[0])
    if t in checked:
        return pc
    checked.add(t)
    if pc & TOP:
        n[kind + " kernel"] += 1
    if pc != panda.arch.get_pc(cpu):
        bad(kind + ": tb_pc is the CPU's pc", f"{pc:#x} cpu {panda.arch.get_pc(cpu):#x}")
    st["tbs"][k] = (tb.cflags & CF_PCREL, tb.pc_bound)
    phys = panda.virt_to_phys(cpu, pc)
    if phys in BAD:
        st["unmapped"] += 1
        return pc
    delta = phys - tb.page_addr[0]
    st["delta"][delta] += 1
    if not pc & TOP:
        st["user_delta"][delta] += 1
        st["where"][(k, tb.page_addr[0])].add((pc, delta))
    return pc


# The images have one vCPU.
@panda.cb_before_block_translate(enabled=False)
def bbt(cpu, pc):
    last["translating"] = pc


@panda.cb_after_block_translate(enabled=False)
def abt(cpu, tb):
    n["after_block_translate"] += 1
    pc = tb_pc(cpu, tb)
    if pc != last["translating"]:
        bad("after_block_translate", f"{pc:#x} translating {last['translating']}")


@panda.cb_before_block_exec(enabled=False)
def bbe(cpu, tb):
    last["bbe"] = (tb, at_entry("before_block_exec", cpu, tb))


@panda.cb_after_block_exec(enabled=False)
def abe(cpu, tb, exit_code):
    n["after_block_exec"] += 1
    pc = tb_pc(cpu, tb)
    t, want = last["bbe"]
    if t != tb or want != pc:
        bad("after_block_exec", f"{pc:#x} entered {want}")


@panda.cb_start_block_exec(enabled=False)
def sbe(cpu, tb):
    last["sbe"] = (tb, at_entry("start_block_exec", cpu, tb))


@panda.cb_end_block_exec(enabled=False)
def ebe(cpu, tb):
    n["end_block_exec"] += 1
    pc = tb_pc(cpu, tb)
    t, want = last["sbe"]
    if t != tb or want != pc:
        bad("end_block_exec", f"{pc:#x} started {want}")


NAMES = ["bbt", "abt", "bbe", "abe", "sbe", "ebe"]


@panda.queue_blocking
def drive():
    panda.revert_sync("root")
    panda.run_serial_cmd("true")
    for name in NAMES:
        panda.enable_callback(name)
    panda.run_serial_cmd(WORK, timeout=1200)
    for name in NAMES:
        panda.disable_callback(name)
    st["out"] = panda.run_serial_cmd("cat /tmp/o1 /tmp/o2")
    panda.end_analysis()


panda.run()

checks = {}
c = n
for kind in ["before_block_exec", "after_block_exec", "start_block_exec", "end_block_exec",
             "after_block_translate"]:
    checks[f"{kind}: tb_pc is the block's address ({c[kind]} blocks)"] = (
        c[kind] > 0 and not any(b.startswith(kind) for b in st["bad"]), "")
ram_delta = st["delta"].most_common(1)[0][0] if st["delta"] else None
user_wrong = sum(n for d, n in st["user_delta"].items() if d != ram_delta)
checks["user blocks: tb_pc maps to the TB's physical address"] = (
    sum(st["user_delta"].values()) > 0 and user_wrong == 0,
    f"{sum(st['user_delta'].values())} distinct (tb, address) pairs, {user_wrong} elsewhere: {dict(st['user_delta'].most_common(4))}")
kernel_wrong = sum(n for d, n in st["delta"].items() if d != ram_delta) - user_wrong
checks["kernel blocks: tb_pc maps to the TB's physical address"] = (
    kernel_wrong == 0, f"{kernel_wrong} elsewhere: {dict(st['delta'].most_common(4))}")
shared = {k: v for k, v in st["where"].items() if len({pc for pc, _ in v}) > 1}
addrs = sorted({pc for v in shared.values() for pc, _ in v})
checks["shared TBs: one TB at several virtual addresses, each right"] = (
    len(shared) >= 10 and all(d == ram_delta for v in shared.values() for _, d in v),
    f"{len(shared)} TBs at {len(addrs)} addresses, e.g. "
    + ", ".join(sorted(f"{pc:#x}" for pc, _ in next(iter(shared.values()), set())))[:200])
tbs = st["tbs"].values()
unbound = sum(1 for pcrel, bound in tbs if pcrel and not bound)
checks["block callbacks leave TBs unbound (CF_PCREL, tb.pc unused)"] = (
    unbound > 0 and not any(bound for _, bound in tbs), f"{unbound} of {len(tbs)} TBs unbound")
out = st.get("out", "").strip().splitlines()
checks["guest ran the workload (head twice)"] = (len(out) == 2 and out[0] == out[1], repr(out))

print(f"TBPC arch={arch} checked={dict(c)} bad={dict(st['checked'])} unmapped={st['unmapped']} ram_delta={ram_delta}")
for kind, items in st["bad"].items():
    print(f"  {kind}: {items}")
failed = False
for name, (ok, detail) in checks.items():
    print(f"  {'PASS' if ok else 'FAIL'} {name}" + (f": {detail}" if detail else ""))
    failed |= not ok
sys.exit(1 if failed else 0)
