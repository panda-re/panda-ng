"""Bounded Linux x86_64 syscall observer conformance (guest execution NOT offline).

Build tests/programs/syscall_observer_x86_64.S using its header recipe, retaining
symbols, then (manager's selected panda-python, private compatible image):
  timeout 900 panda-python tests/syscall_observer.py IMAGE.json ELF legacy
  timeout 900 panda-python tests/syscall_observer.py IMAGE.json ELF native

Each command is a fresh process/engine. Both use the SAME ELF and assertions:
exact six mmap arguments and syscall PCs; late subscription; unknown ENOSYS;
no-return exit; fork child+parent returns; a signal-restarted read with exactly
one ordinary completion. Failure is retained, never converted into an exception
skip. The native prototype's historical fork fallback may fail child checks.
No icount, timing, compiler invocation or fixture download occurs in this test.

Restore cancellation, replay, exclusive conflicts, true dlclose/reload and warm
TB lifetime are NOT covered here. Python's plugin dlopen cache means merely
calling unload_plugin/load_plugin would not prove a physical dlclose. Run the
existing lifecycle/teardown suites and add a native harness before acceptance.
"""
import base64
import json
from pathlib import Path
import subprocess
import sys

from pandare2 import Panda
from testlib import Checks

MASK = (1 << 64) - 1


def symbols(elf):
    result = subprocess.run(["nm", "-n", str(elf)], check=True, capture_output=True,
                            text=True, timeout=15)
    found = {}
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) == 3:
            found[fields[2]] = int(fields[0], 16)
    required = ("six_call", "late_call", "unknown_call", "fork_call", "parent_exit",
                "child_exit", "restart_read", "signal_exit")
    if any(name not in found for name in required):
        raise ValueError("fixture must retain expected assembly symbols")
    return found


def signed(value):
    return value - (1 << 64) if value >= 1 << 63 else value


def main():
    if len(sys.argv) != 4 or sys.argv[3] not in ("legacy", "native"):
        raise ValueError("usage: syscall_observer.py PRIVATE_IMAGE.json ELF legacy|native")
    image, elf, backend = sys.argv[1:]
    elf = Path(elf).resolve()
    pcs = symbols(elf)
    panda = Panda(generic=image)
    if panda.arch_name != "x86_64":
        raise ValueError("only Linux x86_64 fixture supported")
    panda.load_plugin("syscalls2", {"native-observer": backend == "native"})
    checks = Checks("x86_64")
    state = {"phase": None, "events": [], "late": False, "errors": [], "output": {}}

    def record(kind, cpu, pc, args=(), number=None):
        if state["phase"] and 0x400000 <= pc < 0x402000:
            state["events"].append({"phase": state["phase"], "kind": kind,
                                    "pc": pc, "args": [int(a) & MASK for a in args],
                                    "number": number,
                                    "ret": signed(panda.arch.get_retval(cpu, convention="syscall"))})

    @panda.ppp("syscalls2", "on_sys_mmap_enter", observe_only=True)
    def mmap_enter(cpu, pc, *args):
        record("six-enter", cpu, pc, args)

    @panda.ppp("syscalls2", "on_sys_mmap_return", observe_only=True)
    def mmap_return(cpu, pc, *args):
        record("six-return", cpu, pc, args)

    @panda.ppp("syscalls2", "on_sys_getpid_enter", observe_only=True)
    def late_enter(cpu, pc):
        record("late-enter", cpu, pc)
        if pc == pcs["late_call"] and not state["late"]:
            state["late"] = True
            @panda.ppp("syscalls2", "on_sys_getpid_return", name="late_return", observe_only=True)
            def late_return(cpu, pc):
                record("late-return", cpu, pc)

    @panda.ppp("syscalls2", "on_unknown_sys_enter", observe_only=True)
    def unknown_enter(cpu, pc, nr):
        record("unknown-enter", cpu, pc, number=int(nr))

    @panda.ppp("syscalls2", "on_unknown_sys_return", observe_only=True)
    def unknown_return(cpu, pc, nr):
        record("unknown-return", cpu, pc, number=int(nr))

    @panda.ppp("syscalls2", "on_sys_fork_enter", observe_only=True)
    def fork_enter(cpu, pc):
        record("fork-enter", cpu, pc)

    @panda.ppp("syscalls2", "on_sys_fork_return", observe_only=True)
    def fork_return(cpu, pc):
        record("fork-return", cpu, pc)

    @panda.ppp("syscalls2", "on_sys_exit_enter", observe_only=True)
    def exit_enter(cpu, pc, code):
        record("exit-enter", cpu, pc, (code,))

    @panda.ppp("syscalls2", "on_sys_exit_return", observe_only=True)
    def exit_return(cpu, pc, code):
        record("exit-return", cpu, pc, (code,))

    @panda.ppp("syscalls2", "on_sys_read_enter", observe_only=True)
    def read_enter(cpu, pc, *args):
        record("read-enter", cpu, pc, args)

    @panda.ppp("syscalls2", "on_sys_read_return", observe_only=True)
    def read_return(cpu, pc, *args):
        record("read-return", cpu, pc, args)

    @panda.queue_blocking
    def drive():
        try:
            panda.revert_sync("root")
            payload = base64.b64encode(elf.read_bytes()).decode()
            panda.run_serial_cmd("rm -f /tmp/sc-observer.b64 /tmp/sc-observer", timeout=30)
            for i in range(0, len(payload), 512):
                panda.run_serial_cmd(f"printf '%s' '{payload[i:i+512]}' >> /tmp/sc-observer.b64", timeout=30)
            panda.run_serial_cmd("base64 -d /tmp/sc-observer.b64 > /tmp/sc-observer; chmod +x /tmp/sc-observer", timeout=30)
            for phase, arg in (("basic", ""), ("signals", "signals")):
                state["phase"] = phase
                state["output"][phase] = panda.run_serial_cmd(
                    f"timeout 30 /tmp/sc-observer {arg}; echo OBSERVER_RC=$?", timeout=90)
                state["phase"] = None
        except BaseException as error:
            state["errors"].append(repr(error))
        finally:
            panda.end_analysis()

    panda.run()
    print(json.dumps({"backend": backend, "pcs": pcs, "evidence": state}, sort_keys=True), flush=True)
    checks.check("guest orchestration completed", not state["errors"], state["errors"])
    for phase in ("basic", "signals"):
        output = state["output"].get(phase, "").replace("\r", "").splitlines()
        checks.check(f"{phase} functional fixture exit zero", output.count("OBSERVER_RC=0") == 1, output)

    def events(phase, kind, pc):
        return [e for e in state["events"] if e["phase"] == phase and e["kind"] == kind and e["pc"] == pc]

    want = [0, 4096, 3, 0x22, MASK, 0]
    for kind, pc in (("six-enter", pcs["six_call"]), ("six-return", pcs["six_call"] + 2)):
        got = events("basic", kind, pc)
        checks.check(f"{kind} exact PC and all six arguments", len(got) == 1 and got[0]["args"] == want, got)
        if kind.endswith("return"):
            checks.check("mmap successful return value", len(got) == 1 and got[0]["ret"] > 0, got)
    late = events("basic", "late-return", pcs["late_call"] + 2)
    checks.check("late return subscription observed once", len(late) == 1 and late[0]["ret"] > 0, late)
    for kind, pc in (("unknown-enter", pcs["unknown_call"]), ("unknown-return", pcs["unknown_call"] + 2)):
        got = events("basic", kind, pc)
        checks.check(f"{kind} exact unknown number", len(got) == 1 and got[0]["number"] == 7000, got)
        if kind.endswith("return"):
            checks.check("unknown returns ENOSYS", len(got) == 1 and got[0]["ret"] == -38, got)
    fork = events("basic", "fork-return", pcs["fork_call"] + 2)
    checks.check("fork returns once in child and parent", len(fork) == 2 and
                 sorted(e["ret"] == 0 for e in fork) == [False, True] and
                 all(e["ret"] >= 0 for e in fork), fork)
    exits = [e for e in state["events"] if e["phase"] == "basic" and e["kind"] == "exit-enter"]
    checks.check("parent and child no-return exit entries", sorted(e["pc"] for e in exits) ==
                 sorted([pcs["parent_exit"], pcs["child_exit"]]), exits)
    checks.check("exit never emits a completion", not any(e["kind"] == "exit-return" for e in state["events"]))
    reads = events("signals", "read-return", pcs["restart_read"] + 2)
    checks.check("signal-restarted read emits one ordinary completion", len(reads) == 1 and reads[0]["ret"] == 1, reads)
    checks.check("restart instruction PC never emitted as completion", not events("signals", "read-return", pcs["restart_read"]))
    print("UNTESTED: restore cancellation, replay, exclusive conflicts, true dlclose/reload and warm-TB lifetime", flush=True)
    return checks.report(f"SYSCALL OBSERVER backend={backend} (experimental)")


if __name__ == "__main__":
    sys.exit(main())
