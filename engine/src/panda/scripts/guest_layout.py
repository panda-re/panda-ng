#!/usr/bin/env python3
"""Generate panda/src/guest-write.c's layout map from this build's debug info.

    guest_layout.py <gdb> <probe library .a> <output .h>

meson.build runs this for each softmmu target on libguest-layout-probe-<target>.a, built from
panda/src/guest-layout-probe.c with the target's flags and -g. It takes the probe object out of
the archive and runs itself inside gdb, which reads CPUArchState and every struct it reaches
through pointer fields (up to MAX_HOPS pointers away) from the object's debug info. So every
offset and size in the map is the one this build's compiler gave, on this host: the layout
depends on the host (pthread_mutex_t inside QemuMutex is 40 bytes on x86_64 glibc and 48 on
aarch64) and on which accelerators it builds (an aarch64 host has no KVM fields in CPUX86State).

For each struct the map lists:
- every host-pointer range (data and function pointers, at any depth of by-value nesting): a
  write overlapping one is refused, because a host address differs between the recording and
  the replay processes;
- every followable pointer: one whose pointee is a complete struct (one element) or an array
  of scalars whose element count LENGTHS gives. A write path may follow only these.
A pointee whose definition guest-layout-probe.c doesn't include is incomplete and not followed.

SPDX-License-Identifier: GPL-2.0-or-later
"""
import os
import subprocess
import sys
import tempfile

ROOT_TYPE = 'CPUArchState'
MAX_HOPS = 4        # pointers a path may follow from CPUArchState (GUEST_WRITE_PATH_MAX)

# Element counts of pointers to scalar arrays, which the debug info can't know: field path in
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
LENGTHS = {'arm': ARM_LENGTHS, 'aarch64': ARM_LENGTHS}


def extract_member(archive, out):
    """The first object in a static library: an ar archive, or a thin one (meson's default),
    whose members are paths relative to the archive. Returns the object's path."""
    with open(archive, 'rb') as f:
        data = f.read()
    thin = data.startswith(b'!<thin>\n')
    if not thin and not data.startswith(b'!<arch>\n'):
        sys.exit(f'{archive}: not an ar archive')
    pos, names = 8, b''
    while pos + 60 <= len(data):
        name = data[pos:pos + 16].decode().strip()
        size = int(data[pos + 48:pos + 58].decode().strip())
        stored = not thin or name.startswith('/') and not name[1:2].isdigit()
        body = data[pos + 60:pos + 60 + size] if stored else b''
        pos += 60 + (size + (size & 1) if stored else 0)
        if name == '//':
            names = body        # long-name table
            continue
        if name.startswith('/') and not name[1:2].isdigit():
            continue            # symbol table
        if not thin:
            with open(out, 'wb') as f:
                f.write(body)
            return out
        if name.startswith('/'):
            start = int(name[1:])
            name = names[start:names.index(b'\n', start)].decode()
        return os.path.join(os.path.dirname(archive), name.rstrip('/'))
    sys.exit(f'{archive}: no object')


def main():
    gdb, archive, output = sys.argv[1:4]
    target = os.path.basename(output)[len('guest-layout-'):-len('.h')]
    with tempfile.TemporaryDirectory() as tmp:
        obj = extract_member(archive, os.path.join(tmp, 'probe.o'))
        cmd = [gdb, '-batch', '-nx', '-ex',
               f'python import sys; sys.argv = {[__file__, obj, output, target]!r}',
               '-x', __file__]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode or not os.path.exists(output):
            sys.exit(f'guest_layout.py: gdb failed for {target}:\n{r.stdout}{r.stderr}')


# --- inside gdb ---------------------------------------------------------------------------


def generate(gdb, output, target):
    arch = target.split('-')[0]
    lengths = LENGTHS.get(arch, {})

    def strip(t):
        while True:
            t = t.strip_typedefs()
            u = t.unqualified()
            if u == t:
                return t
            t = u

    def is_pointer(t):
        return strip(t).code == gdb.TYPE_CODE_PTR

    def is_struct(t):
        return strip(t).code in (gdb.TYPE_CODE_STRUCT, gdb.TYPE_CODE_UNION)

    def complete(t):
        t = strip(t)
        return is_struct(t) and t.sizeof > 0 and len(t.fields()) > 0

    def tname(t):
        t = strip(t)
        return t.tag or t.name or str(t)

    def pointer_fields(t):
        """[(offset, path, pointee type or None for code, in union)] through by-value nesting."""
        out = []

        def visit(t, offset, path, in_union):
            t = strip(t)
            if t.code == gdb.TYPE_CODE_PTR:
                target = strip(t.target())
                out.append((offset, path, None if target.code == gdb.TYPE_CODE_FUNC else target,
                            in_union))
            elif t.code == gdb.TYPE_CODE_ARRAY:
                lo, hi = t.range()
                elem = strip(t.target())
                if hi >= lo and elem.sizeof and has_pointers(elem):
                    for i in range(lo, hi + 1):
                        visit(elem, offset + (i - lo) * elem.sizeof, f'{path}[{i}]', in_union)
            elif t.code in (gdb.TYPE_CODE_STRUCT, gdb.TYPE_CODE_UNION):
                union = t.code == gdb.TYPE_CODE_UNION
                for f in t.fields():
                    if f.bitsize or not hasattr(f, 'bitpos'):
                        continue    # bitfields hold no pointers; static members have no place
                    name = f.name
                    sub = path if not name else (f'{path}.{name}' if path else name)
                    visit(f.type, offset + f.bitpos // 8, sub, in_union or union)

        visit(t, 0, '', False)
        return out

    seen_ptr = {}

    def has_pointers(t):
        t = strip(t)
        key = str(t)
        if key in seen_ptr:
            return seen_ptr[key]
        seen_ptr[key] = False
        if t.code == gdb.TYPE_CODE_PTR:
            r = True
        elif t.code == gdb.TYPE_CODE_ARRAY:
            r = has_pointers(t.target())
        elif t.code in (gdb.TYPE_CODE_STRUCT, gdb.TYPE_CODE_UNION):
            r = any(has_pointers(f.type) for f in t.fields()
                    if not f.bitsize and hasattr(f, 'bitpos'))
        else:
            r = False
        seen_ptr[key] = r
        return r

    pointer_size = gdb.lookup_type('void').pointer().sizeof
    root = strip(gdb.lookup_type(ROOT_TYPE))
    types, index, queue, length_exprs = [], {tname(root): 0}, [(root, 0)], []
    while queue:
        t, hops = queue.pop(0)
        host, follow = [], []
        for off, path, pointee, in_union in pointer_fields(t):
            host.append((off, pointer_size))
            if pointee is None or in_union or hops >= MAX_HOPS:
                continue
            if complete(pointee):
                name = tname(pointee)
                if name not in index:
                    index[name] = len(types) + len(queue) + 1
                    queue.append((pointee, hops + 1))
                follow.append((off, index[name], pointee.sizeof, -1, path))
            elif hops == 0 and path in lengths and pointee.sizeof:
                length_exprs.append(lengths[path])
                follow.append((off, -1, pointee.sizeof, len(length_exprs) - 1, path))
        types.append((tname(t), t.sizeof, merge(host), follow))
    found = {f[4] for f in types[0][3]}
    if set(lengths) - found:
        raise SystemExit(f'{target}: LENGTHS fields not found as scalar pointers: '
                         f'{sorted(set(lengths) - found)}')
    top = sorted((f.bitpos // 8, strip(f.type).sizeof, f.name) for f in root.fields()
                 if f.name and not f.bitsize and hasattr(f, 'bitpos'))

    lines = ['/* SPDX-' 'License-Identifier: GPL-2.0-or-later */',
             f'/* Generated by panda/scripts/guest_layout.py from this build\'s debug info for',
             f' * {target}; included by panda/src/guest-write.c only. */',
             f'/* {len(types)} types */']
    for i, (name, size, host, follow) in enumerate(types):
        lines.append(f'static const GuestLayoutRange guest_layout_host_{i}[] = {{')
        lines += [f'    {{ {o}, {s} }},' for o, s in host]
        lines.append('    { 0, 0 } /* end */')
        lines.append('};')
        lines.append(f'static const GuestLayoutPointer guest_layout_follow_{i}[] = {{')
        lines += [f'    {{ {o}, {t}, {e}, {n} }}, /* {p} */' for o, t, e, n, p in follow]
        lines.append('    { 0, 0, 0, 0 } /* end */')
        lines.append('};')
    lines.append('static const GuestLayoutType guest_layout_types[] = {')
    for i, (name, size, host, follow) in enumerate(types):
        lines.append(f'    {{ "{name}", {size}, guest_layout_follow_{i}, {len(follow)}, '
                     f'guest_layout_host_{i}, {len(host)} }},')
    lines.append('};')
    lines.append('static const GuestLayoutField guest_layout_fields[] = {')
    lines += [f'    {{ {o}, {s}, "{n}" }},' for o, s, n in top]
    lines.append('};')
    lines.append('static size_t guest_layout_length(CPUState *cs, int entry)')
    lines.append('{')
    if length_exprs:
        lines.append('    switch (entry) {')
        lines += [f'    case {i}: return {e};' for i, e in enumerate(length_exprs)]
        lines.append('    }')
    lines.append('    return 0;')
    lines.append('}')
    with open(output, 'w') as f:
        f.write('\n'.join(lines) + '\n')


def merge(ranges):
    """Sorted, with touching and overlapping ranges joined."""
    out = []
    for off, size in sorted(ranges):
        if out and off <= out[-1][0] + out[-1][1]:
            out[-1] = (out[-1][0], max(out[-1][1], off + size - out[-1][0]))
        else:
            out.append((off, size))
    return out


try:
    import gdb  # noqa: F401  (inside gdb)
except ImportError:
    gdb = None

if gdb is not None:
    gdb.execute(f'file {sys.argv[1]}', to_string=True)
    generate(gdb, sys.argv[2], sys.argv[3])
elif __name__ == '__main__':
    main()
