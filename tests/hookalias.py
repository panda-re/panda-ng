"""Hooks on shared-library code at different addresses in two processes.

Run: panda-python tests/hookalias.py <dir>/debian-13-x86_64.json

Two shells block opening a FIFO each. Their libc is mapped at different (ASLR) addresses, and the
code after the openat system call in libc's open has run before, in other processes, from a
CF_PCREL block shared by every mapping. The test reads each blocked process's user pc from
/proc/<pid>/syscall (the instruction after the system call), hooks both addresses through the
hooks plugin (panda.hook), then releases the processes: each hook must fire, at its own address.
Without hooked pages translating per virtual address, the shared block (translated where no hook
was) would run there unhooked.
"""
import sys

from pandare2 import Panda

panda = Panda(generic=sys.argv[1])
fired = {}
# Each reader reads the line before it exits: one that exited right after its open returned could
# close the FIFO before the shell's echo wrote to it, and the echo's SIGPIPE killed the login shell
# (the test then timed out at a login prompt, hooks or not).
SETUP = ("rm -f /tmp/f1 /tmp/f2 /tmp/p1 /tmp/p2; mkfifo /tmp/f1 /tmp/f2; "
         "sh -c 'echo $$ > /tmp/p1; exec 3< /tmp/f1; read x <&3' & "
         "sh -c 'echo $$ > /tmp/p2; exec 3< /tmp/f2; read x <&3' & sleep 2")


# --8<-- [start:drive]
@panda.queue_blocking
def drive():
    panda.revert_sync("root")
    panda.run_serial_cmd("cat /etc/os-release > /dev/null; ls / > /dev/null")
    panda.run_serial_cmd(SETUP, timeout=120)
    pcs = []
    for i in (1, 2):
        out = panda.run_serial_cmd(f"cat /proc/$(cat /tmp/p{i})/syscall").split()
        pcs.append(int(out[-1], 16))
    fired["pcs"] = pcs
    for i, pc in enumerate(pcs):
        def make(i, pc):
            @panda.hook(pc, kernel=False)
            def h(cpu, tb, hook):
                fired.setdefault(i, hook.addr)
        make(i, pc)
    panda.run_serial_cmd("echo a > /tmp/f1; echo b > /tmp/f2; sleep 1; wait", timeout=120)
    panda.end_analysis()


panda.run()
# --8<-- [end:drive]
pcs = fired.get("pcs", [])
print(f"HOOKALIAS arch={panda.arch_name} pcs={[hex(p) for p in pcs]} fired={ {k: hex(v) for k, v in fired.items() if k != 'pcs'} }")
checks = {
    "two processes, the same libc code at different addresses":
        len(pcs) == 2 and pcs[0] != pcs[1] and pcs[0] & 0xfff == pcs[1] & 0xfff,
    "the hook fires in the first process": fired.get(0) == (pcs[0] if pcs else None),
    "the hook fires in the second process": fired.get(1) == (pcs[1] if len(pcs) > 1 else None),
}
failed = False
for name, ok in checks.items():
    print(f"  {'PASS' if ok else 'FAIL'} {name}")
    failed |= not ok
sys.exit(1 if failed else 0)
