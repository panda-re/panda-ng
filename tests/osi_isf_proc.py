"""osi_isf against the guest's own /proc, on a test machine with any kernel and its ISF.

Run: panda-python tests/osi_isf_proc.py <busybox json> [--isf ISF] [--provenance P] [--symbols S]
     [--allow-untrusted] [--nokaslr] [--drop TYPE.MEMBER] [--expect CAP=STATUS,...] [--kaslr-slide]
(a tests/mkbusybox.py image, whose -kernel can be any kernel: mkbusybox.py --kernel; the machine is
tests/testmachine.py's, built the first time)

The ISF defaults to linux_builder's cosi.<arch>.json.xz beside the image's kernel. The kernel boots
as built (with KASLR, if it has it; --nokaslr turns it off). The guest runs a long-lived process
from a copy of busybox in /tmp (a tmpfs, so its path crosses a mount point), and the host reads
/proc through the agent: every process's stat, that process's maps, /proc/modules. Then OSI is
asked from a block callback, and for each capability osi_isf_report() says is available its
answers must agree with /proc:

- process list (tasks, pid, name, ppid): every process /proc lists that was there before the
  listing started is in OSI's list with the same name and parent;
- start_time: each process's start time, in /proc's clock ticks (USER_HZ 100), within 2 ticks;
- mappings, mapping_files, mount_paths, heap_stack: the process's mappings are /proc's
  (the gate areas aside), with the same paths and [heap]/[stack];
- modules: the names in /proc/modules;
- current, asid, task_change: a task change into `cat /proc/self/stat` gives cat's pid, tid and
  parent, and the CPU's address space; so does the first user-mode block after it.

A capability the report says is unavailable must answer "not available" (NULL, or -1 for a pid,
0 for an asid or start time), with the others still checked. --drop removes a member from the
ISF first (a damaged ISF); --expect checks the report's statuses.
"""
import argparse
import json
import lzma
import os
import sys
import tempfile

from testlib import Checks, progress
from testmachine import Agent, Machine

PANDA_TEST = dict(area="osi", tier="guest", fixture="busybox", arches=["x86_64", "aarch64", "arm", "mips64"],
                  plugins=["osi", "osi_isf"], timeout=600)

CAPS = ["tasks", "pid", "name", "ppid", "thread_group", "asid", "start_time", "current", "mappings",
        "mapping_files", "mount_paths", "heap_stack", "modules", "task_change"]
GATES = {"[vectors]", "[vsyscall]"}     # /proc shows them, but they are no VMA of the process

ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--isf")
ap.add_argument("--provenance")
ap.add_argument("--symbols")
ap.add_argument("--allow-untrusted", action="store_true")
ap.add_argument("--nokaslr", action="store_true")
ap.add_argument("--drop", action="append", default=[])
ap.add_argument("--expect", default="")
ap.add_argument("--kaslr-slide", action="store_true", help="the kernel must have moved (KASLR)")
ap.add_argument("--mismatch", action="store_true",
                help="the ISF is another kernel's: it must be refused, and OSI must answer nothing")
opt = ap.parse_args()

m = Machine(opt.image, append="nokaslr" if opt.nokaslr else "")
args = m.image["extra_args"]
kernel = args[args.index("-kernel") + 1]
isf = opt.isf or os.path.join(os.path.dirname(kernel), f"cosi.{os.path.basename(kernel).split('.', 1)[1]}.json.xz")
if not os.path.isfile(isf):
    sys.exit(f"osi_isf_proc: no ISF at {isf} (--isf)")
work = tempfile.mkdtemp(prefix="osi_isf_proc-")
if opt.drop:
    with (lzma.open if isf.endswith(".xz") else open)(isf, "rt") as f:
        d = json.load(f)
    for tm in opt.drop:
        t, mem = tm.split(".", 1)
        d["user_types"][t]["fields"].pop(mem)
    isf = os.path.join(work, "damaged.json")
    with open(isf, "w") as f:
        json.dump(d, f)

panda = m.panda()
agent = Agent(panda)
panda.load_plugin("osi", {"disable-autoload": "true"})
pargs = {"isf": isf}
for k in ("provenance", "symbols"):
    if getattr(opt, k):
        pargs[k] = getattr(opt, k)
if opt.allow_untrusted:
    pargs["allow_untrusted"] = "true"
panda.load_plugin("osi_isf", pargs)
osi = panda.plugins["osi"]
ffi = panda.ffi
seen = {"changes": [], "user": [], "sample": None, "arm_user": False}


def s(p):
    return ffi.string(p).decode(errors="replace") if p != ffi.NULL else None


def report():
    return json.loads(ffi.string(panda.plugins["osi_isf"].osi_isf_report()).decode())


@panda.ppp("osi", "on_task_change")
def task_change(cpu):
    p = osi.get_current_process(cpu)
    t = osi.get_current_thread(cpu)
    rec = (s(p.name), p.pid, p.ppid, p.asid, panda.current_asid(cpu)) if p != ffi.NULL else None
    seen["changes"].append((rec, (t.pid, t.tid) if t != ffi.NULL else None))
    if len(seen["changes"]) > 20000:
        del seen["changes"][:10000]
    seen["arm_user"] = True
    panda.enable_callback("user_block")


@panda.cb_before_block_exec(enabled=False)
def user_block(cpu, tb):
    # The first user-mode block after a task change: current read outside the kernel.
    if panda.in_kernel_mode(cpu):
        return
    p = osi.get_current_process(cpu)
    seen["user"].append((p.pid, p.ppid, p.asid, panda.current_asid(cpu)) if p != ffi.NULL else None)
    if len(seen["user"]) > 20000:
        del seen["user"][:10000]
    panda.disable_callback("user_block")


def osi_sample(cpu, pid):
    out = {"current": osi.get_current_process(cpu) != ffi.NULL}
    arr = osi.get_processes(cpu)
    out["procs"] = None
    if arr != ffi.NULL:
        procs = []
        for i in range(panda.garray_len(arr)):
            p = osi.get_one_proc(arr, i)
            procs.append(dict(pid=p.pid, ppid=p.ppid, name=s(p.name), asid=p.asid, start=p.create_time))
            if p.pid == pid and "maps" not in out:
                maps = osi.get_mappings(cpu, p)
                out["maps"] = None
                if maps != ffi.NULL:
                    out["maps"] = [(mm.base, mm.base + mm.size, s(mm.name), s(mm.file))
                                   for mm in (osi.get_one_module(maps, j) for j in range(panda.garray_len(maps)))]
                    osi.cleanup_garray(maps)
        osi.cleanup_garray(arr)
        out["procs"] = procs
    mods = osi.get_modules(cpu)
    out["modules"] = None
    if mods != ffi.NULL:
        out["modules"] = [s(osi.get_one_module(mods, j).name) for j in range(panda.garray_len(mods))]
        osi.cleanup_garray(mods)
    return out


@panda.cb_before_block_exec(enabled=False)
def sample(cpu, tb):
    seen["sample"] = osi_sample(cpu, seen["pid"])
    panda.disable_callback("sample")


def parse_stat(line):
    line = line.strip()
    if not line or ")" not in line:
        return None
    pid = int(line.split(" ", 1)[0])
    comm = line[line.index("(") + 1:line.rindex(")")]
    rest = line[line.rindex(")") + 2:].split()
    return dict(pid=pid, name=comm, ppid=int(rest[1]), start=int(rest[19]))


def parse_maps(text):
    out = []
    for line in text.splitlines():
        f = line.split(None, 5)
        if len(f) < 5:
            continue
        lo, hi = (int(x, 16) for x in f[0].split("-"))
        out.append((lo, hi, f[5].strip() if len(f) > 5 else ""))
    return out


truth = {}


@panda.queue_blocking
def drive():
    try:
        m.revert(panda, agent)
        # A shell's background job opens /dev/null, which only devtmpfs (2.6.32+) makes.
        agent.run("[ -e /dev/null ] || mknod /dev/null c 1 3; cp /bin/busybox /tmp/busybox")
        rc, o = agent.run("/tmp/busybox sleep 1000 > /tmp/sleep.out 2>&1 & echo $!")
        seen["pid"] = int([w for w in o.split() if w.isdigit()][-1])
        rc, o = agent.run(f"echo $$; uname -r; cat /proc/{seen['pid']}/maps; echo ====; cat /proc/modules; "
                          "echo ====; for p in /proc/[0-9]*; do cat $p/stat 2>/dev/null; echo; done")
        head, mods, stats = o.decode(errors="replace").split("====\n")
        head = head.splitlines()
        truth["collector"], truth["release"] = int(head[0]), head[1]
        truth["maps"] = parse_maps("\n".join(head[2:]))
        truth["modules"] = sorted(l.split()[0] for l in mods.splitlines() if l.strip())
        truth["procs"] = {p["pid"]: p for p in map(parse_stat, stats.splitlines()) if p}
        progress(f"/proc: {len(truth['procs'])} processes, {len(truth['maps'])} mappings of {seen['pid']}")
        panda.enable_callback("sample")
        agent.run("true")
        rc, o = agent.run("exec cat /proc/self/stat")
        truth["cat"] = parse_stat(o.decode(errors="replace"))
        agent.run("true")
    except Exception as e:
        truth["error"] = repr(e)
    panda.end_analysis()


panda.run()

rep = report()
caps = {c: rep["capabilities"][c]["status"] for c in CAPS}
ok = {c: caps[c] != "unavailable" for c in CAPS}
pr = rep["pairing"]
print(f"kernel {truth.get('release')}: ISF {rep['kernel']} ({rep['isf_kind']}, {rep['version']}), "
      f"pairing {pr.get('state')} {pr.get('method', '')} slide {pr.get('slide', 0):#x}")
for c in CAPS:
    e = rep["capabilities"][c]
    print(f"  {c}: {e['status']} {e.get('strategy') or ''} {e.get('reason') or ''}")
C = Checks(panda.arch_name)
C.check("the guest ran the commands", "error" not in truth and truth.get("procs") and truth.get("cat"),
        truth.get("error"))
if opt.mismatch:
    smp = seen["sample"] or {}
    C.check("the ISF is refused: not this kernel's", pr.get("state") == "mismatch", pr)
    C.check("OSI answers nothing: no processes, current process, modules or task changes",
            smp.get("procs", "x") is None and smp.get("current") is False and smp.get("modules", "x") is None
            and not seen["changes"], (smp, len(seen["changes"])))
    sys.exit(C.report(f"osi_isf_proc {panda.arch_name} {truth.get('release')} (another kernel's ISF)"))
C.check("the ISF is paired with the kernel", pr.get("state") == "paired", pr)
if opt.kaslr_slide:
    C.check("the kernel moved (KASLR) and the slide was found", pr.get("slide", 0) != 0, pr)
for kv in filter(None, opt.expect.split(",")):
    k, v = kv.split("=")
    C.check(f"report: {k} is {v}", caps.get(k) == v, caps.get(k))

smp = seen["sample"] or {}
procs = smp.get("procs")
tp = truth.get("procs", {})
pid = seen.get("pid")

# Process list.
if ok["tasks"]:
    C.check("process list: OSI lists processes", procs, smp)
    by_pid = {p["pid"]: p for p in procs or []}
    older = [p for p in tp.values() if p["pid"] < truth.get("collector", 0)]
    missing = [p["pid"] for p in older if p["pid"] not in by_pid] if ok["pid"] else []
    if ok["pid"]:
        C.check(f"process list: every process /proc listed is OSI's ({len(older)})", older and not missing, missing)
    if ok["pid"] and ok["name"]:
        # /proc shows a kernel thread's full name (and a kworker's workqueue), comm its first 15
        # characters.
        bad = [(p["pid"], p["name"], by_pid[p["pid"]]["name"]) for p in older
               if p["pid"] in by_pid and not (by_pid[p["pid"]]["name"] and p["name"].startswith(by_pid[p["pid"]]["name"]))]
        C.check("process list: names are /proc's", not bad, bad[:5])
    if ok["pid"] and ok["ppid"]:
        bad = [(p["pid"], p["ppid"], by_pid[p["pid"]]["ppid"]) for p in older
               if p["pid"] in by_pid and by_pid[p["pid"]]["ppid"] != p["ppid"]]
        C.check("process list: parents are /proc's", not bad, bad[:5])
    if ok["pid"] and ok["start_time"]:
        bad = [(p["pid"], p["start"], by_pid[p["pid"]]["start"] // 10_000_000) for p in older
               if p["pid"] in by_pid and abs(by_pid[p["pid"]]["start"] // 10_000_000 - p["start"]) > 2]
        C.check(f"start_time ({rep['capabilities']['start_time'].get('strategy')}): /proc's within 2 ticks",
                not bad, bad[:5])
    for c, field, none in (("pid", "pid", -1), ("name", "name", None), ("ppid", "ppid", -1),
                           ("start_time", "start", 0), ("asid", "asid", 0)):
        if not ok[c]:
            C.check(f"{c} unavailable: every process's {field} is {none!r}",
                    procs and all(p[field] == none for p in procs), procs[:3] if procs else procs)
else:
    C.check("tasks unavailable: no process list", procs is None, procs)

# Mappings of the long-lived process.
if ok["tasks"] and ok["pid"]:
    om = smp.get("maps")
    if ok["mappings"]:
        want = sorted((lo, hi, p) for lo, hi, p in truth.get("maps", []) if p not in GATES)
        got = sorted(om or [])
        C.check(f"mappings: /proc's ranges ({len(want)})", [w[:2] for w in want] == [g[:2] for g in got],
                (want, got))
        pairs = list(zip(want, got))
        if ok["mapping_files"]:
            files = [(w[2], g[3]) for w, g in pairs if w[2].startswith("/")]
            if ok["mount_paths"]:
                bad = [f for f in files if f[0] != f[1]]
                C.check("mapping files: /proc's paths, across the /tmp mount", files and not bad, bad or files)
            else:
                bad = [f for f in files if not f[1] or os.path.basename(f[0]) != os.path.basename(f[1])]
                C.check("mapping files: /proc's file names", files and not bad, bad or files)
        else:
            C.check("mapping_files unavailable: no mapping has a file", all(g[3] is None for g in got), got)
        if ok["heap_stack"]:
            bad = [(w, g) for w, g in pairs if w[2] in ("[heap]", "[stack]") and w[2] != g[2]]
            C.check("mappings: [heap] and [stack] as /proc labels them",
                    any(w[2] == "[stack]" for w in want) and not bad, bad)
    else:
        C.check("mappings unavailable: get_mappings answers NULL", "maps" in smp and om is None,
                (om, pid, [p["pid"] for p in procs or []][-8:]))

if ok["modules"]:
    C.check("modules: /proc/modules", sorted(smp.get("modules") or []) == truth.get("modules"),
            (smp.get("modules"), truth.get("modules")))
else:
    C.check("modules unavailable: get_modules answers NULL", smp.get("modules", "x") is None, smp.get("modules"))

# The current process.
cat = truth.get("cat") or {}
if ok["current"]:
    recs = [r for r, t in seen["changes"] if r and r[1] == cat.get("pid")] if ok["pid"] else []
    if ok["task_change"] and ok["pid"]:
        C.check("task change into cat: OSI's current process is cat's pid", recs, len(seen["changes"]))
        # The last while cat has its mm: after the exec, before exit_mm.
        live = [x for x in recs if x[3]] if ok["asid"] else recs
        r = (live or recs or [None])[-1]
        if r and ok["name"]:
            C.check("current process: named cat", any(x[0] == "cat" for x in recs), recs[:3])
        if r and ok["ppid"]:
            C.check("current process: cat's parent", r[2] == cat.get("ppid"), (r, cat))
        # On MIPS the engine's current_asid is EntryHi's ASID, not the page directory's address.
        same_asid = not panda.arch_name.startswith("mips")
        if r and ok["asid"] and same_asid:
            C.check("current process: its asid is the CPU's", r[3] == r[4], recs[-4:])
        threads = [t for (r2, t) in seen["changes"] if r2 and r2[1] == cat.get("pid")]
        C.check("current thread: cat's pid and tid", threads and threads[0] == (cat["pid"], cat["pid"]), threads[:2])
        # The first user-mode block after each task change; cat's, if one came before the next
        # change, else every process's.
        users = [u for u in seen["user"] if u and u[0] == cat.get("pid")] or \
            [u for u in seen["user"] if u and u[0] in tp]
        C.check("current process in user mode: a /proc process (cat), in its address space",
                users and (not ok["asid"] or not same_asid or all(u[2] == u[3] for u in users)),
                [u for u in users if u[2] != u[3]][:4] or seen["user"][-4:])
    C.check("sample: a current process", smp.get("current"), smp)
else:
    C.check("current unavailable: get_current_process answers NULL", smp.get("current") is False, smp)
if ok["task_change"]:
    C.check("task changes", len(seen["changes"]) > 0, len(seen["changes"]))
else:
    C.check("task_change unavailable: no task changes reported", not seen["changes"], len(seen["changes"]))

with open(os.path.join(work, "report.json"), "w") as f:
    json.dump(rep, f, indent=1)
rc = C.report(f"osi_isf_proc {panda.arch_name} {truth.get('release')}")
import shutil
shutil.rmtree(work, ignore_errors=True)
sys.exit(rc)
