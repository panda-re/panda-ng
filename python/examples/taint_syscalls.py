#!/usr/bin/env python3
'''
taint_syscalls.py: which read() calls does each write() carry data from?

Run: panda-python python/examples/taint_syscalls.py <dir>/debian-13-x86_64.json [workdir]

The argument is an image description written by pandare2.mkimage (tests/README.md says how to
build images). Run it on a copy of the image: reverting and running the guest writes to it.

Taint runs only in replay, so this records a short command first, then replays it with taint
and labels on (the "taint" capability):
- each read() that returns data labels the bytes it filled with its own label (1, 2, ...);
- each write() to stdout prints the labels that reach its bytes, i.e. which reads it copies or
  computes from;
- a count of conditional branches on tainted data (cb_taint_branch) is printed at the end.

What to expect: head's write carries exactly the read of /etc/os-release. tr's write does not
carry the pipe read: tr maps each character through a lookup table, an address dependence, which
taint (data flow only) does not follow; it carries the console reads that typed its arguments
(the table is built from them). The computed line carries the typed digits of $((6*7)).
'''
import os
import sys
import tempfile
from collections import Counter
from pandare2 import Panda

CMD = "head -c 32 /etc/os-release | tr a-z A-Z; echo; echo computed-$((6*7))"
image = sys.argv[1]
work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix="taint-example-")
recording = os.path.join(work, "example")
caps = ["analysis", "rr", "taint"]

if not os.path.exists(recording + "-rr-nondet.log"):
    panda = Panda(generic=image, rr=True, capabilities=caps)

    @panda.queue_blocking
    def record():
        panda.revert_sync("root")
        panda.record_cmd(CMD, recording_name=recording, snap_name=None)
        panda.end_analysis()

    panda.run()
    # one engine per process: replay in a fresh one
    os.execv(sys.executable, [sys.executable] + sys.argv[:1] + [image, work])

panda = Panda(generic=image, rr=True, capabilities=caps)
reads = {}              # label -> (fd, bytes)
branches = Counter()    # pc -> tainted branches there


@panda.cb_start_block_exec(observe_only=True)
def start(cpu, tb):
    panda.taint_enable_labels()
    panda.taint_enable()
    panda.disable_callback("start")


@panda.ppp("syscalls2", "on_sys_read_return", observe_only=True)
def read_return(cpu, pc, fd, buf, count):
    n = panda.arch.get_retval(cpu, convention="syscall")
    if 0 < n <= count:
        label = len(reads) + 1
        reads[label] = (fd, n)
        panda.taint_label_va(cpu, buf, n, label=label)


@panda.ppp("syscalls2", "on_sys_write_enter", observe_only=True)
def write_enter(cpu, pc, fd, buf, count):
    if fd != 1 or not 0 < count <= 4096:
        return
    try:
        text = panda.virtual_memory_read(cpu, buf, count)
    except ValueError:
        return
    sources = set()
    for labels in panda.taint_labels_va(cpu, buf, count):
        sources |= set(labels or [])
    print(f"write({text!r}) carries data from reads {sorted(sources) or 'none'}")


@panda.cb_taint_branch
def tainted_branch(cpu, pc, mask):
    branches[pc] += 1


panda.run_replay(recording)
for label, (fd, n) in reads.items():
    print(f"read {label}: fd {fd}, {n} bytes")
print(f"{sum(branches.values())} tainted conditional branches at {len(branches)} addresses")
