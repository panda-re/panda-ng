"""Panda.reset() on an image built by pandare2.mkimage.

Run: panda-python tests/reset.py <dir>/debian-13-x86_64.json

Reverts to the root snapshot, runs a command, then calls Panda.reset() (main-loop queue:
qemu_system_reset_request, then cont). The CPU must restart at the board's reset vector: a block
there is translated after the call (the vCPU may run a few blocks before the reset is acted on).
"""
import sys
import time

from pandare2 import Panda

RESET_PC = {"x86_64": 0xfffffff0, "i386": 0xfffffff0, "aarch64": 0x0, "riscv64": 0x1000}

panda = Panda(generic=sys.argv[1])
seen = {"armed": False, "hit": False, "blocks": 0}


@panda.cb_before_block_translate
def translate(cpu, pc):
    if not seen["armed"] or seen["hit"]:
        return
    seen["blocks"] += 1
    if pc == RESET_PC.get(panda.arch_name):
        seen["hit"] = True
        panda.end_analysis()


@panda.queue_blocking
def drive():
    panda.revert_sync("root")
    seen["before"] = panda.run_serial_cmd("echo before").strip()
    panda.flush_tb()  # so the reset's first block is translated, not found cached
    seen["armed"] = True
    panda.reset()
    time.sleep(120)  # if the reset vector never comes, don't hang
    if not seen["hit"]:
        panda.end_analysis()


panda.run()
want = RESET_PC.get(panda.arch_name)
print(f"RESET arch={panda.arch_name} before={seen.get('before')!r} want={hex(want)} "
      f"hit={seen['hit']} blocks_after_call={seen['blocks']}")
checks = {
    "the guest ran before the reset": seen.get("before") == "before",
    "after reset() the CPU restarts at the reset vector": seen["hit"],
}
for k, ok in checks.items():
    print(f"  {'PASS' if ok else 'FAIL'} {k}")
sys.exit(0 if all(checks.values()) else 1)
