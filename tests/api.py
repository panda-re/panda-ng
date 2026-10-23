"""Standard PANDA API checks on an image built by pandare2.mkimage.

Run: panda-python tests/api.py <dir>/debian-13-x86_64.json

Every check is against ground truth from the guest. tests/programs/api_<arch>.S makes two
hypercalls, (MAGIC, n, &marker, sp, pc of the hypercall), and stores "STOREDOK" at marker+16
between them, then writes its marker to stdout. At hypercall 1 the test reads and writes guest
memory through the API (virtual and physical, user and kernel addresses) and turns on the
per-instruction and memory callbacks; at hypercall 2 it turns them off. What the program prints
shows the writes landed; what the callbacks saw between the hypercalls must include the program's
own pc and store. A savevm/loadvm round trip checks snapshots.
"""
import base64
import json
import lzma
import os
import sys
from collections import Counter

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "programs"))
from api_programs import PROGRAMS  # noqa: E402

from pandare2 import Panda  # noqa: E402

MAGIC = 0x50616e64
MARKER = b"AAAAAAAABBBBBBBBCCCCCCCCtailtail"
RETADDR_REG = {"aarch64": "LR", "riscv64": "RA"}
MAP_ADDR = 0x10_0000_0000      # 64 GiB: past the test images' RAM, inside both arches' phys space
EXPECTED_OUTPUT = "VIRTWRITPHYSWRITSTOREDOKtailtail"
PROGRAM_BASE, PROGRAM_END = 0x400000, 0x420000

panda = Panda(generic=sys.argv[1])
arch = panda.arch_name
panda.enable_precise_pc()
image_dir = os.path.dirname(os.path.abspath(sys.argv[1]))
with open(sys.argv[1]) as f:
    image = json.load(f)
with lzma.open(os.path.join(image_dir, image["isf"])) as f:
    banner_addr = json.load(f)["symbols"]["linux_banner"]["address"]

# Callback types the engine doesn't invoke yet (no call site in qemu_builder's series or its
# panda/ and contrib/plugins/ sources), and other known gaps. A gap is reported, not failed; a gap
# that passes fails the run, so this list stays true. before_loadvm isn't registered at all: its
# trampoline is commented out in panda/src/callbacks.c and registering it aborts.
GAPS = {
    "mmio_before_write fires": "no call site for panda_callbacks_mmio_before_write",
    "mmio_after_read fires": "commented out in contrib/plugins/panda_plugin_interface.c",
    "before_handle_interrupt fires": "the cpu_exec_interrupt wrapper is commented out in panda/src/wrap_ops.c",
    "current_pc is the hypercall's pc (precise pc)":
        "panda_enable_precise_pc sets panda_update_pc, which nothing reads; pc is the TB's start",
}
checks = {}
counts = Counter()
seen = {"tb_pc_exec": [0, []], "tb_pc_translate": [0, []], "blk": [], "sbe": [], "insn_pcs": set(), "virt_writes": [], "virt_reads": [], "phys_writes": [],
        "before_writes": [], "before_reads": [], "phys_before_writes": [], "asids": [], "asid_pairs": []}


def check(name, ok, detail=""):
    checks[name] = (bool(ok), detail)


def in_program(pc):
    return PROGRAM_BASE <= pc < PROGRAM_END


# Cheap callbacks, on for the whole run.
@panda.cb_asid_changed
def asid_changed(cpu, old, new):
    counts["asid_changed"] += 1
    seen["asids"].append(new)
    if len(seen["asid_pairs"]) < 5000:
        # asid_changed runs before the write: current_asid is still the old value.
        seen["asid_pairs"].append((old, new, panda.current_asid(cpu)))
    return False


@panda.cb_before_handle_exception
def exception(cpu, index):
    counts["before_handle_exception"] += 1
    return index


@panda.cb_before_handle_interrupt
def interrupt(cpu, index):
    counts["before_handle_interrupt"] += 1
    return index


@panda.cb_after_loadvm
def after_loadvm(cpu):
    counts["after_loadvm"] += 1


@panda.cb_main_loop_wait
def main_loop_wait():
    counts["main_loop_wait"] += 1


@panda.cb_mmio_after_read
def mmio_read(cpu, physaddr, vaddr, size, val):
    counts["mmio_after_read"] += 1


@panda.cb_mmio_before_write
def mmio_write(cpu, physaddr, vaddr, size, val):
    counts["mmio_before_write"] += 1


# Per-block, per-instruction and memory callbacks: only between the program's two hypercalls.
WINDOW = []


def window_cb(decorator, name):
    def register(fn):
        WINDOW.append(name)
        return decorator(name=name, enabled=False)(fn)
    return register


def tb_pc_check(key, tb_pc, want):
    # Every TB in the window, not only the program's: [checked, first mismatches].
    seen[key][0] += 1
    if tb_pc != want and len(seen[key][1]) < 4:
        seen[key][1].append(f"{tb_pc:#x}!={want:#x}")


@window_cb(panda.cb_before_block_translate, "before_block_translate")
def bbt(cpu, pc):
    seen["translating"] = pc
    if in_program(pc):
        counts["before_block_translate"] += 1


# Block addresses come from panda.tb_pc (tb.pc is 0 for a shared CF_PCREL block). Here
# insn_translate is registered, so every TB is bound and tb_pc is tb.pc; tests/tbpc.py covers
# unbound blocks.
@window_cb(panda.cb_after_block_translate, "after_block_translate")
def abt(cpu, tb):
    pc = panda.tb_pc(cpu, tb)
    tb_pc_check("tb_pc_translate", pc, seen.get("translating"))
    if in_program(pc):
        counts["after_block_translate"] += 1


@window_cb(panda.cb_before_block_exec, "before_block_exec")
def bbe(cpu, tb):
    pc = panda.tb_pc(cpu, tb)
    tb_pc_check("tb_pc_exec", pc, panda.arch.get_pc(cpu))
    if in_program(pc):
        counts["before_block_exec"] += 1
        seen["blk"].append(("b", pc, None))


@window_cb(panda.cb_after_block_exec, "after_block_exec")
def abe(cpu, tb, exit_code):
    pc = panda.tb_pc(cpu, tb)
    if in_program(pc):
        counts["after_block_exec"] += 1
        seen["blk"].append(("a", pc, exit_code))


@window_cb(panda.cb_start_block_exec, "start_block_exec")
def sbe(cpu, tb):
    pc = panda.tb_pc(cpu, tb)
    if in_program(pc):
        counts["start_block_exec"] += 1
        seen["sbe"].append(pc)


@window_cb(panda.cb_end_block_exec, "end_block_exec")
def ebe(cpu, tb):
    pc = panda.tb_pc(cpu, tb)
    if in_program(pc):
        counts["end_block_exec"] += 1
        # The TB at label 1, past the hypercall and a jump (x86: out, jmp, 2 bytes each), ends in a
        # call (x86), bl (aarch64) or jal (riscv64), after zeroing where its return address goes.
        if pc == seen["hc1"] + (4 if arch == "x86_64" else 8) and "call_slot" not in seen:
            seen["call_slot"] = (panda.virtual_memory_read(cpu, seen["sp"] - 8, 8, fmt="int")
                                 if arch == "x86_64" else panda.arch.get_reg(cpu, RETADDR_REG[arch]))


@window_cb(panda.cb_insn_translate, "insn_translate")
def it(cpu, pc):
    return in_program(pc)


@window_cb(panda.cb_insn_exec, "insn_exec")
def ie(cpu, pc):
    if in_program(pc):
        counts["insn_exec"] += 1
        seen["insn_pcs"].add(pc)
    return 0


@window_cb(panda.cb_virt_mem_after_write, "virt_mem_after_write")
def vmaw(cpu, pc, addr, size, buf):
    if in_program(pc):
        seen["virt_writes"].append((addr, size, bytes(panda.ffi.buffer(buf, size))))


@window_cb(panda.cb_virt_mem_before_write, "virt_mem_before_write")
def vmbw(cpu, pc, addr, size, buf):
    if in_program(pc):
        counts["virt_mem_before_write"] += 1
        # The value about to be stored, and what memory holds before the store.
        seen["before_writes"].append((addr, size, bytes(panda.ffi.buffer(buf, size)),
                                      bytes(panda.virtual_memory_read(cpu, addr, size))))


@window_cb(panda.cb_virt_mem_before_read, "virt_mem_before_read")
def vmbr(cpu, pc, addr, size):
    if in_program(pc):
        seen["before_reads"].append((addr, size))


@window_cb(panda.cb_phys_mem_before_write, "phys_mem_before_write")
def pmbw(cpu, pc, addr, size, buf):
    if in_program(pc):
        seen["phys_before_writes"].append((addr, size, bytes(panda.ffi.buffer(buf, size))))


@window_cb(panda.cb_phys_mem_after_write, "phys_mem_after_write")
def pmaw(cpu, pc, addr, size, buf):
    if in_program(pc):
        seen["phys_writes"].append((addr, size, bytes(panda.ffi.buffer(buf, size))))


@window_cb(panda.cb_virt_mem_after_read, "virt_mem_after_read")
def vmar(cpu, pc, addr, size, buf):
    counts["virt_mem_after_read"] += 1
    if in_program(pc):
        seen["virt_reads"].append((addr, size, bytes(panda.ffi.buffer(buf, size))))


@window_cb(panda.cb_phys_mem_after_read, "phys_mem_after_read")
def pmar(cpu, pc, addr, size, buf):
    counts["phys_mem_after_read"] += 1


def args(cpu):
    return [panda.arch.get_arg(cpu, i, convention="syscall") for i in range(1, 6)]


def hypercall_1(cpu, marker, sp, pc):
    seen["sp"], seen["hc1"] = sp, pc
    check("hypercall arguments", in_program(marker) and in_program(pc), f"marker={marker:#x} pc={pc:#x}")
    check("current_pc is the hypercall's pc (precise pc)", panda.current_pc(cpu) == pc and panda.arch.get_pc(cpu) == pc,
          f"{panda.current_pc(cpu):#x} {panda.arch.get_pc(cpu):#x} want {pc:#x}")
    check("current_sp", panda.current_sp(cpu) == sp, f"{panda.current_sp(cpu):#x} want {sp:#x}")
    check("in_kernel is false in user mode",
          not panda.in_kernel(cpu) and not panda.in_kernel_mode(cpu) and not panda.in_kernel_code_linux(cpu))
    asid = panda.current_asid(cpu)
    last = seen["asids"][-1] if seen["asids"] else None
    check("current_asid is the last asid_changed value", last is not None and asid == last,
          f"{asid:#x} last {last}")
    check("current_asid is nonzero for a user process", asid != 0, f"{asid:#x}")
    # The process identifier is the top-level page table's physical address. Not always page
    # aligned: arm64 kernels whose top level is smaller than a page (52-bit VA built, 48-bit at
    # run time) take it from a cache (debian-13-aarch64: 0x...a00). x86 PAE's is 32-byte aligned.
    check("current_asid is a page table's address (32-byte aligned, RAM)",
          asid % 32 == 0 and panda.physical_memory_read(asid, 8) is not None, f"{asid:#x}")
    seen["asid_field"] = panda.current_asid_field(cpu)

    # User memory.
    data = panda.virtual_memory_read(cpu, marker, len(MARKER))
    check("virtual_memory_read (user)", bytes(data) == MARKER, repr(bytes(data)))
    phys = panda.virt_to_phys(cpu, marker)
    check("virt_to_phys (user)", phys not in (-1, 2**64 - 1), f"{phys:#x}")
    pdata = panda.physical_memory_read(phys, len(MARKER))
    check("physical_memory_read at virt_to_phys", bytes(pdata) == MARKER, repr(bytes(pdata)))
    host = panda.libpanda.panda_map_virt_to_host(cpu, marker, len(MARKER))
    check("map_virt_to_host", host != panda.ffi.NULL
          and bytes(panda.ffi.buffer(host, len(MARKER))) == MARKER)

    # Kernel memory, from user mode.
    kdata = bytes(panda.virtual_memory_read(cpu, banner_addr, 14))
    check("virtual_memory_read (kernel, from user mode)", kdata == b"Linux version ", repr(kdata))
    kphys = panda.virt_to_phys(cpu, banner_addr)
    kok = kphys not in (-1, 2**64 - 1)
    check("virt_to_phys (kernel, from user mode)", kok, f"{kphys:#x}")
    if kok:
        check("physical_memory_read at kernel virt_to_phys",
              bytes(panda.physical_memory_read(kphys, 14)) == b"Linux version ")

    # Writes; the program prints them.
    panda.virtual_memory_write(cpu, marker, b"VIRTWRIT")
    check("virtual_memory_write reads back", bytes(panda.virtual_memory_read(cpu, marker, 8)) == b"VIRTWRIT")
    panda.physical_memory_write(panda.virt_to_phys(cpu, marker + 8), b"PHYSWRIT")
    check("physical_memory_write reads back virtually",
          bytes(panda.virtual_memory_read(cpu, marker + 8, 8)) == b"PHYSWRIT")
    seen["store_phys"] = panda.virt_to_phys(cpu, marker + 16)

    # Plugins: load, find, unload.
    panda.load_plugin("cskeleton")
    loaded = panda.libpanda.panda_get_plugin_by_name(b"cskeleton") != panda.ffi.NULL
    panda.unload_plugin("cskeleton")
    check("load_plugin / get_plugin_by_name", loaded)

    panda.enable_memcb()
    for name in WINDOW:
        panda.enable_callback(name)
    # TB chaining stays on: the block-exec callbacks run from each block's own code.
    panda.flush_tb()


def hypercall_2(cpu):
    # Unloading happens before the next TB lookup (before_find_fast); many have run since hypercall 1.
    seen["unloaded"] = panda.libpanda.panda_get_plugin_by_name(b"cskeleton") == panda.ffi.NULL
    for name in WINDOW:
        panda.disable_callback(name)
    panda.disable_memcb()
    seen["window_closed"] = True


@panda.hypercall(MAGIC)
def hypercall(cpu):
    counts["guest_hypercall"] += 1
    _, op, marker, sp, pc = args(cpu)
    seen["marker"] = marker
    if op == 1:
        hypercall_1(cpu, marker, sp, pc)
    elif op == 2:
        hypercall_2(cpu)


@panda.queue_blocking
def drive():
    panda.revert_sync("root")
    data = base64.b64decode(PROGRAMS[arch])
    panda.run_serial_cmd("rm -f /tmp/api")
    for i in range(0, len(data), 64):
        chunk = "".join(f"\\x{b:02x}" for b in data[i:i + 64])
        panda.run_serial_cmd(f"printf '{chunk}' >> /tmp/api")
    seen["output"] = panda.run_serial_cmd("chmod +x /tmp/api && /tmp/api; echo; echo rc=$?")

    # Snapshots: state saved with savevm comes back with loadvm, and delvm removes it.
    panda.run_serial_cmd("echo one > /tmp/snap")
    seen["savevm"] = panda.run_monitor_cmd("savevm api-test").strip()
    panda.run_serial_cmd("echo two > /tmp/snap")
    panda.revert_sync("api-test")
    seen["after_revert"] = panda.run_serial_cmd("cat /tmp/snap").strip()
    panda.run_monitor_cmd("delvm api-test")
    seen["snapshots"] = panda.run_monitor_cmd("info snapshots")
    # map_memory: new RAM past the guest's, read back through physical memory.
    panda.map_memory("api-test-ram", 0x10000, MAP_ADDR)
    panda.physical_memory_write(MAP_ADDR + 0x100, b"MAPPEDOK")
    seen["mapped"] = bytes(panda.physical_memory_read(MAP_ADDR + 0x100, 8))
    panda.end_analysis()


panda.run()

out = seen.get("output", "")
check("guest output shows the API writes and its own store", EXPECTED_OUTPUT in out and "rc=0" in out, repr(out))
check("guest_hypercall: both hypercalls", counts["guest_hypercall"] == 2, counts["guest_hypercall"])
marker = seen.get("marker", 0)
check("insn_translate/insn_exec see the program", counts["insn_exec"] > 5 and any(in_program(p) for p in seen["insn_pcs"]),
      f"{counts['insn_exec']} insns")
check("virt_mem_after_read sees the load",
      any(r[0] == marker + 16 and r[1] == 8 for r in seen["virt_reads"]), seen["virt_reads"][:4])
check("virt_mem_after_write sees the store",
      (marker + 16, 8, b"STOREDOK") in seen["virt_writes"], seen["virt_writes"][:4])
bw = [w for w in seen["before_writes"] if w[:3] == (marker + 16, 8, b"STOREDOK")]
check("virt_mem_before_write sees the store's value before memory has it",
      bw and bw[0][3] == b"CCCCCCCC", seen["before_writes"][:4])
check("virt_mem_before_read sees the load", (marker + 16, 8) in seen["before_reads"], seen["before_reads"][:4])
check("phys_mem_before_write sees the store",
      (seen.get("store_phys"), 8, b"STOREDOK") in seen["phys_before_writes"], seen["phys_before_writes"][:4])
check("phys_mem_after_write sees the store",
      (seen.get("store_phys"), 8, b"STOREDOK") in seen["phys_writes"], seen["phys_writes"][:4])
for key, where in [("tb_pc_translate", "after_block_translate (before_block_translate's pc)"),
                   ("tb_pc_exec", "before_block_exec (the CPU's pc)")]:
    n, bad = seen[key]
    check(f"tb_pc is the block's address in {where}", n and not bad, f"{n} TBs, {bad}")
# The window opens inside hypercall 1's block (whose start passed with the callbacks off: no
# before/after for it) and closes inside hypercall 2's (before ran, after didn't).
blk = seen["blk"]
seen["bbe"] = [e[1] for e in blk if e[0] == "b"]
seen["abe"] = [e[1] for e in blk if e[0] == "a"]
alternate = all(blk[i][0] == ("b" if i % 2 == 0 else "a") and (i % 2 == 0 or blk[i][1] == blk[i - 1][1])
                for i in range(len(blk)))
check("chaining on: before/after_block_exec pair up on every block start_block_exec sees",
      seen["bbe"] and alternate and seen["abe"] == seen["bbe"][:-1] and seen["sbe"] == seen["bbe"],
      f"before={[hex(p) for p in seen['bbe']]} after={[hex(p) for p in seen['abe']]} "
      f"start={[hex(p) for p in seen['sbe']]} blk={[(k, hex(p), c) for k, p, c in blk]}")
check("after_block_exec: every program block ran to its end (exit code 0)",
      all(c == 0 for k, _, c in blk if k == "a"), [c for k, _, c in blk if k == "a"])
check("end_block_exec runs after the block's last instruction", seen.get("call_slot"),
      f"return address {seen.get('call_slot')}")
for name in ["before_block_translate", "after_block_translate", "before_block_exec", "after_block_exec",
             "start_block_exec", "end_block_exec", "virt_mem_before_write", "virt_mem_after_read",
             "phys_mem_after_read", "asid_changed", "before_handle_exception", "before_handle_interrupt",
             "main_loop_wait", "mmio_after_read", "mmio_before_write"]:
    check(f"{name} fires", counts[name] > 0, counts[name])
pairs = seen["asid_pairs"]
bad = [(hex(o), hex(n), hex(c)) for o, n, c in pairs if o == n or c != o]
chain = [i for i in range(1, len(pairs)) if pairs[i][0] != pairs[i - 1][1]]
check("asid_changed: old != new, old is current_asid, each old is the previous new",
      pairs and not bad and len(chain) <= 3,   # a loadvm restores another asid without a write
      f"{len(pairs)} changes, bad {bad[:4]}, breaks at {chain[:4]}")
check("current_asid_field reads", isinstance(seen.get("asid_field"), int), seen.get("asid_field"))
check("after_loadvm fires on each revert", counts["after_loadvm"] == 2, counts["after_loadvm"])
check("savevm, loadvm restores the saved state", seen.get("after_revert") == "one",
      f"savevm={seen.get('savevm')!r} cat={seen.get('after_revert')!r}")
check("map_memory adds RAM that physical memory r/w reaches", seen.get("mapped") == b"MAPPEDOK",
      seen.get("mapped"))
check("unload_plugin unloads it (by hypercall 2)", seen.get("unloaded"))
check("delvm removes the snapshot", "api-test" not in seen.get("snapshots", "api-test"))

print(f"API arch={arch} counts={dict(counts)}")
failed = False
for name, (ok, detail) in checks.items():
    gap = GAPS.get(name, GAPS.get((name, arch)))
    if gap:
        status = "XPASS" if ok else "GAP"
        detail = "gap closed: take it out of GAPS" if ok else gap
        failed |= ok
    else:
        status = "PASS" if ok else "FAIL"
        failed |= not ok
    print(f"  {status} {name}" + ("" if status == "PASS" else f": {detail}"))
sys.exit(1 if failed else 0)
