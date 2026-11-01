'''
Guest writes: the one API for changing guest state from analysis code (``panda.guest``).

Without record/replay each call writes at once. While recording, a write goes ahead only inside
a mutating callback at a site that logs it (a ``@panda.hypercall`` declared ``mutates=True``, for
now): the engine logs it and replay repeats it at the same instruction without running the
callback. Anywhere else during record/replay the engine refuses it and this raises
GuestWriteRefused; a write is never dropped silently. See docs/design/rr-guest-mutation-api.md.

``panda.guest.env(cpu)`` is a view of the vCPU's CPUArchState: reads pass through, and an
assignment to a field (``env.regs[0] = 1``, ``env.cp15.ttbr0_el[1] = x``) becomes an env write at
that field's offset. A pointer field gives a view of what it points to, and writes through it
are logged by path (``env.pmsav7.drbar[3] = x``): host addresses differ between the recording
and replay processes, the path from env doesn't. The engine checks every path against its layout
map; assigning to a pointer itself is refused.
'''


class GuestWriteError(RuntimeError):
    '''A guest write the engine did not make. ``result`` is the engine's GuestWriteResult.'''

    def __init__(self, what, result, reason):
        super().__init__(f"{what}: {reason}")
        self.result = result


class GuestWriteRefused(GuestWriteError, PermissionError):
    '''No logging scope: an observe-only callback, a replay, or a site that does not log writes.'''


class GuestWriteInvalid(GuestWriteError, ValueError):
    '''Bad vCPU, register, length or path (including a host pointer or repointing).'''


class GuestWriteFault(GuestWriteError, ValueError):
    '''The address does not translate or is not RAM, or a path met a NULL pointer.'''


class GuestWriteDenied(GuestWriteError, PermissionError):
    '''The target refused the register write.'''


class GuestWriteTooBig(GuestWriteError):
    '''Past the per-site write budget.'''


_ERRORS = {1: GuestWriteRefused, 2: GuestWriteInvalid, 3: GuestWriteFault,
           4: GuestWriteDenied, 5: GuestWriteTooBig}


class Guest:
    '''``panda.guest``: guest writes through the engine's logged API.'''

    def __init__(self, panda):
        self._panda = panda

    def _check(self, result, what):
        result = int(result)
        if result:
            reason = self._panda.ffi.string(self._panda.libpanda.panda_guest_write_strerror(result))
            raise _ERRORS.get(result, GuestWriteError)(what, result, reason.decode())

    def _admit(self, what):
        self._panda._needs_engine(what)
        self._panda._check_guest_mutation(what)

    def write_virt(self, cpu, addr, data):
        '''Write bytes at a guest virtual address of cpu's address space.'''
        self._admit("guest.write_virt")
        data = bytes(data)
        self._check(self._panda.libpanda.panda_guest_write_virt(cpu, addr, data, len(data)),
                    f"guest.write_virt({addr:#x})")

    def write_phys(self, addr, data):
        '''Write bytes at a guest physical address (RAM only).'''
        self._admit("guest.write_phys")
        data = bytes(data)
        self._check(self._panda.libpanda.panda_guest_write_phys(addr, data, len(data)),
                    f"guest.write_phys({addr:#x})")

    def reg_lookup(self, cpu, name):
        '''(gdbstub number, size in bytes) of the register named as in the target's gdb XML.'''
        size = self._panda.ffi.new("size_t *")
        num = self._panda.libpanda.panda_guest_reg_lookup(cpu, name.encode(), size)
        if num < 0:
            raise GuestWriteInvalid(f"guest.reg_lookup({name!r})", 2, "no such register")
        return num, size[0]

    def set_reg(self, cpu, reg, value):
        '''
        Write a register through the target's setter. reg is a gdb XML name ("x0", "rip",
        "xmm3") or a gdbstub number; value is bytes of the register's size or an int, stored in
        the guest's byte order.
        '''
        self._admit("guest.set_reg")
        if isinstance(reg, str):
            num, size = self.reg_lookup(cpu, reg)
        else:
            num, size = reg, None
        if isinstance(value, int):
            if size is None:
                raise GuestWriteInvalid("guest.set_reg", 2, "an int value needs a register name")
            value = (value & ((1 << (8 * size)) - 1)).to_bytes(size, self._panda.endianness)
        value = bytes(value)
        self._check(self._panda.libpanda.panda_guest_write_reg(cpu, num, value, len(value)),
                    f"guest.set_reg({reg!r})")

    def set_retval(self, cpu, value):
        '''The return register of a hypercall or syscall, as panda_set_retval.'''
        self._admit("guest.set_retval")
        self._check(self._panda.libpanda.panda_guest_set_retval(cpu, value & (2**64 - 1)),
                    "guest.set_retval")

    def write_env(self, cpu, offset, data, path=()):
        '''
        Bytes at offset of the vCPU's CPUArchState, or, with path, of what the pointer fields at
        those offsets lead to (each relative to the struct reached so far).
        '''
        self._admit("guest.write_env")
        data = bytes(data)
        derefs = self._panda.ffi.new("uint32_t[]", list(path)) if path else self._panda.ffi.NULL
        self._check(self._panda.libpanda.panda_guest_write_env_path(cpu, derefs, len(path), offset,
                                                                    data, len(data)),
                    f"guest.write_env(path={list(path)}, offset={offset})")

    def env_sync(self, cpu):
        '''Recompute what the target derives from env (arm hflags) and flush the TLB.'''
        self._admit("guest.env_sync")
        self._check(self._panda.libpanda.panda_guest_env_sync(cpu), "guest.env_sync")

    def env(self, cpu):
        '''A view of cpu's CPUArchState whose assignments are guest writes.'''
        ffi = self._panda.ffi
        live = self._panda.cpu_env(cpu)
        return _View(self, cpu, ffi.typeof(live).item, (), 0, live)


def _kind(ctype):
    return ctype.kind if ctype.kind != 'enum' else 'primitive'


class _View:
    '''
    A struct, union, array or pointee inside a vCPU's env: ``path`` is the pointer fields
    followed from env, ``offset`` where this view starts in the last struct reached, ``live``
    the cffi object to read from.
    '''
    __slots__ = ('_guest', '_cpu', '_ctype', '_path', '_offset', '_live')

    def __init__(self, guest, cpu, ctype, path, offset, live):
        object.__setattr__(self, '_guest', guest)
        object.__setattr__(self, '_cpu', cpu)
        object.__setattr__(self, '_ctype', ctype)
        object.__setattr__(self, '_path', path)
        object.__setattr__(self, '_offset', offset)
        object.__setattr__(self, '_live', live)

    # where a member is: (ctype, offset in the current struct, live cffi value)
    def _field(self, name):
        if self._ctype.kind not in ('struct', 'union'):
            raise AttributeError(f"{self._ctype.cname} has no fields")
        for fname, field in self._ctype.fields:
            if fname == name:
                if field.bitsize != -1:
                    raise GuestWriteInvalid(f"env .{name}", 2, "bitfields are not supported")
                return field.type, self._offset + field.offset, getattr(self._live, name)
        raise AttributeError(f"{self._ctype.cname} has no field {name!r}")

    def _item(self, index):
        if self._ctype.kind == 'array':
            if not 0 <= index < self._ctype.length:
                raise IndexError(f"{self._ctype.cname}: index {index}")
            item = self._ctype.item
            return item, self._offset + index * self._guest._panda.ffi.sizeof(item), self._live[index]
        if self._ctype.kind == 'pointer':
            # a pointee array: the engine knows its length
            item = self._ctype.item
            return item, index * self._guest._panda.ffi.sizeof(item), self._live[index]
        raise TypeError(f"{self._ctype.cname} is not indexable")

    def _wrap(self, ctype, offset, live):
        kind = _kind(ctype)
        if kind in ('struct', 'union', 'array'):
            return _View(self._guest, self._cpu, ctype, self._path, offset, live)
        if kind == 'pointer':
            # what it points to, reached by following it: offset is the pointer field's own
            if live == self._guest._panda.ffi.NULL:
                return None
            pointee = ctype.item
            path = self._path + (offset,)
            if _kind(pointee) in ('struct', 'union'):
                return _View(self._guest, self._cpu, pointee, path, 0, live)
            return _View(self._guest, self._cpu, ctype, path, 0, live)
        return live

    def _store(self, ctype, offset, value, what):
        ffi = self._guest._panda.ffi
        kind = _kind(ctype)
        if kind == 'pointer':
            raise GuestWriteInvalid(what, 2, "a pointer can't be repointed")
        if isinstance(value, (bytes, bytearray)):
            data = bytes(value)
            if len(data) != ffi.sizeof(ctype):
                raise GuestWriteInvalid(what, 2, f"{len(data)} bytes for a {ffi.sizeof(ctype)}-byte field")
        elif kind == 'primitive':
            data = ffi.buffer(ffi.new(f"{ctype.cname} *", value))[:]
        else:
            raise GuestWriteInvalid(what, 2, f"assign bytes to a {ctype.cname}")
        self._guest.write_env(self._cpu, offset, data, self._path)

    def __getattr__(self, name):
        return self._wrap(*self._field(name))

    def __setattr__(self, name, value):
        ctype, offset, _ = self._field(name)
        self._store(ctype, offset, value, f"env .{name}")

    def __getitem__(self, index):
        return self._wrap(*self._item(index))

    def __setitem__(self, index, value):
        ctype, offset, _ = self._item(index)
        self._store(ctype, offset, value, f"env [{index}]")

    def __len__(self):
        if self._ctype.kind == 'array':
            return self._ctype.length
        raise TypeError(f"{self._ctype.cname} has no length")

    def __repr__(self):
        return f"<env view {self._ctype.cname} path={list(self._path)} offset={self._offset}>"
