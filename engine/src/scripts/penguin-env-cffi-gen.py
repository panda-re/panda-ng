#!/usr/bin/env python3
#
# Generate CFFI access to CPUArchState from a built Penguin QEMU library.
#
# This gives Penguin's compatibility layer typed access to the full
# per-target CPU state (env) -- coprocessor registers, timers, FPU state
# -- beyond the GDB core register set. Field names are enumerated from
# the library's DWARF, so nothing can drift from the binary. Two
# artifacts are produced per target:
#
# 1. A compiled CFFI API-mode extension module (_penguin_qemu_env_*).
#    The cdef uses real type names with `...` ellipses and is compiled
#    against the actual QEMU headers using the same flags as the library
#    build (harvested from compile_commands.json), so the C compiler is
#    the layout authority. Bitfields and anonymous members are fully
#    supported. The module is tied to the build's CPython ABI.
#
# 2. An ABI-mode header (*_env.h) usable with plain ffi.cdef + dlopen on
#    any Python. Every struct is layout-exact: verified field-by-field
#    against DWARF offsets using cffi itself, with unrepresentable
#    members (bitfields, exotic types) dropped and padded over.
#
# Penguin prefers the compiled module and falls back to the header.

import argparse
import json
import re
import shlex
import sys
from pathlib import Path

import cffi
from elftools.elf.elffile import ELFFile

MAX_REPAIR_PASSES = 200

C_IDENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")

BASE_TYPES = {
    ("boolean", 1): "uint8_t",  # avoid _Bool: cdef'd alongside headers that typedef bool
    ("signed", 1): "int8_t",
    ("signed", 2): "int16_t",
    ("signed", 4): "int32_t",
    ("signed", 8): "int64_t",
    ("signed char", 1): "int8_t",
    ("unsigned", 1): "uint8_t",
    ("unsigned", 2): "uint16_t",
    ("unsigned", 4): "uint32_t",
    ("unsigned", 8): "uint64_t",
    ("unsigned char", 1): "uint8_t",
    ("float", 4): "float",
    ("float", 8): "double",
    ("UTF", 1): "uint8_t",
    ("UTF", 2): "uint16_t",
    ("UTF", 4): "uint32_t",
}

ENCODING_NAMES = {
    0x01: "address",
    0x02: "boolean",
    0x04: "float",
    0x05: "signed",
    0x06: "signed char",
    0x07: "unsigned",
    0x08: "unsigned char",
    0x10: "UTF",
}


def log(msg):
    print(f"penguin-env-cffi-gen: {msg}", file=sys.stderr)


class Member:
    def __init__(self, name, ctype, offset, suffix=""):
        self.name = name
        self.ctype = ctype  # C type string, e.g. "uint32_t" or "struct foo"
        self.suffix = suffix  # array suffix, e.g. "[32]"
        self.offset = offset  # DWARF byte offset within parent


class StructDef:
    def __init__(self, tag, kind, size):
        self.tag = tag  # C tag name
        self.kind = kind  # "struct" or "union"
        self.size = size  # DWARF byte size
        self.members = []  # emitted Members (skipped ones omitted)
        self.pads = {}  # insert-before-index -> pad byte count
        self.trailing_pad = 0
        self.opaque = False

    def render(self):
        lines = [f"{self.kind} {self.tag} {{"]
        if self.opaque:
            lines.append(f"    uint8_t _penguin_opaque[{max(self.size, 1)}];")
        else:
            for idx, member in enumerate(self.members):
                pad = self.pads.get(idx, 0)
                if pad:
                    lines.append(f"    uint8_t _penguin_pad{idx}[{pad}];")
                lines.append(f"    {member.ctype} {member.name}{member.suffix};")
            if self.trailing_pad:
                lines.append(
                    f"    uint8_t _penguin_pad_tail[{self.trailing_pad}];")
            if not self.members and not self.trailing_pad:
                lines.append(f"    uint8_t _penguin_empty[{max(self.size, 1)}];")
        lines.append("};")
        return "\n".join(lines)


class EnvTypeExtractor:
    def __init__(self, dwarf):
        self.dwarf = dwarf
        self.structs = {}  # tag -> StructDef
        self.order = []  # emission order (dependencies first)
        self.die_tags = {}  # die offset -> tag
        self.anon_count = 0
        self.warnings = []

    # ---- DWARF navigation helpers ----

    def _attr(self, die, name):
        attr = die.attributes.get(name)
        return attr.value if attr is not None else None

    def _type_die(self, die):
        if "DW_AT_type" not in die.attributes:
            return None
        return die.get_DIE_from_attribute("DW_AT_type")

    def _strip_cv(self, die):
        while die is not None and die.tag in (
            "DW_TAG_const_type",
            "DW_TAG_volatile_type",
            "DW_TAG_restrict_type",
            "DW_TAG_atomic_type",
        ):
            die = self._type_die(die)
        return die

    def _resolve_typedefs(self, die):
        die = self._strip_cv(die)
        while die is not None and die.tag == "DW_TAG_typedef":
            die = self._strip_cv(self._type_die(die))
        return die

    def _die_name(self, die):
        name = self._attr(die, "DW_AT_name")
        if isinstance(name, bytes):
            name = name.decode("utf-8", "replace")
        return name

    def _member_offset(self, die):
        value = self._attr(die, "DW_AT_data_member_location")
        if value is None:
            return 0
        if isinstance(value, int):
            return value
        # exprloc form: DW_OP_plus_uconst <ULEB128>
        data = bytes(value)
        if data and data[0] == 0x23:
            result = 0
            shift = 0
            for byte in data[1:]:
                result |= (byte & 0x7F) << shift
                if not byte & 0x80:
                    break
                shift += 7
            return result
        raise ValueError(f"unsupported member location encoding: {value!r}")

    # ---- type resolution ----

    def _base_ctype(self, die):
        size = self._attr(die, "DW_AT_byte_size")
        encoding = self._attr(die, "DW_AT_encoding")
        key = (ENCODING_NAMES.get(encoding), size)
        return BASE_TYPES.get(key)

    def _array_dims(self, die):
        dims = []
        for child in die.iter_children():
            if child.tag != "DW_TAG_subrange_type":
                continue
            count = self._attr(child, "DW_AT_count")
            if count is None:
                upper = self._attr(child, "DW_AT_upper_bound")
                count = upper + 1 if isinstance(upper, int) else 0
            if not isinstance(count, int):
                count = 0
            dims.append(count)
        return dims or [0]

    def member_ctype(self, type_die):
        """
        Resolve a member's type to (ctype, array_suffix) or None when it
        cannot be represented (the member is then dropped and padded over).
        """
        die = self._resolve_typedefs(type_die)
        if die is None:
            return None

        if die.tag == "DW_TAG_pointer_type":
            return ("void *", "")

        if die.tag == "DW_TAG_base_type":
            ctype = self._base_ctype(die)
            return (ctype, "") if ctype else None

        if die.tag == "DW_TAG_enumeration_type":
            size = self._attr(die, "DW_AT_byte_size") or 4
            ctype = BASE_TYPES.get(("unsigned", size))
            return (ctype, "") if ctype else None

        if die.tag == "DW_TAG_array_type":
            element = self.member_ctype(self._type_die(die))
            if element is None:
                return None
            ctype, suffix = element
            dims = self._array_dims(die)
            if any(d <= 0 for d in dims):
                return None
            return (ctype, "".join(f"[{d}]" for d in dims) + suffix)

        if die.tag in ("DW_TAG_structure_type", "DW_TAG_union_type"):
            tag = self.emit_struct(die)
            if tag is None:
                return None
            kind = "struct" if die.tag == "DW_TAG_structure_type" else "union"
            return (f"{kind} {tag}", "")

        return None

    # ---- struct emission ----

    def emit_struct(self, die):
        """Emit a struct/union definition; returns its tag or None."""
        if die.offset in self.die_tags:
            return self.die_tags[die.offset]
        if self._attr(die, "DW_AT_declaration"):
            return None
        size = self._attr(die, "DW_AT_byte_size")
        if not size:
            return None

        name = self._die_name(die)
        if name and C_IDENT_RE.match(name):
            tag = f"penguin_env_{name}"
        else:
            self.anon_count += 1
            tag = f"penguin_env_anon{self.anon_count}"
        # Disambiguate distinct DIEs that share a source-level name.
        base_tag = tag
        n = 1
        while tag in self.structs:
            n += 1
            tag = f"{base_tag}_{n}"

        kind = "struct" if die.tag == "DW_TAG_structure_type" else "union"
        sdef = StructDef(tag, kind, size)
        self.die_tags[die.offset] = tag
        self.structs[tag] = sdef

        for child in die.iter_children():
            if child.tag != "DW_TAG_member":
                continue
            mname = self._die_name(child)
            if mname is None or not C_IDENT_RE.match(mname):
                self.warnings.append(
                    f"{tag}: anonymous member dropped (padded)")
                continue
            if "DW_AT_bit_size" in child.attributes:
                self.warnings.append(
                    f"{tag}.{mname}: bitfield dropped (padded)")
                continue
            resolved = self.member_ctype(self._type_die(child))
            if resolved is None:
                self.warnings.append(
                    f"{tag}.{mname}: unrepresentable type dropped (padded)")
                continue
            ctype, suffix = resolved
            offset = self._member_offset(child)
            sdef.members.append(Member(mname, ctype, offset, suffix))

        if kind == "union":
            # Pin union size regardless of which members were dropped.
            sdef.members = [m for m in sdef.members if m.offset == 0]
            sdef.trailing_pad = 0
            sdef.members.append(Member("_penguin_union_pad", "uint8_t",
                                       0, f"[{size}]"))

        self.order.append(tag)
        return tag

    # ---- rendering / verification ----

    def render_all(self):
        return "\n\n".join(self.structs[tag].render() for tag in self.order)

    def _verify_once(self, ffi):
        """Return the first mismatch found, or None when layout is exact."""
        for tag in self.order:
            sdef = self.structs[tag]
            cname = f"{sdef.kind} {sdef.tag}"
            if sdef.opaque or sdef.kind == "union":
                actual = ffi.sizeof(cname)
                if actual != sdef.size:
                    return (sdef, "size", actual)
                continue
            for idx, member in enumerate(sdef.members):
                if not member.name:
                    continue
                actual = ffi.offsetof(cname, member.name)
                if actual != member.offset:
                    return (sdef, idx, actual)
            actual = ffi.sizeof(cname)
            if actual != sdef.size:
                return (sdef, "size", actual)
        return None

    def verify_and_repair(self):
        for _ in range(MAX_REPAIR_PASSES):
            ffi = cffi.FFI()
            try:
                ffi.cdef(self.render_all())
            except Exception as exc:  # cdef parse error: cannot repair
                raise SystemExit(f"generated cdef failed to parse: {exc}")
            mismatch = self._verify_once(ffi)
            if mismatch is None:
                return
            sdef, where, actual = mismatch
            if where == "size":
                if actual < sdef.size and not sdef.opaque:
                    sdef.trailing_pad += sdef.size - actual
                    continue
                self.warnings.append(
                    f"{sdef.tag}: size mismatch ({actual} != {sdef.size}); "
                    "made opaque")
                sdef.opaque = True
                continue
            member = sdef.members[where]
            if actual < member.offset:
                sdef.pads[where] = sdef.pads.get(where, 0) + (
                    member.offset - actual)
                continue
            self.warnings.append(
                f"{sdef.tag}.{member.name}: offset mismatch "
                f"({actual} > {member.offset}); struct made opaque")
            sdef.opaque = True
        raise SystemExit("layout repair did not converge")


class ApiCdefEmitter:
    """
    Emit a cdef for CFFI API mode: real type names, `...` ellipsis in every
    struct so the C compiler resolves all layout. Bitfields and anonymous
    members are declared, not dropped. All pointers are void *.
    """

    def __init__(self, extractor):
        # Reuse the DWARF navigation helpers from the ABI extractor.
        self.x = extractor
        self.defs = []  # rendered top-level struct definitions, in order
        self.die_refs = {}  # die offset -> "struct NAME" reference or None
        self.emitted_names = set()
        self.warnings = []

    def _members_body(self, die, indent="    ", partial=True):
        """
        Render member declarations. With partial=True a trailing `...;`
        lets the compiler resolve layout and unrenderable members are
        simply omitted. With partial=False (anonymous inline types, where
        cffi forbids ellipsis) every member must render; returns None if
        any cannot.
        """
        lines = []
        for child in die.iter_children():
            if child.tag != "DW_TAG_member":
                continue
            mname = self.x._die_name(child)
            type_die = self.x._type_die(child)
            bits = child.attributes.get("DW_AT_bit_size")
            rendered = self._member_type(type_die, indent=indent,
                                         strict=not partial)
            if rendered is None:
                if not partial:
                    return None
                self.warnings.append(
                    f"api: {self.x._die_name(die) or '<anon>'}."
                    f"{mname or '<anon>'} omitted")
                continue
            ctype, suffix = rendered
            if bits is not None:
                # cffi rejects a struct that carries BOTH bitfields and a
                # trailing `...;` ("using both bitfields and '...;'"), and the
                # ellipsis is what makes the compiler -- not us -- fix the
                # layout. So under partial=True the bitfield is dropped rather
                # than declared; the ABI-header path above already drops
                # bitfields for the same reason, so the two agree. Without
                # this, QEMU >= 11.1's all-bitfield `float_status` cost the
                # compiled env module on 9 of 11 system targets.
                if partial:
                    self.warnings.append(
                        f"api: {self.x._die_name(die) or '<anon>'}."
                        f"{mname or '<anon>'} bitfield dropped "
                        "(cffi forbids bitfields alongside `...`)")
                    continue
                if mname is None or suffix:
                    return None
                lines.append(f"{indent}{ctype} {mname} : {bits.value};")
            elif mname is None:
                # Anonymous struct/union member: keep it anonymous so
                # flattened field access keeps working.
                if "\n" in ctype:
                    lines.append(f"{indent}{ctype};")
                elif not partial:
                    return None
            else:
                lines.append(f"{indent}{ctype} {mname}{suffix};")
        if partial:
            lines.append(f"{indent}...;")
        return lines

    def _inline_body(self, die, kind, indent):
        inner = self._members_body(die, indent=indent + "    ", partial=False)
        if not inner:
            # None: unrepresentable member inside. Empty: a GNU zero-size
            # struct marker (e.g. end_reset_fields), which cffi sizes at 1.
            return None
        return "\n".join([f"{kind} {{", *inner, f"{indent}}}"])

    def _member_type(self, type_die, indent="    ", strict=False):
        die = self.x._resolve_typedefs(type_die)
        if die is None:
            return None
        if die.tag == "DW_TAG_pointer_type":
            return ("void *", "")
        if die.tag == "DW_TAG_base_type":
            ctype = self.x._base_ctype(die)
            return (ctype, "") if ctype else None
        if die.tag == "DW_TAG_enumeration_type":
            size = self.x._attr(die, "DW_AT_byte_size") or 4
            ctype = BASE_TYPES.get(("unsigned", size))
            return (ctype, "") if ctype else None
        if die.tag == "DW_TAG_array_type":
            element = self._member_type(self.x._type_die(die), indent=indent,
                                        strict=strict)
            if element is None:
                return None
            ctype, suffix = element
            dims = self.x._array_dims(die)
            if any(d <= 0 for d in dims):
                return None
            return (ctype, "".join(f"[{d}]" for d in dims) + suffix)
        if die.tag in ("DW_TAG_structure_type", "DW_TAG_union_type"):
            kind = "struct" if die.tag == "DW_TAG_structure_type" else "union"
            name = self.x._die_name(die)
            if name and C_IDENT_RE.match(name) and not strict:
                ref = self.emit(die)
                if ref is None:
                    return None
                return (ref, "")
            # Anonymous type -- or a named one needed inside an anonymous
            # inline type, where cffi forbids partial (`...`) types: inline
            # its complete definition. Field access is by member-name path,
            # so the C compiler still verifies the layout.
            if self.x._attr(die, "DW_AT_declaration"):
                return None
            body = self._inline_body(die, kind, indent)
            if body is None:
                return None
            return (body, "")
        return None

    def emit(self, die):
        """Emit a named struct/union top-level; returns 'struct NAME'."""
        if die.offset in self.die_refs:
            return self.die_refs[die.offset]
        if self.x._attr(die, "DW_AT_declaration"):
            return None
        kind = "struct" if die.tag == "DW_TAG_structure_type" else "union"
        name = self.x._die_name(die)
        if not name or not C_IDENT_RE.match(name):
            return None
        if name in self.emitted_names:
            # A distinct definition under an already-used tag would
            # conflict; rely on the first one (same closure, same type).
            self.die_refs[die.offset] = f"{kind} {name}"
            return self.die_refs[die.offset]
        self.emitted_names.add(name)
        self.die_refs[die.offset] = f"{kind} {name}"
        body = self._members_body(die)
        self.defs.append("\n".join([f"{kind} {name} {{", *body, "};"]))
        return self.die_refs[die.offset]

    def render(self, root_die):
        root_ref = self.emit(root_die)
        if root_ref is None:
            raise SystemExit("api: CPUArchState has no usable tag")
        decls = "\n\n".join(self.defs)
        tag = root_ref.split(" ", 1)[1]
        if tag != "CPUArchState":
            decls += f"\n\ntypedef {root_ref} CPUArchState;"
        else:
            decls += f"\n\ntypedef struct CPUArchState CPUArchState;"
        return decls


def harvest_compile_flags(build_dir, qemu_target):
    """
    Pull -I/-D/-include flags from the compile command of a file built for
    this target, so the module sees the exact configuration of the library.
    """
    commands = json.loads((build_dir / "compile_commands.json").read_text())
    marker = f"libqemu-{qemu_target}.a.p"
    entry = next(
        (c for c in commands
         if marker in c.get("output", "") or marker in c.get("command", "")),
        None)
    if entry is None:
        raise SystemExit(f"no compile command found for {qemu_target}")

    tokens = shlex.split(entry["command"])
    base = Path(entry["directory"])
    flags = []
    skip_next_for = None
    for token in tokens[1:]:
        if skip_next_for:
            flags.extend([skip_next_for, token])
            skip_next_for = None
            continue
        if token in ("-include", "-isystem", "-iquote"):
            skip_next_for = token
            continue
        if token.startswith("-I"):
            path = Path(token[2:])
            if not path.is_absolute():
                path = base / path
            flags.append(f"-I{path}")
        elif token.startswith("-D"):
            flags.append(token)
    return flags


def compile_env_module(build_dir, qemu_target, mode, api_cdef):
    module_name = "_penguin_qemu_env_{}_{}".format(
        mode, qemu_target.replace("-softmmu", "").replace("-", "_"))
    flags = harvest_compile_flags(build_dir, qemu_target)

    builder = cffi.FFI()
    builder.cdef(api_cdef)
    builder.set_source(
        module_name,
        '#include "qemu/osdep.h"\n#include "cpu.h"\n',
        # -UNDEBUG: distutils adds -DNDEBUG, which osdep.h rejects.
        # The cffi-generated source assigns opaque struct pointers (e.g.
        # CPUBreakpoint *, ARMMMUFaultInfo *) to the `void *` fields of the
        # ellipsis structs. GCC >= 14 promotes -Wincompatible-pointer-types
        # (and -Wint-conversion) to errors by default, so -w alone is not
        # enough -- turn those diagnostics off explicitly so the module builds
        # on a modern toolchain (Ubuntu 22.04's gcc still treats them as
        # warnings; the Nix gcc is 15.x).
        extra_compile_args=flags
        + [
          "-w",
          "-UNDEBUG",
          "-Wno-incompatible-pointer-types",
          "-Wno-int-conversion",
        ],
    )
    out_dir = build_dir / "penguin-qemu-env"
    out_dir.mkdir(exist_ok=True)
    artifact = builder.compile(tmpdir=str(out_dir), verbose=False)
    return Path(artifact).name


def find_root_dies(dwarf):
    """Locate the CPUArchState typedef target and struct CPUState size."""
    env_die = None
    cpu_state_size = None
    cus = list(dwarf.iter_CUs())

    def cu_name(cu):
        die = cu.get_top_DIE()
        name = die.attributes.get("DW_AT_name")
        if name is None:
            return ""
        value = name.value
        return value.decode("utf-8", "replace") if isinstance(value, bytes) else value

    # Target CUs define CPUArchState; check them first to finish fast.
    for cu in sorted(cus, key=lambda c: ("/target/" not in cu_name(c))):
        top = cu.get_top_DIE()
        for die in top.iter_children():
            if (env_die is None and die.tag == "DW_TAG_typedef"
                    and die.attributes.get("DW_AT_name") is not None
                    and die.attributes["DW_AT_name"].value == b"CPUArchState"):
                target = die.get_DIE_from_attribute("DW_AT_type")
                if target is not None and "DW_AT_byte_size" in target.attributes:
                    env_die = target
            if (cpu_state_size is None
                    and die.tag == "DW_TAG_structure_type"
                    and die.attributes.get("DW_AT_name") is not None
                    and die.attributes["DW_AT_name"].value == b"CPUState"
                    and "DW_AT_byte_size" in die.attributes):
                cpu_state_size = die.attributes["DW_AT_byte_size"].value
        if env_die is not None and cpu_state_size is not None:
            break
    return env_die, cpu_state_size


def generate(library_path):
    """Return (abi_header_text, api_cdef_text) for one library."""
    with open(library_path, "rb") as handle:
        elf = ELFFile(handle)
        if not elf.has_dwarf_info():
            raise SystemExit(f"{library_path}: no DWARF info (stripped?)")
        dwarf = elf.get_dwarf_info()
        env_die, cpu_state_size = find_root_dies(dwarf)
        if env_die is None:
            raise SystemExit(f"{library_path}: CPUArchState typedef not found")
        if cpu_state_size is None:
            raise SystemExit(f"{library_path}: struct CPUState not found")

        extractor = EnvTypeExtractor(dwarf)
        root_tag = extractor.emit_struct(env_die)
        if root_tag is None:
            raise SystemExit(f"{library_path}: CPUArchState unresolvable")
        extractor.verify_and_repair()

        api_emitter = ApiCdefEmitter(EnvTypeExtractor(dwarf))
        api_cdef = api_emitter.render(env_die)
        for warning in api_emitter.warnings:
            log(f"note: {warning}")

    for warning in extractor.warnings:
        log(f"note: {warning}")

    body = extractor.render_all()
    root = extractor.structs[root_tag]
    header = "\n".join([
        "/*",
        " * Generated by scripts/penguin-env-cffi-gen.py from "
        f"{Path(library_path).name}.",
        " *",
        " * Layout-exact CFFI declarations for this target's CPUArchState,",
        " * verified field-by-field against the library's DWARF. Members",
        " * that cannot be represented (bitfields, exotic types) are",
        " * replaced by explicit padding. All pointers are void *.",
        " *",
        " * Obtain the env pointer with penguin_cpu_env(cpu) and call",
        " * penguin_sync_cpu_state(cpu) first when running under KVM.",
        " */",
        "",
        f"#define PENGUIN_CPU_STATE_SIZE {cpu_state_size}",
        "",
        body,
        "",
        f"typedef {root.kind} {root.tag} CPUArchState;",
        "",
    ])
    return header, api_cdef


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--manifest", required=True,
                        help="cffi manifest written by penguin-cffi-gen.py; "
                             "entries gain env_header/env_module keys")
    parser.add_argument("--mode", default=None,
                        help="override manifest mode (system/kvm)")
    args = parser.parse_args()

    build_dir = Path(args.build_dir)
    manifest_path = Path(args.manifest)
    manifest = json.loads(manifest_path.read_text())
    mode = args.mode or manifest.get("mode", "system")

    headers = {}
    modules = {}
    for entry in manifest["headers"]:
        library = build_dir / entry["library"]
        env_header = entry["header"].replace(".h", "_env.h")
        target = entry.get("qemu_target", library.name)
        if target not in headers:
            log(f"generating {env_header} from {library.name}")
            header, api_cdef = generate(library)
            headers[target] = header
            try:
                modules[target] = compile_env_module(
                    build_dir, target, mode, api_cdef)
                log(f"compiled {modules[target]}")
            except Exception as exc:
                log(f"warning: compiled env module for {target} failed "
                    f"({exc}); shipping ABI header only")
                modules[target] = None
        (build_dir / env_header).write_text(headers[target])
        entry["env_header"] = env_header
        if modules[target]:
            entry["env_module"] = modules[target]

    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    log(f"updated {manifest_path} with {len(manifest['headers'])} env headers")


if __name__ == "__main__":
    main()
