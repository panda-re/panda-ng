Plugin: osi_isf
===========

Summary
-------

Linux OSI (the `osi` plugin's interface) from a Volatility 3 ISF of the guest kernel: the
process list, the current process and thread, memory mappings with file paths, and kernel
modules. Beyond that interface, the plugin gives a process's facts (its threads and address
space, arguments and environment, executable, cwd and root, credentials, open files with their
sockets and pipes, and memory map with permissions) and, with `lifecycle=true`, events for
process and thread creation, exec, exit, reparenting and pid reuse. The ISF can come from the kernel's DWARF (dwarf2json, as `pandare2.mkimage` makes),
from BTF (btf2json, with symbols from a System.map or kallsyms file), or from kernel-lift for a
stripped kernel, whose members carry their provenance.

What the plugin can answer is decided when it loads, before it reads guest memory. Each
capability has one or more strategies, each tied to a range of kernel versions and, where it
matters, to architectures, and each declaring the symbols, struct members (with their shape and
width), sizes and enum constants it reads. For each capability the plugin takes the first
strategy for the kernel's version whose inputs all resolve, have the declared shape and are
trusted. The plugin then reads only the inputs of chosen strategies. A capability without one
answers "not available" and the others keep working.

The kernel version comes from the ISF's `linux_banner`, from `version=`, or, failing both, from
the guest's banner once it is readable (until then, strategies are chosen by what the ISF has).

Arguments
---------

* `isf`: string, required. The ISF (`.json` or `.json.xz`).
* `symbols`: string. A System.map or `/proc/kallsyms` file: symbols the ISF lacks (a btf2json
  ISF has none), or overrides of the ISF's.
* `provenance`: string. A kernel-lift ISF's `provenance.json`. Default: the file the ISF's
  metadata names, next to the ISF.
* `allow_untrusted`: bool. Use a kernel-lift ISF's reference-carried and unverified members
  (and its record sizes and enum constants, which kernel-lift doesn't verify). Default: false,
  trusted members only: target-verified, dynamically verified or inferred, and not in the ISF's
  `unverified_needed`.
* `thread_size`: THREAD_SIZE, for the current task from the kernel stack (32-bit arm before
  5.18, arm64 before 4.10, mips) when the ISF's (`union thread_union`, or the `init_stack`
  array) is absent or, in a kernel-lift ISF, the reference's. kernel-lift's modabi reads it from
  the kernel's own code.
* `version`: string. The kernel version (`4.10.0`), for an ISF without `linux_banner`.
* `report`: string. Write the capability report (JSON) to this file at load.
* `lifecycle`: bool. Report lifecycle events (`on_osi_isf_event`, below). Default: false; the
  events cost a hook on `do_exit` and the returns of fork, vfork, clone, clone3 and execve.

`pandare2` passes a `generic=` image's ISF as `isf` unless the arguments name one.

Pairing and KASLR
-----------------

Before it answers anything, the plugin checks that the ISF describes the running kernel:
`linux_banner` must read as the ISF's banner (or, without one, as `Linux version` and the
version), and `init_task`'s task list must link back to it and name it `swapper`. When the
kernel isn't at the ISF's addresses, the plugin looks for the KASLR slide from the kernel's pc
(the image's text, `_stext` to `_etext`, must hold it) at page steps, and applies it to every
kernel symbol (x86_64's per-CPU offsets aside). The check runs at address-space changes and OSI
calls until it is decided; a kernel the ISF doesn't describe gets no answers, with the reason
in the log and the report. `nokaslr` is not needed.

Capabilities and strategies
---------------------------

| capability | what it gives |
|---|---|
| `tasks` | `get_processes`, `get_process_handles` |
| `pid` | process and thread ids (`OsiProc.pid`, `get_current_thread`, `get_process_pid`) |
| `name` | `OsiProc.name` |
| `ppid` | `OsiProc.ppid`, `get_process_ppid` |
| `thread_group` | a thread's process is its group leader (else the thread itself) |
| `asid` | `OsiProc.asid`: the physical address of the page directory |
| `start_time` | `OsiProc.create_time`, in nanoseconds since boot |
| `current` | `get_current_process`, `get_current_process_handle`, `get_current_thread` |
| `mappings` | `get_mappings`, `get_mapping_by_addr`, unknown mappings |
| `mapping_files` | file-backed mappings with their paths, `get_mapping_base_address_by_name`, `has_mapping_prefix` |
| `mount_paths` | file paths from the root of the mount tree (else from the file's own filesystem) |
| `heap_stack` | `[heap]` and `[stack]`, `get_proc_mem` |
| `modules` | `get_modules` |
| `task_change` | `on_task_change`, from a hook on `finish_task_switch` and at exec |
| `threads` | a process's threads (`osi_isf_facts`: `threads`) and their address space (`mm`) |
| `file_paths` | a file's path within its filesystem (for mappings, exe, cwd, root and fds) |
| `lifecycle` | `FORK`, `THREAD` and `EXEC` events (`lifecycle=true`) |
| `exit_codes` | `EXIT` events with the wait status |
| `reparent` | `REPARENT` events |
| `pid_reuse` | `PID_REUSE` events |
| `cmdline` | `cmdline` and `environ` |
| `exe_path` | `exe` |
| `fs_paths` | `cwd` and `root` |
| `creds` | `creds`: uid, gid, suid, sgid, euid, egid, fsuid, fsgid |
| `cap_sets` | `caps`: the effective, permitted, inheritable and bounding sets |
| `open_files` | `files`: each fd's path, kind, inode, flags and offset |
| `sockets` | `files[].socket`: family, type, state, local and remote address, unix path |
| `vma_details` | `vmas`: each mapping's permissions, file offset, file and inode |

A capability that isn't available answers NULL (an empty result), -1 for a pid or parent pid,
and 0 for an asid or start time. The first call to one logs why it isn't available.

The strategies (`osi_isf_check --strategies` prints this table):

| capability | strategy | kernels | arches | reads |
|---|---|---|---|---|
| tasks | init_task.tasks | any | all | `init_task`, `task_struct.tasks` (struct list_head) |
| pid | task_struct.pid/tgid | any | all | `task_struct.pid` (4-byte integer), `task_struct.tgid` (4-byte integer) |
| name | task_struct.comm | any | all | `task_struct.comm` (char array) |
| ppid | real_parent->tgid | any | all | `task_struct.real_parent` (pointer), `task_struct.tgid` (4-byte integer) |
| thread_group | group_leader | any | all | `task_struct.group_leader` (pointer) |
| asid | mm->pgd | any | all | `task_struct.mm` (pointer), `mm_struct.pgd` (pointer) |
| start_time | u64 nanoseconds | 3.17.0+ | all | `task_struct.start_time` (8-byte integer) |
| start_time | struct timespec | 2.6.0-<3.17.0 | all | `task_struct.start_time` (struct timespec), `timespec.tv_sec` (pointer-sized integer), `timespec.tv_nsec` (pointer-sized integer) |
| threads | signal->thread_head | 3.14.0+ | all | `task_struct.signal` (pointer), `signal_struct.thread_head` (struct list_head), `task_struct.thread_node` (struct list_head), `task_struct.mm` (pointer), capability pid, capability thread_group |
| threads | thread_group ring | 2.6.0-<6.7.0 | all | `task_struct.thread_group` (struct list_head), `task_struct.mm` (pointer), capability pid, capability thread_group |
| current | pcpu_hot.current_task | 6.2.0+ | x86_64, i386 | `pcpu_hot`, `pcpu_hot.current_task` (pointer), `__per_cpu_offset` (optional) |
| current | current_task | 2.6.30-<6.2.0 | x86_64, i386 | `current_task\|per_cpu__current_task`, `__per_cpu_offset` (optional) |
| current | __entry_task | 4.10.0+ | aarch64 | `__entry_task`, `__per_cpu_offset` (optional) |
| current | sp_el1 thread_info | 3.7.0-<4.10.0 | aarch64 | `thread_info.task` (pointer), `THREAD_SIZE` |
| current | __entry_task | 5.18.0+ | arm | `__entry_task`, `__per_cpu_offset` (optional) |
| current | __current | 5.18.0+ | arm | `__current` |
| current | svc sp thread_info | 2.6.0-<5.18.0 | arm | `thread_info.task` (pointer), `THREAD_SIZE` |
| current | kernelsp thread_info | 2.6.0+ | mips | `kernelsp`, `thread_info.task` (pointer), `THREAD_SIZE` |
| current | tp/sscratch | 4.15.0+ | riscv | registers only |
| mappings | maple tree | 6.1.0+ | all | `task_struct.mm` (pointer), `mm_struct.mm_mt` (struct maple_tree), `maple_tree.ma_root` (pointer), `maple_range_64.pivot` (pointer-sized element array), `maple_range_64.slot` (pointer-sized element array), `maple_arange_64.pivot` (pointer-sized element array), `maple_arange_64.slot` (pointer-sized element array), `vm_area_struct.vm_start` (pointer-sized integer), `vm_area_struct.vm_end` (pointer-sized integer), `vm_area_struct.vm_mm` (pointer), `vm_area_struct.vm_flags` (pointer-sized integer) (optional) |
| mappings | mmap list | 2.6.0-<6.1.0 | all | `task_struct.mm` (pointer), `mm_struct.mmap` (pointer), `vm_area_struct.vm_next` (pointer), `vm_area_struct.vm_start` (pointer-sized integer), `vm_area_struct.vm_end` (pointer-sized integer), `vm_area_struct.vm_mm` (pointer), `vm_area_struct.vm_flags` (pointer-sized integer) (optional) |
| file_paths | f_path.dentry | 2.6.20+ | all | `file.f_path.dentry` (pointer), `dentry.d_parent` (pointer), `dentry.d_name.name` (pointer) |
| file_paths | f_dentry | 2.6.0-<2.6.20 | all | `file.f_dentry` (pointer), `dentry.d_parent` (pointer), `dentry.d_name.name` (pointer) |
| mapping_files | vm_file | any | all | `vm_area_struct.vm_file` (pointer), `vm_area_struct.vm_pgoff` (pointer-sized integer) (optional), capability mappings, capability file_paths |
| mount_paths | struct mount | 3.3.0+ | all | `file.f_path.mnt` (pointer), `mount.mnt` (struct vfsmount), `mount.mnt_parent` (pointer), `mount.mnt_mountpoint` (pointer), `vfsmount.mnt_root` (pointer), capability file_paths |
| mount_paths | vfsmount | 2.6.20-<3.3.0 | all | `file.f_path.mnt` (pointer), `vfsmount.mnt_parent` (pointer), `vfsmount.mnt_mountpoint` (pointer), `vfsmount.mnt_root` (pointer), capability file_paths |
| heap_stack | mm_struct brk/start_stack | any | all | `task_struct.mm` (pointer), `mm_struct.start_brk` (pointer-sized integer), `mm_struct.brk` (pointer-sized integer), `mm_struct.start_stack` (pointer-sized integer) |
| modules | mem[MOD_TEXT] | 6.4.0+ | all | `modules`, `module.list` (struct list_head), `module.name` (char array), `module.mem` (element array), `sizeof(module_memory)`, `module_memory.base` (pointer), `module_memory.size` (4-byte integer), `mod_mem_type.MOD_TEXT` |
| modules | core_layout | 4.5.0-<6.4.0 | all | `modules`, `module.list` (struct list_head), `module.name` (char array), `module.core_layout.base` (pointer), `module.core_layout.size` (4-byte integer) |
| modules | module_core | 2.6.0-<4.5.0 | all | `modules`, `module.list` (struct list_head), `module.name` (char array), `module.module_core` (pointer), `module.core_size` (4-byte integer) |
| task_change | finish_task_switch | any | all | `finish_task_switch*`, capability current |
| lifecycle | system calls | any | all | `task_struct.real_parent` (pointer), capability tasks, capability pid, capability current, capability ppid, capability thread_group |
| exit_codes | do_exit hook | any | all | `do_exit`, `task_struct.signal` (pointer), `signal_struct.live.counter` (4-byte integer) (optional), capability lifecycle |
| exit_codes | exit system calls | any | all | capability lifecycle |
| reparent | children at the parent's exit | any | all | `task_struct.children` (struct list_head) (optional), `task_struct.sibling` (struct list_head) (optional), capability exit_codes, capability tasks, capability ppid, capability task_change |
| pid_reuse | (pid, start_time) | any | all | capability lifecycle, capability start_time |
| cmdline | mm arg/env ranges | any | all | `task_struct.mm` (pointer), `mm_struct.arg_start` (pointer-sized integer), `mm_struct.arg_end` (pointer-sized integer), `mm_struct.env_start` (pointer-sized integer), `mm_struct.env_end` (pointer-sized integer), capability pid, capability current |
| exe_path | mm->exe_file | 2.6.26+ | all | `task_struct.mm` (pointer), `mm_struct.exe_file` (pointer), capability file_paths |
| fs_paths | fs_struct paths | 2.6.25+ | all | `task_struct.fs` (pointer), `fs_struct.root` (struct path), `fs_struct.pwd` (struct path), `path.mnt` (pointer), `path.dentry` (pointer), `fs_struct.seq.sequence\|seq.seqcount.sequence` (4-byte integer) (optional), capability file_paths |
| fs_paths | fs_struct dentries | 2.6.0-<2.6.25 | all | `task_struct.fs` (pointer), `fs_struct.root` (pointer), `fs_struct.pwd` (pointer), `fs_struct.rootmnt` (pointer), `fs_struct.pwdmnt` (pointer), capability file_paths |
| creds | task->cred | 2.6.29+ | all | `task_struct.cred` (pointer), `cred.uid.val\|uid` (4-byte integer), `cred.gid.val\|gid` (4-byte integer), `cred.suid.val\|suid` (4-byte integer), `cred.sgid.val\|sgid` (4-byte integer), `cred.euid.val\|euid` (4-byte integer), `cred.egid.val\|egid` (4-byte integer), `cred.fsuid.val\|fsuid` (4-byte integer), `cred.fsgid.val\|fsgid` (4-byte integer) |
| creds | task_struct ids | 2.6.0-<2.6.29 | all | `task_struct.uid` (4-byte integer), `task_struct.gid` (4-byte integer), `task_struct.suid` (4-byte integer), `task_struct.sgid` (4-byte integer), `task_struct.euid` (4-byte integer), `task_struct.egid` (4-byte integer), `task_struct.fsuid` (4-byte integer), `task_struct.fsgid` (4-byte integer) |
| cap_sets | cred u64 | 6.3.0+ | all | `task_struct.cred` (pointer), `cred.cap_effective.val` (8-byte integer), `cred.cap_permitted.val` (8-byte integer), `cred.cap_inheritable.val` (8-byte integer), `cred.cap_bset.val` (8-byte integer) (optional) |
| cap_sets | cred u32[] | 2.6.29-<6.3.0 | all | `task_struct.cred` (pointer), `cred.cap_effective.cap` (4-byte element array), `cred.cap_permitted.cap` (4-byte element array), `cred.cap_inheritable.cap` (4-byte element array), `cred.cap_bset.cap` (4-byte element array) (optional) |
| cap_sets | task_struct u32[] | 2.6.25-<2.6.29 | all | `task_struct.cap_effective.cap` (4-byte element array), `task_struct.cap_permitted.cap` (4-byte element array), `task_struct.cap_inheritable.cap` (4-byte element array) |
| cap_sets | task_struct u32 | 2.6.0-<2.6.25 | all | `task_struct.cap_effective` (4-byte integer), `task_struct.cap_permitted` (4-byte integer), `task_struct.cap_inheritable` (4-byte integer) |
| open_files | files->fdt | 2.6.14+ | all | `task_struct.files` (pointer), `files_struct.fdt` (pointer), `fdtable.max_fds` (4-byte integer), `fdtable.fd` (pointer), `file.f_flags` (4-byte integer), `file.f_pos` (8-byte integer), `dentry.d_inode` (pointer), `inode.i_ino` (pointer-sized integer), `inode.i_mode` (2-byte integer), `dentry.d_sb` (pointer) (optional), `super_block.s_type` (pointer) (optional), `file_system_type.name` (pointer) (optional), capability file_paths |
| sockets | sock_common | 2.6.38+ | all | `file.private_data` (pointer), `socket.type` (2-byte integer), `socket.sk` (pointer), `inet_sock.sk.__sk_common.skc_family` (2-byte integer), `inet_sock.sk.__sk_common.skc_state` (1-byte integer), `inet_sock.sk.__sk_common.skc_rcv_saddr` (4-byte integer), `inet_sock.sk.__sk_common.skc_daddr` (4-byte integer), `inet_sock.sk.__sk_common.skc_num` (2-byte integer), `inet_sock.sk.__sk_common.skc_dport` (2-byte integer), `unix_sock.addr` (pointer) (optional), `unix_address.len` (4-byte integer) (optional), `unix_address.name` (member) (optional), `sockaddr_un.sun_path` (char array) (optional), `inet_sock.sk.__sk_common.skc_v6_rcv_saddr` (struct in6_addr) (optional), `inet_sock.sk.__sk_common.skc_v6_daddr` (struct in6_addr) (optional), capability open_files |
| sockets | inet_sock inet_* | 2.6.33+ | all | `file.private_data` (pointer), `socket.type` (2-byte integer), `socket.sk` (pointer), `inet_sock.sk.__sk_common.skc_family` (2-byte integer), `inet_sock.sk.__sk_common.skc_state` (1-byte integer), `inet_sock.sk.__sk_common.skc_rcv_saddr\|inet_rcv_saddr` (4-byte integer), `inet_sock.sk.__sk_common.skc_daddr\|inet_daddr` (4-byte integer), `inet_sock.inet_num` (2-byte integer), `inet_sock.inet_dport` (2-byte integer), `unix_sock.addr` (pointer) (optional), `unix_address.len` (4-byte integer) (optional), `unix_address.name` (member) (optional), `sockaddr_un.sun_path` (char array) (optional), `inet_sock.sk.__sk_common.skc_v6_rcv_saddr` (struct in6_addr) (optional), `inet_sock.sk.__sk_common.skc_v6_daddr` (struct in6_addr) (optional), capability open_files |
| sockets | inet_sock | 2.6.0-<2.6.33 | all | `file.private_data` (pointer), `socket.type` (2-byte integer), `socket.sk` (pointer), `inet_sock.sk.__sk_common.skc_family` (2-byte integer), `inet_sock.sk.__sk_common.skc_state` (1-byte integer), `inet_sock.rcv_saddr` (4-byte integer), `inet_sock.daddr` (4-byte integer), `inet_sock.num` (2-byte integer), `inet_sock.dport` (2-byte integer), `unix_sock.addr` (pointer) (optional), `unix_address.len` (4-byte integer) (optional), `unix_address.name` (member) (optional), `sockaddr_un.sun_path` (char array) (optional), `inet_sock.sk.__sk_common.skc_v6_rcv_saddr` (struct in6_addr) (optional), `inet_sock.sk.__sk_common.skc_v6_daddr` (struct in6_addr) (optional), capability open_files |
| vma_details | vm_flags, vm_pgoff | any | all | `vm_area_struct.vm_flags` (pointer-sized integer), `vm_area_struct.vm_pgoff` (pointer-sized integer), `vm_area_struct.vm_file` (pointer), `dentry.d_inode` (pointer) (optional), `inode.i_ino` (pointer-sized integer) (optional), capability mappings |

`task_struct.pid` and `tgid` are init-namespace numbers in every version, as `/proc` in the init
namespace shows them. `comm`'s length is the array's. Per-CPU variables are read at
`__per_cpu_offset[cpu]` plus the symbol, or at the symbol on a uniprocessor kernel.

A member path with alternatives (`cred.uid.val|uid`: `kuid_t` wraps the id since 3.5, unless the
kernel was built without strict type checks before 3.14) takes the first that resolves, and the
report says which (`used`). A capability can also need another (`mapping_files` needs `mappings`
and `file_paths`); a strategy outside the
kernel's version range is not used even if its inputs resolve (the report says so), so a layout
that doesn't match the version is reported rather than read.

### Provenance

For a kernel-lift ISF each member is trusted only if every named member on its path is
target-verified, dynamically verified (with the ISF's offset) or inferred in the lift's
provenance, and not listed in `metadata.kernel_lift.unverified_needed`. kernel-lift classes the
members of anonymous structs and unions in the named record that holds them, and so does the
plugin. Record sizes, `THREAD_SIZE` and enum constants are the reference build's, so they count
as reference-carried. Without the provenance file, only members the ISF's metadata names as
dynamically verified or inferred are trusted. A DWARF or BTF ISF is trusted throughout. When a
module's ISF is around too, give the plugin the kernel's: a module's ISF carries its own copy
of the kernel's types, laid out as the headers it was built against were.

Process facts
-------------

```
char *osi_isf_facts(CPUState *cpu, int32_t pid, uint64_t start_time, const char *which);
void osi_isf_free(char *s);
```

`osi_isf_facts` returns JSON about process `pid` (a thread group id): `which` is a
comma-separated list of `threads`, `cmdline`, `environ`, `exe`, `fs` (cwd and root), `creds`,
`caps`, `files` and `vmas`, or `all`. With `start_time` not 0 it answers only for the process
that started then: a pid reused since names another process (`OsiProc.create_time`, and the
events' `start_time`, give it). It returns NULL before the ISF is paired with the kernel; free
the result with `osi_isf_free`.

```json
{"pid": 66, "start_time": 4039767936, "name": "osifacts", "ppid": 65, "taskd": "0x...", "mm": "0x...",
 "threads": [{"tid": 66, "name": "osifacts", "start_time": 4039767936, "mm": "0x...", "taskd": "0x..."}],
 "cmdline": ["/tmp/osifacts", "hold", "extra-arg"], "environ": ["OSIFACTS_VAR=hello", "..."],
 "exe": "/tmp/osifacts", "cwd": "/tmp/osidir", "root": "/",
 "creds": {"uid": 21, "gid": 11, "suid": 23, "sgid": 13, "euid": 22, "egid": 12, "fsuid": 22, "fsgid": 12},
 "caps": {"effective": "0x0", "permitted": "0x0", "inheritable": "0x0", "bounding": "0x1ffffffffff"},
 "files": [{"fd": 3, "kind": "file", "ino": 7, "fs": "tmpfs", "flags": 32834, "pos": 1234, "path": "/tmp/osifacts.dat"},
           {"fd": 4, "kind": "pipe", "ino": 1208, "fs": "pipefs", "path": "pipe:[1208]", "...": "..."},
           {"fd": 6, "kind": "socket", "path": "socket:[1210]", "socket": {"family": 2, "type": 1,
            "state": 10, "listening": true, "local": "127.0.0.1:5555", "remote": "0.0.0.0:0"}}],
 "vmas": [{"start": "0x400000", "end": "0x401000", "perms": "r--p", "flags": "0x...", "offset": 0,
           "file": "/tmp/osifacts", "ino": 9}]}
```

- A fact the kernel can't give (its capability isn't available) or that can't be read now is
  `{"unavailable": "<why>"}` in its place: `files[].socket`, a unix socket's `path` and an IPv6
  address too.
- `cmdline` and `environ` are user memory: they are read only while the process's address space
  is the CPU's (in a callback in its context: a task change into it, its system calls, its user
  code), and a page that isn't present (or, on MIPS, isn't in the TLB, which is all the host can
  read user memory through) makes them unavailable with the address.
- Paths are from the root of the mount tree, as `/proc` shows them to a reader that isn't
  chrooted. Files without one have `/proc`'s names: `pipe:[ino]`, `socket:[ino]`,
  `anon_inode:<name>`; a file of another internal mount has `internal_mount: true`.
- `flags` is `f_flags` (without `O_CLOEXEC`, which is the fd table's), `pos` `f_pos`. A socket's
  ports are numbers, its addresses dotted (IPv4) or `[hex]` (IPv6); `listening` is TCP_LISTEN,
  which unix sockets use too.
- A mapping's permissions are VM_READ, VM_WRITE, VM_EXEC and VM_MAYSHARE (`s`), the same bits in
  every version; its offset is `vm_pgoff` in 4 KiB pages.
- The ids and capability sets are the process's (its leader's) credentials.

From Python:

```python
p = panda.plugins["osi_isf"].osi_isf_facts(cpu, pid, 0, b"files,vmas")
facts = json.loads(panda.ffi.string(p))
panda.plugins["osi_isf"].osi_isf_free(p)
```

Lifecycle events
----------------

With `lifecycle=true`, osi_isf calls `on_osi_isf_event(CPUState *cpu, const OsiIsfEvent *ev)`
(`osi_isf_types.h`; strings valid during the call):

| kind | when | fields |
|---|---|---|
| `OSI_ISF_FORK` | a new process: fork, vfork, clone without `CLONE_THREAD` | the child (`pid`, `tid`, `start_time`, `taskd`), the creator (`creator_pid`, `creator_tid`, `creator_start`), `ppid` the parent at creation, `clone_flags`, `syscall` |
| `OSI_ISF_THREAD` | a new thread (`CLONE_THREAD`) | as FORK |
| `OSI_ISF_EXEC` | a program started | `old_exe`, `new_exe`, `filename` (execve's path), `syscall` |
| `OSI_ISF_EXIT` | a task is ending (do_exit) | `exit_status` (the wait status: code << 8, or the signal), `group_exit` (1 the process's last task) |
| `OSI_ISF_REPARENT` | a process got a new parent | `ppid`, `old_ppid` |
| `OSI_ISF_PID_REUSE` | a task got an id an earlier task had (before its FORK or THREAD) | `old_start`, the earlier task's start time |

- **Creation** comes from the system calls through syscalls2, on the engine's pairing
  (`syscalls2_current_call` and `syscalls2_current_args`): at fork, vfork, clone or clone3's
  entry the creator, its parent and the flags are kept with the call; the child is reported at
  the return (its id is the return value, in the caller's pid namespace), at a vfork or `CLONE_VM`
  child's first return, or as soon as the child is seen before that (switched to, or making a
  call), from the pending call whose creator is its parent. The parent at creation is the
  creator's (`CLONE_PARENT`, `CLONE_THREAD`) or the creator, as it was at the entry: by the time
  the child is seen its parent may have changed. A task's creation always comes before its other
  events. Tasks a kernel thread starts are not reported created; tasks alive when the events
  start are known and not reported. `clone_flags` is clone's or clone3's argument (clone3's read
  from the caller's memory), `CLONE_VM | CLONE_VFORK` for vfork and 0 for fork.
- **Exec**: the old program (`exe_path`, else the name) at execve's entry, the new one when the
  program starts (proc_start_linux), or, when that can't read the new program's stack (MIPS),
  when the task is next seen with another mm. A failed execve reports nothing.
- **Exit**: the strategy `do_exit hook` reads do_exit's argument, which is every task's end and
  its wait status (signals included). `group_exit` is 1 for the last of the process's tasks
  to enter do_exit (with the thread list; `signal->live` without it, which can race). Without the
  symbol, the exit and exit_group calls give voluntary exits only.
- **Reparent**: at a process's end its children are noted (from its tasks' `children` lists,
  else from every process's parent), and each is reported when a later task change finds its
  parent changed.
- **Pid reuse**: tasks are known by (tid, start time); a new task whose id was another's is
  reported with that one's start time.

The report
----------

The load log has a line per capability: available (with its strategy and version range),
available from untrusted inputs, or unavailable with the input that is missing, has the wrong
shape or isn't trusted. Callers get the same as JSON:

```
const char *osi_isf_report(void);           // each capability's status, strategy, inputs
                                             // (value, width, provenance class), the
                                             // strategies considered, and the pairing
int osi_isf_capability(const char *name);   // 2 available, 1 from untrusted inputs,
                                             // 0 not available (or not paired, or a lifecycle
                                             // capability without lifecycle=true), -1 unknown
```

From Python: `panda.plugins["osi_isf"].osi_isf_report()` (a `char *`; `panda.ffi.string` it).

`osi_isf_check` (installed beside the plugin) prints the report for an ISF without a guest:

```
osi_isf_check --isf cosi.armel.json.xz --arch arm [--provenance provenance.json]
              [--symbols System.map] [--version 6.13.0] [--allow-untrusted] [--text]
              [--all-attempts]
```

Limits
------

* No `current` strategy for PowerPC and LoongArch (the rest works there), nor for x86 before
  2.6.30 (the PDA).
* On MIPS the engine's `current_asid` is EntryHi's ASID, while `OsiProc.asid` is the page
  directory's physical address, as on the other arches.
* Paths don't cross bind mounts or mount namespaces other than the task's, and a deleted file's
  path has no ` (deleted)`.
* A child's id at its creator's return is in the creator's pid namespace; the child is looked
  up by it among init-namespace ids, so in another pid namespace it is found only when it was
  seen first (switched to, or making a call).
* Mapping offsets assume 4 KiB pages.
* The KASLR search needs a kernel-mode pc in the kernel's text; it is tried at address-space
  changes, which are in the kernel.

Dependencies
------------

`osi`, `hooks` (the `finish_task_switch` and `do_exit` hooks) and `proc_start_linux` (exec), which
it loads; `lifecycle=true` uses `syscalls2`, which proc_start_linux loads.
