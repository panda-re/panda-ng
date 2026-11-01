#!/usr/bin/env python3
"""Generate the engine's guest-write layout map from the checked-in bindgen output.

    python3 tools/guest-layout/gen.py [--check]

rust/panda-bindings/src/autogen/<arch>.rs is bindgen's view of each target's headers: every
struct's fields with their types, and layout assertions giving each struct's size and each
field's offset. From it this writes engine/src/panda/src/guest-layout.gen.h, which
panda/src/guest-write.c includes for its target. For CPUArchState, and for every struct
reachable from it through pointer fields (up to MAX_HOPS pointers away), the map lists:

- every host-pointer range (data and function pointers, at any depth of by-value nesting):
  a write overlapping one is refused, because a host address differs between the recording
  and the replay processes;
- every followable pointer: one whose pointee is a struct in the map (one element) or an
  array of scalars whose element count LENGTHS gives. A write path may follow only these.

The env-level entries carry offsetof() checks, so a target whose CPUArchState changed under
a stale map fails to compile. --check exits 1 if the checked-in header is stale (nix run
.#regen-rust-bindings first if the engine's headers changed).
"""
import hashlib
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
AUTOGEN = ROOT / 'rust/panda-bindings/src/autogen'
OUT = ROOT / 'engine/src/panda/src/guest-layout.gen.h'

# arch: the preprocessor condition that selects it inside the engine's per-target build.
# The little-endian mips variants have their big-endian twin's layout (checked below).
TARGETS = {
    'aarch64': 'defined(TARGET_AARCH64)',
    'arm': 'defined(TARGET_ARM) && !defined(TARGET_AARCH64)',
    'x86_64': 'defined(TARGET_X86_64)',
    'i386': 'defined(TARGET_I386) && !defined(TARGET_X86_64)',
    'loongarch64': 'defined(TARGET_LOONGARCH64)',
    'mips64': 'defined(TARGET_MIPS64)',
    'mips': 'defined(TARGET_MIPS) && !defined(TARGET_MIPS64)',
    'ppc64': 'defined(TARGET_PPC64)',
    'ppc': 'defined(TARGET_PPC) && !defined(TARGET_PPC64)',
    'riscv64': 'defined(TARGET_RISCV64)',
    'riscv32': 'defined(TARGET_RISCV32)',
}
TWINS = {'mipsel': 'mips', 'mips64el': 'mips64'}

# Element counts of pointers to scalar arrays, which bindgen can't know: field path in
# CPUArchState -> C expression on `cs` (CPUState *). Every other scalar pointer is refused.
ARM_LENGTHS = {
    'pmsav7.drbar': 'ARM_CPU(cs)->pmsav7_dregion',
    'pmsav7.drsr': 'ARM_CPU(cs)->pmsav7_dregion',
    'pmsav7.dracr': 'ARM_CPU(cs)->pmsav7_dregion',
    'pmsav8.rbar[0]': 'ARM_CPU(cs)->pmsav7_dregion',
    'pmsav8.rbar[1]': 'ARM_CPU(cs)->pmsav7_dregion',
    'pmsav8.rlar[0]': 'ARM_CPU(cs)->pmsav7_dregion',
    'pmsav8.rlar[1]': 'ARM_CPU(cs)->pmsav7_dregion',
    'pmsav8.hprbar': 'ARM_CPU(cs)->pmsav8r_hdregion',
    'pmsav8.hprlar': 'ARM_CPU(cs)->pmsav8r_hdregion',
    'sau.rbar': 'ARM_CPU(cs)->sau_sregion',
    'sau.rlar': 'ARM_CPU(cs)->sau_sregion',
}
LENGTHS = {'aarch64': ARM_LENGTHS, 'arm': ARM_LENGTHS}

ROOT_TYPE = 'CPUArchState'
MAX_HOPS = 4        # pointers a path may follow from CPUArchState (GUEST_WRITE_PATH_MAX)
HOST_POINTER = 8    # x86_64 build host (the bindings are generated there)

PRIMITIVES = {
    'u8': 1, 'i8': 1, 'bool': 1, 'u16': 2, 'i16': 2, 'u32': 4, 'i32': 4, 'f32': 4,
    'u64': 8, 'i64': 8, 'f64': 8, 'u128': 16, 'i128': 16, 'usize': 8, 'isize': 8,
    '::std::os::raw::c_char': 1, '::std::os::raw::c_schar': 1, '::std::os::raw::c_uchar': 1,
    '::std::os::raw::c_short': 2, '::std::os::raw::c_ushort': 2,
    '::std::os::raw::c_int': 4, '::std::os::raw::c_uint': 4,
    '::std::os::raw::c_long': 8, '::std::os::raw::c_ulong': 8,
    '::std::os::raw::c_longlong': 8, '::std::os::raw::c_ulonglong': 8,
    '::std::os::raw::c_float': 4, '::std::os::raw::c_double': 8,
}
# bindgen appends '_' to C names that are Rust keywords
RUST_KEYWORDS = set('''abstract as async await become box break const continue crate do dyn else enum
    extern false final fn for if impl in let loop macro match mod move mut override priv pub ref
    return self static struct super trait true try type typeof unsafe unsized use virtual where
    while yield'''.split())


class Bindings:
    def __init__(self, text):
        self.structs = {}   # name -> (is_union, [(field, type)])
        for m in re.finditer(r'^pub (struct|union) (\w+) \{\n(.*?)^\}', text, re.M | re.S):
            self.structs[m.group(2)] = (m.group(1) == 'union', members(m.group(3)))
        self.aliases = {n: ' '.join(t.split()) for n, t in
                        re.findall(r'^pub type (\w+) = (.+?);$', text, re.M | re.S)}
        self.sizes = {n: int(s) for n, s in re.findall(
            r'\["Size of (\w+)"\]\s*\[::std::mem::size_of::<\w+>\(\) - (\d+)usize\]', text)}
        self.offsets = {(t, f): int(o) for t, f, o in re.findall(
            r'\["Offset of field: (\w+)::(\w+)"\]\s*\[::std::mem::offset_of!\(\w+, \w+\) - (\d+)usize\]',
            text)}

    def resolve(self, ty):
        seen = set()
        while ty in self.aliases and ty not in seen:
            seen.add(ty)
            ty = self.aliases[ty]
        return ty

    def size(self, ty):
        ty = self.resolve(ty.strip())
        if ty in PRIMITIVES:
            return PRIMITIVES[ty]
        if is_pointer(ty):
            return HOST_POINTER
        a = array(ty)
        if a:
            return self.size(a[0]) * a[1]
        if ty in self.sizes:
            return self.sizes[ty]
        if ty.startswith('__BindgenBitfieldUnit<'):
            return self.size(ty[len('__BindgenBitfieldUnit<'):-1])
        if ty.startswith('__IncompleteArrayField<') or ty.startswith('__BindgenUnionField<'):
            return 0
        if ty.startswith('::std::marker::PhantomData'):
            return 0
        raise KeyError(ty)


def members(body):
    """[(field, type)] of a struct body; rustfmt splits long (function pointer) types."""
    out, depth, item = [], 0, ''
    for c in body:
        if c == ',' and depth == 0:
            m = re.match(r'\s*pub (\w+):\s*(.+)$', item, re.S)
            if m:
                out.append((m.group(1), ' '.join(m.group(2).split())))
            item = ''
            continue
        depth += c in '[<('
        depth -= c in ']>)' and not item.endswith('-')    # '->' is not a bracket
        item += c
    return out


def is_function(ty):
    return re.match(r'(::std::option::)?Option<\s*unsafe extern', ty) is not None


def is_pointer(ty):
    return ty.startswith('*mut ') or ty.startswith('*const ') or is_function(ty)


def array(ty):
    """'[T; Nusize]' -> (T, N), splitting at the outermost ';'."""
    if not (ty.startswith('[') and ty.endswith(']')):
        return None
    inner, depth = ty[1:-1], 0
    for i, c in enumerate(inner):
        depth += c in '[<('
        depth -= c in ']>)'
        if c == ';' and depth == 0:
            return inner[:i].strip(), int(inner[i + 1:].strip().removesuffix('usize'))
    return None


def c_name(field):
    return field[:-1] if field.endswith('_') and field[:-1] in RUST_KEYWORDS else field


def join(path, field):
    """C designator: bindgen's anonymous members (__bindgen_anon_N) have no name in C."""
    if field.startswith('__bindgen_anon_'):
        return path
    field = c_name(field)
    return f'{path}.{field}' if path else field


class Walker:
    """Pointer fields of one struct, through by-value nesting (structs, unions, arrays)."""

    def __init__(self, b):
        self.b = b

    def fields(self, struct):
        b = self.b
        out = []    # (offset, size, path, pointee or None for a function pointer, in_union)

        def visit(ty, offset, path, in_union):
            ty = b.resolve(ty.strip())
            if is_pointer(ty):
                pointee = None if is_function(ty) else b.resolve(ty.split(' ', 1)[1].strip())
                out.append((offset, HOST_POINTER, path, pointee, in_union))
                return
            a = array(ty)
            if a:
                elem = b.size(a[0])
                if elem and self.has_pointers(a[0]):
                    for i in range(a[1]):
                        visit(a[0], offset + i * elem, f'{path}[{i}]', in_union)
                return
            if ty in b.structs:
                union, members = b.structs[ty]
                for field, fty in members:
                    if field.startswith('_bindgen_') or field.startswith('__bindgen_padding'):
                        continue
                    off = 0 if union else b.offsets.get((ty, field))
                    if off is None:
                        continue    # bitfield storage: no pointers
                    visit(fty, offset + off, join(path, field), in_union or union)

        visit(struct, 0, '', False)
        return out

    def has_pointers(self, ty, seen=None):
        seen = seen or set()
        ty = self.b.resolve(ty.strip())
        if is_pointer(ty):
            return True
        a = array(ty)
        if a:
            return self.has_pointers(a[0], seen)
        if ty in self.b.structs and ty not in seen:
            seen.add(ty)
            return any(self.has_pointers(f, seen) for _, f in self.b.structs[ty][1])
        return False


def merge(ranges):
    """Sorted, with touching and overlapping ranges joined."""
    out = []
    for off, size in sorted(ranges):
        if out and off <= out[-1][0] + out[-1][1]:
            out[-1] = (out[-1][0], max(out[-1][1], off + size - out[-1][0]))
        else:
            out.append((off, size))
    return out


def layout(arch, b):
    w = Walker(b)
    lengths = LENGTHS.get(arch, {})
    types, index, queue = [], {}, [(ROOT_TYPE, 0)]
    index[ROOT_TYPE] = 0
    length_exprs = []
    while queue:
        name, hops = queue.pop(0)
        host, follow = [], []
        for off, size, path, pointee, in_union in w.fields(name):
            host.append((off, size))
            if pointee is None or in_union or hops >= MAX_HOPS:
                continue
            target = b.resolve(pointee)
            if target in b.structs and target in b.sizes and b.sizes[target]:
                if target not in index:
                    index[target] = len(types) + len(queue) + 1
                    queue.append((target, hops + 1))
                follow.append((off, index[target], b.sizes[target], -1, path))
            elif name == ROOT_TYPE and path in lengths:
                try:
                    elem = b.size(target)
                except KeyError:
                    continue
                if not elem:
                    continue
                length_exprs.append(lengths[path])
                follow.append((off, -1, elem, len(length_exprs) - 1, path))
        types.append((name, b.sizes[name], merge(host), follow))
    # queue order assigned the indices; types were appended in the same order
    assert [t[0] for t in types] == sorted(index, key=index.get)
    missing = set(lengths) - {f[4] for f in types[0][3]}
    if missing:
        raise SystemExit(f'{arch}: LENGTHS fields not found as scalar pointers: {sorted(missing)}')
    top = [(b.offsets[(ROOT_TYPE, f)], b.size(t), c_name(f)) for f, t in b.structs[ROOT_TYPE][1]
           if (ROOT_TYPE, f) in b.offsets and not f.startswith('__bindgen')]
    return types, length_exprs, top


def emit(arch, cond, types, length_exprs, top):
    tag = arch.upper()
    digest = hashlib.sha256(repr((types, length_exprs, top)).encode()).hexdigest()
    lines = [f'#if {cond}', f'/* {arch}: {len(types)} types */']
    lines.append(f'QEMU_BUILD_BUG_ON(sizeof(CPUArchState) != {types[0][1]});')
    for off, _, _, _, path in types[0][3]:
        lines.append(f'QEMU_BUILD_BUG_ON(offsetof(CPUArchState, {path}) != {off});')
    for i, (name, size, host, follow) in enumerate(types):
        lines.append(f'static const GuestLayoutRange guest_layout_host_{i}[] = {{')
        lines += [f'    {{ {o}, {s} }},' for o, s in host] or ['    { 0, 0 },']
        lines.append('};')
        lines.append(f'static const GuestLayoutPointer guest_layout_follow_{i}[] = {{')
        lines += [f'    {{ {o}, {t}, {e}, {n} }}, /* {p} */' for o, t, e, n, p in follow] \
            or ['    { 0, 0, 0, 0 },']
        lines.append('};')
    lines.append('static const GuestLayoutType guest_layout_types[] = {')
    for i, (name, size, host, follow) in enumerate(types):
        lines.append(f'    {{ "{name}", {size}, guest_layout_follow_{i}, {len(follow)}, '
                     f'guest_layout_host_{i}, {len(host)} }},')
    lines.append('};')
    lines.append('static const GuestLayoutField guest_layout_fields[] = {')
    lines += [f'    {{ {o}, {s}, "{n}" }},' for o, s, n in sorted(top)]
    lines.append('};')
    lines.append('static size_t guest_layout_length(CPUState *cs, int entry)')
    lines.append('{')
    if length_exprs:
        lines.append('    switch (entry) {')
        lines += [f'    case {i}: return {e};' for i, e in enumerate(length_exprs)]
        lines.append('    }')
    lines.append('    return 0;')
    lines.append('}')
    lines.append(f'#define GUEST_LAYOUT_HASH 0x{digest[:8]}u /* {tag} */')
    lines.append(f'#endif /* {arch} */')
    return lines


def generate():
    out = ['/* SPDX-License-Identifier: GPL-2.0-or-later */',
           '/* Generated by tools/guest-layout/gen.py from rust/panda-bindings/src/autogen;',
           ' * do not edit. Included by panda/src/guest-write.c only. */', '']
    results = {}
    for arch, cond in TARGETS.items():
        b = Bindings((AUTOGEN / f'{arch}.rs').read_text())
        results[arch] = layout(arch, b)
        out += emit(arch, cond, *results[arch]) + ['']
    for twin, base in TWINS.items():
        b = Bindings((AUTOGEN / f'{twin}.rs').read_text())
        if layout(twin, b) != results[base]:
            raise SystemExit(f'{twin} differs from {base}: give it its own TARGETS entry')
    return '\n'.join(out).rstrip() + '\n'


def main():
    text = generate()
    if '--check' in sys.argv[1:]:
        if not OUT.exists() or OUT.read_text() != text:
            print(f'{OUT.relative_to(ROOT)} is stale: python3 tools/guest-layout/gen.py', file=sys.stderr)
            sys.exit(1)
        return
    OUT.write_text(text)
    print(f'wrote {OUT.relative_to(ROOT)}')


if __name__ == '__main__':
    main()
