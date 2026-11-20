Plugin: osi_isf
===========

Summary
-------

Linux OSI (the `osi` plugin's interface) from a Volatility 3 ISF of the guest kernel: the
process list, the current process and thread, memory mappings with file paths, and kernel
modules. The ISF can come from the kernel's DWARF (dwarf2json, as `pandare2.mkimage` makes),
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
| current | pcpu_hot.current_task | 6.2.0+ | x86_64, i386 | `pcpu_hot`, `pcpu_hot.current_task` (pointer), `__per_cpu_offset` (optional) |
| current | current_task | 2.6.30-<6.2.0 | x86_64, i386 | `current_task`, `__per_cpu_offset` (optional) |
| current | __entry_task | 4.10.0+ | aarch64 | `__entry_task`, `__per_cpu_offset` (optional) |
| current | sp_el1 thread_info | 3.7.0-<4.10.0 | aarch64 | `thread_info.task` (pointer), `THREAD_SIZE` |
| current | __entry_task | 5.18.0+ | arm | `__entry_task`, `__per_cpu_offset` (optional) |
| current | __current | 5.18.0+ | arm | `__current` |
| current | svc sp thread_info | 2.6.0-<5.18.0 | arm | `thread_info.task` (pointer), `THREAD_SIZE` |
| current | kernelsp thread_info | 2.6.0+ | mips | `kernelsp`, `thread_info.task` (pointer), `THREAD_SIZE` |
| current | tp/sscratch | 4.15.0+ | riscv | registers only |
| mappings | maple tree | 6.1.0+ | all | `task_struct.mm` (pointer), `mm_struct.mm_mt` (struct maple_tree), `maple_tree.ma_root` (pointer), `maple_range_64.pivot` (pointer-sized element array), `maple_range_64.slot` (pointer-sized element array), `maple_arange_64.pivot` (pointer-sized element array), `maple_arange_64.slot` (pointer-sized element array), `vm_area_struct.vm_start` (pointer-sized integer), `vm_area_struct.vm_end` (pointer-sized integer), `vm_area_struct.vm_mm` (pointer), `vm_area_struct.vm_flags` (pointer-sized integer) (optional) |
| mappings | mmap list | 2.6.0-<6.1.0 | all | `task_struct.mm` (pointer), `mm_struct.mmap` (pointer), `vm_area_struct.vm_next` (pointer), `vm_area_struct.vm_start` (pointer-sized integer), `vm_area_struct.vm_end` (pointer-sized integer), `vm_area_struct.vm_mm` (pointer), `vm_area_struct.vm_flags` (pointer-sized integer) (optional) |
| mapping_files | f_path.dentry | 2.6.20+ | all | `vm_area_struct.vm_file` (pointer), `file.f_path.dentry` (pointer), `dentry.d_parent` (pointer), `dentry.d_name.name` (pointer), `vm_area_struct.vm_pgoff` (pointer-sized integer) (optional), capability mappings |
| mapping_files | f_dentry | 2.6.0-<2.6.20 | all | `vm_area_struct.vm_file` (pointer), `file.f_dentry` (pointer), `dentry.d_parent` (pointer), `dentry.d_name.name` (pointer), `vm_area_struct.vm_pgoff` (pointer-sized integer) (optional), capability mappings |
| mount_paths | struct mount | 3.3.0+ | all | `file.f_path.mnt` (pointer), `mount.mnt` (struct vfsmount), `mount.mnt_parent` (pointer), `mount.mnt_mountpoint` (pointer), `vfsmount.mnt_root` (pointer), capability mapping_files |
| mount_paths | vfsmount | 2.6.20-<3.3.0 | all | `file.f_path.mnt` (pointer), `vfsmount.mnt_parent` (pointer), `vfsmount.mnt_mountpoint` (pointer), `vfsmount.mnt_root` (pointer), capability mapping_files |
| heap_stack | mm_struct brk/start_stack | any | all | `task_struct.mm` (pointer), `mm_struct.start_brk` (pointer-sized integer), `mm_struct.brk` (pointer-sized integer), `mm_struct.start_stack` (pointer-sized integer) |
| modules | mem[MOD_TEXT] | 6.4.0+ | all | `modules`, `module.list` (struct list_head), `module.name` (char array), `module.mem` (element array), `sizeof(module_memory)`, `module_memory.base` (pointer), `module_memory.size` (4-byte integer), `mod_mem_type.MOD_TEXT` |
| modules | core_layout | 4.5.0-<6.4.0 | all | `modules`, `module.list` (struct list_head), `module.name` (char array), `module.core_layout.base` (pointer), `module.core_layout.size` (4-byte integer) |
| modules | module_core | 2.6.0-<4.5.0 | all | `modules`, `module.list` (struct list_head), `module.name` (char array), `module.module_core` (pointer), `module.core_size` (4-byte integer) |
| task_change | finish_task_switch | any | all | `finish_task_switch*`, capability current |

`task_struct.pid` and `tgid` are init-namespace numbers in every version, as `/proc` in the init
namespace shows them. `comm`'s length is the array's. Per-CPU variables are read at
`__per_cpu_offset[cpu]` plus the symbol, or at the symbol on a uniprocessor kernel.

A capability can also need another (`mapping_files` needs `mappings`); a strategy outside the
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
                                             // 0 not available (or not paired), -1 unknown
```

From Python: `panda.plugins["osi_isf"].osi_isf_report()` (a `char *`; `panda.ffi.string` it).

`osi_isf_check` (installed beside the plugin) prints the report for an ISF without a guest:

```
osi_isf_check --isf cosi.armel.json.xz --arch arm [--provenance provenance.json]
              [--symbols System.map] [--version 6.13.0] [--allow-untrusted] [--text]
```

Limits
------

* No `current` strategy for PowerPC and LoongArch (the rest works there), nor for x86 before
  2.6.30 (the PDA).
* On MIPS the engine's `current_asid` is EntryHi's ASID, while `OsiProc.asid` is the page
  directory's physical address, as on the other arches.
* Paths don't cross bind mounts or mount namespaces other than the task's.
* The KASLR search needs a kernel-mode pc in the kernel's text; it is tried at address-space
  changes, which are in the kernel.

Dependencies
------------

`osi`, `hooks` (the `finish_task_switch` hook) and `proc_start_linux` (exec), which it loads.
