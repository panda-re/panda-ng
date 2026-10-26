#!/usr/bin/env python3
'''
example_syscalls.py: print the system calls the guest makes while it runs a few commands.

Run: panda-python python/examples/example_syscalls.py <dir>/debian-13-x86_64.json

The argument is an image description written by pandare2.mkimage (tests/README.md says how to
build images). Run it on a copy of the image: reverting and running the guest writes to it.

syscalls2 (loaded by panda.ppp) calls on_all_sys_enter for every system call with its number,
and on_sys_openat_enter for each openat with its arguments. The image's description sets the
guest OS (its "os"), which selects syscalls2's system call table.
'''
from collections import Counter
from sys import argv
from pandare2 import Panda

panda = Panda(generic=argv[1])
calls = Counter()

@panda.queue_blocking
def run_cmds():
    panda.revert_sync("root")
    for command in ["ls -la /", "whoami", "uname -r"]:
        print(panda.run_serial_cmd(command))
    panda.end_analysis()

@panda.ppp("syscalls2", "on_all_sys_enter")
def sys_enter(cpu, pc, callno):
    calls[callno] += 1

@panda.ppp("syscalls2", "on_sys_openat_enter")
def openat_enter(cpu, pc, dfd, filename, flags, *mode):
    try:
        print(f"openat({panda.read_str(cpu, filename)!r})")
    except ValueError:
        pass    # the name isn't paged in

panda.run()
print("most frequent system call numbers:", calls.most_common(10))
