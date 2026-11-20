"""osi_isf's capability report, without a guest (osi_isf_check, the plugin's host tool).

Run: python3 tests/osi_isf_check.py <plugin dir> [--manifest corpus.json] [--out matrix.json]

1. Eras: synthetic ISFs laid out as Linux 2.6.31 to 6.13 lays out the members OSI reads, on the
   arches osi_isf serves. Each capability must be given the strategy that era and arch call for
   (EXPECT below, written from the kernel's history, not from the plugin's table).
2. Damage: the same ISFs with an input removed, given the wrong width or type, marked unverified
   or reference-carried (a kernel-lift ISF), or with symbols only in a System.map. The capability
   that reads it must be reported unavailable, naming the input, while the others stay available;
   allow_untrusted must bring back a capability whose only problem is trust.
3. Corpus (--manifest): real ISFs, each with optional ground truth (the kernel's own DWARF ISF).
   For every capability the report claims, every input it reads must have the value the truth
   gives it: a claimed capability with a wrong input fails the run. --out writes the
   version x capability matrix.

The manifest is a JSON list of {"name", "arch", "isf", "provenance"?, "symbols"?, "version"?,
"truth"?, "truth_arch"?, "expect"?: {capability: status}}.
"""
import argparse
import base64
import json
import os
import subprocess
import sys
import tempfile

PANDA_TEST = dict(area="osi", tier="static", args=["{plugin_dir}"], timeout=900)

CAPS = ["tasks", "pid", "name", "ppid", "thread_group", "asid", "start_time", "current", "threads",
        "file_paths", "mappings", "mapping_files", "mount_paths", "heap_stack", "modules", "task_change",
        "lifecycle", "exit_codes", "reparent", "pid_reuse", "cmdline", "exe_path", "fs_paths", "creds",
        "cap_sets", "open_files", "sockets", "vma_details"]
# The capabilities of the first round (the OSI interface), and those of process facts and lifecycle.
OSI_CAPS = CAPS[:16]
BITS = {"x86_64": 64, "i386": 32, "aarch64": 64, "arm": 32, "mips": 32, "mips64": 64, "riscv64": 64}


def find_tool(plugin_dir):
    for p in (os.path.join(plugin_dir, "osi_isf", "osi_isf_check"), os.path.join(plugin_dir, "osi_isf_check")):
        if os.access(p, os.X_OK):
            return p
    sys.exit(f"osi_isf_check: not found under {plugin_dir}")


# ---------------------------------------------------------------- synthetic ISFs

def ptr(to, kind="struct"):
    return {"kind": "pointer", "subtype": {"kind": kind, "name": to}}


def base(name):
    return {"kind": "base", "name": name}


def struct(name):
    return {"kind": "struct", "name": name}


def array(n, sub):
    return {"kind": "array", "count": n, "subtype": sub}


def era_isf(ver, arch, banner=True):
    """An ISF with the members, sizes and symbols OSI reads, laid out as kernel ver on arch."""
    v = tuple(ver)
    p = BITS[arch] // 8
    ul = base("long unsigned int")
    types = {}

    def t(name, fields, size=None):
        off, fs = 0, {}
        for fname, ftype in fields:
            fs[fname] = {"offset": off, "type": ftype}
            off += 64   # every member its own slot: offsets need only be distinct
        types[name] = {"kind": "struct", "size": size or off or 8, "fields": fs}

    t("list_head", [("next", ptr("list_head")), ("prev", ptr("list_head"))], 2 * p)
    t("timespec", [("tv_sec", base("long int")), ("tv_nsec", base("long int"))], 2 * p)
    task = [("tasks", struct("list_head")), ("pid", base("int")), ("tgid", base("int")),
            ("comm", array(16, base("char"))), ("mm", ptr("mm_struct")),
            ("real_parent", ptr("task_struct")), ("group_leader", ptr("task_struct")),
            ("start_time", base("long long unsigned int") if v >= (3, 17) else struct("timespec")),
            ("signal", ptr("signal_struct")), ("fs", ptr("fs_struct")), ("files", ptr("files_struct"))]
    # Threads: signal->thread_head and task->thread_node since 3.14; the thread_group ring until 6.7.
    if v < (6, 7):
        task.append(("thread_group", struct("list_head")))
    if v >= (3, 14):
        task.append(("thread_node", struct("list_head")))
    t("atomic_t", [("counter", base("int"))], 4)
    t("signal_struct", [("live", struct("atomic_t"))] + ([("thread_head", struct("list_head"))] if v >= (3, 14) else []))
    # Credentials: task->cred since 2.6.29 (kuid_t since 3.5), the task's own ids before; capability
    # sets a u64 since 6.3, u32 cap[2] since 2.6.25.
    ids = ["uid", "gid", "suid", "sgid", "euid", "egid", "fsuid", "fsgid"]
    capsets = ["cap_effective", "cap_permitted", "cap_inheritable", "cap_bset"]
    if v >= (6, 3):
        t("kernel_cap_struct", [("val", base("long long unsigned int"))], 8)
    else:
        t("kernel_cap_struct", [("cap", array(2, base("unsigned int")))], 8)
    if v >= (3, 5):
        t("kuid_t", [("val", base("unsigned int"))], 4)
    idtype = struct("kuid_t") if v >= (3, 5) else base("unsigned int")
    if v >= (2, 6, 29):
        task.append(("cred", ptr("cred")))
        t("cred", [(i, idtype) for i in ids] + [(c, struct("kernel_cap_struct")) for c in capsets])
    else:
        task += [(i, base("unsigned int")) for i in ids]
        task += [(c, struct("kernel_cap_struct") if v >= (2, 6, 25) else base("unsigned int")) for c in capsets[:3]]
    t("task_struct", task)
    # cwd and root: struct path since 2.6.25 (a seqcount since 2.6.36, seqcount_spinlock_t 5.10),
    # dentries and vfsmounts before.
    if v >= (2, 6, 25):
        fs = [("root", struct("path")), ("pwd", struct("path"))]
        if v >= (5, 10):
            t("seqcount", [("sequence", base("unsigned int"))], 4)
            t("seqcount_spinlock", [("seqcount", struct("seqcount"))], 4)
            fs.append(("seq", struct("seqcount_spinlock")))
        elif v >= (2, 6, 36):
            t("seqcount", [("sequence", base("unsigned int"))], 4)
            fs.append(("seq", struct("seqcount")))
    else:
        fs = [("root", ptr("dentry")), ("pwd", ptr("dentry")), ("rootmnt", ptr("vfsmount")), ("pwdmnt", ptr("vfsmount"))]
    t("fs_struct", fs)
    t("files_struct", [("fdt", ptr("fdtable"))])
    t("fdtable", [("max_fds", base("unsigned int")), ("fd", {"kind": "pointer", "subtype": ptr("file")})])
    t("inode", [("i_mode", base("short unsigned int")), ("i_ino", ul)])
    t("file_system_type", [("name", ptr("char", "base"))])
    t("super_block", [("s_type", ptr("file_system_type"))])
    # Sockets: inet_sock's num/dport/rcv_saddr/daddr, inet_-prefixed from 2.6.33; the addresses
    # in sock_common from 2.6.38, the ports from 3.5; IPv6 addresses there from 3.13.
    common = [("skc_family", base("short unsigned int")), ("skc_state", base("unsigned char"))]
    if v >= (2, 6, 38):
        common += [("skc_daddr", base("unsigned int")), ("skc_rcv_saddr", base("unsigned int"))]
    if v >= (3, 5):
        common += [("skc_dport", base("short unsigned int")), ("skc_num", base("short unsigned int"))]
    if v >= (3, 13):
        t("in6_addr", [("u6_addr8", array(16, base("unsigned char")))], 16)
        common += [("skc_v6_daddr", struct("in6_addr")), ("skc_v6_rcv_saddr", struct("in6_addr"))]
    t("sock_common", common)
    t("sock", [("__sk_common", struct("sock_common"))])
    if v < (2, 6, 33):
        inet = [("num", base("short unsigned int")), ("dport", base("short unsigned int")),
                ("rcv_saddr", base("unsigned int")), ("daddr", base("unsigned int"))]
    else:
        inet = [] if v >= (3, 5) else [("inet_num", base("short unsigned int")), ("inet_dport", base("short unsigned int"))]
        if v < (2, 6, 38):
            inet += [("inet_rcv_saddr", base("unsigned int")), ("inet_daddr", base("unsigned int"))]
    t("inet_sock", [("sk", struct("sock"))] + inet)
    t("socket", [("type", base("short int")), ("sk", ptr("sock"))])
    t("sockaddr_un", [("sun_family", base("short unsigned int")), ("sun_path", array(108, base("char")))], 110)
    t("unix_address", [("len", base("int")), ("name", array(0, struct("sockaddr_un")))])
    t("unix_sock", [("sk", struct("sock")), ("addr", ptr("unix_address"))])
    mm = [("pgd", ptr("pgd_t")), ("start_brk", ul), ("brk", ul), ("start_stack", ul), ("arg_start", ul),
          ("arg_end", ul), ("env_start", ul), ("env_end", ul)]
    if v >= (2, 6, 26):
        mm.append(("exe_file", ptr("file")))
    mm.append(("mm_mt", struct("maple_tree")) if v >= (6, 1) else ("mmap", ptr("vm_area_struct")))
    if v >= (6, 1):
        # Since 6.1 everything but cpu_bitmap is in an anonymous struct.
        t("unnamed_mm", mm)
        t("mm_struct", [("unnamed_field_0", struct("unnamed_mm")), ("cpu_bitmap", array(0, ul))])
        types["mm_struct"]["fields"]["unnamed_field_0"]["anonymous"] = True
        t("maple_tree", [("ma_root", ptr("void", "base"))])
        t("maple_range_64", [("pivot", array(15, ul)), ("slot", array(16, ptr("void", "base")))])
        t("maple_arange_64", [("pivot", array(9, ul)), ("slot", array(10, ptr("void", "base")))])
    else:
        t("mm_struct", mm)
    vma = [("vm_start", ul), ("vm_end", ul), ("vm_mm", ptr("mm_struct")), ("vm_flags", ul),
           ("vm_file", ptr("file")), ("vm_pgoff", ul)]
    if v < (6, 1):
        vma.append(("vm_next", ptr("vm_area_struct")))
    t("vm_area_struct", vma)
    t("path", [("mnt", ptr("vfsmount")), ("dentry", ptr("dentry"))])
    t("file", [("f_path", struct("path")) if v >= (2, 6, 20) else ("f_dentry", ptr("dentry")),
               ("f_flags", base("unsigned int")), ("f_pos", base("long long int")),
               ("private_data", ptr("void", "base"))])
    t("qstr", [("name", ptr("char", "base"))])
    t("dentry", [("d_parent", ptr("dentry")), ("d_name", struct("qstr")), ("d_inode", ptr("inode")),
                 ("d_sb", ptr("super_block"))])
    if v >= (3, 3):
        t("vfsmount", [("mnt_root", ptr("dentry"))])
        t("mount", [("mnt_parent", ptr("mount")), ("mnt_mountpoint", ptr("dentry")), ("mnt", struct("vfsmount"))])
    else:
        t("vfsmount", [("mnt_root", ptr("dentry")), ("mnt_parent", ptr("vfsmount")),
                       ("mnt_mountpoint", ptr("dentry"))])
    mod = [("list", struct("list_head")), ("name", array(56, base("char")))]
    enums = {}
    if v >= (6, 4):
        t("module_memory", [("base", ptr("void", "base")), ("size", base("unsigned int"))])
        mod.append(("mem", array(7, struct("module_memory"))))
        enums["mod_mem_type"] = {"size": 4, "base": "unsigned int", "constants": {"MOD_TEXT": 0}}
    elif v >= (4, 5):
        t("module_layout", [("base", ptr("void", "base")), ("size", base("unsigned int"))])
        mod.append(("core_layout", struct("module_layout")))
    else:
        mod += [("module_core", ptr("void", "base")), ("core_size", base("unsigned int"))]
    t("module", mod)
    t("thread_info", [("task", ptr("task_struct"))])
    types["thread_union"] = {"kind": "union", "size": 8192 if p == 4 else 16384, "fields": {}}

    syms = {"init_task": 0xc0800000, "modules": 0xc0810000, "finish_task_switch.isra.0": 0xc0100000,
            "do_exit": 0xc0110000,
            "__per_cpu_offset": 0xc0820000, "_stext": 0xc0008000, "_etext": 0xc0600000}
    if arch in ("x86_64", "i386"):
        if v >= (6, 2):
            syms["pcpu_hot"] = 0x2000
            t("pcpu_hot", [("current_task", ptr("task_struct"))])
        else:
            # Before 2.6.34 per-CPU variables were named per_cpu__<name>.
            syms["current_task" if v >= (2, 6, 34) else "per_cpu__current_task"] = 0x2000
    elif arch == "aarch64" and v >= (4, 10):
        syms["__entry_task"] = 0xc0830000
    elif arch == "arm" and v >= (5, 18):
        syms["__entry_task"] = 0xc0830000
    elif arch.startswith("mips"):
        syms["kernelsp"] = 0xc0840000
    symbols = {k: {"address": a} for k, a in syms.items()}
    if banner:
        text = f"Linux version {'.'.join(map(str, ver))} (synthetic) #1 SMP\n\0".encode()
        symbols["linux_banner"] = {"address": 0xc0700000, "constant_data": base64.b64encode(text).decode(),
                                   "type": array(0, base("char"))}
    sizes = {"char": 1, "unsigned char": 1, "short int": 2, "short unsigned int": 2, "int": 4,
             "unsigned int": 4, "long int": p, "long unsigned int": p, "long long unsigned int": 8,
             "long long int": 8}
    base_types = {n: {"size": s, "signed": "unsigned" not in n, "kind": "int", "endian": "little"}
                  for n, s in sizes.items()}
    base_types["pointer"] = {"size": p, "signed": False, "kind": "int", "endian": "little"}
    return {"metadata": {"format": "6.2.0", "producer": {"name": "synthetic"}},
            "base_types": base_types, "user_types": types, "enums": enums, "symbols": symbols, "typedefs": {}}


# What each era and arch calls for, from the kernel's history (None: not available).
def expect(ver, arch):
    v = tuple(ver)
    e = {c: c for c in CAPS}   # placeholder: available, strategy not pinned
    e["start_time"] = "u64 nanoseconds" if v >= (3, 17) else "struct timespec"
    e["mappings"] = "maple tree" if v >= (6, 1) else "mmap list"
    e["mount_paths"] = "struct mount" if v >= (3, 3) else "vfsmount"
    e["modules"] = "mem[MOD_TEXT]" if v >= (6, 4) else "core_layout" if v >= (4, 5) else "module_core"
    if arch in ("x86_64", "i386"):
        e["current"] = "pcpu_hot.current_task" if v >= (6, 2) else "current_task"
    elif arch == "aarch64":
        e["current"] = "__entry_task" if v >= (4, 10) else "sp_el1 thread_info"
    elif arch == "arm":
        e["current"] = "__entry_task" if v >= (5, 18) else "svc sp thread_info"
    elif arch.startswith("mips"):
        e["current"] = "kernelsp thread_info"
    elif arch == "riscv64":
        e["current"] = "tp/sscratch"
    e["threads"] = "signal->thread_head" if v >= (3, 14) else "thread_group ring"
    e["file_paths"] = "f_path.dentry" if v >= (2, 6, 20) else "f_dentry"
    e["exit_codes"] = "do_exit hook"
    e["exe_path"] = "mm->exe_file" if v >= (2, 6, 26) else None
    e["fs_paths"] = "fs_struct paths" if v >= (2, 6, 25) else "fs_struct dentries"
    e["creds"] = "task->cred" if v >= (2, 6, 29) else "task_struct ids"
    e["cap_sets"] = "cred u64" if v >= (6, 3) else "cred u32[]" if v >= (2, 6, 29) else \
        "task_struct u32[]" if v >= (2, 6, 25) else "task_struct u32"
    e["sockets"] = "sock_common" if v >= (3, 5) else "inet_sock inet_*" if v >= (2, 6, 33) else "inet_sock"
    return e


ERAS = [((2, 6, 24), "arm"), ((2, 6, 31), "x86_64"), ((2, 6, 36), "arm"), ((3, 4, 0), "mips"), ((3, 10, 0), "x86_64"),
        ((3, 14, 77), "arm"), ((3, 18, 140), "aarch64"), ((4, 10, 0), "mips64"), ((4, 14, 0), "arm"),
        ((5, 10, 229), "x86_64"), ((5, 10, 0), "aarch64"), ((6, 1, 0), "i386"), ((6, 6, 0), "x86_64"),
        ((6, 6, 0), "arm"), ((6, 13, 0), "arm"), ((6, 13, 0), "mips"), ((6, 12, 0), "riscv64")]


# ---------------------------------------------------------------- running the tool

class Tool:
    def __init__(self, path, work):
        self.path, self.work, self.n = path, work, 0

    def write(self, isf, name="isf.json"):
        self.n += 1
        f = os.path.join(self.work, f"{self.n}-{name}")
        with open(f, "w") as fh:
            json.dump(isf, fh)
        return f

    def report(self, isf, arch, *extra):
        r = subprocess.run([self.path, "--isf", isf, "--arch", arch, *extra], capture_output=True, text=True)
        if r.returncode:
            raise RuntimeError(f"osi_isf_check {isf} {arch}: {r.stderr.strip()}")
        return json.loads(r.stdout)


def tool_arch(arch):
    return "mips" if arch.startswith("mips") else arch


class Checks:
    def __init__(self):
        self.failed = 0

    def check(self, name, ok, detail=""):
        print(f"  {'PASS' if ok else 'FAIL'} {name}" + ("" if ok else f": {detail}"))
        self.failed += not ok


def check_eras(tool, C):
    print("eras:")
    for ver, arch in ERAS:
        rep = tool.report(tool.write(era_isf(ver, arch)), tool_arch(arch))
        exp = expect(ver, arch)
        caps = rep["capabilities"]
        # None: the era has no strategy for it (exe_file is 2.6.26+).
        bad = {c: (caps[c]["status"], caps[c].get("strategy"), caps[c].get("reason"))
               for c in CAPS if (caps[c]["status"] != "available") != (exp[c] is None)
               or (exp[c] not in (c, None) and caps[c].get("strategy") != exp[c])}
        C.check(f"{'.'.join(map(str, ver))} {arch}: every capability, with its era's strategy", not bad, bad)


def ul_type(isf):
    return base("long unsigned int")


def cap_reason(rep, cap):
    c = rep["capabilities"][cap]
    return c["status"], c.get("reason", "")


def check_damage(tool, C):
    print("damage:")
    good = era_isf((6, 6, 0), "x86_64")

    def damaged(edit, ver=(6, 6, 0), arch="x86_64", banner=True):
        isf = era_isf(ver, arch, banner)
        edit(isf)
        return isf

    def others_ok(rep, but):
        return all(rep["capabilities"][c]["status"] == "available" for c in CAPS if c not in but)

    # A member removed: its capabilities go (and those that need them), the rest stay.
    rep = tool.report(tool.write(damaged(lambda i: i["user_types"]["task_struct"]["fields"].pop("comm"))), "x86_64")
    st, why = cap_reason(rep, "name")
    C.check("comm removed: name unavailable, naming task_struct.comm",
            st == "unavailable" and "comm" in why and others_ok(rep, ["name"]), (st, why))

    # The wrong width: an 8-byte pid.
    rep = tool.report(tool.write(damaged(lambda i: i["user_types"]["task_struct"]["fields"]["pid"].update(
        type=base("long long int")))), "x86_64")
    st, why = cap_reason(rep, "pid")
    on_pid = ["pid", "threads", "lifecycle", "exit_codes", "reparent", "pid_reuse", "cmdline"]
    C.check("an 8-byte pid: pid unavailable (width), and what needs it; the rest available",
            st == "unavailable" and "8 bytes" in why and others_ok(rep, on_pid)
            and all(rep["capabilities"][c]["status"] == "unavailable" for c in on_pid), (st, why))

    # The wrong type for the era: a timespec start_time in a 6.6 kernel is no u64, and the
    # timespec strategy is for kernels before 3.17.
    rep = tool.report(tool.write(damaged(lambda i: i["user_types"]["task_struct"]["fields"]["start_time"].update(
        type=struct("timespec")))), "x86_64")
    st, why = cap_reason(rep, "start_time")
    C.check("a timespec start_time in 6.6: unavailable, an 8-byte read refused",
            st == "unavailable" and "not a 8-byte integer" in why and "3.17" in why, (st, why))

    # The version decides: a 3.10 kernel with a u64 start_time (the 3.17+ layout).
    rep = tool.report(tool.write(damaged(lambda i: i["user_types"]["task_struct"]["fields"]["start_time"].update(
        type=base("long long unsigned int")), ver=(3, 10, 0))), "x86_64")
    st, why = cap_reason(rep, "start_time")
    C.check("a u64 start_time in 3.10: unavailable (the u64 strategy is for 3.17+)",
            st == "unavailable" and "3.17" in why, (st, why))

    # No banner, no version: chosen by what the ISF has; --version pins it.
    nb = tool.write(era_isf((3, 10, 0), "x86_64", banner=False))
    rep = tool.report(nb, "x86_64")
    C.check("no banner: version unknown, strategies by the ISF's contents",
            rep["version"] == "unknown" and rep["capabilities"]["start_time"].get("strategy") == "struct timespec"
            and rep["capabilities"]["mappings"].get("strategy") == "mmap list", rep["version"])
    rep = tool.report(nb, "x86_64", "--version", "6.6.0")
    st, why = cap_reason(rep, "mappings")
    C.check("--version 6.6 on a 3.10 layout: mappings unavailable (mmap list is for <6.1)",
            st == "unavailable" and "6.1" in why, (st, why))

    # A missing symbol: tasks go; a System.map brings it back.
    isf = damaged(lambda i: i["symbols"].pop("init_task"))
    f = tool.write(isf)
    rep = tool.report(f, "x86_64")
    st, why = cap_reason(rep, "tasks")
    C.check("no init_task symbol: tasks unavailable", st == "unavailable" and "init_task" in why, (st, why))
    smap = os.path.join(tool.work, "System.map")
    with open(smap, "w") as fh:
        fh.write("ffffffff82a0c940 D init_task\nffffffff81000000 T _stext\n")
    rep = tool.report(f, "x86_64", "--symbols", smap)
    C.check("... and init_task from a System.map: tasks available",
            rep["capabilities"]["tasks"]["status"] == "available"
            and rep["capabilities"]["tasks"]["inputs"][0].get("class") == "symbol file", cap_reason(rep, "tasks"))

    # kernel-lift: an unverified member is not used unless allowed.
    def write_lift(ver, arch, unverified=(), carried=None, name="lift"):
        isf = era_isf(ver, arch)
        isf["metadata"]["kernel_lift"] = {"unverified_needed": list(unverified), "inferred_needed": [],
                                          "provenance": "provenance.json"}
        d = os.path.join(tool.work, name)
        os.makedirs(d, exist_ok=True)
        prov = {"fields": {}}
        for tname, ty in isf["user_types"].items():
            # Classed flattened, as kernel-lift does: an anonymous member's members are the record's.
            names = []
            for f, fv in ty["fields"].items():
                inner = isf["user_types"].get(fv["type"].get("name", "")) if fv.get("anonymous") else None
                names += list(inner["fields"]) if inner else [f]
            bad = (carried or {}).get(tname, [])
            prov["fields"][tname] = {"target-verified": {f: {} for f in names if f not in bad},
                                     "reference-carried": list(bad)}
        with open(os.path.join(d, "provenance.json"), "w") as fh:
            json.dump(prov, fh)
        f = os.path.join(d, "cosi.json")
        with open(f, "w") as fh:
            json.dump(isf, fh)
        return f

    # mm_struct.pgd is reference-carried: kernel-lift classes it in the named record, though it
    # sits in an anonymous struct since 6.1.
    lf = write_lift((6, 6, 0), "x86_64", unverified=["task_struct.tgid"],
                    carried={"mm_struct": ["pgd", "start_brk", "brk", "start_stack", "mm_mt"]})
    rep = tool.report(lf, "x86_64")
    st, why = cap_reason(rep, "pid")
    C.check("lift, tgid unverified: pid and ppid unavailable (untrusted)",
            st == "unavailable" and "untrusted" in why and "tgid" in why
            and rep["capabilities"]["ppid"]["status"] == "unavailable", (st, why))
    st, why = cap_reason(rep, "asid")
    C.check("lift, mm_struct.pgd reference-carried: asid unavailable; name, tasks, current available",
            st == "unavailable" and "pgd" in why and all(rep["capabilities"][c]["status"] == "available"
                                                          for c in ("name", "tasks", "current")), (st, why))
    rep13 = tool.report(write_lift((6, 13, 0), "arm", name="lift613"), "arm")
    st, why = cap_reason(rep13, "modules")
    C.check("lift 6.13: sizeof(module_memory) and MOD_TEXT are not verified, so modules unavailable",
            st == "unavailable" and "module_memory" in why and others_ok(rep13, ["modules"]),
            (st, why, {c: cap_reason(rep13, c) for c in CAPS if rep13["capabilities"][c]["status"] != "available"}))
    # THREAD_SIZE (the stack-based current task): a lift's is the reference's, so mips's current
    # needs thread_size= (kernel-lift's modabi reads it from the kernel's code).
    lm = write_lift((6, 13, 0), "mips", name="liftmips")
    st, why = cap_reason(tool.report(lm, "mips"), "current")
    C.check("lift, mips: current unavailable, THREAD_SIZE reference-carried", st == "unavailable"
            and "THREAD_SIZE" in why, (st, why))
    st, why = cap_reason(tool.report(lm, "mips", "--thread-size", "16384"), "current")
    C.check("... and available with --thread-size", st == "available", (st, why))

    # Since 4.16 a kernel may have no union thread_union: init_stack[] gives THREAD_SIZE.
    def no_union(i):
        i["user_types"].pop("thread_union")
        i["symbols"]["init_stack"] = {"address": 0xc0900000, "type": array(2048, base("long unsigned int"))}
    rep = tool.report(tool.write(damaged(no_union, ver=(6, 13, 0), arch="mips64")), "mips")
    cur = rep["capabilities"]["current"]
    C.check("no thread_union, init_stack[2048] of 8 bytes: THREAD_SIZE 16384",
            cur["status"] == "available" and any(i["input"] == "THREAD_SIZE" and i.get("value") == 16384
                                                 for i in cur["inputs"]), cur.get("inputs"))
    rep = tool.report(tool.write(damaged(lambda i: i["user_types"].pop("thread_union"), ver=(6, 13, 0),
                                         arch="mips")), "mips")
    st, why = cap_reason(rep, "current")
    C.check("no thread_union, no init_stack: current unavailable (THREAD_SIZE)",
            st == "unavailable" and "THREAD_SIZE" in why, (st, why))

    # Process facts and lifecycle.
    def drop(t, m):
        return lambda i: i["user_types"][t]["fields"].pop(m)
    rep = tool.report(tool.write(damaged(lambda i: (drop("task_struct", "thread_node")(i),
                                                    drop("signal_struct", "thread_head")(i)),
                                         ver=(6, 13, 0), arch="arm")), "arm")
    st, why = cap_reason(rep, "threads")
    C.check("6.13 without thread_head/thread_node: threads unavailable (the thread_group ring is gone since 6.7)",
            st == "unavailable" and "thread_head" in why and others_ok(rep, ["threads"]), (st, why))
    rep = tool.report(tool.write(damaged(lambda i: i["user_types"]["cred"]["fields"].update(
        {k: {"offset": v["offset"], "type": base("unsigned int")} for k, v in i["user_types"]["cred"]["fields"].items()
         if not k.startswith("cap_")}), ver=(3, 10, 0))), "x86_64")
    ins = {x["input"]: x.get("used") for x in rep["capabilities"]["creds"]["inputs"]}
    C.check("3.10 with plain uid_t in cred (no strict type checks): creds from the uid alternative",
            rep["capabilities"]["creds"]["status"] == "available" and ins.get("cred.uid.val|uid") == "uid", ins)
    rep = tool.report(tool.write(damaged(lambda i: i["user_types"]["kernel_cap_struct"].update(
        fields={"val": {"offset": 0, "type": base("long long unsigned int")}}), ver=(6, 1, 0), arch="i386")), "i386")
    st, why = cap_reason(rep, "cap_sets")
    C.check("6.1 with a u64 kernel_cap_t: cap_sets unavailable (the u64 strategy is for 6.3+)",
            st == "unavailable" and "6.3" in why, (st, why))
    rep = tool.report(tool.write(damaged(lambda i: i["user_types"]["sock_common"]["fields"].pop("skc_num"))), "x86_64")
    st, why = cap_reason(rep, "sockets")
    C.check("no skc_num (nor inet_num): sockets unavailable, open files available",
            st == "unavailable" and "skc_num" in why and others_ok(rep, ["sockets"]), (st, why))
    rep = tool.report(tool.write(damaged(lambda i: i["user_types"]["unix_sock"]["fields"].pop("addr"))), "x86_64")
    C.check("no unix_sock.addr: sockets available without unix socket paths",
            rep["capabilities"]["sockets"]["status"] == "available"
            and any("addr" in d for d in rep["capabilities"]["sockets"].get("degraded", [])),
            rep["capabilities"]["sockets"].get("degraded"))
    rep = tool.report(tool.write(damaged(lambda i: i["user_types"]["mm_struct"]["fields"]["exe_file"].update(
        type=ul_type(i)), ver=(5, 10, 229))), "x86_64")
    st, why = cap_reason(rep, "exe_path")
    C.check("exe_file an integer: exe_path unavailable (shape)", st == "unavailable" and "not a pointer" in why
            and others_ok(rep, ["exe_path"]), (st, why))
    rep = tool.report(tool.write(damaged(lambda i: i["symbols"].pop("do_exit"))), "x86_64")
    C.check("no do_exit symbol: exit statuses from the exit system calls",
            rep["capabilities"]["exit_codes"].get("strategy") == "exit system calls", cap_reason(rep, "exit_codes"))
    rep = tool.report(tool.write(damaged(lambda i: i["user_types"]["file"]["fields"].pop("f_path"))), "x86_64")
    gone = [c for c in CAPS if rep["capabilities"][c]["status"] == "unavailable"]
    C.check("no file.f_path: file paths, and everything that names files, unavailable",
            sorted(gone) == sorted(["file_paths", "mapping_files", "mount_paths", "exe_path", "fs_paths",
                                    "open_files", "sockets"]), gone)
    lc = write_lift((6, 6, 0), "x86_64", carried={"cred": ["euid"]}, name="liftcred")
    st, why = cap_reason(tool.report(lc, "x86_64"), "creds")
    C.check("lift, cred.euid reference-carried: creds unavailable (untrusted)",
            st == "unavailable" and "euid" in why and "untrusted" in why, (st, why))

    rep = tool.report(lf, "x86_64", "--allow-untrusted")
    C.check("lift with --allow-untrusted: pid, asid and mappings available from untrusted inputs",
            all(rep["capabilities"][c]["status"] == "available-untrusted" for c in ("pid", "asid", "mappings")),
            {c: rep["capabilities"][c]["status"] for c in ("pid", "asid", "mappings")})
    # A lift without its provenance.json trusts nothing it can't show.
    os.remove(os.path.join(os.path.dirname(lf), "provenance.json"))
    rep = tool.report(lf, "x86_64")
    C.check("lift without provenance.json: members unknown, so untrusted",
            rep["capabilities"]["name"]["status"] == "unavailable" and "unknown" in cap_reason(rep, "name")[1],
            cap_reason(rep, "name"))
    # No strategy for an arch: a clear reason.
    rep = tool.report(tool.write(good), "ppc")
    needs_current = ["current", "task_change", "lifecycle", "exit_codes", "reparent", "pid_reuse", "cmdline"]
    C.check("ppc: current unavailable (no strategy for ppc), so what needs it; the rest available",
            cap_reason(rep, "current") == ("unavailable", "no strategy for ppc")
            and all(rep["capabilities"][c]["status"] == "unavailable" for c in needs_current)
            and others_ok(rep, needs_current),
            {c: cap_reason(rep, c) for c in CAPS if rep["capabilities"][c]["status"] != "available"})


# ---------------------------------------------------------------- the corpus

def inputs_of(rep, cap):
    return {i["input"]: i.get("value") for i in rep["capabilities"][cap]["inputs"] if i.get("ok")}


def grade(rep, truth, cap):
    """The inputs a claimed capability reads that the truth gives another value (symbols aside:
    the truth's symbols are of the same image)."""
    tin = {}
    for c in truth["capabilities"].values():
        for at in c.get("attempts", []):
            for i in at["inputs"]:
                if i.get("ok"):
                    tin[(i["kind"], i["input"])] = i.get("value")
    wrong = []
    for i in rep["capabilities"][cap]["inputs"]:
        if not i.get("ok") or i["kind"] == "sym" or (i.get("optional") and not i.get("trusted")):
            continue
        tv = tin.get((i["kind"], i["input"]), "absent")
        if tv != i.get("value"):
            wrong.append(f"{i['input']}={i.get('value')} (truth {tv})")
    return wrong


def check_corpus(tool, C, manifest, out):
    print("corpus:")
    rows = []
    for e in manifest:
        args = []
        for k in ("provenance", "symbols", "version"):
            if e.get(k):
                args += [f"--{k}", e[k]]
        try:
            rep = tool.report(e["isf"], e["arch"], *args)
        except RuntimeError as err:
            C.check(f"{e['name']}: report", False, err)
            continue
        row = {"name": e["name"], "arch": e["arch"], "version": rep["version"], "isf_kind": rep["isf_kind"],
               "caps": {c: [rep["capabilities"][c]["status"], rep["capabilities"][c].get("strategy")] for c in CAPS},
               "reasons": {c: rep["capabilities"][c].get("reason") for c in CAPS
                           if rep["capabilities"][c]["status"] == "unavailable"}}
        if e.get("truth"):
            # Every strategy's inputs, so that a lift that falls back to a later strategy is graded too.
            truth = tool.report(e["truth"], e.get("truth_arch", e["arch"]), "--all-attempts")
            wrong = {c: grade(rep, truth, c) for c in CAPS if rep["capabilities"][c]["status"] == "available"}
            wrong = {c: w for c, w in wrong.items() if w}
            # What a trusted ISF would also give: capabilities lost to trust, for the matrix.
            row["truth_caps"] = {c: truth["capabilities"][c]["status"] for c in CAPS}
            row["wrong"] = wrong
            untrusted_wrong = {}
            if rep["isf_kind"] == "kernel-lift":
                rep_u = tool.report(e["isf"], e["arch"], *args, "--allow-untrusted")
                for c in CAPS:
                    if rep_u["capabilities"][c]["status"] == "available-untrusted":
                        w = grade(rep_u, truth, c)
                        if w:
                            untrusted_wrong[c] = w
                row["untrusted_caps"] = {c: rep_u["capabilities"][c]["status"] for c in CAPS}
            row["untrusted_wrong"] = untrusted_wrong
            C.check(f"{e['name']} ({rep['version']} {e['arch']}, {rep['isf_kind']}): no claimed capability "
                    f"reads a wrong input", not wrong, wrong)
        if e.get("expect"):
            got = {c: rep["capabilities"][c]["status"] for c in e["expect"]}
            C.check(f"{e['name']}: expected statuses", got == e["expect"], {c: (got[c], e["expect"][c])
                                                                          for c in got if got[c] != e["expect"][c]})
        rows.append(row)
        avail = sum(1 for c in CAPS if row["caps"][c][0] == "available")
        print(f"    {e['name']}: {rep['version']} {e['arch']} {rep['isf_kind']}: {avail}/{len(CAPS)} available")
    if out:
        with open(out, "w") as fh:
            json.dump(rows, fh, indent=1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("plugin_dir")
    ap.add_argument("--manifest")
    ap.add_argument("--out")
    a = ap.parse_args()
    C = Checks()
    with tempfile.TemporaryDirectory() as work:
        tool = Tool(find_tool(a.plugin_dir), work)
        check_eras(tool, C)
        check_damage(tool, C)
        if a.manifest:
            with open(a.manifest) as fh:
                check_corpus(tool, C, json.load(fh), a.out)
    print(f"osi_isf_check: {'FAIL' if C.failed else 'PASS'} ({C.failed} failed)")
    sys.exit(1 if C.failed else 0)


if __name__ == "__main__":
    main()
