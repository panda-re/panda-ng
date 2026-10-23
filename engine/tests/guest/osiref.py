"""osiref.py: apis/hostosi.py's walks in Python, on dwarffi live structs over
the kernel's memory: the reference the C walks (system/penguin-osi.c) are
checked against, besides the guest's own /proc.  ~100x slower than C.

The guest kernel's own data structures, read from the host: processes, file
descriptors, memory mappings and the process list, by walking ``task_struct``,
``files_struct``/``fdtable``, ``mm_struct`` (its maple tree, or the VMA list of
older kernels), ``dentry`` and ``mount`` with dwarffi over the kernel's ISF
(``plugins.kffi.isf``), on live guest memory.

No guest code runs: no portal, no kernel module, no guest syscall.  A query
reads kernel memory, which never faults, through QEMU; the pages it reads are
kept for the length of the query, so one query sees one state of the kernel
and reads each page once.  A process's own memory (its arguments and
environment) can need a fault: the current task's is read through
``plugins.mem`` as any callback's read (faulting it in when it must); another
task's through its page tables, without faults (None if a page is out).

The calls are ``plugins.osi``'s, as generators (``yield from``)::

    proc = yield from plugins.HostOSI.get_proc()
    args = yield from plugins.HostOSI.get_args()
    fds = yield from plugins.HostOSI.get_fds()
    maps = yield from plugins.HostOSI.get_mappings()

They answer for the current task: the task of the syscall trap callback that
runs them (``compat/syscall_trap.py`` sets it), or another one by ``pid=``.
The results are plain records (``types.SimpleNamespace``) with the field names
of igloo's ``osi_proc``, ``osi_fd_entry`` and ``osi_module``.  Names follow the
kernel's own (``/proc``): ``d_path`` for files (``socket:[ino]``,
``pipe:[ino]``, ``anon_inode:...``, `` (deleted)``), and ``/proc/<pid>/maps``'
rules for mappings (``[heap]``, ``[stack]``, special mappings such as
``[vdso]``, the gate area).

Supported: 64-bit little-endian kernels (x86_64, aarch64), Linux 4.x-6.x
layouts that the ISF describes.
"""

import struct
from types import SimpleNamespace
from typing import Any, Generator, List, Optional

from dwarffi.backend import MemoryBackend
from dwarffi.dffi import DFFI
from penguin import Plugin, plugins

PAGE = 4096
M64 = (1 << 64) - 1
AT_FDCWD = -100
USER_HZ = 100
MAX_DEPTH = 256         # path components / list nodes walked before giving up
MAX_FDS = 65536

# vm_flags
VM_READ, VM_WRITE, VM_EXEC, VM_MAYSHARE, VM_GROWSDOWN = 0x1, 0x2, 0x4, 0x80, 0x100
# maple tree (include/linux/maple_tree.h)
MAPLE_NODE_MASK = 0xff
MAPLE_LEAF_64, MAPLE_RANGE_64, MAPLE_ARANGE_64 = 1, 2, 3
MM_CONTEXT_HAS_VSYSCALL = 1     # x86: mm->context.flags bit

# a dentry's d_op->d_dname, by the function's name: the name d_path() gives
DNAMES = {
    "sockfs_dname": lambda name, ino: f"socket:[{ino}]",
    "pipefs_dname": lambda name, ino: f"pipe:[{ino}]",
    "anon_inodefs_dname": lambda name, ino: f"anon_inode:{name}",
    "simple_dname": lambda name, ino: f"/{name} (deleted)",
    "dmabuffs_dname": lambda name, ino: f"/dmabuf:{name}",
}


class OSIError(Exception):
    """the guest's state could not be read"""


class KernelMemory(MemoryBackend):
    """The guest kernel's memory for dwarffi: read through QEMU a page at a
    time, and kept until reset() (one query)."""

    def __init__(self, read_page):
        self._read_page = read_page
        self.pages = {}
        self.reads = 0

    def reset(self) -> None:
        self.pages.clear()

    def page(self, base: int) -> bytes:
        pg = self.pages.get(base)
        if pg is None:
            pg = self._read_page(base)
            self.reads += 1
            if pg is None:
                raise OSIError(f"kernel memory at {base:#x} is not mapped")
            self.pages[base] = pg
        return pg

    def read(self, address: int, size: int) -> bytes:
        address &= M64
        off = address & (PAGE - 1)
        if off + size <= PAGE:
            return self.page(address - off)[off:off + size]
        out = []
        while size > 0:
            off = address & (PAGE - 1)
            n = min(size, PAGE - off)
            out.append(self.page(address - off)[off:off + n])
            address += n
            size -= n
        return b"".join(out)

    def write(self, address: int, data: bytes) -> None:
        raise OSIError("HostOSI does not write guest memory")


def _query(f):
    """a public call: a fresh view of kernel memory; OSIError -> None"""
    def run(self, *a, **kw):
        self.mem.reset()
        try:
            return (yield from f(self, *a, **kw))
        except (OSIError, MemoryError) as e:
            self.logger.debug(f"{f.__name__}: {e}")
            return None
    run.__name__ = f.__name__
    run.__doc__ = f.__doc__
    return run


class PyOSI(Plugin):
    """the reference walks, in Python on dwarffi live structs"""

    def __init__(self) -> None:
        self.mem = KernelMemory(self._read_page)
        self.ffi = DFFI(plugins.kffi.isf, backend=self.mem)
        if self.panda.bits != 64 or getattr(self.panda, "endianness", "little") != "little":
            raise OSIError("PyOSI: 64-bit little-endian kernels only, for now")
        self.arch = self.panda.arch_name
        self._q = struct.Struct("<Q")
        self._syms = {}
        self._fnames = {}
        for fn in list(DNAMES) + ["special_mapping_name", "gate_vma_name"]:
            a = self._sym(fn)
            if a:
                self._fnames[a] = fn
        self._maple = self._field("mm_struct", "mm_mt")      # 6.1+: VMAs in a maple tree
        self._off_mnt = self.ffi.offsetof("struct mount", "mnt")
        self._off_tasks = self.ffi.offsetof("struct task_struct", "tasks")
        if self._field("task_struct", "thread_node"):
            self._off_thread = self.ffi.offsetof("struct task_struct", "thread_node")
            self._off_thead = self.ffi.offsetof("struct signal_struct", "thread_head")
        else:
            self._off_thread = self.ffi.offsetof("struct task_struct", "thread_group")
            self._off_thead = None
        self._walk = None
        self._release = None

    # --- memory ---

    def _read_page(self, base: int) -> Optional[bytes]:
        try:
            return plugins.mem.read_bytes_panda(self.panda.get_cpu(), base, PAGE)
        except ValueError:
            return None

    def _u64(self, addr: int) -> int:
        return self._q.unpack(self.mem.read(addr, 8))[0]

    def _cstr(self, addr: int, limit: int = 4096) -> str:
        out = b""
        while addr and len(out) < limit:
            n = PAGE - (addr & (PAGE - 1))
            chunk = self.mem.read(addr, n)
            i = chunk.find(b"\0")
            if i >= 0:
                return (out + chunk[:i]).decode("latin-1")
            out += chunk
            addr += n
        return out.decode("latin-1")

    def _sym(self, name: str) -> int:
        if name not in self._syms:
            s = self.ffi.get_symbol(name)
            self._syms[name] = int(getattr(s, "address", 0) or 0) if s else 0
        return self._syms[name]

    def _field(self, t: str, f: str) -> bool:
        """whether this kernel's struct @t has @f (anonymous members included)"""
        try:
            self.ffi.offsetof(f"struct {t}", f)
            return True
        except Exception:
            return False

    def _s(self, t: str, addr: int):
        if not addr:
            raise OSIError(f"NULL {t}")
        return self.ffi.from_address(f"struct {t}", addr)

    def _list(self, head: int, off: int) -> Generator[int, None, None]:
        """the containers on a list_head at @head, linked at @off"""
        node = self._u64(head)
        for _ in range(MAX_FDS):
            if node == head or not node:
                return
            yield node - off
            node = self._u64(node)
        raise OSIError(f"list at {head:#x} does not end")

    # --- tasks ---

    def _current(self) -> int:
        task = getattr(self.panda._callback_state(), "task", 0)
        if not task:
            raise OSIError("no current task (HostOSI answers in syscall trap callbacks)")
        return task

    def _processes(self) -> Generator[int, None, None]:
        init = self._sym("init_task")
        yield from self._list(init + self._off_tasks, self._off_tasks)

    def _threads(self, leader: int) -> Generator[int, None, None]:
        if self._off_thead is None:
            yield leader
            yield from self._list(leader + self._off_thread, self._off_thread)
            return
        sig = self._s("task_struct", leader).signal.address
        yield from self._list(sig + self._off_thead, self._off_thread)

    def _task(self, pid: Optional[int]) -> int:
        if pid is None:
            return self._current()
        for p in self._processes():
            for t in self._threads(p):
                if self._s("task_struct", t).pid == pid:
                    return t
        raise OSIError(f"no task {pid}")

    def _proc(self, task: int) -> SimpleNamespace:
        t = self._s("task_struct", task)
        cred = t.cred.deref()
        parent = t.real_parent.address
        p = SimpleNamespace(
            taskd=task, pid=t.pid, tgid=t.tgid,
            ppid=self._s("task_struct", parent).tgid if parent else 0,
            name=bytes(t.comm).split(b"\0", 1)[0].decode("latin-1"),
            create_time=t.start_time,
            uid=cred.uid.val, gid=cred.gid.val, euid=cred.euid.val, egid=cred.egid.val,
            pgd=0)
        mm = t.mm.address
        if mm:
            m = self._s("mm_struct", mm)
            p.pgd = m.pgd.address
            for f in ("map_count", "start_brk", "brk", "start_stack", "start_code",
                      "end_code", "start_data", "end_data", "arg_start", "arg_end",
                      "env_start", "env_end", "mmap_base", "task_size"):
                setattr(p, f, getattr(m, f))
        return p

    # --- paths: fs/d_path.c ---

    def _qstr(self, d) -> str:
        q = d.d_name
        return self.mem.read(q.name.address, q.len).decode("latin-1")

    def _d_path(self, mnt: int, dentry: int, root) -> str:
        d = self._s("dentry", dentry)
        mount = mnt - self._off_mnt
        is_root = d.d_parent.address == dentry
        # d_path(): a dentry with its own name (sockets, pipes, ...)
        op = d.d_op.address
        if op and (not is_root or dentry != self._s("vfsmount", mnt).mnt_root.address):
            fn = self._s("dentry_operations", op).d_dname.address
            if fn:
                ino = self._s("inode", d.d_inode.address).i_ino if d.d_inode.address else 0
                make = DNAMES.get(self._fnames.get(fn))
                if make is None:
                    return f"[d_dname {fn:#x}]"
                return make(self._qstr(d), ino)
        # prepend_path(): up the dentries, and the mounts, to the root
        parts, cur = [], dentry
        for _ in range(MAX_DEPTH):
            if cur == root[1] and mnt == root[0]:
                break
            c = self._s("dentry", cur)
            m = self._s("mount", mount)
            mroot = m.mnt.mnt_root.address
            if cur == mroot or c.d_parent.address == cur:
                parent = m.mnt_parent.address
                if cur != mroot or parent == mount:
                    break               # escaped the mount, or the global root
                cur, mount = m.mnt_mountpoint.address, parent
                mnt = mount + self._off_mnt
                continue
            parts.append(self._qstr(c))
            cur = c.d_parent.address
        path = "/" + "/".join(reversed(parts))
        if d.d_hash.pprev.address == 0 and not is_root:
            path += " (deleted)"        # d_unlinked()
        return path

    def _root(self, t) -> tuple:
        fs = t.fs.deref()
        return fs.root.mnt.address, fs.root.dentry.address

    def _file_path(self, file: int, root) -> str:
        p = self._s("file", file).f_path
        return self._d_path(p.mnt.address, p.dentry.address, root)

    # --- user memory of another task: its page tables, by the direct map ---

    def _pt_walker(self):
        if self._walk is None:
            if self.arch == "x86_64":
                base = self._sym("page_offset_base")
                off = self._u64(base) if base else 0xffff888000000000
                l5 = self._sym("__pgtable_l5_enabled")
                levels = 5 if l5 and self.mem.read(l5, 4) != b"\0\0\0\0" else 4
                self._walk = ("x86", off, levels, None)
            else:
                # VA_BITS from where the image is: the top half of the
                # kernel's VA space from 5.4 (the linear map below it), the
                # bottom half before (the linear map above)
                lead = 64 - (~self._sym("init_task") & M64).bit_length()
                flipped = self._kver() >= (5, 4)
                va = self._sym("vabits_actual")
                bits = self._u64(va) if va else (65 - lead if flipped else 64 - lead)
                memstart = self._u64(self._sym("memstart_addr"))
                off = -(1 << (bits if flipped else bits - 1)) & M64
                self._walk = ("arm64", off, 4 if bits > 39 else 3, memstart)
        return self._walk

    def _kver(self) -> tuple:
        """the kernel's release (major, minor), from init_uts_ns"""
        if self._release is None:
            u = self._sym("init_uts_ns")
            rel = self._cstr(u + self.ffi.offsetof("struct uts_namespace", "name", "release"))
            self._release = tuple(int(x) for x in rel.split("-")[0].split(".")[:2])
        return self._release

    def _user_pa(self, pgd: int, va: int) -> Optional[int]:
        kind, off, levels, memstart = self._pt_walker()
        if kind == "x86":
            table = pgd - off
            pa = lambda p: off + p            # noqa: E731
        else:
            table = (pgd & ~off) + memstart
            pa = lambda p: (p - memstart) | off   # noqa: E731
        for lvl in range(levels - 1, -1, -1):
            shift = 12 + 9 * lvl
            e = self._u64(pa(table) + ((va >> shift) & 511) * 8)
            if kind == "x86":
                if not e & 1:
                    return None
                frame = e & 0x000ffffffffff000
                if lvl in (1, 2) and e & 0x80:      # 1G / 2M page
                    return (frame & ~((1 << shift) - 1)) + (va & ((1 << shift) - 1))
            else:
                if not e & 1:
                    return None
                frame = e & 0x0000fffffffff000
                if lvl and not e & 2:               # a block
                    return (frame & ~((1 << shift) - 1)) + (va & ((1 << shift) - 1))
            table = frame
        return table + (va & (PAGE - 1))

    def _user_read(self, pgd: int, va: int, n: int) -> Optional[bytes]:
        kind, off, _, memstart = self._pt_walker()
        out = []
        while n > 0:
            k = min(n, PAGE - (va & (PAGE - 1)))
            p = self._user_pa(pgd, va)
            if p is None:
                return None
            out.append(self.mem.read(off + p if kind == "x86" else (p - memstart) | off, k))
            va += k
            n -= k
        return b"".join(out)

    def _area(self, pid: Optional[int], which: str) -> Generator[Any, Any, Optional[bytes]]:
        task = self._task(pid)
        t = self._s("task_struct", task)
        m = self._s("mm_struct", t.mm.address)
        lo, hi = getattr(m, f"{which}_start"), getattr(m, f"{which}_end")
        if not lo or hi <= lo:
            return None
        n = min(hi - lo, 64 * 1024)
        if task == getattr(self.panda._callback_state(), "task", 0):
            data = yield from plugins.mem.read_bytes(lo, n)
            return data
        return self._user_read(m.pgd.address, lo, n)

    # --- mappings ---

    def _maple_walk(self, enc: int, lo: int, hi: int, out: list, depth: int = 0) -> None:
        """the entries of a maple tree node (lib/maple_tree.c ma_data_end)"""
        if depth > 16:
            raise OSIError("maple tree too deep")
        node = enc & ~MAPLE_NODE_MASK
        typ = (enc >> 3) & 0xf
        n = self._s("maple_node", node)
        if typ == MAPLE_ARANGE_64:
            r = n.ma64
            pivots, slots = list(r.pivot), [s.address for s in r.slot]
            end = r.meta.end
        elif typ in (MAPLE_LEAF_64, MAPLE_RANGE_64):
            r = n.mr64
            pivots = list(r.pivot)
            slots = [s.address for s in r.slot]
            last = len(pivots) - 1
            if not pivots[last]:
                end = r.meta.end
            elif pivots[last] == hi:
                end = last
            else:
                end = len(pivots)
        else:
            raise OSIError(f"maple node type {typ}")
        smin = lo
        for i in range(end + 1):
            smax = pivots[i] if i < len(pivots) and pivots[i] else hi
            s = slots[i]
            if s:
                if typ == MAPLE_LEAF_64:
                    out.append(s)
                else:
                    self._maple_walk(s, smin, smax, out, depth + 1)
            smin = smax + 1

    def _vmas(self, m) -> List[int]:
        out = []
        if self._maple:
            root = m.mm_mt.ma_root.address
            if root & 3 == 2 and root > 4096:       # xa_is_node()
                self._maple_walk(root, 0, M64, out)
            elif root:
                out.append(root)
        else:
            v = m.mmap.address
            for _ in range(MAX_FDS):
                if not v:
                    break
                out.append(v)
                v = self._s("vm_area_struct", v).vm_next.address
        return out

    def _vma(self, v: int, m, root, prev=None) -> SimpleNamespace:
        """one mapping, named as fs/proc/task_mmu.c get_vma_name() names it"""
        a = self._s("vm_area_struct", v)
        flags = a.vm_flags
        start = a.vm_start
        # before the stack guard gap (4.12, and stable): the guard page hidden
        if (not self._sym("stack_guard_gap") and flags & VM_GROWSDOWN and
                not (prev and prev.vm_end == start and prev.vm_flags & VM_GROWSDOWN)):
            start += PAGE
        # vma_is_initial_heap() from 6.6; inclusive before
        strict = self._kver() >= (6, 6)
        file = a.vm_file.address
        mm = a.vm_mm.address
        name = ""
        if file:
            name = self._file_path(file, root)
        else:
            ops = a.vm_ops.address
            fn = self._s("vm_operations_struct", ops).name.address if ops else 0
            kind = self._fnames.get(fn)
            if kind == "special_mapping_name":
                sm = self._s("vm_special_mapping", a.vm_private_data.address)
                name = self._cstr(sm.name.address)
            elif kind == "gate_vma_name":
                name = "[vsyscall]"
            elif fn:
                name = f"[vm_ops {fn:#x}]"
            elif not mm:
                name = "[vdso]"
            elif (a.vm_start < m.brk and a.vm_end > m.start_brk if strict else
                  a.vm_start <= m.brk and a.vm_end >= m.start_brk):
                name = "[heap]"
            elif a.vm_start <= m.start_stack <= a.vm_end:
                name = "[stack]"
        perms = "".join((
            "r" if flags & VM_READ else "-", "w" if flags & VM_WRITE else "-",
            "x" if flags & VM_EXEC else "-", "s" if flags & VM_MAYSHARE else "p"))
        return SimpleNamespace(base=start, size=a.vm_end - start,
                               offset=(a.vm_pgoff << 12) if file else 0, flags=flags,
                               perms=perms, name=name, file=file)

    # --- the calls (plugins.osi's) ---

    @_query
    def get_proc(self, pid: Optional[int] = None) -> Generator[Any, Any, Optional[SimpleNamespace]]:
        """the task's osi_proc fields"""
        return self._proc(self._task(pid))
        yield

    @_query
    def get_all_procs(self) -> Generator[Any, Any, Optional[List[SimpleNamespace]]]:
        """every process (thread group leader), init's tasks list"""
        return [self._proc(p) for p in self._processes()]
        yield

    @_query
    def get_args(self, pid: Optional[int] = None) -> Generator[Any, Any, Optional[List[str]]]:
        """the argument vector, from mm->arg_start..arg_end"""
        data = yield from self._area(pid, "arg")
        if data is None:
            return None
        return data.rstrip(b"\0").decode("latin-1").split("\0")

    @_query
    def get_env(self, pid: Optional[int] = None) -> Generator[Any, Any, Optional[dict]]:
        """the environment, from mm->env_start..env_end"""
        data = yield from self._area(pid, "env")
        if data is None:
            return None
        env = {}
        for kv in data.rstrip(b"\0").decode("latin-1").split("\0"):
            k, _, v = kv.partition("=")
            env[k] = v
        return env

    def get_proc_name(self, pid: Optional[int] = None) -> Generator[Any, Any, Optional[str]]:
        """argv[0]'s base name, or the comm"""
        args = yield from self.get_args(pid)
        if args and args[0]:
            return args[0].rsplit("/", 1)[-1]
        p = yield from self.get_proc(pid)
        return p.name if p else None

    @_query
    def get_fds(self, pid: Optional[int] = None, start_fd: int = 0,
                count: Optional[int] = None) -> Generator[Any, Any, Optional[List[SimpleNamespace]]]:
        """open files: fd and d_path() name, from the task's fdtable"""
        t = self._s("task_struct", self._task(pid))
        root = self._root(t)
        if start_fd == AT_FDCWD:
            pwd = t.fs.deref().pwd
            return [SimpleNamespace(fd=AT_FDCWD, name=self._d_path(
                pwd.mnt.address, pwd.dentry.address, root))]
        fdt = t.files.deref().fdt.deref()
        n = min(fdt.max_fds, MAX_FDS)
        files = struct.unpack(f"<{n}Q", self.mem.read(fdt.fd.address, 8 * n))
        out = []
        for fd in range(max(start_fd, 0), n):
            if files[fd]:
                out.append(SimpleNamespace(fd=fd, file=files[fd],
                                           name=self._file_path(files[fd], root)))
                if count and len(out) >= count:
                    break
        return out
        yield

    def get_fd_name(self, fd: int, pid: Optional[int] = None) -> Generator[Any, Any, Optional[str]]:
        fds = yield from self.get_fds(pid, start_fd=fd, count=1)
        if fds and fds[0].fd == fd:
            return fds[0].name
        return None

    def get_cwd(self, pid: Optional[int] = None) -> Generator[Any, Any, Optional[str]]:
        return (yield from self.get_fd_name(AT_FDCWD, pid))

    @_query
    def get_proc_exe(self, pid: Optional[int] = None) -> Generator[Any, Any, Optional[str]]:
        """mm->exe_file's path"""
        t = self._s("task_struct", self._task(pid))
        exe = self._s("mm_struct", t.mm.address).exe_file.address
        return self._file_path(exe, self._root(t)) if exe else None
        yield

    @_query
    def get_mappings(self, pid: Optional[int] = None) -> Generator[Any, Any, Optional[List[SimpleNamespace]]]:
        """the mappings, in address order, as /proc/<pid>/maps lists them"""
        t = self._s("task_struct", self._task(pid))
        m = self._s("mm_struct", t.mm.address)
        root = self._root(t)
        vs = self._vmas(m)
        maps = [self._vma(v, m, root, self._s("vm_area_struct", vs[i - 1]) if i else None)
                for i, v in enumerate(vs)]
        gate = self._sym("gate_vma")
        if gate and self.arch == "x86_64" and \
                m.context.flags & (1 << MM_CONTEXT_HAS_VSYSCALL):
            maps.append(self._vma(gate, m, root))
        return maps
        yield
