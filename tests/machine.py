"""The standard PANDA API on a machine built with pandare2.MachineBuilder (the engine's built-in
`configurable` machine), with no image: a bare-metal program on a CPU, a file-backed ROM, RAM, an
unimplemented-peripheral region and a UART whose interrupt is wired to the CPU.

Run: panda-python tests/machine.py <arch> [<arch> ...]   (aarch64, arm, riscv64)
Each arch runs in its own process (one Panda per process).

tests/programs/cfg_<arch>.S reads the ROM's first word, prints "CFG-OK <arch>" on the UART, makes
hypercall 1 (MAGIC, 1, &marker, rom word, sp), stores "STOREDOK" at marker+16 and loads it, writes
and reads the unimplemented region, makes hypercall 2 (MAGIC, 2, &marker, unimp read, loaded value),
turns on the UART's interrupt and makes hypercall 3 (MAGIC, 3, 0, the CPU's pending-interrupt
register). Between hypercalls 1 and 2 the per-block, per-instruction and memory callbacks are on.
"""
import base64
import os
import subprocess
import sys
import tempfile
import threading
from collections import Counter

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "programs"))
from cfg_programs import PROGRAMS  # noqa: E402

MAGIC = 0x50616e64
MARKER = b"AAAAAAAABBBBBBBBCCCCCCCCtailtail"
ROMDATA = b"ROMDATA!"

# Per arch: CPU, RAM base, ROM, UART (type, base, irq target), unimplemented region, the pending bit.
LAYOUT = {
    "aarch64": dict(cpu="cortex-a53", ram=0x40000000, rom=0x0, uart=("pl011", 0x09000000, "cpu:0"),
                    unimp=0x0a000000, pending=1 << 7, word=8),
    "arm": dict(cpu="cortex-a15", ram=0x40000000, rom=0x0, uart=("pl011", 0x09000000, "cpu:0"),
                unimp=0x0a000000, pending=1 << 7, word=4),
    "riscv64": dict(cpu="rv64", ram=0x80000000, rom=0x20000000,
                    uart=("serial-mm", 0x10000000, "cpu:11"), unimp=0x10010000, pending=1 << 11,
                    word=8),
}


def run(arch):
    from pandare2 import Panda, MachineBuilder

    L = LAYOUT[arch]
    tmp = tempfile.mkdtemp(prefix="machine-")
    elf = os.path.join(tmp, f"cfg_{arch}.elf")
    with open(elf, "wb") as f:
        f.write(base64.b64decode(PROGRAMS[arch]))
    rom = os.path.join(tmp, "rom.bin")
    with open(rom, "wb") as f:
        f.write(ROMDATA)
    serial = os.path.join(tmp, "serial.out")

    utype, ubase, uirq = L["uart"]
    m = (MachineBuilder(arch)
         .cpu(L["cpu"])
         .ram(L["ram"], "16M")
         .rom_file(L["rom"], rom, name="boot")
         .unimp("sysctl", L["unimp"], 0x1000)
         .device(utype, ubase, id="uart0", irq=[uirq], chardev="serial0")
         .kernel(elf))
    print("args:", " ".join(m.args()), flush=True)
    panda = Panda(arch=arch, machine=m, extra_args=["-display", "none", "-serial", f"file:{serial}"])

    checks, counts = {}, Counter()
    seen = {"insn_pcs": set(), "virt_writes": [], "virt_reads": [], "phys_writes": [], "bbe": [],
            "abt": []}

    def check(name, ok, detail=""):
        checks[name] = (bool(ok), detail)

    def in_program(pc):
        return L["ram"] <= pc < L["ram"] + 0x10000

    def args(cpu):
        return [panda.arch.get_arg(cpu, i, convention="syscall") for i in range(1, 6)]

    @panda.cb_after_machine_init
    def ami(cpu):
        counts["after_machine_init"] += 1
        seen["reset_pc"] = panda.arch.get_pc(cpu)

    @panda.cb_main_loop_wait
    def mlw():
        counts["main_loop_wait"] += 1

    WINDOW = []

    def window_cb(decorator, name):
        def register(fn):
            WINDOW.append(name)
            return decorator(name=name, enabled=False)(fn)
        return register

    @window_cb(panda.cb_after_block_translate, "after_block_translate")
    def abt(cpu, tb):
        if in_program(tb.pc):
            seen["abt"].append(tb.pc)

    @window_cb(panda.cb_before_block_exec, "before_block_exec")
    def bbe(cpu, tb):
        if in_program(tb.pc):
            seen["bbe"].append((tb.pc, panda.arch.get_pc(cpu)))

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

    @window_cb(panda.cb_virt_mem_after_read, "virt_mem_after_read")
    def vmar(cpu, pc, addr, size, buf):
        if in_program(pc):
            seen["virt_reads"].append((addr, size))

    @window_cb(panda.cb_phys_mem_after_write, "phys_mem_after_write")
    def pmaw(cpu, pc, addr, size, buf):
        if in_program(pc):
            seen["phys_writes"].append((addr, size, bytes(panda.ffi.buffer(buf, size))))

    @panda.hypercall(MAGIC)
    def hypercall(cpu):
        counts["guest_hypercall"] += 1
        _, op, a, b, c = args(cpu)
        if op == 1:
            seen["marker"] = a
            w = L["word"]
            check("hypercall 1: the program read the file-backed ROM",
                  b == int.from_bytes(ROMDATA[:w], "little"), f"{b:#x}")
            check("hypercall arguments and pc are in the program", in_program(a)
                  and in_program(panda.arch.get_pc(cpu)), f"marker={a:#x} pc={panda.arch.get_pc(cpu):#x}")
            check("current_sp is the program's sp", panda.current_sp(cpu) == c,
                  f"{panda.current_sp(cpu):#x} want {c:#x}")
            check("in_kernel (bare metal runs privileged)", panda.in_kernel(cpu))
            data = bytes(panda.virtual_memory_read(cpu, a, len(MARKER)))
            check("virtual_memory_read", data == MARKER, repr(data))
            phys = panda.virt_to_phys(cpu, a)
            check("virt_to_phys (MMU off: identity)", phys == a, f"{phys:#x}")
            check("physical_memory_read", bytes(panda.physical_memory_read(a, len(MARKER))) == MARKER)
            check("physical_memory_read of the ROM",
                  bytes(panda.physical_memory_read(L["rom"], 8)) == ROMDATA)
            panda.virtual_memory_write(cpu, a, b"VIRTWRIT")
            check("virtual_memory_write reads back physically",
                  bytes(panda.physical_memory_read(a, 8)) == b"VIRTWRIT")
            panda.enable_memcb()
            for name in WINDOW:
                panda.enable_callback(name)
            panda.disable_tb_chaining()
            panda.flush_tb()
        elif op == 2:
            for name in WINDOW:
                panda.disable_callback(name)
            panda.disable_memcb()
            panda.enable_tb_chaining()
            check("hypercall 2: the unimplemented region reads 0", b == 0, f"{b:#x}")
            want = int.from_bytes(b"STOREDOK"[:L["word"]], "little")
            check("hypercall 2: the program loaded its own store", c == want, f"{c:#x}")
        elif op == 3:
            check("UART interrupt wired to the CPU: pending at the CPU", b & L["pending"],
                  f"{b:#x} want bit {L['pending']:#x}")
            panda.end_analysis()

    timer = threading.Timer(120, lambda: (print("TIMEOUT", flush=True), os._exit(2)))
    timer.start()
    panda.run()
    timer.cancel()

    marker = seen.get("marker", 0)
    w = L["word"]
    out = open(serial, "rb").read().decode(errors="replace")
    check("serial: the program printed over the configured UART", f"CFG-OK {arch}" in out, repr(out))
    check("after_machine_init fires", counts["after_machine_init"] == 1, counts["after_machine_init"])
    check("main_loop_wait fires", counts["main_loop_wait"] > 0, counts["main_loop_wait"])
    check("guest_hypercall: three hypercalls", counts["guest_hypercall"] == 3, counts["guest_hypercall"])
    check("insn_translate/insn_exec see the program", counts["insn_exec"] > 5, counts["insn_exec"])
    check("after_block_translate sees the program", seen["abt"], seen["abt"][:4])
    check("before_block_exec: tb.pc is the CPU's pc", seen["bbe"] and all(p == q for p, q in seen["bbe"]),
          [(hex(p), hex(q)) for p, q in seen["bbe"][:4]])
    store = (marker + 16, w, b"STOREDOK"[:w])
    check("virt_mem_after_write sees the store", store in seen["virt_writes"], seen["virt_writes"][:4])
    check("phys_mem_after_write sees the store", store in seen["phys_writes"], seen["phys_writes"][:4])
    check("virt_mem_after_read sees the load", (marker + 16, w) in seen["virt_reads"], seen["virt_reads"][:4])
    check("virt_mem_after_write sees the store to the unimplemented region",
          any(a == L["unimp"] + 4 for a, _, _ in seen["virt_writes"]), seen["virt_writes"][:4])

    print(f"MACHINE arch={arch} counts={dict(counts)}")
    failed = False
    for name, (ok, detail) in checks.items():
        print(f"  {'PASS' if ok else 'FAIL'} {name}" + ("" if ok else f": {detail}"))
        failed |= not ok
    return 1 if failed else 0


if __name__ == "__main__":
    if len(sys.argv) == 2:
        sys.exit(run(sys.argv[1]))
    rc = 0
    for a in sys.argv[1:] or list(LAYOUT):
        r = subprocess.run([sys.executable, os.path.abspath(__file__), a])
        print(f"== {a}: {'PASS' if r.returncode == 0 else 'FAIL'} (rc={r.returncode})", flush=True)
        rc |= r.returncode
    sys.exit(rc)
