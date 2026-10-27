"""Edge coverage (panda_cov_*, Panda.enable_fast_coverage) in a fast-restore fuzzing loop.

Run: panda-python tests/fastcov.py <dir>/debian-13-x86_64.json [laps]

tests/programs/fastcov_<arch>.S, a static program at 0x400000, makes hypercall 1 with the address
of its 8-byte input, then runs 64 rounds that each take one of two paths by one bit of the input,
then makes hypercall 2 and exits. At hypercall 1 the test turns coverage on for the program's code
only ([0x400000, 0x410000): nothing else runs there) and takes a fast snapshot. Each lap restores,
clears the map, writes an input and runs to hypercall 2, where the lap's map is kept. The inputs
alternate between all bits clear and all bits set; the second half of the laps also has a PANDA
block callback (before_block_exec) enabled.

The program's run is the same from the snapshot every time (interrupts come between blocks, and the
kernel's blocks are outside the filter), so the maps must be exact:
- every lap's map has edges, with hit counts above 1 (the rounds loop);
- laps with the same input give byte-identical maps: prev goes back with the snapshot, block ids
  (from physical addresses, which the restore puts back) don't change between laps, and on x86_64
  and aarch64 that holds for CF_PCREL blocks too;
- the two inputs give different maps;
- the maps are the same with the block callback enabled (and it ran);
- panda_fast_restore leaves the map alone (lap 0's map is still there after the restore that
  starts lap 1, until fast_coverage_clear), and fast_coverage_map() is panda_cov_map() itself;
- after disable_fast_coverage a lap adds nothing to the map;
- panda_fast_verify() == 0 after the last restore, and the program then runs to its exit.
"""
import base64
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "programs"))
from fastcov_programs import PROGRAMS  # noqa: E402

from pandare2 import Panda  # noqa: E402

MAGIC = 0x50616e64
INPUTS = [b"\0" * 8, b"\xff" * 8]
TEXT = (0x400000, 0x410000)


def main():
    image = sys.argv[1]
    laps = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    panda = Panda(generic=image)
    lib = panda.libpanda
    st = {"snap": False, "input": None, "i": -1, "maps": [], "err": None, "survived": None,
          "same_ptr": None, "after_disable": None, "verify": None, "blocks": 0,
          "phase": "on", "hc": [], "out": ""}

    @panda.cb_before_block_exec(name="bbe", enabled=False)
    def bbe(cpu, tb):
        st["blocks"] += 1

    def start_lap():
        """Main loop, VM stopped: restore, clear the map, write the input, run."""
        i = st["i"]
        if lib.panda_fast_restore() != 0:
            st["err"] = f"lap {i}: panda_fast_restore failed"
            panda.end_analysis()
            return
        if i == 1 and st["phase"] == "on":
            # the restore that started this lap left lap 0's map in place
            st["survived"] = bytes(panda.fast_coverage_map()) == st["maps"][0]
        panda.fast_coverage_clear()
        if i == laps // 2 and st["phase"] == "on":
            panda.enable_callback("bbe")
        panda.physical_memory_write(st["input"], INPUTS[i % len(INPUTS)])
        lib.panda_cont()

    def end_lap():
        """Main loop, VM stopped at hypercall 2: keep the lap's map, then the next lap."""
        try:
            if st["phase"] == "disabled":
                st["after_disable"] = sum(panda.fast_coverage_map())
                if lib.panda_fast_restore() == 0:
                    st["verify"] = lib.panda_fast_verify()
                panda.disable_callback("bbe")
                st["phase"] = "done"
                lib.panda_cont()       # the program runs to its exit
                return
            st["maps"].append(bytes(panda.fast_coverage_map()))
            st["i"] += 1
            if st["i"] < laps:
                start_lap()
                return
            # one more lap with coverage off: the map must not change
            panda.disable_fast_coverage()
            panda.fast_coverage_clear()
            st["phase"] = "disabled"
            st["i"] = 0
            start_lap()
        except Exception as e:  # noqa: BLE001 - reported as a failed check
            st["err"] = repr(e)
            panda.end_analysis()

    def snap():
        try:
            m = panda.enable_fast_coverage(filter_lo=TEXT[0], filter_hi=TEXT[1])
            st["same_ptr"] = (int(panda.ffi.cast("uintptr_t", lib.panda_cov_map())) ==
                              int(panda.ffi.cast("uintptr_t", panda.ffi.from_buffer(m))))
            if lib.panda_fast_snap() != 0:
                raise RuntimeError("panda_fast_snap failed")
            st["i"] = 0
            start_lap()
        except Exception as e:  # noqa: BLE001
            st["err"] = repr(e)
            panda.end_analysis()

    @panda.hypercall(MAGIC)
    def hypercall(cpu):
        op = panda.arch.get_arg(cpu, 2, convention="syscall")
        st["hc"].append(op)
        if op == 1 and not st["snap"]:
            st["snap"] = True
            st["input"] = panda.virt_to_phys(cpu, panda.arch.get_arg(cpu, 3, convention="syscall"))
            if st["input"] in (-1, 2**64 - 1):
                st["err"] = "the input's page is not mapped at hypercall 1"
            panda.vm_stop()
            panda.queue_main_loop_wait_fn(snap)
        elif op == 2 and st["phase"] in ("on", "disabled"):
            panda.vm_stop()
            panda.queue_main_loop_wait_fn(end_lap)

    @panda.queue_blocking
    def drive():
        panda.revert_sync("root")
        data = base64.b64decode(PROGRAMS[panda.arch_name])
        panda.run_serial_cmd("rm -f /tmp/fastcov")
        for i in range(0, len(data), 64):
            chunk = "".join(f"\\x{b:02x}" for b in data[i:i + 64])
            panda.run_serial_cmd(f"printf '{chunk}' >> /tmp/fastcov")
        st["out"] = panda.run_serial_cmd("chmod +x /tmp/fastcov; /tmp/fastcov; echo rc=$?", timeout=600)
        panda.end_analysis()

    panda.run()

    maps = st["maps"]
    ok = True

    def check(name, cond):
        nonlocal ok
        ok &= bool(cond)
        print(f"  {'PASS' if cond else 'FAIL'} {name}")

    def edges(m):
        return sum(1 for b in m if b)

    for i, m in enumerate(maps):
        print(f"FASTCOV lap {i} input={INPUTS[i % 2].hex()} cb={int(i >= laps // 2)} "
              f"edges={edges(m)} hits={sum(m)}")
    print(f"FASTCOV hypercalls={st['hc'][:4]}... ({len(st['hc'])}) block callbacks={st['blocks']} "
          f"out={st['out']!r}")
    check("no error", st["err"] is None)
    if st["err"]:
        print("   ", st["err"])
    check(f"{laps} laps ran", len(maps) == laps)
    check("every lap's map has edges, with hit counts above 1",
          maps and all(edges(m) > 0 and max(m) > 1 for m in maps))
    same = [all(m == maps[k] for m in maps[k::2]) for k in (0, 1)]
    check("laps with the same input give byte-identical maps", maps and all(same))
    check("the two inputs give different maps", len(maps) > 1 and maps[0] != maps[1])
    check("the maps are the same with a block callback enabled (and it ran)",
          maps and all(same) and st["blocks"] > 0)
    check("panda_fast_restore left the map alone", st["survived"])
    check("fast_coverage_map() is panda_cov_map()", st["same_ptr"])
    check("a lap with coverage off adds nothing", st["after_disable"] == 0)
    check("RAM equals the snapshot after the last restore", st["verify"] == 0)
    check("the program then ran to its exit", "rc=0" in st["out"])
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
