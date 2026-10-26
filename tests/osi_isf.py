"""osi_isf checks on an image built by pandare2.mkimage.

Run: panda-python tests/osi_isf.py <dir>/debian-13-x86_64.json

Reverts to the root snapshot and runs cat on the guest. When cat opens /etc/os-release, asks OSI
(osi_isf, from the image's ISF) for the current process and thread, its memory mappings, the
process list and the kernel modules, and checks them against what the shell reports.
"""
import json
import os
import sys

from pandare2 import Panda

# --8<-- [start:setup]
panda = Panda(generic=sys.argv[1])
with open(sys.argv[1]) as f:
    isf = os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])), json.load(f)["isf"])
panda.load_plugin("osi", {"disable-autoload": "true"})
panda.load_plugin("osi_isf", {"isf": isf})
seen = {"task_changes": 0}
# --8<-- [end:setup]


# --8<-- [start:callbacks]
def s(p):
    return panda.ffi.string(p).decode() if p != panda.ffi.NULL else None


@panda.ppp("osi", "on_task_change")
def task_change(cpu):
    seen["task_changes"] += 1


@panda.ppp("syscalls2", "on_sys_openat_enter")
def openat(cpu, pc, dfd, filename, flags, *mode):
    if "proc" in seen:
        return
    try:
        if panda.read_str(cpu, filename) != "/etc/os-release":
            return
    except ValueError:
        return
    osi = panda.plugins["osi"]
    proc = osi.get_current_process(cpu)
    if proc == panda.ffi.NULL:
        return
    seen["proc"] = (s(proc.name), proc.pid, proc.ppid, proc.asid, panda.current_asid(cpu))
    thread = osi.get_current_thread(cpu)
    seen["thread"] = (thread.pid, thread.tid) if thread != panda.ffi.NULL else None
    seen["maps"] = [(s(m.name), s(m.file), m.base, m.size) for m in panda.get_mappings(cpu)]
    seen["procs"] = panda.get_processes_dict(cpu)
    mods = osi.get_modules(cpu)
    n = panda.garray_len(mods) if mods != panda.ffi.NULL else 0
    seen["modules"] = [(s(osi.get_one_module(mods, i).name), osi.get_one_module(mods, i).base,
                        osi.get_one_module(mods, i).size) for i in range(n)]
    if n:
        osi.cleanup_garray(mods)
    mapping = osi.get_mapping_by_addr(cpu, proc, panda.arch.get_pc(cpu))
    seen["pc_mapping"] = s(mapping.name) if mapping != panda.ffi.NULL else None
# --8<-- [end:callbacks]


# --8<-- [start:drive]
@panda.queue_blocking
def drive():
    panda.revert_sync("root")
    out = panda.run_serial_cmd("sh -c 'echo PID=$$ PPID=$PPID; exec cat /etc/os-release'").strip().splitlines()
    seen["pid"], seen["ppid"] = (int(kv.split("=")[1]) for kv in out[0].split())
    seen["lsmod"] = panda.run_serial_cmd("cut -d' ' -f1 /proc/modules | sort | tr '\\n' ' '").split()
    panda.end_analysis()


panda.run()
# --8<-- [end:drive]

proc = seen.get("proc")
maps = seen.get("maps", [])
names = {m[0] for m in maps}
files = {m[1] for m in maps if m[1]}
procs = seen.get("procs", {})
mods = seen.get("modules", [])
print(f"OSI_ISF arch={panda.arch_name} proc={proc} thread={seen.get('thread')} pid={seen['pid']} "
      f"ppid={seen['ppid']} maps={len(maps)} procs={len(procs)} modules={len(mods)} "
      f"task_changes={seen['task_changes']} pc_mapping={seen.get('pc_mapping')}")
for m in maps:
    print(f"  {m[2]:#x}+{m[3]:#x} {m[0]} {m[1] or ''}")
print("  modules:", sorted(m[0] for m in mods)[:12])
checks = {
    "current process": proc is not None and proc[0] == "cat" and proc[1] == seen["pid"],
    "parent pid": proc is not None and proc[2] == seen["ppid"],
    "asid matches the CPU's": proc is not None and proc[3] == proc[4],
    "current thread": seen.get("thread") == (seen["pid"], seen["pid"]),
    "mappings: cat, libc, ld.so, heap or stack": "/usr/bin/cat" in files
        and any(n and n.startswith("libc.so") for n in names)
        and any(n and n.startswith("ld-linux") for n in names) and "[stack]" in names,
    "mappings sorted and disjoint": all(a[2] + a[3] <= b[2] for a, b in zip(maps, maps[1:])),
    "process list": procs.get(seen["pid"], {}).get("name") == "cat" and procs.get(1, {}).get("name") == "systemd"
        and procs.get(seen["ppid"], {}).get("name") == "bash",
    "modules match /proc/modules": sorted(m[0] for m in mods) == sorted(seen["lsmod"])
        and all(m[1] and m[2] for m in mods),
    "mapping by address (the syscall's pc)": (seen.get("pc_mapping") or "").startswith("libc.so"),
    "task changes": seen["task_changes"] > 0,
}
for name, ok in checks.items():
    print(f"  {'PASS' if ok else 'FAIL'} {name}")
sys.exit(0 if all(checks.values()) else 1)
