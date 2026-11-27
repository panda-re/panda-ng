"""osi_linux's current process, and syscalls2's returns, on a guest with two vCPUs.

Run: panda-python tests/osi_linux_smp.py <dir>/debian-13-aarch64.json   (a pandare2.mkimage image
with its kernelinfo; it boots cold with -smp 2, 2-3 min)

Four perl loops of getppid() run at once, each under a shell of its own (so each loop has a parent
of its own), while syscalls2 reports every getppid return. The guest's return value is the truth:
- syscalls2: within one address space (one process) every getppid return has the same value, and
  every getppid entry has its return (a return paired with another call, or another task's call,
  would bring another value);
- osi_linux: at each of those returns the current process's ppid is that value. Its answer is
  cached per vCPU thread: a cache shared by the vCPUs (MTTCG) gave, now and then, the pid and ppid
  of another task with the same name (another perl) on the other vCPU.
"""
import collections
import re
import sys

from pandare2 import Panda

PANDA_TEST = dict(area="osi", tier="guest", fixture="cloud-debian-13", arches=["aarch64"],
                  plugins=["osi", "osi_linux", "syscalls2"], timeout=900)

LOOPS, COUNT = 4, 20000
img = sys.argv[1]
panda = Panda(generic=img, extra_args=["-smp", "2"])
panda.load_plugin("syscalls2", {"load-info": False})
panda.load_plugin("osi", {"disable-autoload": "true"})
panda.load_plugin("osi_linux")                  # the image's kernelinfo (its kconf_file)
st = {"on": False, "enter": 0, "cpus": set()}
events = []                                     # (asid, x0, osi ppid or None)


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
                   None if p == panda.ffi.NULL else p.ppid))
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
odd = osi_bad = osi_none = checked = 0
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
        _, x0, ppid = events[i]
        if x0 != common:
            odd += 1
        if ppid is None:
            osi_none += 1
        elif ppid != x0:
            osi_bad += 1
if loops != LOOPS:
    fails.append("each loop's returns found (%d address spaces with %d+): %d" % (LOOPS, COUNT, loops))
if odd:
    fails.append("syscalls2: getppid returns with another value in the loop's process: %d" % odd)
if st["enter"] != len(events):
    fails.append("syscalls2: as many getppid returns as entries: %d entries, %d returns"
                 % (st["enter"], len(events)))
if osi_bad or osi_none:
    fails.append("osi_linux: the current process's ppid at the loops' returns: %d wrong, %d none"
                 " of %d" % (osi_bad, osi_none, checked))
print("osi_linux_smp: %d returns, %d checked in %d loops" % (len(events), checked, loops))
for f in fails:
    print("FAIL", f)
print("osi_linux_smp:", "FAIL" if fails else "PASS")
sys.exit(1 if fails else 0)
