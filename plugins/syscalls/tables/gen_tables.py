#!/usr/bin/env python3
"""Writes the `syscalls` plugin's system call tables (generated/*.c) from the prototype files.

    gen_tables.py [--outdir DIR]

Sources, per profile (PROFILES below): syscalls2's generated-in/*_prototypes.txt, this
directory's generated-in/linux_{riscv32,loongarch64,x32,arm_oabi}_prototypes.txt
(linux_extra_prototypes.py), and syscalls2's generated-in/context_target.json for the no-return
and double-return flags.

Each call gets its number, name, return type and, per argument, the declared C type (without
__user), a kind (SyscallArgKind in ../syscalls_tables.h), its width and signedness in the guest
ABI, and the argument slots it takes. Each argument also carries what syscalls2 makes of it
(syscall_parser.py's Argument: callback type, syscall_argtype_t, size, name and struct name), so
a syscalls2-compatible layer built on these tables has syscalls2's names and C signatures. The
argument names are syscalls2's: the declared name, "arg<i>" when the prototype has none, and
reserved words prefixed with '_'.

A type this does not know fails the run, so a new prototype can't get a guessed kind.
Prototypes are taken as they are; defects are not corrected here.
"""
import argparse
import json
import logging
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "../../.."))
S2 = os.path.join(ROOT, "plugins/syscalls2")
sys.path.insert(0, os.path.join(S2, "scripts"))
import syscall_parser as s2  # noqa: E402  (syscalls2's generator: compat fields)

logging.getLogger().setLevel(logging.WARNING)

# enum qemu_plugin_syscall_abi (engine/src/include/system/syscall-trap.h), then the appended ones.
ABIS = ["X86_64", "I386", "AARCH64", "ARM", "ARM_OABI", "MIPS", "PPC64", "PPC", "RISCV64",
        "RISCV32", "LOONGARCH64", "MIPS_N32", "MIPS_N64", "X32"]

# Per ABI: pointer and C long size, and how a 64-bit integer argument is passed: in one slot
# (None), in two slots ("pair"), or in two slots starting at an even one ("even"; arm EABI, mips
# o32, ppc32: syscall(2)'s "aligned register pairs").
ABI_CONF = {
    "X86_64": (8, 8, None), "I386": (4, 4, "pair"), "AARCH64": (8, 8, None),
    "ARM": (4, 4, "even"), "ARM_OABI": (4, 4, "pair"), "MIPS": (4, 4, "even"),
    "PPC64": (8, 8, None), "PPC": (4, 4, "even"), "RISCV64": (8, 8, None),
    "RISCV32": (4, 4, "pair"), "LOONGARCH64": (8, 8, None), "MIPS_N32": (4, 4, None),
    "MIPS_N64": (8, 8, None), "X32": (4, 4, None),
}

S2IN = "plugins/syscalls2/generated-in"
EXTRA_IN = "plugins/syscalls/tables/generated-in"

# name, os, abi (None: a compat profile spanning several), prototype file, syscalls2 arch (its
# KNOWN_ARCH: callback widths; None: bits only), context_target.json key, syscalls2 serves it,
# number range kept.
LINUX = [
    ("linux_x64", "X86_64", f"{S2IN}/linux_x64_prototypes.txt", "x64", "linux:x64", True, None),
    ("linux_x86", "I386", f"{S2IN}/linux_x86_prototypes.txt", "x86", "linux:x86", True, None),
    ("linux_arm64", "AARCH64", f"{S2IN}/linux_arm64_prototypes.txt", "arm64", "linux:arm64", True, None),
    ("linux_arm", "ARM", f"{S2IN}/linux_arm_prototypes.txt", "arm", "linux:arm", True, None),
    ("linux_arm_oabi", "ARM_OABI", f"{EXTRA_IN}/linux_arm_oabi_prototypes.txt", "arm", "linux:arm", False, None),
    ("linux_mips", "MIPS", f"{S2IN}/linux_mips_prototypes.txt", "mips", "linux:mips", True, None),
    ("linux_mips64", None, f"{S2IN}/linux_mips64_prototypes.txt", "mips64", "linux:mips64", True, None),
    ("linux_mips64n64", "MIPS_N64", f"{S2IN}/linux_mips64_prototypes.txt", "mips64", "linux:mips64", False,
     (5000, 5999)),
    ("linux_mips64n32", "MIPS_N32", f"{S2IN}/linux_mips64n32_prototypes.txt", "mips64n32", "linux:mips64n32",
     True, None),
    ("linux_ppc", "PPC", f"{S2IN}/linux_ppc_prototypes.txt", "ppc", "linux:ppc", True, None),
    ("linux_ppc64", "PPC64", f"{S2IN}/linux_ppc64_prototypes.txt", "ppc64", "linux:ppc64", True, None),
    ("linux_riscv64", "RISCV64", f"{S2IN}/linux_riscv64_prototypes.txt", "riscv64", "linux:riscv64", True, None),
    # The asm-generic ABIs syscalls2 has no profile for take riscv64's flags (the same calls).
    ("linux_riscv32", "RISCV32", f"{EXTRA_IN}/linux_riscv32_prototypes.txt", None, "linux:riscv64", False, None),
    ("linux_loongarch64", "LOONGARCH64", f"{EXTRA_IN}/linux_loongarch64_prototypes.txt", None, "linux:riscv64",
     False, None),
    ("linux_x32", "X32", f"{EXTRA_IN}/linux_x32_prototypes.txt", None, "linux:x64", False, None),
]
# syscalls2's targets (make_all_generated.sh), for the "serves it" column of the others.
S2_TARGETS = {"freebsd_x64", "windows_2000_x86", "windows_xpsp2_x86", "windows_xpsp3_x86", "windows_7_x86",
              "windows_7_x64"}

KINDS = ["INT", "UINT", "LONG", "ULONG", "PTR", "STR", "WSTR", "BUF", "STRUCT_PTR", "STR_ARRAY"]
POINTERS = {"PTR", "STR", "WSTR", "BUF", "STRUCT_PTR", "STR_ARRAY"}

# Scalar types: (kind, width), width "L" for the ABI's C long, "P" for its pointer size.
SCALARS = {
    "common": {
        "int": ("INT", 4), "signed int": ("INT", 4), "unsigned": ("UINT", 4), "unsigned int": ("UINT", 4),
        "long": ("LONG", "L"), "unsigned long": ("ULONG", "L"), "size_t": ("ULONG", "L"),
        "uint32_t": ("UINT", 4), "uint": ("UINT", 4), "pid_t": ("INT", 4), "uid_t": ("UINT", 4),
        "gid_t": ("UINT", 4), "clockid_t": ("INT", 4), "uintptr_t": ("ULONG", "P"),
    },
    "linux": {
        "__s32": ("INT", 4), "s32": ("INT", 4), "u32": ("UINT", 4), "__u32": ("UINT", 4),
        "u64": ("UINT", 8), "__u64": ("UINT", 8), "loff_t": ("INT", 8),
        "off_t": ("LONG", "L"), "__kernel_long_t": ("LONG", "L"), "__kernel_old_time_t": ("LONG", "L"),
        "old_time32_t": ("INT", 4), "aio_context_t": ("ULONG", "L"), "old_sigset_t": ("ULONG", "L"),
        "compat_size_t": ("UINT", 4), "timer_t": ("INT", 4), "mqd_t": ("INT", 4), "key_t": ("INT", 4),
        "key_serial_t": ("INT", 4), "rwf_t": ("INT", 4), "qid_t": ("UINT", 4), "umode_t": ("UINT", 2),
        # HAVE_UID16 ABIs (x86-32, arm): __kernel_old_uid_t is unsigned short.
        "old_uid_t": ("UINT", 2), "old_gid_t": ("UINT", 2),
    },
    "freebsd": {
        "u_long": ("ULONG", "L"), "__socklen_t": ("UINT", 4), "acl_type_t": ("INT", 4),
        "cpulevel_t": ("INT", 4), "cpuwhich_t": ("INT", 4), "cpusetid_t": ("INT", 4), "idtype_t": ("INT", 4),
        "lwpid_t": ("INT", 4), "mode_t": ("UINT", 2), "dev_t": ("UINT", 8), "id_t": ("INT", 8),
        "off_t": ("INT", 8), "key_t": ("LONG", "L"), "semid_t": ("LONG", "P"), "osigset_t": ("UINT", 4),
        "ffcounter": ("UINT", 8),
    },
    "windows": {
        "ULONG": ("UINT", 4), "LONG": ("INT", 4), "USHORT": ("UINT", 2), "BOOLEAN": ("UINT", 1),
        "NTSTATUS": ("INT", 4), "ACCESS_MASK": ("UINT", 4), "LCID": ("UINT", 4), "LANGID": ("UINT", 2),
        "RTL_ATOM": ("UINT", 2), "EXECUTION_STATE": ("UINT", 4), "SECURITY_INFORMATION": ("UINT", 4),
        "WIN32_PROTECTION_MASK": ("UINT", 4), "NOTIFICATION_MASK": ("UINT", 4),
        "HANDLE": ("ULONG", "P"), "ALPC_HANDLE": ("ULONG", "P"), "SIZE_T": ("ULONG", "P"),
        "ULONG_PTR": ("ULONG", "P"), "KAFFINITY": ("ULONG", "P"),
    },
}
# Enumerations passed by value (int-sized).
ENUMS = {
    "windows": {
        "ALPC_MESSAGE_INFORMATION_CLASS", "ALPC_PORT_INFORMATION_CLASS", "APPHELPCOMMAND",
        "ATOM_INFORMATION_CLASS", "AUDIT_EVENT_TYPE", "DEBUGOBJECTINFOCLASS", "DEVICE_POWER_STATE",
        "ENLISTMENT_INFORMATION_CLASS", "EVENT_INFORMATION_CLASS", "EVENT_TYPE", "FILE_INFORMATION_CLASS",
        "FS_INFORMATION_CLASS", "IO_COMPLETION_INFORMATION_CLASS", "IO_SESSION_STATE", "JOBOBJECTINFOCLASS",
        "KEY_INFORMATION_CLASS", "KEY_SET_INFORMATION_CLASS", "KEY_VALUE_INFORMATION_CLASS",
        "KPROFILE_SOURCE", "KTMOBJECT_TYPE", "MEMORY_INFORMATION_CLASS", "MEMORY_RESERVE_TYPE",
        "MUTANT_INFORMATION_CLASS", "OBJECT_INFORMATION_CLASS", "PLUGPLAY_CONTROL_CLASS",
        "PORT_INFORMATION_CLASS", "POWER_ACTION", "POWER_INFORMATION_LEVEL", "PROCESSINFOCLASS",
        "RESOURCEMANAGER_INFORMATION_CLASS", "SECTION_INFORMATION_CLASS", "SECTION_INHERIT",
        "SEMAPHORE_INFORMATION_CLASS", "SHUTDOWN_ACTION", "SYSDBG_COMMAND", "SYSTEM_INFORMATION_CLASS",
        "SYSTEM_POWER_STATE", "THREADINFOCLASS", "TIMER_INFORMATION_CLASS", "TIMER_SET_INFORMATION_CLASS",
        "TIMER_TYPE", "TOKEN_INFORMATION_CLASS", "TOKEN_TYPE", "TRANSACTIONMANAGER_INFORMATION_CLASS",
        "TRANSACTION_INFORMATION_CLASS", "VDMSERVICECLASS", "WAIT_TYPE", "WORKERFACTORYINFOCLASS",
    },
}
# Typedefs of structures (as a pointee: STRUCT_PTR, the typedef's name).
STRUCT_TYPEDEFS = {
    "linux": {"sigset_t", "fd_set", "siginfo_t", "compat_sigset_t"},
    "freebsd": {"sigset_t", "fd_set", "siginfo_t", "stack_t", "cpuset_t", "domainset_t", "cap_rights_t",
                "ucontext_t"},
    "windows": {"OBJECT_ATTRIBUTES"},
}
# Pointer typedefs: (kind, pointee name or None).
POINTER_TYPEDEFS = {
    "linux": {"cap_user_header_t": ("STRUCT_PTR", "__user_cap_header_struct"),
              "cap_user_data_t": ("STRUCT_PTR", "__user_cap_data_struct"),
              "__sighandler_t": ("PTR", None)},
    "freebsd": {"caddr_t": ("BUF", None)},
    "windows": {"PVOID": ("BUF", None), "PCHAR": ("BUF", None), "PWSTR": ("WSTR", None),
                "PIO_APC_ROUTINE": ("PTR", None), "PTIMER_APC_ROUTINE": ("PTR", None),
                "PPS_APC_ROUTINE": ("PTR", None), "LPGUID": ("STRUCT_PTR", "GUID")},
}
# Windows P<x> typedefs of pointers to scalars; any other P<X> is a pointer to structure X.
WIN_SCALAR_POINTERS = {"PULONG", "PLONG", "PUSHORT", "PHANDLE", "PSIZE_T", "PULONG_PTR", "PBOOLEAN",
                       "PACCESS_MASK", "PNTSTATUS", "PLCID", "PRTL_ATOM", "PEXECUTION_STATE", "PALPC_HANDLE"}
QUALIFIERS = {"const", "volatile", "__user", "__force", "restrict", "__restrict"}
CHARS = {"char", "signed char", "unsigned char"}

# <number> <return type> <name>(<arguments>); -- one or more per line
PROTO = re.compile(r"(?:^|(?<=;))\s*(\d+)\s+([^;]*?\S)\s*?(\w+)\s*\(([^;]*)\);")


class Match:
    """syscall_parser.SysCall's view of a prototype line (its regex's groups)."""
    def __init__(self, *groups):
        self.groups_ = groups

    def group(self, i):
        return self.groups_[i - 1]


def tokens(text):
    return re.findall(r"\w+|\*|\[\]", text)


def ctype_text(toks):
    out = ""
    for t in toks:
        if t in ("*", "[]"):
            out += ("" if not out or out.endswith("*") else " ") + t
        else:
            out += (" " if out else "") + t
    return out


def classify(toks, name, os_, abi):
    """(kind, width, signed, pointee name) of an argument's type tokens (no name, no __user)."""
    ptr, long_ = ABI_CONF[abi][0], ABI_CONF[abi][1]
    depth = sum(t in ("*", "[]") for t in toks)
    words = [t for t in toks if t not in ("*", "[]") and t not in QUALIFIERS]
    base = " ".join(words)
    scalars = SCALARS["common"] | SCALARS.get(os_, {})
    ptypes = POINTER_TYPEDEFS.get(os_, {})
    is_struct = words and words[0] in ("struct", "union")
    is_enum = words and words[0] == "enum" or base in ENUMS.get(os_, ())
    win_struct_ptr = os_ == "windows" and re.fullmatch(r"P[A-Z0-9_]+", base) and base not in WIN_SCALAR_POINTERS \
        and base not in ptypes and base not in ENUMS["windows"]

    def width(w):
        return {"L": long_, "P": ptr}.get(w, w)

    if depth == 0:
        if base in ptypes:
            kind, pointee = ptypes[base]
            return kind, ptr, False, pointee
        if base in WIN_SCALAR_POINTERS and os_ == "windows":
            return "PTR", ptr, False, None
        if win_struct_ptr:
            return "STRUCT_PTR", ptr, False, base[1:]
        if is_enum:
            return "INT", 4, True, None
        if base in scalars:
            kind, w = scalars[base]
            return kind, width(w), kind in ("INT", "LONG"), None
        raise ValueError(f"unknown type {ctype_text(toks)!r}")
    if depth == 1:
        if base in CHARS:
            if base == "char" and not name.endswith("buf"):
                return "STR", ptr, False, None
            return "BUF", ptr, False, None
        if base == "void":
            return "BUF", ptr, False, None
        if is_struct:
            return "STRUCT_PTR", ptr, False, words[1]
        if base in STRUCT_TYPEDEFS.get(os_, ()):
            return "STRUCT_PTR", ptr, False, base
    elif base in CHARS:
        return "STR_ARRAY", ptr, False, None
    known = (base in scalars or base in ptypes or is_enum or is_struct or base == "void" or base in CHARS
             or base in STRUCT_TYPEDEFS.get(os_, ()) or base in WIN_SCALAR_POINTERS or win_struct_ptr)
    if not known:
        raise ValueError(f"unknown type {ctype_text(toks)!r}")
    return "PTR", ptr, False, None


def parse_arg(raw, s2arg, os_, abi):
    """(name, ctype, kind, width, signed, pointee) of one declared argument."""
    toks = tokens(raw.replace("__user", " "))
    r = s2arg.raw
    if not (r.endswith("*") or len(r.split()) == 1 or r in s2.Argument.types32["twoword"]):
        # named (syscall_parser.Argument's rule): drop the name, the last word before any []
        i = len(toks) - 1 - (toks[-1] == "[]")
        if not re.fullmatch(r"\w+", toks[i]):
            raise ValueError(f"no argument name in {raw!r}")
        toks = toks[:i] + toks[i + 1:]
    kind, width, signed, pointee = classify(toks, s2arg.name, os_, abi)
    return ctype_text(toks), kind, width, signed, pointee


def load_context():
    with open(os.path.join(S2, "generated-in/context_target.json")) as f:
        return json.load(f)


def profiles():
    """[profile dict] in a fixed order."""
    out = []
    for name, abi, path, s2arch, ctx, served, rng in LINUX:
        out.append(dict(name=name, os="linux", abi=abi, path=path, s2arch=s2arch, ctx=ctx,
                        served=served, range=rng, version=None))
    out.append(dict(name="freebsd_x64", os="freebsd", abi="X86_64", path=f"{S2IN}/freebsd_x64_prototypes.txt",
                    s2arch="x64", ctx="freebsd:x64", served=True, range=None, version=None))
    for fn in sorted(os.listdir(os.path.join(ROOT, S2IN))):
        m = re.fullmatch(r"windows_(\w+)_(x86|x64)_prototypes\.txt", fn)
        if not m:
            continue
        name = f"windows_{m.group(1)}_{m.group(2)}"
        out.append(dict(name=name, os="windows", abi="I386" if m.group(2) == "x86" else "X86_64",
                        path=f"{S2IN}/{fn}", s2arch=m.group(2), ctx=f"windows_{m.group(1)}:{m.group(2)}",
                        served=name in S2_TARGETS, range=None, version=m.group(1)))
    return out


def build(p, context):
    """The profile's calls: [dict], sorted by number."""
    abi = p["abi"] or "MIPS_N64"
    s2arch = p["s2arch"]
    arch_conf = s2.KNOWN_ARCH[s2arch] if s2arch else {"bits": ABI_CONF[abi][0] * 8}
    tctx = {"arch": s2arch or p["name"], "os": p["os"], "arch_conf": arch_conf} | context.get(p["ctx"], {})
    s2_line = re.compile(r"(\d+) (.+) (\w+) ?\((.*)\);")   # syscall_parser.py's
    with open(os.path.join(ROOT, p["path"])) as f:
        text = f.read().splitlines()
    # What syscalls2 makes of the file (syscall_parser.py's main loop): its SysCall per number,
    # one per (name, argument types and names), the first's argument details standing for all;
    # get_syscall_info() has the numbers of the SysCalls whose first number is generic.
    s2nums, s2calls = {}, {}
    if p["served"]:
        for line in text:
            m = s2_line.match(line)
            if not m:
                continue
            sc = s2.SysCall(m, tctx)
            key = (sc.name, str(sc.args))
            if key in s2calls:
                s2calls[key].nos.append(sc.nos[0])
            else:
                s2calls[key] = sc
            s2nums[int(m.group(1))] = s2calls[key]
    s2names = {sc.name for sc in s2calls.values()}
    calls, seen = [], set()
    for line in text:
        if line.startswith("//"):
            continue
        # a line can hold more than one declaration (freebsd_x64's 325 and 326)
        for m in PROTO.finditer(line):
            num = int(m.group(1))
            if p["range"] and not p["range"][0] <= num <= p["range"][1]:
                continue
            if num in seen:
                raise SystemExit(f"{p['path']}: number {num} twice")
            seen.add(num)
            # the per-call ABI of a multi-ABI profile (mips64: o32, n64, n32 by number)
            cabi = p["abi"] or {4: "MIPS", 5: "MIPS_N64", 6: "MIPS_N32"}[num // 1000]
            rettype, name, args = m.group(2), m.group(3), m.group(4)
            sc = s2.SysCall(Match(m.group(1), rettype, name, args), tctx)
            raws = [a.strip() for a in args.split(",") if a.strip() not in ("", "void")]
            assert len(raws) == len(sc.args), (p["name"], line)
            s2sc = s2nums.get(num)
            if s2sc is not None and s2sc.name != name:
                s2sc = None
            out_args, slot = [], 0
            pairs = ABI_CONF[cabi][2]
            for raw, a, c in zip(raws, sc.args, (s2sc or sc).args):
                try:
                    ctype, kind, width, signed, pointee = parse_arg(raw, a, p["os"], cabi)
                except ValueError as e:
                    raise SystemExit(f"{p['path']}: {num} {name}: {e}")
                n = 1
                if pairs and width == 8 and kind not in POINTERS:
                    if pairs == "even" and slot % 2:
                        slot += 1
                    n = 2
                assert (a.name, a.ctype) == (c.name, c.ctype)
                out_args.append(dict(name=a.name, ctype=ctype, kind=kind, width=width, signed=signed,
                                     pointee=pointee, slot=slot, nslots=n, s2ctype=c.ctype, s2type=c.type,
                                     s2size=int(re.sub(r"\D", "", c.ctype)) // 8, s2tname=c.struct_name))
                slot += n
            flags = []
            if sc.panda_noreturn:
                flags.append("SYSCALLS_NORETURN")
            if sc.panda_double_return:
                flags.append("SYSCALLS_DOUBLE_RETURN")
            if not (s2sc and s2sc.generic):
                flags.append("SYSCALLS_NO_COMPAT_INFO")
            if name not in s2names:
                flags.append("SYSCALLS_NO_COMPAT_CALLBACK")
            calls.append(dict(num=num, name=name, rettype=rettype, args=out_args, nslots=slot, flags=flags,
                              abi=cabi))
    calls.sort(key=lambda c: c["num"])
    # syscalls2's syscall_meta_t (zeros for a profile it doesn't have)
    groups = list(s2calls.values())
    meta = (0, 0, 0)
    if groups:
        meta = (max(max(g.nos) for g in groups),
                max((max(g.nos) for g in groups if g.generic), default=0),
                max(len(g.args) for g in groups))
    return calls, meta


def c_str(s):
    return "NULL" if s is None else '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


S2TYPES = {"U64": 0x00, "U32": 0x01, "U16": 0x02, "S64": 0x10, "S32": 0x11, "S16": 0x12, "BUF_PTR": 0x20,
           "STRUCT_PTR": 0x21, "STR_PTR": 0x22}


def arg_init(a):
    return (f"{{{c_str(a['name'])}, {c_str(a['ctype'])}, {c_str(a['pointee'])}, {c_str(a['s2tname'])}, "
            f"SYSCALLS_ARG_{a['kind']}, {a['width']}, {int(a['signed'])}, {a['slot']}, {a['nslots']}, "
            f"{S2TYPES[a['s2type']]:#04x}, {a['s2size']}}}")


class Pool:
    """Every profile's argument lists, each distinct list once (most are shared: Windows versions,
    Linux ABIs)."""
    def __init__(self):
        self.lines, self.at = [], {}

    def add(self, args):
        key = tuple(arg_init(a) for a in args)
        if key not in self.at:
            self.at[key] = len(self.lines)
            self.lines += key
        return self.at[key]

    def render(self):
        out = ["/* The argument lists of every profile: generated by plugins/syscalls/tables/gen_tables.py.",
               " * Don't edit: regenerate (plugins/syscalls/tables/README.md). */",
               '#include "../../syscalls_tables.h"', "",
               "/* name, ctype, type_name, compat_type_name, kind, width, is_signed, slot, nslots, compat_type,",
               " * compat_size */",
               "const SyscallArg syscalls_arg_pool[] = {"]
        out += [f"    /* {i} */ {line}," if i % 8 == 0 else f"    {line}," for i, line in enumerate(self.lines)]
        out.append("};")
        return "\n".join(out) + "\n"


def render(p, calls, meta, pool):
    sym = p["name"]
    src = os.path.relpath(os.path.join(ROOT, p["path"]), ROOT)
    out = [f"/* {sym}: generated by plugins/syscalls/tables/gen_tables.py from {src}.",
           " * Don't edit: regenerate (plugins/syscalls/tables/README.md). */",
           '#include "../../syscalls_tables.h"', "",
           "extern const SyscallArg syscalls_arg_pool[];", ""]
    out.append("static const SyscallInfo calls[] = {")
    for c in calls:
        flags = " | ".join(c["flags"]) or "0"
        abi = f"SYSCALLS_ABI_{c['abi']}"
        args = f"&syscalls_arg_pool[{pool.add(c['args'])}]" if c["args"] else "NULL"
        decl = ", ".join(f"{a['ctype']}{'' if a['ctype'].endswith('*') else ' '}{a['name']}" for a in c["args"])
        out.append(f"    /* {c['rettype']} {c['name']}({decl or 'void'}) */")
        out.append(f"    {{{c['num']}, {c_str(c['name'])}, {c_str(c['rettype'])}, {abi}, {len(c['args'])}, "
                   f"{c['nslots']}, {flags}, {args}}},")
    out.append("};")
    order = sorted(range(len(calls)), key=lambda i: (calls[i]["name"].encode(), calls[i]["num"]))
    out.append("")
    out.append("static const uint16_t by_name[] = {")
    for k in range(0, len(order), 16):
        out.append("    " + ", ".join(str(i) for i in order[k:k + 16]) + ",")
    out.append("};")
    # Direct index over the numbers from the lowest, while they are dense enough.
    lo = calls[0]["num"]
    span = 0
    for c in calls:
        if c["num"] - lo < 4096:
            span = c["num"] - lo + 1
    dense = [0] * span
    for i, c in enumerate(calls):
        if c["num"] - lo < span:
            dense[c["num"] - lo] = i + 1
    out.append("")
    out.append("static const uint16_t dense[] = {")
    for k in range(0, span, 24):
        out.append("    " + ", ".join(str(v) for v in dense[k:k + 24]) + ",")
    out.append("};")
    abi = f"SYSCALLS_ABI_{p['abi']}" if p["abi"] else "-1"
    pflags = ["SYSCALLS_PROFILE_COMPAT"] if p["served"] else []
    if not p["abi"]:
        pflags.append("SYSCALLS_PROFILE_MULTI_ABI")
    out.append("")
    out.append(f"const SyscallProfile syscalls_profile_{sym} = {{")
    out.append(f"    .name = {c_str(sym)},")
    out.append(f"    .os = SYSCALLS_OS_{p['os'].upper()},")
    out.append(f"    .abi = {abi},")
    out.append(f"    .version = {c_str(p['version'])},")
    out.append(f"    .source = {c_str(src)},")
    out.append(f"    .flags = {' | '.join(pflags) or '0'},")
    out.append(f"    .ncalls = {len(calls)},")
    out.append("    .calls = calls,")
    out.append("    .by_name = by_name,")
    out.append(f"    .dense_lo = {lo},")
    out.append(f"    .dense_n = {span},")
    out.append("    .dense = dense,")
    out.append(f"    .max_args = {max(len(c['args']) for c in calls)},")
    out.append(f"    .compat_max = {meta[0]},")
    out.append(f"    .compat_max_generic = {meta[1]},")
    out.append(f"    .compat_max_args = {meta[2]},")
    out.append("};")
    return "\n".join(out) + "\n"


def render_list(profs):
    out = ["/* The profiles: generated by plugins/syscalls/tables/gen_tables.py. Don't edit. */",
           '#include "../../syscalls_tables.h"', ""]
    for p in profs:
        out.append(f"extern const SyscallProfile syscalls_profile_{p['name']};")
    out.append("")
    out.append("const SyscallProfile *const syscalls_profile_list[] = {")
    for p in profs:
        out.append(f"    &syscalls_profile_{p['name']},")
    out.append("};")
    out.append(f"const size_t syscalls_profile_count = {len(profs)};")
    out.append("")
    out.append("/* Linux's profile for each ABI, the default */")
    out.append("const SyscallProfile *const syscalls_linux_default[SYSCALLS_ABI_COUNT] = {")
    for p in profs:
        if p["os"] == "linux" and p["abi"]:
            out.append(f"    [SYSCALLS_ABI_{p['abi']}] = &syscalls_profile_{p['name']},")
    out.append("};")
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--outdir", default=os.path.join(HERE, "generated"))
    args = ap.parse_args()
    context = load_context()
    profs = profiles()
    os.makedirs(args.outdir, exist_ok=True)
    for f in os.listdir(args.outdir):
        if f.endswith(".c"):
            os.remove(os.path.join(args.outdir, f))
    pool = Pool()
    for p in profs:
        calls, meta = build(p, context)
        if len(calls) > 65535:
            raise SystemExit(f"{p['name']}: too many calls for a uint16_t index")
        with open(os.path.join(args.outdir, f"profile_{p['name']}.c"), "w") as f:
            f.write(render(p, calls, meta, pool))
        print(f"{p['name']}: {len(calls)} calls")
    with open(os.path.join(args.outdir, "args.c"), "w") as f:
        f.write(pool.render())
    with open(os.path.join(args.outdir, "profiles.c"), "w") as f:
        f.write(render_list(profs))
    print(f"{len(pool.lines)} arguments in {len(pool.at)} distinct lists")


if __name__ == "__main__":
    main()
