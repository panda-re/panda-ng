"""panda_tb_pc on blocks QEMU shares between virtual addresses (CF_PCREL), on an image built by
pandare2.mkimage.

Run: panda-python tests/tbpc.py <dir>/debian-13-x86_64.json

Entry-only callbacks preserve PCREL sharing between virtual addresses. Enabling
normal-end callbacks binds source addresses so end observers can identify the
source after the live PC advances. Both phases check CPU-PC synchronization
and physical page identity across processes using different ASLR mappings.
"""
import os
import sys
import time
from collections import Counter, defaultdict

from pandare2 import Panda

CF_PCREL = 0x00020000
# Guest time from instructions (as tests/blocks.py): with Python callbacks on every block and time
# following the host, timer ticks and the scheduling they cause take all of the slowed guest's time
# and the workload starves (seen on all three arches; on aarch64 it ran past the 30-minute limit).
# On every arch: an aarch64 root snapshot taken without icount loads with it since engine patch
# 0081 (PMCEID0).
panda = Panda(generic=sys.argv[1], extra_args=["-icount", "shift=6,sleep=off"])
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
last = {"bbe": (None, None), "sbe": (None, None), "translating": None, "exc": None, "phase": "off"}
# Diagnostics: TBPC_GAP=<s> sleeps between the callbacks' enable/disable calls, TBPC_ORDER=forward disables
# them in the order they were enabled (the order before this test learned to disable in reverse).
GAP = float(os.environ.get("TBPC_GAP", "0"))
FORWARD = os.environ.get("TBPC_ORDER") == "forward"
tb_pc = panda.libpanda.panda_tb_pc


def bad(kind, detail):
    if len(st["bad"][kind]) < 8:
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
    translating = last["translating"]
    last["translating"] = None
    if translating is None:
        return   # translation began before the callbacks were enabled
    if pc != translating:
        bad("after_block_translate", f"{pc:#x} translating {translating:#x}")


def describe(cpu, tb, pc, t, want, extra=""):
    """A wrong pairing in full: this block, the one its start callback saw, the CPU, what came before."""
    exc = last["exc"]
    return (f"tb_pc {pc:#x} tb {key(tb):#x} tb->pc {tb.pc:#x} cflags {tb.cflags:#x} bound {tb.pc_bound}{extra}"
            f" | started tb {key(t):#x} at {want:#x} | cpu pc {panda.arch.get_pc(cpu):#x}"
            f" | phase {last['phase']}, enabled {[x for x in NAMES if panda.is_callback_enabled(x)]}"
            f" | last exception {exc[0] if exc else None} {n['before_block_exec'] - exc[1] if exc else '-'} blocks ago")


@panda.cb_before_handle_exception(enabled=False)
def bhe(cpu, index):
    last["exc"] = (index, n["before_block_exec"])
    return index


@panda.cb_before_block_exec(enabled=False)
def bbe(cpu, tb):
    last["bbe"] = (tb, at_entry("before_block_exec", cpu, tb))


@panda.cb_after_block_exec(enabled=False)
def abe(cpu, tb, exit_code):
    n["after_block_exec"] += 1
    pc = tb_pc(cpu, tb)
    t, want = last["bbe"]
    if t is None:
        return   # the block was running when the callbacks were enabled
    if t != tb or want != pc:
        bad("after_block_exec", describe(cpu, tb, pc, t, want, f" exit_code {exit_code}"))


@panda.cb_start_block_exec(enabled=False)
def sbe(cpu, tb):
    last["sbe"] = (tb, at_entry("start_block_exec", cpu, tb))


@panda.cb_end_block_exec(enabled=False)
def ebe(cpu, tb):
    n["end_block_exec"] += 1
    pc = tb_pc(cpu, tb)
    t, want = last["sbe"]
    if t is None:
        return   # the block was running when the callbacks were enabled
    if t != tb or want != pc:
        bad("end_block_exec", describe(cpu, tb, pc, t, want))


# In this order each "after" callback is enabled after, and disabled before, the one it pairs with:
# panda.enable/disable_callback run from the driver thread while the guest runs, so between two of
# these calls the guest runs blocks (seen: 14 kernel blocks on riscv64 ran with before_block_exec
# already off and after_block_exec still on, each compared with the last block before_block_exec saw).
NAMES = ["bhe", "bbt", "abt", "bbe", "abe", "sbe", "ebe"]


def switch(on):
    last["translating"] = None   # do not reuse a translation from the prior enable window
    last["phase"] = "enabling" if on else "disabling"
    for name in (NAMES if on or FORWARD else reversed(NAMES)):
        (panda.enable_callback if on else panda.disable_callback)(name)
        if GAP:
            time.sleep(GAP)
    last["phase"] = "on" if on else "off"


@panda.queue_blocking
def drive():
    panda.revert_sync("root")
    # A short command to type while the callbacks run: the serial driver busy-waits per character.
    panda.run_serial_cmd(f"echo '{WORK}' > /tmp/w.sh")
    # With chaining on, a Python callback on every block starves the guest's serial output (its
    # driver busy-waits on the transmitter): commands crawl. The callbacks run from the blocks'
    # own code either way (lane-chain), after/end_block_exec once the block has stopped.
    panda.disable_tb_chaining()
    for name in ("bhe", "bbt", "abt", "bbe", "sbe"):
        panda.enable_callback(name)
    panda.run_serial_cmd("sh /tmp/w.sh", timeout=3000)
    st["entry_shared"] = sum(len({pc for pc, _ in v}) > 1 for v in st["where"].values())
    st["entry_unbound"] = sum(pcrel and not bound for pcrel, bound in st["tbs"].values())
    switch(False)
    st["where"].clear()
    st["tbs"].clear()
    switch(True)
    panda.run_serial_cmd("sh /tmp/w.sh", timeout=3000)
    switch(False)
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
checks["entry-only callbacks preserve virtual-address sharing"] = (
    st.get("entry_shared", 0) >= 10 and st.get("entry_unbound", 0) > 0,
    f"{st.get('entry_shared', 0)} shared, {st.get('entry_unbound', 0)} unbound")
tbs = st["tbs"].values()
checks["end observers bind their source address"] = (
    len(tbs) > 0 and all(bound for pcrel, bound in tbs if pcrel),
    f"{len(tbs)} observed source blocks")
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
