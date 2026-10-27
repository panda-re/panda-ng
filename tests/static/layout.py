"""Does pandare2's cffi agree with the compiler on where every CPUState and CPUArchState field is?
No guest, no image.

    panda-python tests/static/layout.py <engine>/include/panda-ng [arch ...]   (default: every arch)

pandare2 reads guest registers through cffi in ABI mode: cffi computes each struct's layout itself
from the declarations in panda_python_<arch>.h, and nothing compiled checks it. Plugins (and the
engine) are compiled against panda_c_<arch>.h, so gcc's layout is the truth. For each arch this
loads pandare2's cffi module (pandare2/autogen/_pandare_ffi_<arch>_softmmu.py, as installed) and
compiles a C program against panda_c_<arch>.h that prints gcc's answers, then compares:

- every field of CPUState and CPUArchState, recursively into nested structs and unions and into
  element 0 of arrays: offset and size (bitfields: first bit and width);
- every field pandare2 reads (READS below) is among them, so a renamed field fails too;
- sizeof and alignof of every complete struct, union and typedef the two headers share.

Exit status 0 if everything matches; 1, with the mismatches, otherwise. Needs gcc and glib's
pkg-config (the flake check `layout` runs it).
"""
import importlib.util
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

# panda-ng's include/ (c_sys_includes.h): the tree's, or PANDA_INCLUDE (the flake check passes it).
REPO_INCLUDE = Path(os.environ.get("PANDA_INCLUDE") or Path(__file__).resolve().parents[2] / "include")

# The fields pandare2 (python/core/pandare2/arch.py) and the tests read, as cffi paths.
X86 = ["regs[0]", "eip", "eflags", "cr[0]", "segs[0].base", "segs[0].selector", "ldt.base",
       "ldt.selector", "ldt.limit", "ldt.flags", "tr.base", "gdt.base", "gdt.limit", "idt.base",
       "idt.limit", "xmm_regs[0]._b_ZMMReg[0]", "hflags", "efer"]
ARM = ["regs[0]", "pc", "xregs[0]", "aarch64", "thumb", "cp15.ttbr0_el[0]"]
MIPS = ["active_tc.gpr[0]", "active_tc.PC", "active_tc.HI[0]", "active_tc.LO[0]",
        "active_tc.DSPControl", "CP0_Status", "fpus[0].fpr[0]", "fpus[0].fcr0"]
PPC = ["gpr[0]", "nip", "lr", "ctr", "msr", "crf[0]", "spr[0]", "spr_cb[0].name",
       "pending_interrupts", "irq_input_state"]
RISCV = ["gpr[0]", "pc", "priv", "sscratch"]
READS = {
    "x86_64": X86, "i386": X86, "arm": ARM, "aarch64": ARM,
    "mips": MIPS, "mipsel": MIPS, "mips64": MIPS, "mips64el": MIPS,
    "ppc": PPC, "ppc64": PPC, "riscv32": RISCV, "riscv64": RISCV,
    "loongarch64": ["gpr[0]", "pc"],
}
ROOTS = ["CPUState", "CPUArchState"]


def cffi_module(arch):
    """pandare2's cffi module for arch, loaded by path (importing pandare2 itself isn't needed).
    PANDARE2_AUTOGEN=<dir> reads the modules from dir instead of the installed pandare2."""
    if os.environ.get("PANDARE2_AUTOGEN"):
        autogen = Path(os.environ["PANDARE2_AUTOGEN"])
    else:
        spec = importlib.util.find_spec("pandare2")
        if spec is None:
            sys.exit("pandare2 is not importable: run this with panda-python (nix build .#python)")
        autogen = Path(spec.submodule_search_locations[0]) / "autogen"
    path = autogen / f"_pandare_ffi_{arch}_softmmu.py"
    mspec = importlib.util.spec_from_file_location(f"_layout_{arch}", path)
    mod = importlib.util.module_from_spec(mspec)
    mspec.loader.exec_module(mod)
    return mod.ffi


def walk(ffi, ctype, prefix, base, out):
    """Every field under ctype: (path, offset, size) or (path, None, (first bit, width))."""
    for name, f in ctype.fields:
        if not name or name.startswith("__cffi_"):
            continue  # alignment markers the header generator adds; gcc has no such field
        path = f"{prefix}{name}"
        off = base + f.offset
        if f.bitshift >= 0:
            out.append((path, None, (off * 8 + f.bitshift, f.bitsize)))
            continue
        t = f.type
        out.append((path, off, ffi.sizeof(t) if t.kind != "array" or t.length is not None else 0))
        descend(ffi, t, path, off, out)


def descend(ffi, t, path, off, out):
    while t.kind == "array":
        if not t.length:
            return
        t = t.item
        path += "[0]"
        out.append((path, off, ffi.sizeof(t)))
    # python_fixes.h's stand-in for __int128 has fields gcc's __int128 hasn't.
    if t.kind in ("struct", "union") and t.fields is not None and "__cffi_int128" not in t.cname:
        walk(ffi, t, path + ".", off, out)


def named_types(ffi, header):
    """(C type expression, cffi size, cffi alignment) for every complete named struct, union and
    typedef that panda_c_<arch>.h also declares."""
    typedefs, structs, unions = ffi.list_types()
    words = set(re.findall(r"\b\w+\b", header))
    out = []
    for kind, names in (("struct ", structs), ("union ", unions), ("", typedefs)):
        for n in names:
            if n.startswith("__cffi") or n not in words:
                continue
            c = kind + n
            try:
                t = ffi.typeof(c)
                if t.kind not in ("struct", "union", "array", "primitive", "enum"):
                    continue
                out.append((c, ffi.sizeof(t), ffi.alignof(t)))
            except (ffi.error, TypeError, ValueError):
                continue  # opaque, or a function type
    return out


def c_probe(arch, fields, types):
    lines = [
        '#include "c_sys_includes.h"',
        f'#include "panda_c_{arch}.h"',
        "#include <stdio.h>",
        "#include <string.h>",
        "static void bits(const unsigned char *b, size_t n, const char *tag) {",
        "    long first = -1, count = 0;",
        "    for (size_t i = 0; i < n * 8; i++)",
        "        if (b[i / 8] >> (i % 8) & 1) { if (first < 0) first = (long)i; count++; }",
        '    printf("B %s %ld %ld\\n", tag, first, count);',
        "}",
    ]
    for root in ROOTS:
        lines.append(f"static union {{ {root} s; unsigned char b[sizeof({root})]; }} u_{root};")
    lines.append("int main(void) {")
    for i, (c, _, _) in enumerate(types):
        lines.append(f'    printf("T {i} %zu %zu\\n", sizeof({c}), _Alignof({c}));')
    for root in ROOTS:
        for i, (path, off, extra) in enumerate(fields[root]):
            tag = f"{root}:{i}"
            if off is None:
                lines.append(f"    memset(&u_{root}, 0, sizeof u_{root}); u_{root}.s.{path} = -1;"
                             f' bits(u_{root}.b, sizeof u_{root}.b, "{tag}");')
            else:
                lines.append(f'    printf("F {tag} %zu %zu\\n", offsetof({root}, {path}),'
                             f" sizeof((({root} *)0)->{path}));")
    lines.append("    return 0;\n}")
    return "\n".join(lines) + "\n"


def gcc_answers(arch, incdir, source, tmp):
    """Compile and run the probe. A "T" line whose type panda_c_<arch>.h lacks (cffi also has
    pandare2's plugin types) is dropped and the probe compiled again; any other error fails."""
    c = Path(tmp) / f"layout_{arch}.c"
    exe = Path(tmp) / f"layout_{arch}"
    glib = subprocess.check_output(["pkg-config", "--cflags", "glib-2.0"], text=True).split()
    lines = source.splitlines()
    for _ in range(5):
        c.write_text("\n".join(lines) + "\n")
        r = subprocess.run(["gcc", "-w", "-O0", "-D_GNU_SOURCE", f"-I{incdir}", f"-I{REPO_INCLUDE}",
                            *glib, str(c), "-o", str(exe)], capture_output=True, text=True)
        if not r.returncode:
            break
        errs = {int(m) - 1 for m in re.findall(rf"^{re.escape(str(c))}:(\d+):\d+: error", r.stderr, re.M)}
        if not errs or any(not lines[i].lstrip().startswith('printf("T ') for i in errs):
            sys.exit(f"{arch}: the gcc probe does not compile:\n{r.stderr[-4000:]}")
        for i in errs:
            lines[i] = ""
    else:
        sys.exit(f"{arch}: the gcc probe does not compile:\n{r.stderr[-4000:]}")
    ans = {}
    for line in subprocess.check_output([str(exe)], text=True).splitlines():
        kind, tag, *vals = line.split()
        ans[(kind, tag)] = tuple(int(v) for v in vals)
    return ans


def check(arch, incdir, tmp):
    header = (Path(incdir) / f"panda_c_{arch}.h").read_text()
    ffi = cffi_module(arch)
    fields = {}
    for root in ROOTS:
        out = []
        walk(ffi, ffi.typeof(root), "", 0, out)
        fields[root] = out
    types = named_types(ffi, header)
    ans = gcc_answers(arch, incdir, c_probe(arch, fields, types), tmp)

    bad = []
    stats = {}
    for root in ROOTS:
        wrong = top = top_wrong = 0
        for i, (path, off, extra) in enumerate(fields[root]):
            tag = f"{root}:{i}"
            if off is None:
                got = ans[("B", tag)]
                ok = got == extra
                mine, theirs = f"bit {extra[0]} width {extra[1]}", f"bit {got[0]} width {got[1]}"
            else:
                got = ans[("F", tag)]
                ok = got == (off, extra)
                mine, theirs = f"offset {off} size {extra}", f"offset {got[0]} size {got[1]}"
            if "." not in path and "[" not in path:
                top += 1
                top_wrong += not ok
            if not ok:
                wrong += 1
                bad.append(f"{root}.{path}: cffi {mine}, gcc {theirs}")
        stats[root] = (len(fields[root]), wrong, top, top_wrong)

    paths = {p for p, _, _ in fields["CPUArchState"]}
    for p in READS[arch]:
        if p not in paths:
            bad.append(f"CPUArchState.{p}: pandare2 reads it, but cffi has no such field")

    type_wrong = compared = 0
    gcc_size = {}
    for i, (c, size, align) in enumerate(types):
        if ("T", str(i)) not in ans:
            continue  # not a type panda_c_<arch>.h declares
        compared += 1
        gs, ga = ans[("T", str(i))]
        gcc_size[c] = gs
        if (gs, ga) != (size, align):
            type_wrong += 1
            bad.append(f"{c}: cffi sizeof {size} alignof {align}, gcc sizeof {gs} alignof {ga}")

    sizes = " ".join(f"{r} {ffi.sizeof(r)}/{gcc_size.get(r, '?')}" for r in ROOTS)
    summary = " ".join(
        f"{r}: {n} fields, {w} wrong (top level {tt}, {tw} wrong);" for r, (n, w, tt, tw) in stats.items())
    print(f"{arch}: {summary} {compared} named types, {type_wrong} wrong; sizeof cffi/gcc {sizes}")
    # The first few of each kind (a misplaced struct drags every field after it along).
    for kind in ("CPUState.", "CPUArchState.", ""):
        mine = [b for b in bad if b.startswith(kind) and (kind or not b.startswith("CPU"))]
        for b in mine[:15]:
            print(f"  MISMATCH {arch} {b}")
        if len(mine) > 15:
            print(f"  ... {len(mine) - 15} more like it")
    return not bad


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    incdir = sys.argv[1]
    arches = sys.argv[2:] or sorted(READS)
    with tempfile.TemporaryDirectory(prefix="layout-") as tmp:
        failed = [a for a in arches if not check(a, incdir, tmp)]
    if failed:
        print(f"FAIL: cffi's layout differs from gcc's on {' '.join(failed)}")
        sys.exit(1)
    print(f"PASS: cffi's layout matches gcc's on {' '.join(arches)}")


if __name__ == "__main__":
    main()
