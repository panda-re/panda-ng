"""Machines built in Python: `MachineBuilder` describes a machine (CPU, memory map, peripherals,
interrupt wiring, entry point) and lowers it to the engine's built-in `configurable` machine,
whose list-valued properties are QAPI types (qapi/panda-machine.json in qemu_builder):

    -M configurable,ram-base=0x40000000,entry=0x40000000,
       region.0.name=boot,region.0.base=0x0,region.0.size=0x1000,region.0.kind=rom,region.0.file=boot.bin,
       device.0.type=pl011,device.0.id=uart0,device.0.mmio.0=0x9000000,device.0.irq.0=cpu:0,
       device.0.prop.chardev=serial0
    -cpu cortex-a53 -m 16M

Pass it to `Panda(arch=..., machine=builder)`. Or name an external board shared object
(`MachineBuilder.board(path, name)`, which becomes `-M path:name`), and load device modules with
`MachineBuilder.module(path)` (`-module path`).

No JSON: the builder writes QEMU's own keyval syntax. It checks what it can (overlapping or
duplicate regions, unknown ids, alias targets) and raises ValueError before the engine starts;
the engine checks the rest (QOM types, MMIO/IRQ counts) and exits with an error naming the element.
"""
import os
import re

_SUFFIX = {"": 1, "K": 1 << 10, "M": 1 << 20, "G": 1 << 30, "T": 1 << 40}


def parse_size(size):
    """An int, or a string like "64M", "0x1000", "4k"."""
    if isinstance(size, int):
        return size
    m = re.fullmatch(r"\s*(0[xX][0-9a-fA-F]+|\d+)\s*([kKmMgGtT]?)[bB]?\s*", str(size))
    if not m:
        raise ValueError(f"not a size: {size!r}")
    return int(m.group(1), 0) * _SUFFIX[m.group(2).upper()]


def _kv(value):
    """A keyval value: commas doubled, booleans on/off, ints in hex."""
    if isinstance(value, bool):
        value = "on" if value else "off"
    elif isinstance(value, int):
        value = hex(value)
    return str(value).replace(",", ",,")


class MachineBuilder:
    """Builds a machine for the engine's `configurable` machine type. Methods chain."""

    def __init__(self, arch=None):
        self.arch = arch
        self._cpu = None
        self._ram = None            # (base, size)
        self._regions = []          # dicts in the QAPI schema's member names
        self._devices = []
        self._entry = None
        self._kernel = None
        self._board = None          # (path, name)
        self._modules = []
        self._props = {}

    # --- CPU, RAM, entry ---------------------------------------------------------------
    def cpu(self, cpu_type):
        """The CPU type (-cpu), e.g. "cortex-a53", "cortex-a15", "rv64"."""
        self._cpu = cpu_type
        return self

    def ram(self, base, size):
        """RAM at `base`: -m `size` mapped at ram-base. Without it there is no -m RAM (Panda
        passes -m 0). `size` is a multiple of 8 KiB (QEMU rounds -m up to that)."""
        size = parse_size(size)
        if size <= 0 or size % 0x2000:
            raise ValueError(f"ram size {size:#x}: must be a positive multiple of 8 KiB "
                             "(use region() for other sizes)")
        self._ram = (int(base), size)
        return self

    def entry(self, addr):
        """PC after every reset (ARM Thumb: set bit 0). Default: -kernel's ELF entry."""
        self._entry = int(addr)
        return self

    def kernel(self, path):
        """-kernel: an ELF (loaded, its entry used unless entry() is set), or a raw image
        loaded at the RAM base."""
        self._kernel = os.path.abspath(os.fspath(path))
        return self

    # --- memory map --------------------------------------------------------------------
    def region(self, name, base, size, kind="ram", file=None, file_offset=None, file_size=None,
               priority=None):
        """A region of the memory map: kind "ram", "rom" or "unimp" (reads 0, writes ignored,
        accesses logged with -d unimp). `file` loads (a slice of) a file into it."""
        if kind not in ("ram", "rom", "unimp"):
            raise ValueError(f"region {name}: kind must be ram, rom or unimp, not {kind!r}")
        r = {"name": name, "base": int(base), "size": parse_size(size), "kind": kind}
        if file is not None:
            r["file"] = os.path.abspath(os.fspath(file))
        if file_offset is not None:
            r["file-offset"] = int(file_offset)
        if file_size is not None:
            r["file-size"] = parse_size(file_size)
        if priority is not None:
            r["priority"] = int(priority)
        self._regions.append(r)
        return self

    def rom(self, name, base, size, file=None, **kw):
        return self.region(name, base, size, kind="rom", file=file, **kw)

    def rom_file(self, base, path, size=None, name=None, offset=None):
        """A ROM holding `path`'s contents (from `offset`); `size` defaults to the file's,
        rounded up to 4 KiB."""
        if size is None:
            n = os.path.getsize(path) - (offset or 0)
            size = max(0x1000, (n + 0xfff) & ~0xfff)
        name = name or os.path.basename(os.fspath(path))
        return self.region(name, base, size, kind="rom", file=path, file_offset=offset)

    def ram_file(self, base, path, size=None, name=None, offset=None):
        """Like rom_file(), writable."""
        if size is None:
            n = os.path.getsize(path) - (offset or 0)
            size = max(0x1000, (n + 0xfff) & ~0xfff)
        name = name or os.path.basename(os.fspath(path))
        return self.region(name, base, size, kind="ram", file=path, file_offset=offset)

    def unimp(self, name, base, size, priority=None):
        """An unimplemented peripheral (Avatar's unmapped-but-tolerated range)."""
        return self.region(name, base, size, kind="unimp", priority=priority)

    def alias(self, name, base, size, of, offset=0, priority=None):
        """Region `of`'s contents (from `offset`) again at `base`."""
        r = {"name": name, "base": int(base), "size": parse_size(size), "alias-of": of}
        if offset:
            r["alias-offset"] = int(offset)
        if priority is not None:
            r["priority"] = int(priority)
        self._regions.append(r)
        return self

    # --- devices -----------------------------------------------------------------------
    def device(self, type, mmio=None, id=None, irq=None, **props):
        """A peripheral by QOM type. `mmio`: base address, or a list (one per MMIO region of the
        device, in order). `irq`: a list, one per interrupt output: "cpu:N", "<id>:N" or None.
        Other keyword arguments are device properties (use **{"num-irq": 64} for dashes), e.g.
        chardev="serial0" for the first -serial."""
        d = {"type": type}
        if id is not None:
            d["id"] = id
        if mmio is not None:
            d["mmio"] = [int(m) for m in (mmio if isinstance(mmio, (list, tuple)) else [mmio])]
        if irq is not None:
            d["irq"] = ["" if i is None else str(i) for i in
                        (irq if isinstance(irq, (list, tuple)) else [irq])]
        if props:
            d["prop"] = dict(props)
        self._devices.append(d)
        return self

    # --- external boards and modules ----------------------------------------------------
    def board(self, path, name=None, **props):
        """Use the machine type `name` from the board shared object at `path` (-M path:name)
        instead of the configurable machine; `props` are that machine's properties."""
        self._board = (os.path.abspath(os.fspath(path)), name)
        self._props.update(props)
        return self

    def module(self, path):
        """Load a panda module (devices, ...) before the machine: -module path."""
        self._modules.append(os.path.abspath(os.fspath(path)))
        return self

    def machine_prop(self, name, value):
        """Any other machine property (-M ...,name=value)."""
        self._props[name] = value
        return self

    # --- lowering ----------------------------------------------------------------------
    @property
    def mem(self):
        """The -m value, from ram(), or None."""
        return f"{self._ram[1] >> 10}K" if self._ram else None

    @property
    def is_board(self):
        """True for an external board (board()), which maps -m RAM itself."""
        return self._board is not None

    def validate(self):
        """Raise ValueError for what can be checked without the engine."""
        if self._board:
            if self._regions or self._devices:
                raise ValueError("an external board has its own memory map and devices")
            return self
        names = [r["name"] for r in self._regions]
        dup = {n for n in names if names.count(n) > 1}
        if dup:
            raise ValueError(f"region names used twice: {sorted(dup)}")
        ids = [d["id"] for d in self._devices if "id" in d]
        dup = {i for i in ids if ids.count(i) > 1}
        if dup:
            raise ValueError(f"device ids used twice: {sorted(dup)}")
        spans = []
        if self._ram:
            spans.append(("RAM", self._ram[0], self._ram[1], 0))
        for r in self._regions:
            if r["size"] <= 0:
                raise ValueError(f"region {r['name']}: size must be positive")
            if "alias-of" in r:
                t = next((x for x in self._regions if x["name"] == r["alias-of"]), None)
                if t is None or "alias-of" in t:
                    raise ValueError(f"region {r['name']}: alias-of {r['alias-of']!r} is not a "
                                     "region (or is itself an alias)")
                if r.get("alias-offset", 0) + r["size"] > t["size"]:
                    raise ValueError(f"region {r['name']}: runs past the end of {t['name']}")
            spans.append((f"region {r['name']}", r["base"], r["size"], r.get("priority", 0)))
        for i, (a, ab, asz, ap) in enumerate(spans):
            for b, bb, bsz, bp in spans[i + 1:]:
                if ap == bp and ab < bb + bsz and bb < ab + asz:
                    raise ValueError(f"{a} [{ab:#x}, {ab + asz:#x}) overlaps {b} "
                                     f"[{bb:#x}, {bb + bsz:#x})")
        for n, d in enumerate(self._devices):
            for irq in d.get("irq", []):
                if not irq:
                    continue
                target, _, num = irq.rpartition(":")
                if not target or not num.isdigit() and not num.lower().startswith("0x"):
                    raise ValueError(f"device[{n}] ({d['type']}): irq {irq!r} is not TARGET:N")
                if target != "cpu" and target not in ids:
                    raise ValueError(f"device[{n}] ({d['type']}): irq target {target!r} is "
                                     "neither 'cpu' nor a device id")
        return self

    def machine_arg(self):
        """The -M value."""
        self.validate()
        parts = []
        if self._board:
            path, name = self._board
            parts.append(_kv(f"{path}:{name}" if name else path))
        else:
            parts.append("configurable")
            if self._ram:
                parts.append(f"ram-base={_kv(self._ram[0])}")
            if self._entry is not None:
                parts.append(f"entry={_kv(self._entry)}")
            for i, r in enumerate(self._regions):
                parts += [f"region.{i}.{k}={_kv(v)}" for k, v in r.items()]
            for i, d in enumerate(self._devices):
                for k, v in d.items():
                    if k in ("mmio", "irq"):
                        parts += [f"device.{i}.{k}.{j}={_kv(x)}" for j, x in enumerate(v)]
                    elif k == "prop":
                        parts += [f"device.{i}.prop.{p}={_kv(x)}" for p, x in v.items()]
                    else:
                        parts.append(f"device.{i}.{k}={_kv(v)}")
        parts += [f"{k}={_kv(v)}" for k, v in self._props.items()]
        return ",".join(parts)

    def args(self):
        """The engine arguments: -module ..., -M ..., -cpu, -kernel (not -m: Panda passes it)."""
        a = []
        for m in self._modules:
            a += ["-module", m]
        a += ["-M", self.machine_arg()]
        if self._cpu:
            a += ["-cpu", self._cpu]
        if self._kernel:
            a += ["-kernel", self._kernel]
        return a

    def __repr__(self):
        return f"MachineBuilder({' '.join(self.args())})"
