#!/usr/bin/env python3
"""Writes syscalls2's Linux prototype tables (generated-in/linux_<abi>_prototypes.txt) from a
Linux source tree: the system call numbers from the kernel's syscall tables, the prototypes from
the kernel's own declarations.

    linux_prototypes.py <linux source: directory or .tar.xz> <generated-in dir>

`nix run .#regen-syscalls` runs it on the flake's pinned kernel (nixpkgs' linux_latest) and then
make_all_generated.sh; the flake check `syscalls2-tables` fails if the committed tables differ
from what that produces (MAINTENANCE.md).

Per ABI (ABIS below):
- numbers: the ABI's rows of its *.tbl (arch/*/syscall*.tbl, or scripts/syscall.tbl's ABI list in
  arch/<arch>/kernel/Makefile.syscalls for arm64 and riscv), plus the ABI's offset (mips: 4000,
  5000, 6000). A row without an entry point, or with sys_ni_syscall, is not a system call there.
- name: the entry point (sys_<x>, the name the kernel's own tables use and syscalls2's callbacks
  are named after: on_sys_<x>_enter). Assembler stubs and arch prefixes are mapped to the sys_<x>
  they wrap (__sys_clone -> sys_clone, sysm_pipe -> sys_pipe, *_wrapper -> *), and a compat entry
  (mips n32) to the native sys_<x> it stands in for.
- prototype: the entry point's declaration in include/linux/syscalls.h, with that header's
  #if/#ifdef evaluated for the ABI (CONFIG_CLONE_BACKWARDS, BITS_PER_LONG, ... in ABIS); else
  the arch's own: asmlinkage declarations in arch/<arch>/include/asm/syscalls*.h, and
  SYSCALL_DEFINE<n>/asmlinkage definitions in arch/<arch>/**/*.c; else EXTRA below. A system call
  with none of these fails the run, so a new one can't be dropped silently.
Numbers only grow, so the newest kernel's table also describes older guests (a number that is
absent from an older kernel is never issued there).
"""
import io
import os
import re
import sys
import tarfile

GENERIC = "scripts/syscall.tbl"
X86_64 = "arch/x86/entry/syscalls/syscall_64.tbl"
X86_32 = "arch/x86/entry/syscalls/syscall_32.tbl"
ARM = "arch/arm/tools/syscall.tbl"
MIPS = "arch/mips/kernel/syscalls/syscall_{}.tbl"
PPC = "arch/powerpc/kernel/syscalls/syscall.tbl"

# The kernel configuration each ABI's kernel has, for syscalls.h's #if/#ifdef: the options that
# choose between two prototypes of one system call (from arch/<arch>/Kconfig's selects). Options
# that only add declarations (__ARCH_WANT_*, ...) are taken as set: the table decides which
# system calls an ABI has. CONFIG_ARCH_SPLIT_ARG64 (x86-32, ppc32) is left unset: it declares a
# 64-bit argument as two u32 halves, and syscall_parser.py already reads a u64 on a 32-bit ABI from
# two registers (the 64-bit ABIs' callbacks, compiled into the same plugin, take the same count).
CONFIG_32 = {"BITS_PER_LONG": 32}
# o32, and n32 (whose entries are the 64-bit kernel's compat ones: 32-bit calls such as fcntl64,
# declared only for BITS_PER_LONG == 32).
MIPS32 = CONFIG_32 | {"CONFIG_CLONE_BACKWARDS": 1}
# arch: arch/<arch>; tables: (tbl, its ABIs, number offset[, config for these rows]).
ABIS = {
    "x64": dict(arch="x86", tables=[(X86_64, "common 64", 0)], config={"BITS_PER_LONG": 64}),
    "x86": dict(arch="x86", tables=[(X86_32, "i386", 0)],
                config=CONFIG_32 | {"CONFIG_CLONE_BACKWARDS": 1, "CONFIG_OLD_SIGACTION": 1,
                                    "CONFIG_HAVE_UID16": 1, "CONFIG_OLD_SIGSUSPEND3": 1}),
    "arm": dict(arch="arm", tables=[(ARM, "common eabi", 0)], arm_private=True,
                config=CONFIG_32 | {"CONFIG_CLONE_BACKWARDS": 1, "CONFIG_HAVE_UID16": 1,
                                    "CONFIG_OLD_SIGACTION": 1, "CONFIG_OLD_SIGSUSPEND3": 1}),
    "arm64": dict(arch="arm64", tables=[("arch/arm64/tools/syscall_64.tbl", "common 64", 0)],
                  makefile_abis="arch/arm64/kernel/Makefile.syscalls",
                  config={"BITS_PER_LONG": 64, "CONFIG_CLONE_BACKWARDS": 1}),
    "riscv64": dict(arch="riscv", tables=[(GENERIC, "common 64", 0)],
                    makefile_abis="arch/riscv/kernel/Makefile.syscalls",
                    config={"BITS_PER_LONG": 64, "CONFIG_CLONE_BACKWARDS": 1}),
    "mips": dict(arch="mips", tables=[(MIPS.format("o32"), "o32", 4000)], config=MIPS32),
    # One table for a 64-bit kernel's three ABIs, told apart by number.
    "mips64": dict(arch="mips", tables=[(MIPS.format("o32"), "o32", 4000, MIPS32),
                                        (MIPS.format("n64"), "n64", 5000),
                                        (MIPS.format("n32"), "n32", 6000, MIPS32)],
                   config={"BITS_PER_LONG": 64, "CONFIG_CLONE_BACKWARDS": 1}),
    "mips64n32": dict(arch="mips", tables=[(MIPS.format("n32"), "n32", 6000)], config=MIPS32),
    "ppc": dict(arch="powerpc", tables=[(PPC, "common 32 nospu", 0)],
                config=CONFIG_32 | {"CONFIG_CLONE_BACKWARDS": 1, "CONFIG_OLD_SIGACTION": 1,
                                    "CONFIG_OLD_SIGSUSPEND": 1}),
    "ppc64": dict(arch="powerpc", tables=[(PPC, "common 64 nospu", 0)],
                  config={"BITS_PER_LONG": 64, "CONFIG_CLONE_BACKWARDS": 1, "CONFIG_OLD_SIGSUSPEND": 1}),
}

# Per kernel arch: entry points with no C declaration in the kernel's headers or the arch's
# sources (assembler stubs).
EXTRA = {
    # arch/arm/kernel/entry-common.S: sys_mmap2 checks the offset and branches to sys_mmap_pgoff.
    "arm": {"sys_mmap2": "long sys_mmap2(unsigned long addr, unsigned long len, unsigned long prot, "
                         "unsigned long flags, unsigned long fd, unsigned long pgoff);"},
    # arch/mips/kernel/scall32-o32.S: sys_syscall, o32's indirect system call (number, then that
    # call's arguments; the first three are in registers).
    "mips": {"sys_syscall": "long sys_syscall(unsigned long number, unsigned long arg1, unsigned long arg2, "
                            "unsigned long arg3);"},
}

# ARM's private system calls (arch/arm/include/uapi/asm/unistd.h __ARM_NR_*, arch/arm/kernel/traps.c
# arm_syscall), numbered from __ARM_NR_BASE = 0x0f0000. Their names and prototypes are the ones
# syscalls2 has always had (ARM_<name>).
ARM_PRIVATE = {
    "breakpoint": "long ARM_breakpoint(void);",
    "cacheflush": "long ARM_cacheflush(unsigned long start, unsigned long end, unsigned long flags);",
    "usr26": "long ARM_user26_mode(void);",
    "usr32": "long ARM_usr32_mode(void);",
    "set_tls": "long ARM_set_tls(unsigned long arg);",
    "get_tls": "long ARM_get_tls(void);",
}


class Source:
    """The few files of a Linux tree this reads, from a directory or a .tar.xz (one pass)."""

    WANT = re.compile(r"(Makefile|.*\.tbl|arch/[^/]+/kernel/Makefile\.syscalls|include/linux/syscalls\.h"
                      r"|arch/(x86|arm|arm64|riscv|mips|powerpc)/.*\.(c|h))$")

    def __init__(self, path):
        self.files = {}
        if os.path.isdir(path):
            for root, _, names in os.walk(path):
                for n in names:
                    rel = os.path.relpath(os.path.join(root, n), path)
                    if self.WANT.match(rel):
                        with open(os.path.join(root, n), errors="replace") as f:
                            self.files[rel] = f.read()
        else:
            links = {}
            with tarfile.open(path) as t:
                for m in t:
                    rel = m.name.split("/", 1)[1] if "/" in m.name else m.name
                    if not self.WANT.match(rel):
                        continue
                    if m.isfile():
                        self.files[rel] = t.extractfile(m).read().decode(errors="replace")
                    elif m.issym():   # arch/arm64/tools/syscall_64.tbl -> scripts/syscall.tbl
                        links[rel] = os.path.normpath(os.path.join(os.path.dirname(rel), m.linkname))
            for rel, target in links.items():
                self.files[rel] = self.files[target]
        mk = self.files["Makefile"]
        v = {k: re.search(rf"^{k} = (\S*)", mk, re.M).group(1) for k in ("VERSION", "PATCHLEVEL", "SUBLEVEL")}
        self.version = f"{v['VERSION']}.{v['PATCHLEVEL']}" + (f".{v['SUBLEVEL']}" if v["SUBLEVEL"] else "")

    def glob(self, prefix, suffixes):
        return sorted(p for p in self.files if p.startswith(prefix) and p.endswith(suffixes))


def table(src, path, abis, offset):
    """[(nr, name, entry)] of the rows of a *.tbl in abis, entry None when it has none."""
    rows = []
    for line in src.files[path].splitlines():
        f = line.split("#")[0].split()
        if len(f) < 3 or f[1] not in abis:
            continue
        entry = f[3] if len(f) > 3 and f[3] != "-" else None
        rows.append((int(f[0]) + offset, f[2], entry))
    return rows


def makefile_abis(src, path):
    m = re.search(r"^syscall_abis_64\s*\+=(.*)$", src.files[path], re.M)
    return m.group(1).split() if m else []


def normalize(sig):
    """One line, single spaces, '*name' and 'name(' as the old tables have them. A function whose
    only parameter is the struct pt_regs * its entry stub passes (arm's sigreturn) takes no
    argument from the caller: (void)."""
    sig = " ".join(sig.replace("asmlinkage", " ").split())
    sig = re.sub(r"\(\s*struct pt_regs\s*\*\s*\w*\s*\)", "(void)", sig)
    sig = ", ".join(t.strip() for t in sig.split(","))
    sig = re.sub(r"\*\s(\w+)(?=[,)])", r"*\1", sig)
    sig = re.sub(r"\s*\(\s*", "(", sig, count=1).replace(" )", ")")
    return sig if sig.endswith(";") else sig + ";"


def defined(name, config):
    return name in config or name.startswith("__ARCH_WANT_") or name == "CONFIG_ADVISE_SYSCALLS"


def eval_condition(expr, config):
    expr = re.sub(r"defined\s*\(\s*(\w+)\s*\)", lambda m: str(int(defined(m.group(1), config))), expr)
    expr = re.sub(r"\b[A-Za-z_]\w*\b", lambda m: str(config.get(m.group(0), int(defined(m.group(0), config)))), expr)
    expr = expr.replace("||", " or ").replace("&&", " and ").replace("!", " not ").replace(" not =", "!=")
    return bool(eval(expr, {}))


def preprocess_ifs(text, config):
    """text with the lines of false #if/#ifdef/#ifndef branches dropped (no macro expansion).
    CONFIG_ARCH_HAS_SYSCALL_WRAPPER is never set: on the arches that have it the declarations it
    hides are still the system calls' C prototypes."""
    out, stack = [], []   # stack: [taking this branch, a branch was taken, parent taking]
    lines = re.sub(r"\\\n", " ", text).splitlines()
    for line in lines:
        m = re.match(r"\s*#\s*(ifdef|ifndef|if|elif|else|endif)\b(.*)", line)
        if m:
            d, arg = m.group(1), m.group(2).split("/*")[0].strip()
            parent = all(s[0] for s in stack)
            if d in ("ifdef", "ifndef", "if"):
                if d == "if":
                    cond = eval_condition(arg, config)
                else:
                    cond = defined(arg, config) == (d == "ifdef")
                stack.append([cond, cond])
            elif d == "elif":
                cond = not stack[-1][1] and eval_condition(arg, config)
                stack[-1] = [cond, stack[-1][1] or cond]
            elif d == "else":
                stack[-1] = [not stack[-1][1], True]
            else:
                stack.pop()
            continue
        if all(s[0] for s in stack):
            out.append(line)
    return "\n".join(out)


def declarations(text, pattern=r"asmlinkage\s+[^;{]*?\b(\w+)\s*\([^;{)]*\)\s*[;{]"):
    """{function name: prototype} of the asmlinkage declarations (and definitions) in text."""
    found = {}
    for m in re.finditer(pattern, text, re.S):
        sig = m.group(0)[:-1].strip()
        found.setdefault(m.group(1), normalize(sig))
    return found


def syscall_defines(text):
    """{sys_<name>: prototype} of the SYSCALL_DEFINE<n>(name, type, arg, ...) in text."""
    found = {}
    for m in re.finditer(r"\bSYSCALL_DEFINE(\d)\s*\(\s*(\w+)\s*((?:,[^()]*(?:\([^()]*\)[^()]*)*)?)\)", text):
        n, name, rest = int(m.group(1)), m.group(2), m.group(3)
        parts = [p.strip() for p in rest.split(",")[1:]] if rest.strip() else []
        if len(parts) != 2 * n:
            continue
        args = ", ".join(f"{parts[2 * i]} {parts[2 * i + 1]}" for i in range(n)) or "void"
        found.setdefault(f"sys_{name}", normalize(f"long sys_{name}({args})"))
    return found


def prototypes_for(src, arch, config):
    """{function name: prototype} available to an ABI, best source first."""
    protos = declarations(preprocess_ifs(src.files["include/linux/syscalls.h"], config))
    # Arch headers declare some without asmlinkage (powerpc's sys_ppc_*).
    plain = r"(?m)^[ \t]*(?:asmlinkage\s+)?(?:long|int|unsigned long|void)\s+\**(\w+)\s*\([^;{)]*\)\s*;"
    for path in src.glob(f"arch/{arch}/include/asm/syscalls", (".h",)):
        for k, v in declarations(src.files[path], plain).items():
            protos.setdefault(k, v)
    for path in src.glob(f"arch/{arch}/", (".c",)):
        text = src.files[path]
        for k, v in list(syscall_defines(text).items()) + list(declarations(text).items()):
            protos.setdefault(k, v)
    for k, v in EXTRA.get(arch, {}).items():
        protos.setdefault(k, v)
    return protos


def table_name(entry):
    """The name the table gives an entry point: the sys_<x> an assembler stub or arch variant
    stands in for, and a compat entry's native sys_<x>."""
    e = re.sub(r"_wrapper$", "", entry)
    e = re.sub(r"^__sys_", "sys_", e)
    e = re.sub(r"^sys(m|n32)_", "sys_", e)
    return re.sub(r"^compat_sys_", "sys_", e)


def abi_table(src, abi, conf):
    lines, missing = [], []
    for path, abis, offset, *config in conf["tables"]:
        protos = prototypes_for(src, conf["arch"], config[0] if config else conf["config"])
        abis = set(abis.split())
        if conf.get("makefile_abis"):
            abis |= set(makefile_abis(src, conf["makefile_abis"]))
        for nr, name, entry in table(src, path, abis, offset):
            if not entry or entry == "sys_ni_syscall":
                continue
            named = table_name(entry)
            # The entry point's own declaration first (sysm_pipe(void), not sys_pipe's), then the
            # function it stands in for; a compat _time64 call (mips n32) is the native one
            # without the suffix.
            names = [entry, named] + ([named[:-len("_time64")]] if named.endswith("_time64") else [])
            found = next((n for n in names if n in protos), None)
            if found is None:
                missing.append(f"{nr} {name} ({entry})")
                continue
            if entry.startswith("compat_sys_"):
                named = found   # the native call it stands in for
            proto = re.sub(rf"\b{re.escape(found)}\s*\(", f"{named}(", protos[found], count=1)
            lines.append((nr, proto))
    if conf.get("arm_private"):
        base = 0x0f0000
        uapi = src.files["arch/arm/include/uapi/asm/unistd.h"]
        for m in re.finditer(r"#define\s+__ARM_NR_(\w+)\s+\(__ARM_NR_BASE\s*\+\s*(\d+)\)", uapi):
            if m.group(1) not in ARM_PRIVATE:
                missing.append(f"ARM private {m.group(1)}")
                continue
            lines.append((base + int(m.group(2)), ARM_PRIVATE[m.group(1)]))
    if missing:
        sys.exit(f"linux_{abi}: no prototype for: " + "; ".join(missing))
    return sorted(lines)


# ABIs whose callbacks are compiled into one plugin build (syscall_parser.py's typedef and runner
# guards: TARGET_I386 builds x86's, TARGET_ARM arm's, ...): a system call name there is one PPP
# callback, so it must take as many arguments in each.
SHARED = [("x86", "x64"), ("arm", "arm64"), ("mips", "mips64", "mips64n32"), ("ppc", "ppc64")]


def arg_count(proto):
    args = re.match(r".*?\w+\((.*)\);$", proto).group(1)
    return 0 if args.strip() in ("", "void") else len(args.split(","))


def check_shared(tables):
    bad = []
    for group in SHARED:
        counts = {}
        for abi in group:
            for _, proto in tables[abi]:
                name = re.match(r".*?(\w+)\(", proto).group(1)
                counts.setdefault(name, {})[abi] = arg_count(proto)
        bad += [f"{n} {c}" for n, c in counts.items() if len(set(c.values())) > 1]
    if bad:
        sys.exit("a system call takes different argument counts in ABIs built together: " + "; ".join(bad))


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src = Source(sys.argv[1])
    tables = {abi: abi_table(src, abi, conf) for abi, conf in ABIS.items()}
    check_shared(tables)
    for abi, conf in ABIS.items():
        lines = tables[abi]
        tbls = ", ".join(sorted({t[0] for t in conf["tables"]}))
        out = os.path.join(sys.argv[2], f"linux_{abi}_prototypes.txt")
        with open(out, "w") as f:
            f.write(f"// Linux {src.version} {abi}: generated by scripts/linux_prototypes.py from {tbls}\n"
                    "// and the kernel's declarations. Don't edit: regenerate (nix run .#regen-syscalls).\n")
            for nr, proto in lines:
                f.write(f"{nr} {proto}\n")
        print(f"{out}: {len(lines)} system calls, highest {max(nr for nr, _ in lines if nr < 0x0f0000)}")


if __name__ == "__main__":
    main()
