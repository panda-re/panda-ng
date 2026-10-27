"""mmio_after_read and mmio_before_write on a real device access; the memory callbacks on it too.

Run: panda-python tests/mmio.py <dir>/debian-13-x86_64.json

tests/programs/mmio_<arch>.S maps a page of device registers from /dev/mem and, between its
hypercalls 1 and 2, makes 4-byte accesses: a store to a device register (mw1), a load of another
(mr), the store again (mw2), then a load and a store of RAM (rr, rw). The device has no driver (so
the kernel lets /dev/mem map it, and nothing else touches it):
- aarch64, riscv64: the virt machine's first virtio-mmio transport, which has no backend: mr
  reads MagicValue (0x74726976), mw writes Status (ignored without a backend);
- x86_64: the std VGA's MMIO BAR, with bochs-drm unbound from it first: mr reads the QEMU
  extended registers' size (8), mw writes their byte order with a value that is neither of its
  two magics (ignored).
The device accesses are in the middle of a TB, so QEMU rewinds each and runs the instruction again
alone; the bridge reports only that second attempt. mw1 is the first access to the mapping: its
first attempt finds no TLB entry, and only the bridge's page walk and region lookup tell it that
the access goes to a device (run 2 sees mw1's before events twice if they don't); mr and mw2 find
the TLB entry mw1 left.

The program runs twice: run 1 with only mmio_after_read and mmio_before_write registered, so
they work on their own (the bridge instruments loads and stores for them); run 2 also registers
the phys/virt memory callbacks, at its hypercall 0. All are enabled at hypercall 1 and disabled at
hypercall 2. Checks, against the program's own ground truth (its mapping, its labels, the value it
loaded):
- mmio_before_write fires for the two device stores, in order, and mmio_after_read for the device
  load, each once, with the register's physical address (page + offset, which is also
  virt_to_phys of the mapping), the mapping's virtual address, size 4 and the value (stored, or
  loaded: the device's constant); neither fires for the RAM accesses;
- the value is a copy: mmio_after_read writes *val, and the program still loads the device's value;
- from mmio_after_read, PANDA's memory readers (panda_physical_memory_read,
  panda_virtual_memory_read) fail on the register rather than access the device; pandare2's
  physical_memory_read and virtual_memory_read should raise ValueError then, and don't (a gap:
  they return zero bytes);
- before events come once per access: a device access from the middle of a block is rewound by
  QEMU and run again alone, and the bridge reports only the access that happens;
- run 2: the phys/virt memory callbacks see the device accesses too, at the accessing
  instruction's pc, with the device's physical address and the value, and the RAM accesses with
  RAM's; the order is phys_mem_before_write, mmio_before_write, phys_mem_after_write for a store
  and mmio_after_read, phys_mem_after_read for a load (classic PANDA's order).
"""
import sys

from testlib import Checks, MAGIC, hypercall_args, in_program, progress, read_quads, upload
from pandare2 import Panda

LABELS = ["mw1", "mr", "mw2", "rr", "rw", "ramword", "mapping"]
LOADED = {"x86_64": ("RCX", "R9"), "aarch64": ("X25", "X26"), "riscv64": ("S6", "S7")}
VIRTIO_MAGIC = 0x74726976
WVAL = 0x5a5aa5a5   # written to the device and to RAM
RAMWORD = 0x1122334455667788
SCRIBBLE = 0xdeadbeef
# (device page, offset read, offset written, value read); x86_64's page is found in the guest.
DEVICE = {"aarch64": (0x0a000000, 0x0, 0x70, VIRTIO_MAGIC), "riscv64": (0x10001000, 0x0, 0x70, VIRTIO_MAGIC),
          "x86_64": (None, 0x600, 0x604, 8)}
MEMCBS = ["vmbw", "vmar", "vmaw", "pmbr", "pmbw", "pmar", "pmaw"]

panda = Panda(generic=sys.argv[1])
arch = panda.arch_name
ffi = panda.ffi
C = Checks(arch, {
    f"[run {n}] pandare2's physical_memory_read and virtual_memory_read raise ValueError on device memory":
    "they raise only on a negative return; panda_physical_memory_rw fails with MEMTX_ERROR (1), and they "
    "return zero bytes" for n in (1, 2)})
page, roff, woff, rvalue = DEVICE[arch]


class Run:
    def __init__(self, n):
        self.n = n
        self.ev = []        # (kind, pc or None, vaddr, paddr, size, value) in the order they fired
        self.window = False
        self.labels = {}
        self.regs = None
        self.phys = {}
        self.readback = {}


runs = []
cur = None


def mine(vaddr):
    """A vaddr the program's window accesses: its device mapping or ramword."""
    L = cur.labels
    return L and (L["mapping"] <= vaddr < L["mapping"] + 4096 or vaddr == L["ramword"])


@panda.cb_mmio_after_read(name="mar", enabled=False)
def mar(cpu, physaddr, vaddr, size, val):
    r = cur
    if r is None or not r.window or not mine(vaddr):
        return
    r.ev.append(("mmio_r", None, vaddr, physaddr, size, val[0]))
    if not r.readback:
        buf = ffi.cast("char*", ffi.new("char[]", size))
        r.readback["C physical"] = panda.libpanda.panda_physical_memory_read_external(physaddr, buf, size)
        r.readback["C virtual"] = panda.libpanda.panda_virtual_memory_read_external(cpu, vaddr, buf, size)
        for how, fn in (("physical", lambda: panda.physical_memory_read(physaddr, size)),
                        ("virtual", lambda: panda.virtual_memory_read(cpu, vaddr, size))):
            try:
                r.readback[how] = bytes(fn())
            except ValueError as e:
                r.readback[how] = e
    val[0] = SCRIBBLE


@panda.cb_mmio_before_write(name="mbw", enabled=False)
def mbw(cpu, physaddr, vaddr, size, val):
    r = cur
    if r is None or not r.window or not mine(vaddr):
        return
    r.ev.append(("mmio_w", None, vaddr, physaddr, size, val[0]))


def mem_event(kind, pc, addr, size, buf=None):
    r = cur
    if r is None or not r.window or not in_program(pc):
        return
    value = int.from_bytes(bytes(ffi.buffer(buf, size)), "little") if buf is not None else None
    r.ev.append((kind, pc, addr if kind[0] == "v" else None, addr if kind[0] == "p" else None, size, value))


def register_memcbs():
    """The phys/virt memory callbacks, disabled: from run 2's hypercall 0, on the vCPU."""
    panda.cb_virt_mem_before_write(name="vmbw", enabled=False)(
        lambda cpu, pc, addr, size, buf: mem_event("vbw", pc, addr, size, buf))
    panda.cb_virt_mem_after_read(name="vmar", enabled=False)(
        lambda cpu, pc, addr, size, buf: mem_event("var", pc, addr, size, buf))
    panda.cb_virt_mem_after_write(name="vmaw", enabled=False)(
        lambda cpu, pc, addr, size, buf: mem_event("vaw", pc, addr, size, buf))
    panda.cb_phys_mem_before_read(name="pmbr", enabled=False)(
        lambda cpu, pc, addr, size: mem_event("pbr", pc, addr, size))
    panda.cb_phys_mem_before_write(name="pmbw", enabled=False)(
        lambda cpu, pc, addr, size, buf: mem_event("pbw", pc, addr, size, buf))
    panda.cb_phys_mem_after_read(name="pmar", enabled=False)(
        lambda cpu, pc, addr, size, buf: mem_event("par", pc, addr, size, buf))
    panda.cb_phys_mem_after_write(name="pmaw", enabled=False)(
        lambda cpu, pc, addr, size, buf: mem_event("paw", pc, addr, size, buf))


def window_cbs(r):
    return ["mar", "mbw"] + (MEMCBS if r.n == 2 else [])


@panda.hypercall(MAGIC)
def hypercall(cpu):
    r = cur
    op, a, b, pc = hypercall_args(panda, cpu)
    progress(f"run {r.n}: hypercall {op}")
    if op == 0:
        if r.n == 2:
            register_memcbs()
        params = b"".join(v.to_bytes(8, "little") for v in (page, roff, woff, WVAL))
        panda.virtual_memory_write(cpu, a, params)
    elif op == 1:
        r.labels = dict(zip(LABELS, read_quads(panda, cpu, a, len(LABELS))))
        m = r.labels["mapping"]
        r.phys = {m + roff: panda.virt_to_phys(cpu, m + roff), m + woff: panda.virt_to_phys(cpu, m + woff),
                  r.labels["ramword"]: panda.virt_to_phys(cpu, r.labels["ramword"])}
        r.window = True
        for name in window_cbs(r):
            panda.enable_callback(name)
    elif op == 2:
        for name in window_cbs(r):
            panda.disable_callback(name)
        r.window = False
        r.regs = [panda.arch.get_reg(cpu, reg) & 0xffffffff for reg in LOADED[arch]]


@panda.queue_blocking
def drive():
    global cur, page
    panda.revert_sync("root")
    if arch == "x86_64":
        # The std VGA (1234:1111); its BAR 2 is the MMIO one. Unbind its driver, which holds the
        # BAR (CONFIG_IO_STRICT_DEVMEM keeps /dev/mem off a region a driver holds).
        d = panda.run_serial_cmd("for d in /sys/bus/pci/devices/*; do [ \"$(cat $d/vendor) $(cat $d/device)\""
                                 " = '0x1234 0x1111' ] && echo $d; done").strip().split()[-1]
        panda.run_serial_cmd(f"[ -e {d}/driver ] && echo {d.rsplit('/', 1)[1]} > {d}/driver/unbind; sleep 1")
        bar = panda.run_serial_cmd(f"sed -n 3p {d}/resource").split()
        page = int(bar[0], 16)
        progress(f"VGA {d}: MMIO BAR at {page:#x}")
    upload(panda, "mmio", arch)
    for n in (1, 2):
        cur = Run(n)
        runs.append(cur)
        progress(f"run {n}: /tmp/mmio")
        cur.output = panda.run_serial_cmd("/tmp/mmio; echo rc=$?")
        cur = None
    panda.end_analysis()


panda.run()


def analyse(r):
    p = f"[run {r.n}] "
    L = r.labels
    C.check(p + "the program mapped the device and exited 0", "rc=0" in r.output and L, repr(r.output))
    if not L:
        return
    m, ram = L["mapping"], L["ramword"]
    rd, wr = m + roff, m + woff
    C.check(p + "virt_to_phys of the mapping is the device's page", (r.phys[rd], r.phys[wr]) == (page + roff, page + woff),
            {hex(k): hex(v) for k, v in r.phys.items()})
    loaded, ram_loaded = r.regs or (None, None)
    C.check(p + "the program loaded the device's value (the register read is a real device access)",
            loaded == rvalue, hex(loaded or 0))
    C.check(p + "the program loaded RAM's value", ram_loaded == RAMWORD & 0xffffffff, hex(ram_loaded or 0))
    mmio = [e for e in r.ev if e[0].startswith("mmio")]
    want = [("mmio_w", None, wr, page + woff, 4, WVAL), ("mmio_r", None, rd, page + roff, 4, rvalue),
            ("mmio_w", None, wr, page + woff, 4, WVAL)]
    show = [(e[0], hex(e[2]), hex(e[3]), e[4], hex(e[5])) for e in mmio]
    C.check(p + "mmio_before_write: each device store, once, in order: physical and virtual address, size, value",
            [e for e in mmio if e[0] == "mmio_w"] == [w for w in want if w[0] == "mmio_w"], show)
    C.check(p + "mmio_after_read: the device load, once: physical and virtual address, size, value",
            [e for e in mmio if e[0] == "mmio_r"] == [w for w in want if w[0] == "mmio_r"], show)
    C.check(p + "the mmio callbacks fire in program order", mmio == want, show)
    C.check(p + "the mmio callbacks don't fire for RAM", not [e for e in mmio if e[2] == ram], show)
    C.check(p + "*val is a copy: writing it in mmio_after_read doesn't change what the guest loads",
            loaded != SCRIBBLE and loaded == rvalue, hex(loaded or 0))
    rb = r.readback
    C.check(p + "from mmio_after_read, panda_physical/virtual_memory_read of the register fail (no device access)",
            rb.get("C physical") not in (None, 0) and rb.get("C virtual") not in (None, 0),
            {k: repr(v) for k, v in rb.items()})
    C.check(p + "pandare2's physical_memory_read and virtual_memory_read raise ValueError on device memory",
            isinstance(rb.get("physical"), ValueError) and isinstance(rb.get("virtual"), ValueError),
            {k: repr(v) for k, v in rb.items()})
    if r.n != 2:
        return
    # Run 2: the phys/virt callbacks on the program's window accesses.
    want = {
        L["mw1"]: [("pbw", None, page + woff, WVAL), ("vbw", wr, None, WVAL), ("mmio_w", wr, page + woff, WVAL),
                   ("paw", None, page + woff, WVAL), ("vaw", wr, None, WVAL)],
        L["mr"]: [("pbr", None, page + roff, None), ("mmio_r", rd, page + roff, rvalue),
                  ("par", None, page + roff, rvalue), ("var", rd, None, rvalue)],
        L["rr"]: [("pbr", None, r.phys[ram], None), ("par", None, r.phys[ram], RAMWORD & 0xffffffff),
                  ("var", ram, None, RAMWORD & 0xffffffff)],
        L["rw"]: [("pbw", None, r.phys[ram], WVAL), ("vbw", ram, None, WVAL), ("paw", None, r.phys[ram], WVAL),
                  ("vaw", ram, None, WVAL)],
    }
    want[L["mw2"]] = want[L["mw1"]]
    # The mmio events have no pc: each belongs to the program access around it.
    got, last = {}, None
    for e in r.ev:
        if e[1] is not None:
            last = e[1]
        if last in want:
            got.setdefault(last, []).append((e[0], e[2], e[3], e[5]))
    bad = []
    for label in ("mw1", "mr", "mw2", "rr", "rw"):
        g = got.get(L[label], [])
        if sorted(g, key=repr) != sorted(want[L[label]], key=repr):
            bad.append(f"{label}: {[(k, hex(v or 0), hex(pa or 0), hex(val or 0)) for k, v, pa, val in g]}")
            continue
        kinds = [x[0] for x in g]
        if label in ("mw1", "mw2"):
            ok = max(kinds.index("pbw"), kinds.index("vbw")) < kinds.index("mmio_w") < min(
                kinds.index("paw"), kinds.index("vaw"))
        elif label == "mr":
            ok = kinds.index("pbr") < kinds.index("mmio_r") < min(kinds.index("par"), kinds.index("var"))
        else:
            ok = True
        if not ok:
            bad.append(f"{label}: order {kinds}")
    C.check(p + "phys/virt callbacks see the device accesses (device physical address, value) and the RAM ones; "
            "mmio_before_write after the before-write callbacks, mmio_after_read before the after-read ones",
            not bad, bad[:5])


for r in runs:
    analyse(r)
sys.exit(C.report(f"mmio callbacks on {arch}"))
