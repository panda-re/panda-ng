#!/usr/bin/env python3
'''
proc_start.py: print the auxiliary vector of each program the guest starts.

Run: panda-python python/examples/proc_start.py <dir>/debian-13-x86_64.json

The argument is an image description written by pandare2.mkimage (tests/README.md says how to
build images). Run it on a copy of the image: reverting and running the guest writes to it.

The proc_start_linux plugin calls on_rec_auxv when a new program starts, with its arguments,
environment and auxiliary vector. LD_SHOW_AUXV=1 makes the guest's loader print the same vector,
to compare. The example also hooks each program's entry point and reads its first instruction
bytes there.
'''
from sys import argv
from pandare2 import Panda

panda = Panda(generic=argv[1])

@panda.queue_blocking
def run_cmds():
    panda.revert_sync("root")
    for command in ["whoami", "uname -r"]:
        print(panda.run_serial_cmd("LD_SHOW_AUXV=1 " + command))
    panda.end_analysis()

@panda.ppp("proc_start_linux", "on_rec_auxv")
def rec_auxv(cpu, tb, av):
    argv = [panda.ffi.string(av.argv[i]).decode() for i in range(av.argc)]
    print(f"started {panda.ffi.string(av.execfn).decode()} argv={argv} envc={av.envc}")
    print(f"  AT_ENTRY=0x{av.entry:x} AT_PHDR=0x{av.phdr:x} AT_BASE=0x{av.base:x} "
          f"AT_PAGESZ={av.pagesz} AT_UID={av.uid} AT_RANDOM=0x{av.random:x}")

    # av is only valid during this callback: keep the address, not av.
    @panda.hook(av.entry, kernel=False)
    def entry(cpu, tb, hook):
        try:
            print(f"  entry 0x{hook.addr:x}: {panda.virtual_memory_read(cpu, hook.addr, 8).hex()}")
        except ValueError:
            print(f"  entry 0x{hook.addr:x}: not paged in")
        hook.enabled = False

panda.run()
