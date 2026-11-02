"""Taint mode: a device register (MMIO) as the taint source, end to end.

Run: panda-python tests/taint_mmio.py <dir>/<image>.json [workdir]   (aarch64, riscv64; mkimage or mkbusybox)

The console is a UART whose receive register the guest reads by MMIO (PL011 on Arm virt, 16550 on
RISC-V virt), so every character typed at the console is an MMIO load. Records a typed echo and an
untyped line, then, each in a fresh process:
- plain: replays with taint off; reports the instruction count at the untyped line's write;
- taint: replays with taint and labels on, labelling every load from the UART's data register with
  label 7 (Panda.taint_mmio_read). Checks:
  - the source callback tainted some loads;
  - every byte the shell reads from the console carries exactly the label 7 (register -> UART
    driver -> tty buffers -> read());
  - echo's write of the typed word is tainted on every byte, with exactly the label 7 (on aarch64
    this goes through glibc's FEAT_MOPS memcpy, whose helpers report their writes);
  - the untyped line (uname's output) is clean;
  - its write comes at the same instruction count as in the plain replay.
x86_64's UART is port I/O, not MMIO: the test says so and passes. (The RTC is not used: RISC-V virt
under ACPI has none, and the kernel prints its seconds through a digit-pair table, an address
dependence that taint does not follow.)
"""
import json
import os
import subprocess
import sys
import tempfile

from pandare2 import Panda

WORD = "typed-q7"
CMD = f"echo {WORD}; uname -m"
UART = {"aarch64": 0x09000000, "riscv64": 0x10000000}   # MMIO base of the console UART
LABEL = 7
# pandare2's defaults (analysis, guest-hypercall, rr) plus taint: syscalls2 sees no syscalls on Arm
# without guest-hypercall.
CAPS = ["analysis", "guest-hypercall", "rr", "taint"] + os.environ.get("TAINT_TEST_CAPS", "").split()


def child(mode, image, name, out):
    panda = Panda(generic=image, rr=True, capabilities=CAPS)
    base = UART[panda.arch_name]
    seen = {"icount": None, "writes": [], "reads": []}

    if mode == "record":
        @panda.queue_blocking
        def drive():
            panda.revert_sync("root")
            panda.record_cmd(CMD, recording_name=name, snap_name=None)
            panda.end_analysis()

        panda.run()
        return

    if mode == "taint":
        @panda.taint_mmio_read
        def uart_read(cpu, vaddr, size):
            phys = panda.virt_to_phys(cpu, vaddr)
            return LABEL if phys == base else 0     # the data register, offset 0 on both

        @panda.cb_start_block_exec(observe_only=True)
        def first(cpu, tb):
            # run_replay only requests the replay; the vCPU can run a block from the boot state
            # before it starts, and taint refuses anything but a replay.
            if not panda.libpanda.rr_in_replay():
                return
            panda.taint_enable_labels()
            seen["enabled"] = panda.taint_enable()
            panda.disable_callback("first")

        @panda.ppp("syscalls2", "on_sys_read_return", observe_only=True)
        def read(cpu, pc, fd, buf, count):
            n = panda.arch.get_retval(cpu, convention="syscall")
            if fd != 0 or not 0 < n <= min(count, 64):
                return
            text = panda.virtual_memory_read(cpu, buf, n).decode(errors="replace")
            seen["reads"].append([text, panda.taint_labels_va(cpu, buf, n)])

    @panda.ppp("syscalls2", "on_sys_write_enter", observe_only=True)
    def write(cpu, pc, fd, buf, count):
        if fd == 1:
            one_write(cpu, buf, count)

    # musl's stdio (busybox guests) writes with writev
    @panda.ppp("syscalls2", "on_sys_writev_enter", observe_only=True)
    def writev(cpu, pc, fd, iov, iovcnt):
        if fd != 1 or iovcnt > 64:
            return
        w = panda.bits // 8
        try:
            raw = panda.virtual_memory_read(cpu, iov, 2 * w * iovcnt)
        except ValueError:
            return
        for i in range(iovcnt):
            base = int.from_bytes(raw[2 * w * i:2 * w * i + w], panda.endianness)
            n = int.from_bytes(raw[2 * w * i + w:2 * w * (i + 1)], panda.endianness)
            one_write(cpu, base, n)

    def one_write(cpu, buf, count):
        if not 0 < count <= 64:
            return
        try:
            text = panda.virtual_memory_read(cpu, buf, count).decode(errors="replace")
        except ValueError:
            return
        if text.strip() == panda.arch_name:
            seen["icount"] = panda.rr_get_guest_instr_count()
        masks = labels = []
        if mode == "taint":
            masks = [-1 if m is None else m for m in panda.taint_query_va(cpu, buf, count)]
            labels = panda.taint_labels_va(cpu, buf, count)
        seen["writes"].append([text, masks, labels])

    panda.run_replay(name)
    if mode == "taint":
        seen["stats"] = {k: v for k, v in panda.taint_stats().items() if k != "h1"}
    with open(out, "w") as f:
        json.dump(seen, f)


def main():
    image = sys.argv[1]
    if json.load(open(image))["arch"] not in UART:
        print("ok   skipped: no MMIO console on this machine (x86's UART is port I/O)")
        return
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix="taint-mmio-")
    name = os.path.join(work, "mmio")
    res = {}
    for mode in ("record", "plain", "taint"):
        out = os.path.join(work, f"{mode}.json")
        subprocess.run([sys.executable, __file__, "--child", mode, image, name, out], check=True)
        if mode != "record":
            res[mode] = json.load(open(out))
    t, p = res["taint"], res["plain"]
    arch = json.load(open(image))["arch"]
    typed = "".join(r[0] for r in t["reads"])
    word = [w for w in t["writes"] if w[0].strip() == WORD]
    clean = [w for w in t["writes"] if w[0].strip() == arch]
    checks = [
        ("taint enabled", t.get("enabled") is True, ""),
        ("the source callback tainted UART loads", t["stats"]["mmio_tainted"] > 0, t["stats"]),
        ("every byte read from the console carries exactly the UART's label",
         WORD in typed and all(l == [LABEL] for r in t["reads"] for l in r[1]), t["reads"]),
        ("echo's write of the typed word carries exactly the UART's label",
         len(word) == 1 and all(m > 0 for m in word[0][1][:len(WORD)])
         and all(l == [LABEL] for l in word[0][2][:len(WORD)]), word),
        ("the untyped line is clean",
         len(clean) == 1 and all(m == 0 for m in clean[0][1]), clean),
        ("its write at the same instruction count as in the plain replay",
         t["icount"] is not None and t["icount"] == p["icount"], f"{t['icount']} vs {p['icount']}"),
    ]
    bad = 0
    for what, ok, detail in checks:
        print(f"{'ok  ' if ok else 'FAIL'} {what}" + ("" if ok else f": {detail}"))
        bad += not ok
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    if sys.argv[1] == "--child":
        child(*sys.argv[2:6])
    else:
        main()
