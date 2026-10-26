#!/usr/bin/env python3
'''
asid.py: print each address-space change while the guest runs a command.

Run: panda-python python/examples/asid.py <dir>/debian-13-x86_64.json

The argument is an image description written by pandare2.mkimage (tests/README.md says how to
build images). Run it on a copy of the image: reverting and running the guest writes to it.

A queued function reverts the guest to its root snapshot, runs `uname -a` on the serial console
and ends the run. The asid_changed callback prints the old and new address space whenever the
guest switches between them.
'''
from sys import argv
from pandare2 import Panda

panda = Panda(generic=argv[1])

@panda.queue_blocking
def run_cmd():
    panda.revert_sync("root")
    print(panda.run_serial_cmd("uname -a"))
    panda.end_analysis()

@panda.cb_asid_changed
def asidchange(cpu, old_asid, new_asid):
    if old_asid != new_asid:
        print(f"asid changed from 0x{old_asid:x} to 0x{new_asid:x}")
    return 0

panda.run()
