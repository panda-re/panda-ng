"""osi_linux's current process on an x86_64 guest with two vCPUs: each vCPU's current task.

Run: panda-python tests/osi_linux_percpu.py <dir>/debian-13-x86_64.json   (a pandare2.mkimage image
with its kernelinfo; it boots cold with -smp 2, 2-3 min)

current_task is a per-cpu variable on x86. osi_linux reads the copy of the CPU the vCPU is (its
per-cpu offset is in GS_BASE in the kernel, KERNEL_GS_BASE in user mode); reading CPU 0's copy on
every vCPU gave, on vCPU 1, whatever task CPU 0 was running.

Four perl loops of getppid() run at once, each under a shell of its own (so each loop has a parent
of its own), while syscalls2 reports every getppid return. The guest's return value is the truth:
at each return in a loop's process, osi_linux's current process has that value as its ppid, on
both vCPUs.
"""
import collections
import re
import sys

from pandare2 import Panda

PANDA_TEST = dict(area="osi", tier="guest", fixture="cloud-debian-13", arches=["x86_64"],
                  plugins=["osi", "osi_linux", "syscalls2"], timeout=900)

LOOPS, COUNT = 4, 20000
img = sys.argv[1]
panda = Panda(generic=img, extra_args=["-smp", "2"])
panda.load_plugin("syscalls2", {"load-info": False})
panda.load_plugin("osi", {"disable-autoload": "true"})
panda.load_plugin("osi_linux")                  # the image's kernelinfo (its kconf_file)
st = {"on": False, "enter": 0, "cpus": set()}
events = []                                     # (asid, return value, osi ppid or None, vCPU)


@panda.ppp("syscalls2", "on_sys_getppid_enter")
def enter(cpu, pc):
    if st["on"]:
        st["enter"] += 1


@panda.ppp("syscalls2", "on_sys_getppid_return")
def ret(cpu, pc):
    if not st["on"]:
        return
    p = panda.plugins["osi"].get_current_process(cpu)
    events.append((panda.current_asid(cpu), panda.arch.get_retval(cpu, convention="syscall"),
                   None if p == panda.ffi.NULL else p.ppid, cpu.cpu_index))
    st["cpus"].add(cpu.cpu_index)


out = {}
LOOP = "bash -c 'perl -e \"getppid() for 1..%d\"; true'" % COUNT


@panda.queue_blocking
def drive():
    try:
        con = panda.serial_console
        con.use_unansi = False
        con.update_expectation(r"login: ")
        con.expect(timeout=900)
        con.update_expectation(panda.expect_prompt)
        con.sendline(b"root")
        con.expect(timeout=300)
        text = panda.run_serial_cmd("nproc", timeout=120)
        out["nproc"] = re.sub(r"\x1b\[[0-9;?]*[A-Za-z]", "", text).strip()   # the shell's escapes
        st["on"] = True
        out["run"] = panda.run_serial_cmd(
            "for k in $(seq %d); do %s & done; wait; echo rc=$?" % (LOOPS, LOOP), timeout=1200)
        st["on"] = False
    except Exception as e:
        out["error"] = repr(e)
    panda.end_analysis()


panda.run()

fails = []
if out.get("error") or "rc=0" not in out.get("run", ""):
    fails.append("the loops ran: %r" % (out.get("error") or out.get("run", "")[-300:]))
if out.get("nproc") != "2":
    fails.append("the guest has 2 vCPUs: %r" % out.get("nproc"))
if st["cpus"] != {0, 1}:
    fails.append("returns on both vCPUs: %s" % sorted(st["cpus"]))
by_asid = collections.defaultdict(list)
for i, e in enumerate(events):
    by_asid[e[0]].append(i)
loops = 0
odd = osi_none = checked = 0
osi_bad = collections.Counter()                 # vCPU -> wrong ppids
for asid, idx in by_asid.items():
    vals = collections.Counter(events[i][1] for i in idx)
    common, n = vals.most_common(1)[0]
    if n < COUNT:
        continue                                # not a loop (a shell, or an address space reused)
    loops += 1
    first = next(i for i in idx if events[i][1] == common)
    last = max(i for i in idx if events[i][1] == common)
    for i in idx:
        if not first <= i <= last:
            continue                            # before the exec into perl, or after it exited
        checked += 1
        _, rv, ppid, vcpu = events[i]
        if rv != common:
            odd += 1
        if ppid is None:
            osi_none += 1
        elif ppid != rv:
            osi_bad[vcpu] += 1
if loops != LOOPS:
    fails.append("each loop's returns found (%d address spaces with %d+): %d" % (LOOPS, COUNT, loops))
if odd:
    fails.append("syscalls2: getppid returns with another value in the loop's process: %d" % odd)
if st["enter"] != len(events):
    fails.append("syscalls2: as many getppid returns as entries: %d entries, %d returns"
                 % (st["enter"], len(events)))
if osi_bad or osi_none:
    fails.append("osi_linux: the current process's ppid at the loops' returns: %d wrong (by vCPU"
                 " %s), %d none of %d" % (sum(osi_bad.values()), dict(osi_bad), osi_none, checked))
print("osi_linux_percpu: %d returns, %d checked in %d loops" % (len(events), checked, loops))
for f in fails:
    print("FAIL", f)
print("osi_linux_percpu:", "FAIL" if fails else "PASS")
sys.exit(1 if fails else 0)
