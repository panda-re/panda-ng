#!/usr/bin/env python3
"""Writes the Linux prototype tables syscalls2 has no file for, with syscalls2's own generator
(plugins/syscalls2/scripts/linux_prototypes.py) and the same kernel source:

    linux_extra_prototypes.py <linux source: directory or .tar.xz> <output dir>

- riscv32: scripts/syscall.tbl, ABIs common, 32 and arch/riscv/kernel/Makefile.syscalls'
  syscall_abis_32 (riscv, memfd_secret); no time32 rows, as the kernel builds it.
- loongarch64: scripts/syscall.tbl, ABIs common, 64 and arch/loongarch/kernel/Makefile.syscalls'
  syscall_abis_64 (none in 6.19).
- x32: arch/x86/entry/syscalls/syscall_64.tbl, ABIs common and x32, numbered with
  __X32_SYSCALL_BIT (0x40000000) set, as user space passes them in rax. A compat entry point
  (compat_sys_<x>) takes the native sys_<x>'s prototype, as linux_prototypes.py does for mips
  n32; the x32-only rt_sigreturn has no C declaration and takes sys_rt_sigreturn's (void).
- arm_oabi: arch/arm/tools/syscall.tbl, ABIs common and oabi, plus ARM's private calls. Where a
  row names an OABI compat entry point (its fifth column) with a C prototype, that prototype is
  used under the native name; otherwise the native entry point's. The assembler-only OABI stubs
  (sys_oabi_pread64, ...) only move a 64-bit argument from an unaligned register pair to the
  aligned one, so the native prototype describes their arguments.

Numbers come only from the kernel's tables; prototypes only from the kernel's declarations or
linux_prototypes.py's EXTRA/RETIRED. A call with no prototype fails the run.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "../../syscalls2/scripts"))
import linux_prototypes as lp  # noqa: E402

# syscalls2's reader keeps only the arches it serves; loongarch's sources are needed too.
lp.Source.WANT = re.compile(r"(Makefile|.*\.tbl|arch/[^/]+/kernel/Makefile\.syscalls|include/linux/syscalls\.h|include/linux/compat\.h"
                            r"|arch/(x86|arm|arm64|riscv|mips|powerpc|loongarch)/.*\.(c|h))$")

X32_BIT = 0x40000000

ABIS = {
    "riscv32": dict(arch="riscv", tbl=lp.GENERIC, abis="common 32", makefile="arch/riscv/kernel/Makefile.syscalls",
                    bits=32, config=lp.CONFIG_32 | {"CONFIG_CLONE_BACKWARDS": 1}),
    "loongarch64": dict(arch="loongarch", tbl=lp.GENERIC, abis="common 64",
                        makefile="arch/loongarch/kernel/Makefile.syscalls", bits=64,
                        config={"BITS_PER_LONG": 64}),
    "x32": dict(arch="x86", tables=[(lp.X86_64, "common x32", X32_BIT)], config={"BITS_PER_LONG": 64}),
}

# arch/arm/kernel/entry-common.S: sys_syscall, OABI's indirect system call: the number in r0, that
# call's six arguments in r1-r6.
OABI_EXTRA = {"sys_syscall": "long sys_syscall(unsigned long number, unsigned long arg1, unsigned long arg2, "
                             "unsigned long arg3, unsigned long arg4, unsigned long arg5, unsigned long arg6);"}

# x32's own entry points with no C declaration: the native call each stands in for.
X32_STUBS = {"compat_sys_x32_rt_sigreturn": "sys_rt_sigreturn"}


def makefile_abis(src, path, bits):
    m = re.search(rf"^syscall_abis_{bits}\s*\+=(.*)$", src.files[path], re.M)
    return m.group(1).split() if m else []


def generic_conf(src, conf):
    abis = conf["abis"] + " " + " ".join(makefile_abis(src, conf["makefile"], conf["bits"]))
    return dict(arch=conf["arch"], tables=[(conf["tbl"], abis, 0)], config=conf["config"])


def candidates(entry):
    """The names abi_table() tries for an entry point, in its order."""
    named = lp.table_name(entry)
    return [entry, named] + ([named[:-len("_time64")]] if named.endswith("_time64") else [])


def x32_table(src, conf):
    """abi_table() for x32. The x32-only entry points with no declaration are mapped to the native
    call they replace; a compat entry point with no native counterpart (compat_sys_preadv64, whose
    loff_t is one register on x32) takes its own declaration in include/linux/compat.h, under the
    row's name."""
    s = type(src).__new__(type(src))
    s.__dict__ = dict(src.__dict__)
    s.files = dict(src.files)
    native = lp.prototypes_for(src, "x86", conf["config"])
    compat = lp.declarations(lp.preprocess_ifs(src.files["include/linux/compat.h"], conf["config"]))
    extra, lines = {}, []
    for line in src.files[lp.X86_64].splitlines():
        f = line.split("#")[0].split()
        if len(f) > 3 and f[1] == "x32":
            if f[3] in X32_STUBS:
                line = line.replace(f[3], X32_STUBS[f[3]])
            elif f[3] in compat and not any(n in native for n in candidates(f[3])):
                extra[f[3]] = re.sub(rf"\b{f[3]}\(", f"sys_{f[2]}(", compat[f[3]], count=1)
        lines.append(line)
    s.files[lp.X86_64] = "\n".join(lines) + "\n"
    saved = lp.EXTRA.get("x86")
    lp.EXTRA["x86"] = dict(saved or {}) | extra
    try:
        return lp.abi_table(s, "x32", conf)
    finally:
        if saved is None:
            del lp.EXTRA["x86"]
        else:
            lp.EXTRA["x86"] = saved


def oabi_table(src):
    conf = lp.ABIS["arm"]
    protos = lp.prototypes_for(src, "arm", conf["config"]) | OABI_EXTRA
    lines, missing = [], []
    for row in src.files[lp.ARM].splitlines():
        f = row.split("#")[0].split()
        if len(f) < 3 or f[1] not in ("common", "oabi"):
            continue
        nr, name = int(f[0]), f[2]
        entry = f[3] if len(f) > 3 and f[3] != "-" else None
        oabi = f[4] if len(f) > 4 else None
        if not entry or entry == "sys_ni_syscall":
            if name in lp.RETIRED:
                lines.append((nr, lp.RETIRED[name]))
            continue
        named = lp.table_name(entry)
        names = ([oabi] if oabi else []) + [entry, named]
        found = next((n for n in names if n in protos), None)
        if found is None:
            missing.append(f"{nr} {name} ({entry})")
            continue
        lines.append((nr, re.sub(rf"\b{re.escape(found)}\s*\(", f"{named}(", protos[found], count=1)))
    base = 0x0f0000
    uapi = src.files["arch/arm/include/uapi/asm/unistd.h"]
    for m in re.finditer(r"#define\s+__ARM_NR_(\w+)\s+\(__ARM_NR_BASE\s*\+\s*(\d+)\)", uapi):
        if m.group(1) not in lp.ARM_PRIVATE:
            missing.append(f"ARM private {m.group(1)}")
            continue
        lines.append((base + int(m.group(2)), lp.ARM_PRIVATE[m.group(1)]))
    if missing:
        sys.exit("linux_arm_oabi: no prototype for: " + "; ".join(missing))
    return sorted(lines)


def tables(src):
    out = {}
    for abi in ("riscv32", "loongarch64"):
        out[abi] = (lp.abi_table(src, abi, generic_conf(src, ABIS[abi])), ABIS[abi]["tbl"])
    out["x32"] = (x32_table(src, ABIS["x32"]), lp.X86_64)
    out["arm_oabi"] = (oabi_table(src), lp.ARM)
    return out


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src = lp.Source(sys.argv[1])
    for abi, (lines, tbl) in tables(src).items():
        path = os.path.join(sys.argv[2], f"linux_{abi}_prototypes.txt")
        with open(path, "w") as f:
            f.write(f"// Linux {src.version} {abi}: generated by plugins/syscalls/tables/linux_extra_prototypes.py "
                    f"from {tbl}\n// with syscalls2's linux_prototypes.py. Don't edit: regenerate "
                    "(nix run .#regen-syscalls).\n")
            for nr, proto in lines:
                f.write(f"{nr} {proto}\n")
        print(f"{path}: {len(lines)} system calls")


if __name__ == "__main__":
    main()
