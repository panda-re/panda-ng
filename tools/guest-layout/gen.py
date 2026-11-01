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

Every offset and size in the map is an offsetof() or sizeof expression, so the compiler of
each build computes it for that host: the layout of CPUArchState and of the structs it points
to depends on the host (pthread_mutex_t inside QemuMutex is 40 bytes on x86_64 glibc and 48 on
aarch64) and on which accelerators the host builds (HOST_CONDITIONAL). The numbers bindgen
gives are kept as checks that run on the host the bindings come from (x86_64 glibc), so a map
stale against the engine's headers fails to compile there. The layout hash that recordings
carry is computed at startup from the compiled map. --check exits 1 if the checked-in header
is stale (nix run .#regen-rust-bindings first if the engine's headers changed).
"""
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

# CPUArchState fields that exist only on some hosts: the condition target/i386/cpu.h puts them
# under, per field. The bindings come from an x86_64 host, which builds KVM and MSHV for x86
# targets; an aarch64 host builds neither. tests/static/guest_layout.py checks this against the
# engine's source.
X86_CONDITIONAL = {
    **{f: 'defined(CONFIG_KVM)' for f in (
        'nested_state', 'xen_vcpu_info_mr', 'xen_vcpu_info_hva', 'xen_vcpu_info_gpa',
        'xen_vcpu_info_default_gpa', 'xen_vcpu_time_info_gpa', 'xen_vcpu_runstate_gpa',
        'xen_vcpu_callback_vector', 'xen_callback_asserted', 'xen_virq',
        'xen_singleshot_timer_ns', 'xen_singleshot_timer', 'xen_periodic_timer_period',
        'xen_periodic_timer', 'xen_timers_lock')},
    'regs_page': 'defined(CONFIG_MSHV)',
    'emu_mmio_buf': 'defined(CONFIG_HVF) || defined(CONFIG_MSHV) || defined(CONFIG_WHPX)',
}
HOST_CONDITIONAL = {'x86_64': X86_CONDITIONAL, 'i386': X86_CONDITIONAL}
# Where the bindings' numbers hold (the checks run only there)
BINDINGS_HOST = 'defined(__x86_64__) && defined(__GLIBC__)'

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


def top_field(path):
    return re.match(r'\w+', path).group(0)


def layout(arch, b):
    """[(name, reach, cond, size, host, follow)], length expressions, CPUArchState's fields.

    reach is (parent type index, pointer path) for every type but CPUArchState; cond the C
    condition under which the type is in the map (None: always); host [(offset, size, path,
    cond)]; follow [(offset, type index or -1, element size, length entry, path, cond)].
    """
    w = Walker(b)
    lengths = LENGTHS.get(arch, {})
    # a conditional field the bindings lack is off on the bindings' host too (MSHV for i386)
    present = {c_name(f) for f, _ in b.structs[ROOT_TYPE][1]}
    conditional = {f: c for f, c in HOST_CONDITIONAL.get(arch, {}).items() if f in present}
    types, index, queue = [], {}, [(ROOT_TYPE, 0, None, None)]
    index[ROOT_TYPE] = 0
    length_exprs = []
    while queue:
        name, hops, reach, cond = queue.pop(0)
        host, follow = [], []
        for off, size, path, pointee, in_union in w.fields(name):
            fcond = conditional.get(top_field(path)) if name == ROOT_TYPE else None
            fcond = fcond or cond
            host.append((off, size, path, fcond))
            if pointee is None or in_union or hops >= MAX_HOPS:
                continue
            target = b.resolve(pointee)
            if target in b.structs and target in b.sizes and b.sizes[target]:
                i = index[name]
                if target not in index:
                    index[target] = len(types) + len(queue) + 1
                    queue.append((target, hops + 1, (i, path), fcond))
                follow.append((off, index[target], b.sizes[target], -1, path, fcond))
            elif name == ROOT_TYPE and path in lengths:
                try:
                    elem = b.size(target)
                except KeyError:
                    continue
                if not elem:
                    continue
                length_exprs.append(lengths[path])
                follow.append((off, -1, elem, len(length_exprs) - 1, path, fcond))
        types.append((name, reach, cond, b.sizes[name], host, follow))
    # queue order assigned the indices; types were appended in the same order
    assert [t[0] for t in types] == sorted(index, key=index.get)
    missing = set(lengths) - {f[4] for f in types[0][5]}
    if missing:
        raise SystemExit(f'{arch}: LENGTHS fields not found as scalar pointers: {sorted(missing)}')
    top = [(b.offsets[(ROOT_TYPE, f)], b.size(t), c_name(f), conditional.get(c_name(f)))
           for f, t in b.structs[ROOT_TYPE][1]
           if (ROOT_TYPE, f) in b.offsets and not f.startswith('__bindgen')]
    return types, length_exprs, top


def runs(host):
    """Touching and overlapping pointers joined, never across a host condition:
    [(offset, size, first path, last path, cond)], sorted."""
    out = []
    for off, size, path, cond in sorted(host, key=lambda h: h[0]):
        if out and out[-1][4] == cond and off <= out[-1][0] + out[-1][1]:
            start, length, first, last, _ = out[-1]
            if off + size > start + length:
                length, last = off + size - start, path
            out[-1] = (start, length, first, last, cond)
        else:
            out.append((off, size, path, path, cond))
    return out


def guarded(lines, cond, body):
    if cond:
        lines.append(f'#if {cond}')
    lines += body
    if cond:
        lines.append('#endif')


def emit(arch, cond, types, length_exprs, top):
    """The map as C, every offset and size computed by the compiler (see the docstring)."""
    lines = [f'#if {cond}', f'/* {arch}: {len(types)} types */']
    texpr = []      # each type as a C type: CPUArchState, or the pointee of the field reaching it
    for name, reach, tcond, size, host, follow in types:
        texpr.append(ROOT_TYPE if reach is None else
                     f'__typeof__(*(({texpr[reach[0]]} *)0)->{reach[1]})')

    def at(i, path):
        return f'offsetof({texpr[i]}, {path})'

    def elem(i, path):
        return f'sizeof(*(({texpr[i]} *)0)->{path})'

    checks = []
    for i, (name, reach, tcond, size, host, follow) in enumerate(types):
        hosts = runs(host)
        lines.append(f'/* {name} */')
        if tcond:
            lines.append(f'#if {tcond}')    # #else: empty tables keep the indices
        lines.append(f'static const GuestLayoutRange guest_layout_host_{i}[] = {{')
        for off, length, first, last, rcond in hosts:
            guarded(lines, rcond if rcond != tcond else None,
                    [f'    {{ {at(i, first)}, {at(i, last)} + sizeof(void *) - {at(i, first)} }},'])
            body = [f'QEMU_BUILD_BUG_ON({at(i, first)} != {off});',
                    f'QEMU_BUILD_BUG_ON({at(i, last)} + sizeof(void *) - {at(i, first)} != {length});']
            checks.append((rcond if rcond != tcond else None, tcond, body))
        lines.append('    { 0, 0 } /* end */')
        lines.append('};')
        lines.append(f'static const GuestLayoutPointer guest_layout_follow_{i}[] = {{')
        for off, t, esize, n, path, fcond in follow:
            guarded(lines, fcond if fcond != tcond else None,
                    [f'    {{ {at(i, path)}, {t}, {elem(i, path)}, {n} }}, /* {path} */'])
            body = [f'QEMU_BUILD_BUG_ON({at(i, path)} != {off});',
                    f'QEMU_BUILD_BUG_ON({elem(i, path)} != {esize});']
            checks.append((fcond if fcond != tcond else None, tcond, body))
        lines.append('    { 0, 0, 0, 0 } /* end */')
        lines.append('};')
        checks.append((None, tcond, [f'QEMU_BUILD_BUG_ON(sizeof({texpr[i]}) != {size});']))
        if tcond:
            lines.append('#else')
            lines.append(f'static const GuestLayoutRange guest_layout_host_{i}[] = {{ {{ 0, 0 }} }};')
            lines.append(f'static const GuestLayoutPointer guest_layout_follow_{i}[] = '
                         '{ { 0, 0, 0, 0 } };')
            lines.append('#endif')
    lines.append('static const GuestLayoutType guest_layout_types[] = {')
    for i, (name, reach, tcond, size, host, follow) in enumerate(types):
        entry = (f'    {{ "{name}", %s, guest_layout_follow_{i}, '
                 f'ARRAY_SIZE(guest_layout_follow_{i}) - 1, guest_layout_host_{i}, '
                 f'ARRAY_SIZE(guest_layout_host_{i}) - 1 }},')
        if tcond:
            lines += [f'#if {tcond}', entry % f'sizeof({texpr[i]})', '#else',
                      entry % '0' + ' /* not on this host */', '#endif']
        else:
            lines.append(entry % f'sizeof({texpr[i]})')
    lines.append('};')
    lines.append('static const GuestLayoutField guest_layout_fields[] = {')
    for off, size, name, fcond in sorted(top):
        guarded(lines, fcond, [f'    {{ offsetof({ROOT_TYPE}, {name}), '
                               f'sizeof((({ROOT_TYPE} *)0)->{name}), "{name}" }},'])
    lines.append('};')
    lines.append('static size_t guest_layout_length(CPUState *cs, int entry)')
    lines.append('{')
    if length_exprs:
        lines.append('    switch (entry) {')
        lines += [f'    case {i}: return {e};' for i, e in enumerate(length_exprs)]
        lines.append('    }')
    lines.append('    return 0;')
    lines.append('}')
    # The bindings' numbers, where they hold
    present = sorted({c for _, c, _ in [(None, x[0], None) for x in checks] if c} |
                     {f[3] for f in top if f[3]})
    lines.append(f'#if {BINDINGS_HOST}' + ''.join(f' && ({c})' for c in present))
    for rcond, tcond, body in checks:
        guarded(lines, ' && '.join(f'({c})' for c in (tcond, rcond) if c) or None, body)
    lines.append(f'#endif /* {BINDINGS_HOST} */')
    lines.append(f'#endif /* {arch} */')
    return lines


def generate():
    out = ['/* SPDX-' 'License-Identifier: GPL-2.0-or-later */',
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
