"""syscalls2's subscriptions change in the middle of a run, and the engine's wants follow.

Run: panda-python tests/syscalls2_wants.py <dir>/busybox-<arch>.json   (an image from
tests/mkbusybox.py; the machine is tests/testmachine.py's)

syscalls2 tells the engine which numbers' entries and returns its subscribers want, at each
subscribe and unsubscribe; a number nobody wants is not delivered at all. The engine counts the
entries that reach it and the calls it keeps for their return (panda_syscall_stats). One
`dd bs=1` makes COUNT reads and writes per phase; between phases the test subscribes and
unsubscribes, and checks per phase:
- nothing subscribed: no entry reaches the engine;
- read's own enter callback: the entries are its calls (the callbacks, within a few of the guest's
  other calls made while the test reads the counters), and no return is delivered;
- read's own return callback as well: each of read's calls returns;
- the enter callback removed (return only): each still returns, no enter callback;
- both removed: nothing again;
- on_all_sys_enter: every call is an entry; removed: nothing;
- write's enter callback subscribing to write's return from inside the callback: the very call's
  return is delivered (the call is kept late), and the calls after it return too;
- on_unknown_sys_enter (a number no table has, from the test program): its entries only.
"""
import argparse
import os
import sys
import threading

from testlib import Checks, progress
from testmachine import Agent, Machine

PANDA_TEST = dict(area="syscalls", tier="guest", fixture="testmachine",
                  arches=["x86_64", "aarch64", "arm", "i386", "riscv64", "mipsel", "mips", "mips64el",
                          "mips64", "ppc", "ppc64"],
                  plugins=["syscalls2"], timeout=900)

COUNT = 400
ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--smp", type=int, default=1)
opt = ap.parse_args()

m = Machine(opt.image, smp=opt.smp)
panda = m.panda()
arch = m.arch
C = Checks(arch)
agent = Agent(panda)

panda.load_plugin("syscalls2", {"load-info": False})
from cffi import FFI
ffi = FFI()
ffi.cdef("bool syscalls2_current_call(void *cpu, uint64_t *id, uint32_t *flags);"
         "typedef struct { uint64_t entries, kept, paired, restarts, children, ended,"
         " elsewhere, misses, evicted, not_kept, resolves, call_key, pending; }"
         " PandaSyscallStats;"
         "void panda_syscall_stats(PandaSyscallStats *out);")
lib = ffi.dlopen(os.path.join(panda.get_plugin_path(), f"libpanda-syscalls2_{arch}-softmmu.so"))
lock = threading.Lock()
n = {}                                  # callback: calls


def bump(k):
    with lock:
        n[k] = n.get(k, 0) + 1


def stats():
    s = ffi.new("PandaSyscallStats *")
    lib.panda_syscall_stats(s)
    return {"entries": s.entries}


def call_id(cpu):
    idp, flp = ffi.new("uint64_t *"), ffi.new("uint32_t *")
    ok = lib.syscalls2_current_call(ffi.cast("void *", int(panda.ffi.cast("uintptr_t", cpu))), idp, flp)
    return idp[0] if ok else None


def on(cb, fn):
    panda.ppp("syscalls2", cb, name=cb)(fn)


def off(cb):
    panda.disable_ppp(cb)


def read_enter(cpu, pc, *a):
    bump("read_enter")


def read_return(cpu, pc, *a):
    bump("read_return")


def all_enter(cpu, pc, no):
    bump("all_enter")


late = {"armed": False, "id": None, "returns": []}


def write_return(cpu, pc, *a):
    late["returns"].append(call_id(cpu))
    bump("write_return")


def write_enter(cpu, pc, *a):
    bump("write_enter")
    if late["armed"]:
        # subscribe to this call's own return, from its entry
        late["armed"] = False
        late["id"] = call_id(cpu)
        on("on_sys_write_return", write_return)


def unknown_enter(cpu, pc, no):
    bump("unknown_enter")


def near(a, b):
    """equal, but for the guest's calls made while the counters are read"""
    return abs(a - b) <= 4 + max(a, b) // 50


R = {}


def phase(name, cmd=f"dd if=/dev/zero of=/dev/null bs=1 count={COUNT} 2>/dev/null"):
    progress(name)
    with lock:
        n.clear()
    s0 = stats()
    agent.run(cmd)
    s1 = stats()
    with lock:
        R[name] = dict({k: s1[k] - s0[k] for k in s1}, **n)
    return R[name]


@panda.queue_blocking
def drive():
    m.revert(panda, agent)
    agent.run("true")
    d = phase("nothing")
    C.check("nothing subscribed: no entry reaches the engine", d["entries"] == 0, d)
    on("on_sys_read_enter", read_enter)
    d = phase("read enter")
    C.check("read's enter callback: the entries are read's", d.get("read_enter", 0) >= COUNT and
            near(d["entries"], d["read_enter"]), d)
    on("on_sys_read_return", read_return)
    d = phase("read enter+return")
    C.check("read's return callback too: each of its calls returns",
            d.get("read_enter", 0) >= COUNT and near(d["entries"], d["read_enter"]) and
            near(d.get("read_return", 0), d["read_enter"]), d)
    off("on_sys_read_enter")
    d = phase("read return")
    C.check("read's return callback alone: each of its calls returns, no enter callback",
            d.get("read_return", 0) >= COUNT and near(d["entries"], d["read_return"]) and
            "read_enter" not in d, d)
    off("on_sys_read_return")
    d = phase("removed")
    C.check("both removed: no entry reaches the engine", d["entries"] == 0, d)
    on("on_all_sys_enter", all_enter)
    d = phase("all enter")
    C.check("on_all_sys_enter: every call is an entry",
            d.get("all_enter", 0) >= 2 * COUNT and near(d["entries"], d["all_enter"]), d)
    off("on_all_sys_enter")
    d = phase("all removed")
    C.check("on_all_sys_enter removed: nothing", d["entries"] == 0, d)
    late["armed"] = True
    on("on_sys_write_enter", write_enter)
    d = phase("late return")
    C.check("a return subscribed from its own call's entry: that call returns, and the rest",
            late["id"] is not None and late["id"] in late["returns"] and
            d.get("write_return", 0) >= COUNT and near(d["write_enter"], d["write_return"]),
            dict(d, late=late["id"], first=late["returns"][:3]))
    off("on_sys_write_enter")
    off("on_sys_write_return")
    d = phase("late removed")
    C.check("write's callbacks removed: nothing", d["entries"] == 0, d)
    on("on_unknown_sys_enter", unknown_enter)
    d = phase("unknown")
    C.check("on_unknown_sys_enter: only unknown numbers reach the engine",
            near(d["entries"], d.get("unknown_enter", 0)), d)
    off("on_unknown_sys_enter")
    panda.end_analysis()


panda.run()
for k, v in R.items():
    print(k, v)
sys.exit(C.report(f"SYSCALLS2_WANTS arch={arch} smp={opt.smp}"))
