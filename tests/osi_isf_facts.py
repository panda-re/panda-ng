"""osi_isf's process facts and lifecycle events against the guest's own /proc, on a test machine
with any kernel and its ISF.

Run: panda-python tests/osi_isf_facts.py <busybox json> [--isf ISF] [--provenance P] [--symbols S]
     [--allow-untrusted] [--drop TYPE.MEMBER] [--expect CAP=STATUS,...] [--out results.json]
(a tests/mkbusybox.py image, whose -kernel can be any kernel: mkbusybox.py --kernel; the machine is
tests/testmachine.py's, built the first time)

osi_isf is loaded with lifecycle=true. In the guest:

- tests/programs/osifacts.c's `hold` mode, started from /tmp/osidir with an argument and a variable
  of its own: a file at offset 1234, a pipe, TCP sockets (listening and connected), a UDP socket, a
  listening unix socket, three threads, ids set apart (setresuid 21 22 23, setresgid 11 12 13).
  Its facts (osi_isf_facts) are compared with /proc/<pid>/{status,task,cmdline,environ,exe,cwd,
  root,fd,fdinfo,maps} and /proc/net/{tcp,udp,unix}: threads (tids, one mm), argv and environment
  (read in its own address space, at a task change into it; elsewhere they must say they are
  unavailable), exe, cwd, root, the eight ids, the capability sets, every fd's path, kind, flags
  and offset, sockets (family, ports, addresses, listening, unix paths) and pipes by inode, and
  every mapping's range, permissions, offset, inode and path.
- Lifecycle: a clone(CLONE_PARENT) child (its parent at creation is its creator's parent), a
  process whose threads exit alone and then together, a clone(CLONE_VM | CLONE_VFORK) child that
  execs, `sh -c 'exit 7'`, a process killed by SIGKILL, an orphan (reparented), an exec, and a
  loop that recycles pids (pid_max lowered). The events must agree with what the shell saw: the
  creator, the parent at creation and the flags; the old and new program; wait statuses; the new
  parent; a PID_REUSE for every pid used twice. A task's creation comes before its other events.

A capability the report says is unavailable must answer that way ({"unavailable": ...}, or no
events of its kind) while the others are checked. The last lines are a CAP line per capability:
match, unavailable (as reported), or FAIL.
"""
import argparse
import atexit
import json
import lzma
import os
import shutil
import sys
import tempfile

from testlib import Checks, progress
from testmachine import Agent, Machine

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "programs"))
import osifacts_programs  # noqa: E402

PANDA_TEST = dict(area="osi", tier="guest", fixture="busybox", arches=["x86_64", "aarch64", "arm", "mips64"],
                  plugins=["osi", "osi_isf", "syscalls2"], timeout=900)

FACT_CAPS = ["threads", "cmdline", "exe_path", "fs_paths", "creds", "cap_sets", "open_files", "sockets",
             "vma_details", "lifecycle", "exit_codes", "reparent", "pid_reuse"]
KINDS = {1: "FORK", 2: "THREAD", 3: "EXEC", 4: "EXIT", 5: "REPARENT", 6: "PID_REUSE"}
CLONE_VM, CLONE_VFORK, CLONE_PARENT, CLONE_THREAD = 0x100, 0x4000, 0x8000, 0x10000
GATES = {"[vectors]", "[vsyscall]", "[sigpage]", "[vdso]", "[vvar]", "[vvar_vclock]"}

ap = argparse.ArgumentParser()
ap.add_argument("image")
ap.add_argument("--isf")
ap.add_argument("--provenance")
ap.add_argument("--symbols")
ap.add_argument("--allow-untrusted", action="store_true")
ap.add_argument("--drop", action="append", default=[])
ap.add_argument("--expect", default="")
ap.add_argument("--recycle", type=int, default=400, help="most processes in the pid-recycling loop")
ap.add_argument("--out")
opt = ap.parse_args()

m = Machine(opt.image)
args = m.image["extra_args"]
kernel = args[args.index("-kernel") + 1]
isf = opt.isf or os.path.join(os.path.dirname(kernel), f"cosi.{os.path.basename(kernel).split('.', 1)[1]}.json.xz")
if not os.path.isfile(isf):
    sys.exit(f"osi_isf_facts: no ISF at {isf} (--isf)")
work = tempfile.mkdtemp(prefix="osi_isf_facts-")
atexit.register(shutil.rmtree, work, True)
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
pargs = {"isf": isf, "lifecycle": "true"}
for k in ("provenance", "symbols"):
    if getattr(opt, k):
        pargs[k] = getattr(opt, k)
if opt.allow_untrusted:
    pargs["allow_untrusted"] = "true"
panda.load_plugin("osi_isf", pargs)
osi = panda.plugins["osi"]
api = panda.plugins["osi_isf"]
ffi = panda.ffi
events = []
st = {"want": None, "cur": None, "any": None, "pid": None}


def s(p):
    return ffi.string(p).decode(errors="replace") if p != ffi.NULL else None


def report():
    return json.loads(ffi.string(api.osi_isf_report()).decode())


def facts(cpu, pid):
    p = api.osi_isf_facts(cpu, pid, 0, b"all")
    if p == ffi.NULL:
        return None
    out = json.loads(ffi.string(p).decode(errors="replace"))
    api.osi_isf_free(p)
    return out


@panda.ppp("osi_isf", "on_osi_isf_event")
def on_event(cpu, ev):
    events.append(dict(kind=KINDS.get(ev.kind, ev.kind), pid=ev.pid, tid=ev.tid, start=ev.start_time,
                       pstart=ev.process_start, cpid=ev.creator_pid, ctid=ev.creator_tid, ppid=ev.ppid,
                       old_ppid=ev.old_ppid, flags=ev.clone_flags, sys=s(ev.syscall), status=ev.exit_status,
                       group=ev.group_exit, old_start=ev.old_start, old_exe=s(ev.old_exe),
                       new_exe=s(ev.new_exe), filename=s(ev.filename)))


@panda.ppp("osi", "on_task_change")
def task_change(cpu):
    # The facts once the process runs: its own address space is the CPU's, so argv is readable.
    if st["want"] is None:
        return
    p = osi.get_current_process(cpu)
    if p != ffi.NULL and p.pid == st["want"]:
        st["cur"] = facts(cpu, st["want"])
        st["want"] = None


@panda.cb_before_block_exec(enabled=False)
def any_block(cpu, tb):
    st["any"] = facts(cpu, st["pid"])
    panda.disable_callback("any_block")


truth = {}
SHELL = {}


def sections(text):
    out, cur = {}, None
    for line in text.splitlines():
        if line.startswith("@@@ "):
            cur = line[4:].strip()
            out[cur] = []
        elif cur:
            out[cur].append(line)
    return out


@panda.queue_blocking
def drive():
    try:
        m.revert(panda, agent)
        agent.put("/tmp/osifacts", __import__("base64").b64decode(osifacts_programs.PROGRAMS[panda.arch_name]))
        agent.run("[ -e /dev/null ] || mknod /dev/null c 1 3; cp /bin/busybox /tmp/busybox; mkdir -p /tmp/osidir; "
                  "ifconfig lo 127.0.0.1 up 2>/dev/null || ip link set lo up")
        rc, o = agent.run("cd /tmp/osidir && OSIFACTS_VAR=hello /tmp/osifacts hold extra-arg > /tmp/hold.out 2>&1 & "
                          "for i in 1 2 3 4 5 6 7 8 9 10; do grep -q ready /tmp/hold.out && break; sleep 0.2; done; "
                          "cat /tmp/hold.out")
        pid = int(o.decode().split()[-1])
        st["pid"] = pid
        P = f"/proc/{pid}"
        rc, o = agent.run(
            f"echo '@@@ status'; cat {P}/status; echo '@@@ task'; ls {P}/task; "
            f"echo '@@@ cmdline'; tr '\\000' '\\n' < {P}/cmdline; echo; echo '@@@ environ'; tr '\\000' '\\n' < {P}/environ; echo; "
            f"echo '@@@ links'; for l in exe cwd root; do echo \"$l $(readlink {P}/$l)\"; done; "
            f"echo '@@@ fd'; for f in {P}/fd/*; do echo \"${{f##*/}} $(readlink $f)\"; done; "
            f"echo '@@@ fdinfo'; for f in {P}/fdinfo/*; do echo \"${{f##*/}} $(tr '\\n\\t' '  ' < $f)\"; done; "
            f"echo '@@@ maps'; cat {P}/maps; echo '@@@ tcp'; cat /proc/net/tcp; echo '@@@ udp'; cat /proc/net/udp; "
            f"echo '@@@ unix'; cat /proc/net/unix; echo '@@@ uname'; uname -r")
        truth.update(sections(o.decode(errors="replace")))
        progress(f"/proc of {pid}: {len(truth.get('fd', []))} fds, {len(truth.get('maps', []))} mappings")
        # Facts from wherever the CPU is, then from inside the process (it wakes every second).
        panda.enable_callback("any_block")
        agent.run("true")
        st["want"] = pid
        agent.run("sleep 2")
        st["want"] = None
        # Lifecycle.
        rc, o = agent.run(
            "echo '@@@ shell'; echo $$; "
            "echo '@@@ cparent'; /tmp/osifacts cparent; sleep 1; "
            "echo '@@@ threads'; /tmp/osifacts threads & p=$!; wait $p; echo \"$p $?\"; "
            "echo '@@@ vfork'; /tmp/osifacts vfork /tmp/busybox true & p=$!; wait $p; echo \"$p $?\"; "
            "echo '@@@ exit7'; sh -c 'exit 7' & p=$!; wait $p; echo \"$p $?\"; "
            "echo '@@@ kill9'; sh -c 'kill -9 $$' & p=$!; wait $p; echo \"$p $?\"; "
            "echo '@@@ exec'; sh -c 'exec /tmp/busybox true' & p=$!; wait $p; echo \"$p $?\"; "
            "echo '@@@ orphan'; sh -c '/tmp/busybox sleep 2 & echo $$ $! > /tmp/orphan; exit 0'; sleep 0.5; "
            "read a b < /tmp/orphan; echo $a $b; echo '@@@ orphanstat'; cat /proc/$b/stat")
        truth.update(sections(o.decode(errors="replace")))
        rc, o = agent.run(
            # pid_max at its least (RESERVED_PIDS + 1): ids count up to 300, then 300 again and again.
            "echo '@@@ pidmax'; cat /proc/sys/kernel/pid_max; echo 301 > /proc/sys/kernel/pid_max; "
            f"echo '@@@ recycle'; seen=' '; n=0; i=0; while [ $i -lt {opt.recycle} ] && [ $n -lt 4 ]; do "
            "/tmp/busybox true & p=$!; wait; case \"$seen\" in *\" $p \"*) n=$((n+1));; esac; seen=\"$seen$p \"; "
            "echo $p; i=$((i+1)); done")
        truth.update(sections(o.decode(errors="replace")))
        agent.run(f"echo {truth['pidmax'][0] if truth.get('pidmax') else 32768} > /proc/sys/kernel/pid_max; "
                  f"kill {pid}; true")
    except Exception as e:
        truth["error"] = repr(e)
    panda.end_analysis()


panda.run()

# ---------------------------------------------------------------- checks

rep = report()
caps = {c: rep["capabilities"][c]["status"] for c in rep["capabilities"]}
ok = {c: caps[c] != "unavailable" for c in caps}
pr = rep["pairing"]
print(f"kernel {truth.get('uname', ['?'])[0]}: ISF {rep['kernel']} ({rep['isf_kind']}, {rep['version']}), "
      f"pairing {pr.get('state')} slide {pr.get('slide', 0):#x}")
for c in FACT_CAPS:
    e = rep["capabilities"][c]
    print(f"  {c}: {e['status']} {e.get('strategy') or ''} {e.get('reason') or ''}")
C = Checks(panda.arch_name)
outcome = {}     # capability -> "match" / "unavailable" / "FAIL"


def cap_check(cap, name, good, detail=""):
    C.check(f"{cap}: {name}", good, detail)
    if not good:
        outcome[cap] = "FAIL"
    elif cap not in outcome:
        outcome[cap] = "match" if ok.get(cap) else "unavailable"


def unavailable(v):
    return isinstance(v, dict) and "unavailable" in v


C.check("the guest ran the workload", "error" not in truth and truth.get("status"), truth.get("error"))
C.check("the ISF is paired with the kernel", pr.get("state") == "paired", pr)
for kv in filter(None, opt.expect.split(",")):
    k, v = kv.split("=")
    C.check(f"report: {k} is {v}", caps.get(k) == v, caps.get(k))
F = st["cur"] or st["any"] or {}
FA = st["any"] or {}
pid = st["pid"]
C.check("osi_isf_facts answered for the process", F.get("pid") == pid and "unavailable" not in F, F)
big_endian = panda.arch_name in ("mips", "mips64", "ppc")


def status_field(name):
    for line in truth.get("status", []):
        if line.startswith(name + ":"):
            return line.split()[1:]
    return None


# Threads and the address space.
tids = sorted(int(x) for x in " ".join(truth.get("task", [])).split() if x.isdigit())
th = F.get("threads")
if ok["threads"]:
    cap_check("threads", "the process's tids are /proc's task/", isinstance(th, list) and
              sorted(t["tid"] for t in th) == tids, (th, tids))
    cap_check("threads", "its threads share one mm, the process's", isinstance(th, list) and
              len({t["mm"] for t in th}) == 1 and th[0]["mm"] == F.get("mm"), th)
else:
    cap_check("threads", "unavailable: says so", unavailable(th), th)

# Arguments and environment: read in the process's own address space; elsewhere unavailable.
cur = st["cur"] or {}
want_argv = [x for x in truth.get("cmdline", []) if x]
want_env = [x for x in truth.get("environ", []) if x]
if ok["cmdline"]:
    for key, want in (("cmdline", want_argv), ("environ", want_env)):
        got = cur.get(key)
        if isinstance(got, list):
            cap_check("cmdline", f"{key} in its address space: /proc's", [x for x in got if x] == want, (got, want))
        else:
            # Paged out, or not in the TLB (MIPS): it must say so.
            cap_check("cmdline", f"{key} in its address space: /proc's, or unavailable with a reason",
                      unavailable(got) and "not readable" in got["unavailable"], got)
    if outcome.get("cmdline") != "FAIL" and not isinstance(cur.get("cmdline"), list):
        outcome["cmdline"] = "unavailable at run time (says so)"
    other = FA.get("cmdline")
    cap_check("cmdline", "argv from another address space: unavailable, or (if it was current) /proc's",
              (unavailable(other) and ("another address space" in other["unavailable"]
                                       or "not readable" in other["unavailable"]))
              or (isinstance(other, list) and [x for x in other if x] == want_argv), other)
    if not st["cur"]:
        C.check("cmdline: a task change into the process was seen", False, "no sample in its address space")
else:
    cap_check("cmdline", "unavailable: says so", unavailable(F.get("cmdline")) and unavailable(F.get("environ")),
              F.get("cmdline"))

links = dict(line.split(" ", 1) for line in truth.get("links", []) if " " in line)
if ok["exe_path"]:
    cap_check("exe_path", "exe is /proc's", F.get("exe") == links.get("exe"), (F.get("exe"), links.get("exe")))
else:
    cap_check("exe_path", "unavailable: says so", unavailable(F.get("exe")), F.get("exe"))
if ok["fs_paths"]:
    cap_check("fs_paths", "cwd and root are /proc's", F.get("cwd") == links.get("cwd") and
              F.get("root") == links.get("root"), (F.get("cwd"), F.get("root"), links))
else:
    cap_check("fs_paths", "unavailable: says so", unavailable(F.get("cwd")) and unavailable(F.get("root")), F.get("cwd"))

uid, gid = status_field("Uid"), status_field("Gid")
cr = F.get("creds")
if ok["creds"]:
    want = dict(zip(["uid", "euid", "suid", "fsuid"], map(int, uid or []))) | \
        dict(zip(["gid", "egid", "sgid", "fsgid"], map(int, gid or [])))
    cap_check("creds", "the eight ids are /proc's (21 22 23, 11 12 13)", isinstance(cr, dict) and
              all(cr.get(k) == v for k, v in want.items()) and want.get("euid") == 22, (cr, want))
else:
    cap_check("creds", "unavailable: says so", unavailable(cr), cr)
cs = F.get("caps")
if ok["cap_sets"]:
    want = {}
    for k, n in (("effective", "CapEff"), ("permitted", "CapPrm"), ("inheritable", "CapInh"), ("bounding", "CapBnd")):
        v = status_field(n)
        if v:
            want[k] = int(v[0], 16)
    cap_check("cap_sets", "the capability sets are /proc's", isinstance(cs, dict) and
              all(int(cs[k], 16) == v for k, v in want.items() if k in cs) and "effective" in cs, (cs, want))
else:
    cap_check("cap_sets", "unavailable: says so", unavailable(cs), cs)

# Open files.
fdlinks = {int(a): b for a, b in (line.split(" ", 1) for line in truth.get("fd", []) if " " in line) if a.isdigit()}
fdinfo = {}
for line in truth.get("fdinfo", []):
    w = line.split()
    if w and w[0].isdigit():
        d = dict(zip(w[1::2], w[2::2]))
        fdinfo[int(w[0])] = (int(d.get("pos:", "0")), int(d.get("flags:", "0"), 8))
files = F.get("files")
if ok["open_files"]:
    got = {f["fd"]: f for f in files} if isinstance(files, list) else {}
    cap_check("open_files", f"the fds are /proc's ({len(fdlinks)})", sorted(got) == sorted(fdlinks), (sorted(got), fdlinks))
    bad = [(fd, got[fd].get("path"), p) for fd, p in fdlinks.items() if fd in got and got[fd].get("path") != p]
    cap_check("open_files", "each fd's path (pipes, sockets and anonymous files by inode) is /proc's", not bad, bad)
    bad = [(fd, got[fd].get("pos"), got[fd].get("flags"), fdinfo[fd]) for fd in fdinfo if fd in got and
           (got[fd].get("pos") != fdinfo[fd][0] or got[fd].get("flags") != fdinfo[fd][1] & ~0o2000000)]
    cap_check("open_files", "each fd's offset and flags are fdinfo's (O_CLOEXEC aside)", not bad
              and any(p == 1234 for p, _ in fdinfo.values()), bad)
    pipes = [f for f in got.values() if f.get("kind") == "pipe"]
    cap_check("open_files", "the pipe's two ends: kind pipe, one inode", len(pipes) == 2 and
              pipes[0]["ino"] == pipes[1]["ino"], pipes)
else:
    cap_check("open_files", "unavailable: says so", unavailable(files), files)


def net_table(name):
    """inode -> (local (addr, port), remote (addr, port), state) from /proc/net/tcp or udp."""
    out = {}
    for line in truth.get(name, [])[1:]:
        w = line.split()
        if len(w) < 10:
            continue

        def ap(x):
            a, p = x.split(":")
            n = int(a, 16)
            b = n.to_bytes(4, "big" if big_endian else "little")
            return ".".join(map(str, b)), int(p, 16)
        out[int(w[9])] = (ap(w[1]), ap(w[2]), int(w[3], 16))
    return out


if ok["sockets"]:
    tcp, udp = net_table("tcp"), net_table("udp")
    unix = {}
    for line in truth.get("unix", [])[1:]:
        w = line.split()
        if len(w) >= 7 and w[6].isdigit():
            unix[int(w[6])] = (w[7] if len(w) > 7 else None, int(w[3], 16) & 0x10000 != 0)
    socks = [f for f in (files if isinstance(files, list) else []) if f.get("kind") == "socket"]
    bad, seen = [], {"listen": 0, "established": 0, "udp": 0, "unix": 0}
    for f in socks:
        so = f.get("socket", {})
        ino = f["ino"]
        if ino in tcp or ino in udp:
            (la, lp), (ra, rp), state = tcp.get(ino) or udp.get(ino)
            want = dict(local=f"{la}:{lp}", remote=f"{ra}:{rp}")
            if so.get("local") != want["local"] or so.get("remote") != want["remote"] or \
                    (ino in tcp and so.get("state") != state):
                bad.append((f["fd"], so, want, state))
            seen["udp" if ino in udp else "listen" if state == 10 else "established"] += 1
        elif ino in unix:
            path, listening = unix[ino]
            if so.get("path") != path or so.get("listening") != listening:
                bad.append((f["fd"], so, unix[ino]))
            seen["unix"] += 1
    cap_check("sockets", "TCP (listening, connected), UDP and unix sockets: /proc/net's addresses, ports, "
              "states and paths", not bad and all(seen.values()), (bad, seen, socks))
else:
    socks = [f.get("socket") for f in (files if isinstance(files, list) else []) if f.get("kind") == "socket"]
    cap_check("sockets", "unavailable: says so", all(unavailable(x) for x in socks), socks)

vm = F.get("vmas")
if ok["vma_details"]:
    want = []
    for line in truth.get("maps", []):
        w = line.split(None, 5)
        if len(w) < 5:
            continue
        lo, hi = (int(x, 16) for x in w[0].split("-"))
        path = w[5].strip() if len(w) > 5 else ""
        if path in GATES and path in ("[vectors]", "[vsyscall]"):
            continue
        want.append((lo, hi, w[1], int(w[2], 16) if int(w[4]) else 0, int(w[4]), path if int(w[4]) else None))
    got = sorted((int(v["start"], 16), int(v["end"], 16), v["perms"], v["offset"], v.get("ino", 0),
                  v.get("file")) for v in vm) if isinstance(vm, list) else []
    cap_check("vma_details", f"ranges, permissions, offsets, inodes and paths are /proc's maps ({len(want)})",
              got == sorted(want), [(a, b) for a, b in zip(got, sorted(want)) if a != b][:6] or (got, want))
else:
    cap_check("vma_details", "unavailable: says so", unavailable(vm), vm)

# ---------------------------------------------------------------- lifecycle


def ev(kind, **kw):
    return [e for e in events if e["kind"] == kind and all(e.get(k) == v for k, v in kw.items())]


def first(kind, **kw):
    x = ev(kind, **kw)
    return x[0] if x else None


def idx(e):
    return events.index(e) if e in events else -1


def first_ints(key, line=0):
    lines = [x for x in truth.get(key) or [] if x.strip()] or [""]
    w = lines[line].split()
    return [int(x) for x in w if x.lstrip("-").isdigit()] or [-1, -1]


shell = first_ints("shell")[0]
cp_child = first_ints("cparent")[-1]           # "child <tid>"
tp, tstatus = (first_ints("threads", -1) + [-1])[:2]
progress(f"{len(events)} lifecycle events")
if ok["lifecycle"]:
    # clone(CLONE_PARENT): the creator is osifacts, the parent at creation the shell.
    f = first("FORK", pid=cp_child)
    cap_check("lifecycle", "clone(CLONE_PARENT): a FORK whose parent at creation is the creator's parent",
              f and f["flags"] & CLONE_PARENT and f["ppid"] == shell and f["cpid"] != shell and f["sys"] in
              ("clone", "clone3"), (f, shell))
    # Threads.
    thr = ev("THREAD", pid=tp)
    fk = first("FORK", pid=tp)
    cap_check("lifecycle", "two THREAD events in the process, after its FORK, from its main thread",
              len(thr) == 2 and fk and all(idx(fk) < idx(t) and t["ctid"] == tp and t["flags"] & CLONE_THREAD
                                           for t in thr), (fk, thr))
    # vfork: clone(CLONE_VM | CLONE_VFORK), and the child's exec.
    vp = first_ints("vfork", -1)[0]
    vf = [e for e in ev("FORK") if e["cpid"] == vp]
    vx = first("EXEC", pid=vf[0]["pid"]) if vf else None
    cap_check("lifecycle", "clone(CLONE_VM|CLONE_VFORK): a FORK with its flags, then the child's EXEC of "
              "/tmp/busybox from /tmp/osifacts", vf and vf[0]["flags"] & (CLONE_VM | CLONE_VFORK) == CLONE_VM | CLONE_VFORK
              and vx and vx["old_exe"] in ("/tmp/osifacts", "osifacts") and vx["new_exe"] in ("/tmp/busybox", "busybox")
              and vx["filename"] == "/tmp/busybox" and idx(vf[0]) < idx(vx), (vf, vx))
    ep = first_ints("exec", -1)[0]
    ex = ev("EXEC", pid=ep)
    cap_check("lifecycle", "sh -c 'exec /tmp/busybox true': its last EXEC is from /bin/busybox to /tmp/busybox",
              ex and ex[-1]["new_exe"] in ("/tmp/busybox", "busybox") and ex[-1]["old_exe"] in ("/bin/busybox", "sh")
              and ex[-1]["filename"] == "/tmp/busybox", ex)
    forks = [first("FORK", pid=first_ints(k, -1)[0]) for k in ("exit7", "kill9", "exec") if truth.get(k)]
    cap_check("lifecycle", "the shell's children: FORK events from the shell, parent the shell",
              len(forks) == 3 and all(f and f["cpid"] == shell and f["ppid"] == shell and not f["flags"] & CLONE_THREAD
                                      for f in forks), forks)
    late = [e for e in events if e["kind"] in ("EXEC", "EXIT", "THREAD") and first("FORK", pid=e["pid"])
            and idx(first("FORK", pid=e["pid"])) > idx(e)]
    cap_check("lifecycle", "no task's event comes before its creation", not late, late[:4])
else:
    cap_check("lifecycle", "unavailable: no FORK, THREAD or EXEC events",
              not ev("FORK") and not ev("THREAD") and not ev("EXEC"), events[:4])

# Whether an exit was the process's last is known from the thread list (or signal->live): else -1.
LAST, NOT_LAST = ((1,), (0,)) if ok["threads"] else ((1, -1), (0, -1))
if ok["exit_codes"]:
    by_hook = rep["capabilities"]["exit_codes"].get("strategy") == "do_exit hook"
    e7 = first_ints("exit7", -1)[0]
    k9 = first_ints("kill9", -1)[0]
    x7 = [e for e in ev("EXIT", pid=e7) if e["group"] in LAST]
    cap_check("exit_codes", "sh -c 'exit 7': its EXIT, the process's last, status 7 << 8",
              x7 and x7[-1]["status"] == 7 << 8, ev("EXIT", pid=e7))
    if by_hook:
        x9 = [e for e in ev("EXIT", pid=k9) if e["group"] in LAST]
        cap_check("exit_codes", "killed by SIGKILL: status 9", x9 and x9[-1]["status"] == 9, ev("EXIT", pid=k9))
    tx = ev("EXIT", pid=tp) if ok["lifecycle"] else []
    cap_check("exit_codes", "threads: one exits alone (4 << 8, not the last), the process ends with 3 << 8",
              any(e["status"] == 4 << 8 and e["group"] in NOT_LAST for e in tx) and
              any(e["status"] == 3 << 8 and e["group"] in LAST for e in tx) and tstatus == 3, (tx, tstatus))
    cx = [e for e in ev("EXIT", pid=cp_child) if e["group"] in LAST] if ok["lifecycle"] else []
    cap_check("exit_codes", "the CLONE_PARENT child: 5 << 8", cx and cx[-1]["status"] == 5 << 8, cx)
else:
    cap_check("exit_codes", "unavailable: no EXIT events", not ev("EXIT"), ev("EXIT")[:3])

if ok["reparent"]:
    o = truth.get("orphan", [""])[0].split()
    os_, orphan = (int(o[0]), int(o[1])) if len(o) == 2 else (-1, -1)
    now = None
    for line in truth.get("orphanstat", []):
        w = line.split()
        if w and int(w[0]) == orphan:
            now = int(line[line.rindex(")") + 2:].split()[1])
    r = first("REPARENT", pid=orphan)
    cap_check("reparent", "the orphan: a REPARENT from its exited parent to /proc's new parent",
              r and r["old_ppid"] == os_ and r["ppid"] == now, (r, os_, now, ev("REPARENT")))
else:
    cap_check("reparent", "unavailable: no REPARENT events", not ev("REPARENT"), ev("REPARENT"))

pids = [int(x) for x in truth.get("recycle", []) if x.strip().isdigit()]
reused = sorted({p for p in pids if pids.count(p) > 1})
if ok["pid_reuse"]:
    got = [e["tid"] for e in ev("PID_REUSE")]
    missing = [p for p in reused if got.count(p) < pids.count(p) - 1]
    bad = [e for e in ev("PID_REUSE") if e["old_start"] == e["start"]]
    cap_check("pid_reuse", f"a PID_REUSE each time the loop got a pid again ({len(pids) - len(set(pids))} times), "
              "with another start time", reused and not missing and not bad, (missing, bad, len(pids)))
else:
    cap_check("pid_reuse", "unavailable: no PID_REUSE events", not ev("PID_REUSE"), ev("PID_REUSE")[:3])

rc = C.report(f"osi_isf_facts {panda.arch_name} {truth.get('uname', ['?'])[0]}")
for c in FACT_CAPS:
    print(f"CAP {c}: {outcome.get(c, 'not checked')} ({caps.get(c)})")
if opt.out:
    with open(opt.out, "w") as fh:
        json.dump(dict(arch=panda.arch_name, release=truth.get("uname", ["?"])[0], caps=caps, outcome=outcome,
                       facts=F, events=events, truth=truth), fh, indent=1, default=str)
sys.exit(rc)
