'''Taint mode (the "taint" capability): Panda methods over the engine's taint API (exec/taint.h).

Taint runs only in replay. Shadow values are bit masks: a set bit means that bit of guest data
depends on a labelled source. RAM is addressed by ram_addr (see taint_va_to_ram); registers by
gdbstub number.

    panda = Panda(generic="x86_64", rr=True, capabilities=["analysis", "rr", "taint"])

    @panda.cb_start_block_exec(observe_only=True)
    def first(cpu, tb):
        panda.taint_enable()
        panda.disable_callback("first")

    @panda.cb_taint_branch
    def on_branch(cpu, pc, mask):
        print(f"tainted branch at {pc:#x}")
'''

_CDEFS = """
bool taint_enable(void);
void taint_disable(void);
bool taint_enabled(void);
void taint_label_ram(uint64_t ram_addr, uint64_t len, uint8_t mask);
uint8_t taint_query_ram(uint64_t ram_addr);
uint64_t taint_count_ram(void);
uint64_t taint_va_to_ram(void *cpu, uint64_t va);
int taint_label_reg(void *cpu, int regno, uint8_t mask);
int taint_query_reg(void *cpu, int regno, uint8_t *out, int len);
typedef void (*TaintDeviceWriteFn)(void *opaque, uint64_t ram_addr, uint64_t len);
void taint_set_device_write_cb(TaintDeviceWriteFn fn, void *opaque);
typedef void (*TaintEventFn)(void *opaque, void *cpu, uint64_t pc, uint64_t mask);
void taint_set_branch_cb(TaintEventFn fn, void *opaque);
void taint_set_pc_cb(TaintEventFn fn, void *opaque);
void taint_stats(char *buf, size_t len);
bool taint_enable_labels(bool on);
bool taint_set_address_mode(bool on);
void taint_label_ram_l(uint64_t ram_addr, uint64_t len, uint8_t mask, uint32_t label);
int taint_label_reg_l(void *cpu, int regno, uint8_t mask, uint32_t label);
int taint_query_ram_labels(uint64_t ram_addr, uint32_t *out, int max);
int taint_query_reg_labels(void *cpu, int regno, int byte, uint32_t *out, int max);
typedef uint32_t (*TaintMmioReadFn)(void *opaque, void *cpu, uint64_t vaddr, unsigned size);
void taint_set_mmio_read_cb(TaintMmioReadFn fn, void *opaque);
"""

NO_RAM = 2**64 - 1


class TaintMixin:
    '''Taint methods of Panda. The engine must be started with the "taint" capability.'''

    def _taint_lib(self):
        if getattr(self, "_taint", None) is None:
            import cffi
            ffi = cffi.FFI()
            ffi.cdef(_CDEFS)
            self._taint = (ffi, ffi.dlopen(self.libpanda_path), {})
        return self._taint

    def _taint_cpu(self, cpu):
        ffi, _, _ = self._taint_lib()
        return ffi.cast("void *", int(self.ffi.cast("uintptr_t", cpu)))

    def taint_enable(self):
        '''Turn taint on (replay only; translated code is flushed). False if it can't be.'''
        return bool(self._taint_lib()[1].taint_enable())

    def taint_disable(self):
        '''Turn taint off and drop all shadow state.'''
        self._taint_lib()[1].taint_disable()

    def taint_enabled(self):
        return bool(self._taint_lib()[1].taint_enabled())

    def taint_enable_labels(self, on=True):
        '''Turn label sets (which sources reach each byte) on or off; costs extra only while on.
        Call before taint_enable.'''
        return bool(self._taint_lib()[1].taint_enable_labels(on))

    def taint_address_mode(self, on=True):
        '''Address mode on or off: a load through a tainted address is tainted, and so is a store
        through one (table lookups, pointer chasing). Not with label sets: False if they are on.'''
        return bool(self._taint_lib()[1].taint_set_address_mode(on))

    def taint_label_ram(self, ram_addr, length=1, mask=0xFF, label=None):
        '''Set the shadow of RAM bytes [ram_addr, ram_addr + length) to mask (0 clears), and with
        labels on, give them the single label `label` (a nonzero u32).'''
        lib = self._taint_lib()[1]
        if label is None:
            lib.taint_label_ram(ram_addr, length, mask)
        else:
            lib.taint_label_ram_l(ram_addr, length, mask, label)

    def taint_labels_ram(self, ram_addr):
        '''The labels that reach a RAM byte: a sorted list (empty if clean), or None if more
        than the engine keeps per byte (256).'''
        ffi, lib, _ = self._taint_lib()
        buf = ffi.new("uint32_t[]", 256)
        n = lib.taint_query_ram_labels(ram_addr, buf, 256)
        return None if n < 0 else list(buf[0:n])

    def taint_labels_va(self, cpu, va, length=1):
        '''taint_labels_ram for each guest virtual byte; [] where unmapped.'''
        out = []
        for i in range(length):
            ra = self.taint_va_to_ram(cpu, va + i)
            out.append([] if ra is None else self.taint_labels_ram(ra))
        return out

    def taint_labels_reg(self, cpu, regno, byte=0):
        '''The labels that reach one byte of a register (gdbstub number), as taint_labels_ram.'''
        ffi, lib, _ = self._taint_lib()
        buf = ffi.new("uint32_t[]", 256)
        n = lib.taint_query_reg_labels(self._taint_cpu(cpu), regno, byte, buf, 256)
        return None if n < 0 else list(buf[0:n])

    def taint_query_ram(self, ram_addr, length=1):
        '''Shadow bytes of RAM from ram_addr: an int for length 1, else a list.'''
        lib = self._taint_lib()[1]
        out = [lib.taint_query_ram(ram_addr + i) for i in range(length)]
        return out[0] if length == 1 else out

    def taint_count_ram(self):
        '''Number of tainted RAM bytes.'''
        return self._taint_lib()[1].taint_count_ram()

    def taint_va_to_ram(self, cpu, va):
        '''The ram_addr of guest virtual address va through cpu's page tables, or None.'''
        ra = self._taint_lib()[1].taint_va_to_ram(self._taint_cpu(cpu), va)
        return None if ra == NO_RAM else ra

    def taint_label_va(self, cpu, va, length=1, mask=0xFF, label=None):
        '''Label guest virtual bytes [va, va + length), as taint_label_ram. Returns the number
        of bytes labelled (unmapped bytes are skipped).'''
        n = 0
        for i in range(length):
            ra = self.taint_va_to_ram(cpu, va + i)
            if ra is not None:
                self.taint_label_ram(ra, 1, mask, label)
                n += 1
        return n

    def taint_query_va(self, cpu, va, length=1):
        '''Shadow bytes of guest virtual bytes [va, va + length), None where unmapped.'''
        out = []
        for i in range(length):
            ra = self.taint_va_to_ram(cpu, va + i)
            out.append(None if ra is None else self.taint_query_ram(ra))
        return out

    def taint_label_reg(self, cpu, regno, mask=0xFF, label=None):
        '''Label every byte of the register with gdbstub number regno (as taint_label_ram).
        Returns its size.'''
        lib = self._taint_lib()[1]
        if label is None:
            n = lib.taint_label_reg(self._taint_cpu(cpu), regno, mask)
        else:
            n = lib.taint_label_reg_l(self._taint_cpu(cpu), regno, mask, label)
        if n < 0:
            raise ValueError(f"register {regno} is not a single field of the CPU state")
        return n

    def taint_query_reg(self, cpu, regno):
        '''Shadow bytes of the register with gdbstub number regno, lowest-addressed first.'''
        ffi, lib, _ = self._taint_lib()
        buf = ffi.new("uint8_t[]", 64)
        n = lib.taint_query_reg(self._taint_cpu(cpu), regno, buf, 64)
        if n < 0:
            raise ValueError(f"register {regno} is not a single field of the CPU state")
        return list(buf[0:n])

    def taint_stats(self):
        '''Counters as a dict; "h1" maps helper names to tainted calls without a model.'''
        ffi, lib, _ = self._taint_lib()
        buf = ffi.new("char[]", 1 << 16)
        lib.taint_stats(buf, 1 << 16)
        lines = ffi.string(buf).decode().splitlines()
        words = lines[0].split()
        out = {words[i]: int(words[i + 1]) for i in range(0, len(words) - 1, 2)}
        out["h1"] = {l.split()[1]: int(l.split()[2]) for l in lines[1:] if l.startswith("h1 ")}
        return out

    def _taint_set(self, kind, setter, sig, fn, wrap):
        ffi, lib, keep = self._taint_lib()
        if fn is None:
            keep.pop(kind, None)
            getattr(lib, setter)(ffi.NULL, ffi.NULL)
            return None
        cb = ffi.callback(sig, wrap)
        keep[kind] = cb
        getattr(lib, setter)(cb, ffi.NULL)
        return fn

    def taint_device_write(self, fn):
        '''Decorator: fn(ram_addr, length) runs after a device or DMA write into RAM has cleared
        its shadow, and may label it (disk or network input as a source). None unregisters.'''
        return self._taint_set("device", "taint_set_device_write_cb",
                               "void(void *, uint64_t, uint64_t)", fn,
                               lambda opaque, ra, n: fn(ra, n))

    def taint_mmio_read(self, fn):
        '''Decorator: fn(cpu, vaddr, size) runs for each guest load from a device register (an
        address that is not RAM) while taint is on, and returns a label (nonzero int) to taint the
        loaded value, or 0/None to leave it clean: MMIO as a taint source. Registers may be stale
        when it runs; decide from the address. None unregisters.'''
        def wrap(opaque, cpu, vaddr, size):
            r = fn(self.ffi.cast("CPUState *", int(self._taint_lib()[0].cast("uintptr_t", cpu))),
                   vaddr, size)
            return int(r or 0)
        if fn is None:
            return self._taint_set("mmio", "taint_set_mmio_read_cb", None, None, None)
        return self._taint_set("mmio", "taint_set_mmio_read_cb",
                               "uint32_t(void *, void *, uint64_t, unsigned)", fn, wrap)

    def _taint_event(self, kind, setter, fn):
        def wrap(opaque, cpu, pc, mask):
            fn(self.ffi.cast("CPUState *", int(self._taint_lib()[0].cast("uintptr_t", cpu))),
               pc, mask)
        return self._taint_set(kind, setter, "void(void *, void *, uint64_t, uint64_t)", fn, wrap)

    def cb_taint_branch(self, fn):
        '''Decorator: fn(cpu, pc, mask) for each conditional branch whose condition is tainted.
        Checks exist only while one is registered; None unregisters.'''
        return self._taint_event("branch", "taint_set_branch_cb", fn)

    def cb_taint_pc(self, fn):
        '''Decorator: fn(cpu, target, mask) for each indirect jump to a tainted target. The
        target's shadow is then cleared (taint is data flow only).'''
        return self._taint_event("pc", "taint_set_pc_cb", fn)
