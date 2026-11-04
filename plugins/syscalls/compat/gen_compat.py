#!/usr/bin/env python3
"""Write syscalls2's PPP callback declarations from the syscalls tables.

The files in generated/ declare the same callbacks, with the same names and C signatures, as
syscalls2's generated/syscalls_ext_typedefs_<arch>.h and syscall_ppp_{extern,boilerplate,
register}_{enter,return}: a plugin that uses syscalls2's callbacks builds against these
unchanged. They are made from the tables (../tables/gen_tables.py), not from syscalls2's
generator, and tests/static/syscalls_compat.py checks that they equal syscalls2's.

The parameters of a callback are the compat fields of the tables' arguments (syscalls2's types,
including its widths). Where profiles of one syscalls2 arch have a callback of the same name
with different parameters, the one syscalls2 declares wins: within a profile the last one, and
across profiles the one whose OS sorts last.
"""

import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "../tables"))
import gen_tables as tables  # noqa: E402

# syscalls2's arches (syscall_parser.KNOWN_ARCH), in its order, with their #if conditions.
ARCHES = list(tables.s2.KNOWN_ARCH)
PYPANDA = """\
// BEGIN_PYPANDA_NEEDS_THIS -- do not delete this comment bc pypanda
// api autogen needs it.  And don't put any compiler directives
// between this and END_PYPANDA_NEEDS_THIS except includes of other
// files in this directory that contain subsections like this one."""
VIM = "/* vim: set tabstop=4 softtabstop=4 noexpandtab ft=cpp: */"


def callbacks():
    """{arch: {callback name: parameter list}}, as syscalls2 declares them."""
    context = tables.load_context()
    out = {a: {} for a in ARCHES}
    profs = [p for p in tables.profiles() if p["served"] and p["s2arch"]]
    for p in sorted(profs, key=lambda p: p["ctx"].split(":")[0]):
        calls, _ = tables.build(p, context)
        for c in calls:
            if "SYSCALLS_NO_COMPAT_CALLBACK" in c["flags"]:
                continue
            params = ["CPUState* cpu", "target_ulong pc"] + [f"{a['s2ctype']} {a['name']}" for a in c["args"]]
            out[p["s2arch"]][c["name"]] = ", ".join(params)
    return out


def typedefs(cbs):
    lines = [PYPANDA]
    for name in sorted(cbs, key=str.lower):   # jinja's dictsort
        for kind in ("ENTER", "RETURN"):
            cb = f"on_{name}_{kind.lower()}"
            guard = f"TYPEDEFS_PPP_SYSCALL_ON_{name.upper()}_{kind}"
            lines += [f"#ifndef {guard}", f"#define {guard} 1", f"PPP_CB_TYPEDEF(void, {cb}, {cbs[name]});",
                      "#endif"]
    lines += ["", "// END_PYPANDA_NEEDS_THIS -- do not delete this comment!", ""]
    return "\n".join(lines)


ALL = {"enter": ["on_unknown_sys_enter", "on_all_sys_enter", "on_all_sys_enter2"],
       "return": ["on_unknown_sys_return", "on_all_sys_return", "on_all_sys_return2"]}


def ppp_list(all_cbs, kind, macro, guard, condition, sort_arches):
    """syscall_ppp_<extern|boilerplate|register>_<kind>: one #if block per arch."""
    out = []
    arches = sorted(ARCHES) if sort_arches else ARCHES
    for arch in arches:
        conf = tables.s2.KNOWN_ARCH[arch]
        body = [f"#if {condition(conf)}"]
        for name in sorted(all_cbs[arch], key=str.lower):
            cb = f"on_{name}_{kind}"
            if guard:
                g = f"{guard}_ON_{name.upper()}_{kind.upper()}"
                body += [f"#ifndef {g}", f"#define {g}", f"{macro}({cb})", "#endif"]
            else:
                body.append(f"{macro}({cb})")
        body.append("#endif")
        out.append("\n".join(body) + "\n")
    tail = "\n".join(f"{macro}({cb})" for cb in ALL[kind])
    return "".join(out) + "\n" + tail + "\n\n" + VIM


def boilerplate_target(conf):
    return conf.get("boilerplate_target", conf["qemu_target"])


def qemu_target(conf):
    return conf["qemu_target"]


def files():
    """{file name: contents}"""
    cbs = callbacks()
    out = {f"syscalls_ext_typedefs_{a}.h": typedefs(cbs[a]) for a in ARCHES}
    for kind in ("enter", "return"):
        out[f"syscall_ppp_extern_{kind}.h"] = ppp_list(cbs, kind, "PPP_CB_EXTERN", "PPP_CB_EXTERN",
                                                       boilerplate_target, False)
        out[f"syscall_ppp_boilerplate_{kind}.cpp"] = ppp_list(
            cbs, kind, "PPP_CB_BOILERPLATE", "PPP_CB_BOILERPLATE_ENTER", boilerplate_target, False)  # syscalls2: ENTER for both
        out[f"syscall_ppp_register_{kind}.cpp"] = ppp_list(cbs, kind, "PPP_PROT_REG_CB", None, qemu_target,
                                                           True)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--outdir", default=os.path.join(HERE, "generated"))
    args = ap.parse_args()
    os.makedirs(args.outdir, exist_ok=True)
    for name, text in files().items():
        with open(os.path.join(args.outdir, name), "w") as f:
            f.write(text)
        print(name)


if __name__ == "__main__":
    main()
