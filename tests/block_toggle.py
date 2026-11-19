"""Block callbacks cost one TB flush per run: the first registration of any of them instruments
every block for entry and end, for the rest of the run, behind gates the block tests at run time.
Enabling and disabling, and registering and unregistering after that, retranslate nothing.

Run: panda-python tests/block_toggle.py <image.json>

After the revert, nothing is registered (the bridge instruments no block); then three block
callbacks are registered disabled: "entry" (start_block_exec), "end" (end_block_exec)
and "ctx" (before_block_exec), which asid_changed enables and disables at context switches, as
pandare2's procname filter does: while tests/programs/blocks_<arch>.S runs, enabled exactly while
its address space is current; at other times enabled and disabled again at every switch. The program runs RUNS times; at its
first hypercall entry and end are set to one of four enable sets, at its second both are
disabled. Checks:
- the first registration flushes once; the TB flush count (info jit) then stays the same over
  the runs, while the callbacks were enabled or disabled hundreds of times (each would flush every
  TB if the translation followed what is enabled), after all three are unregistered, and after a
  new one is registered again;
- entry and end see each of the program loop's 1000 entries and exits in a run where they are
  enabled, and none where they are not; no callback runs while it is disabled;
- ctx sees all 1000 loop entries of every run;
- the bridge instruments for entry and end (panda_bridge_wants) whatever is enabled, and still
  after the block callbacks are unregistered;
- a start_block_exec registered after the others were unregistered sees the loop's entries;
- a block callback registered disabled while the guest runs never runs: PROBES times, "probe"
  (start_block_exec) and "probe_end" (end_block_exec) are registered with enabled=False and
  deleted again while a shell loop runs, without a flush.
"""
import cffi
import re
import sys
from collections import Counter

from testlib import Checks, progress, upload
from pandare2 import Panda

RUNS = 16
PROBES = 3000
MIN_TOGGLES = 100
MODES = [(False, False), (True, False), (False, True), (True, True)]
LABELS = ["setup", "loop", "odd", "even", "fault", "fault_store", "sys"]

panda = Panda(generic=sys.argv[1], extra_args=["-icount", "shift=6,sleep=off"])
arch = panda.arch_name
C = Checks(arch)
probe = cffi.FFI()
probe.cdef("unsigned panda_bridge_wants(void);")
engine = probe.dlopen(panda.libpanda_path)

S = dict(on={"entry": False, "end": False, "ctx": False}, toggles=0, prog_asid=None, run=None,
         off_calls=Counter(), shapes=set(), labels={})
runs = []


def set_cb(name, on):
    if S["on"][name] != on:
        (panda.enable_callback if on else panda.disable_callback)(name)
        S["on"][name] = on
        S["toggles"] += 1
        S["shapes"].add(engine.panda_bridge_wants() & 6)


def count(name, cpu, tb):
    if S.get("gone"):
        return
    if not S["on"][name]:
        S["off_calls"][name] += 1
    elif S["run"] is not None and panda.tb_pc(cpu, tb) == S["labels"].get("loop"):
        S["run"][name] += 1


def entry(cpu, tb):
    count("entry", cpu, tb)


def end(cpu, tb):
    count("end", cpu, tb)


def ctx(cpu, tb):
    count("ctx", cpu, tb)


def again(cpu, tb):
    if S["run"] is not None and panda.tb_pc(cpu, tb) == S["labels"].get("loop"):
        S["run"]["again"] += 1


def probe(cpu, tb):
    S["off_calls"]["probe"] += 1


def probe_end(cpu, tb):
    S["off_calls"]["probe_end"] += 1


# Registered by drive(), after the revert.
CBS = {"entry": panda.callback.start_block_exec(entry), "end": panda.callback.end_block_exec(end),
       "ctx": panda.callback.before_block_exec(ctx), "again": panda.callback.start_block_exec(again),
       "probe": panda.callback.start_block_exec(probe), "probe_end": panda.callback.end_block_exec(probe_end)}


@panda.cb_asid_changed
def asid_changed(cpu, old, new):
    if S.get("gone") or "registered" not in S:
        pass
    elif S["prog_asid"] is not None:
        set_cb("ctx", new == S["prog_asid"])
    else:
        set_cb("ctx", True)
        set_cb("ctx", False)
    S["asid"] = new
    return False


@panda.hypercall(0x50616e64)
def hypercall(cpu):
    op = panda.arch.get_arg(cpu, 2, convention="syscall")
    if op == 1:
        table = panda.arch.get_arg(cpu, 3, convention="syscall")
        data = bytes(panda.virtual_memory_read(cpu, table, 8 * len(LABELS)))
        S["labels"] = {name: int.from_bytes(data[i * 8:i * 8 + 8], "little") for i, name in enumerate(LABELS)}
        S["prog_asid"] = S.get("asid")
        S["run"] = runs[-1]
        if not S.get("gone"):
            set_cb("ctx", True)
            set_cb("entry", S["run"]["mode"][0])
            set_cb("end", S["run"]["mode"][1])
    elif op == 2:
        if not S.get("gone"):
            set_cb("entry", False)
            set_cb("end", False)
        S["run"] = None
        S["prog_asid"] = None


def flush_count():
    out = panda.run_monitor_cmd("info jit")
    m = re.search(r"TB flush count\s+(\d+)", out)
    return int(m.group(1)) if m else None


KIND = {"entry": panda.callback.start_block_exec, "end": panda.callback.end_block_exec,
        "ctx": panda.callback.before_block_exec, "again": panda.callback.start_block_exec,
        "probe": panda.callback.start_block_exec, "probe_end": panda.callback.end_block_exec}


def register(name):
    panda.register_callback(KIND[name], CBS[name], name, enabled=False)


@panda.queue_blocking
def drive():
    panda.revert_sync("root")
    upload(panda, "blocks", arch, "/tmp/block-toggle")
    S["shape_none"] = engine.panda_bridge_wants() & 6
    S["flushes_none"] = flush_count()
    for name in ("entry", "end", "ctx"):
        register(name)
    S["registered"] = True
    panda.run_serial_cmd("true")         # the first registration's flush runs before guest code
    S["flushes_before"], S["toggles_before"] = flush_count(), S["toggles"]
    for i in range(RUNS):
        runs.append(Counter(mode=MODES[i % len(MODES)]))
        runs[-1]["output"] = panda.run_serial_cmd("/tmp/block-toggle; echo rc=$?")
    # Registered disabled while the guest runs (a shell loop keeps it busy), then deleted: neither
    # may run in between.
    panda.run_serial_cmd("(while :; do :; done) >/dev/null 2>&1 & echo $! > /tmp/busy")
    for _ in range(PROBES):
        for name in ("probe", "probe_end"):
            register(name)
        for name in ("probe", "probe_end"):
            panda.delete_callback(name)
    panda.run_serial_cmd("kill $(cat /tmp/busy)")
    S["flushes_after"], S["toggles_after"] = flush_count(), S["toggles"]
    progress(f"flush count {S['flushes_before']} -> {S['flushes_after']}, "
             f"{S['toggles_after'] - S['toggles_before']} enables/disables")
    # Unregister every block callback: instrumentation stays, nothing is flushed.
    S["gone"] = True
    for name in ("entry", "end", "ctx"):
        panda.delete_callback(name)
    S["shape_unregistered"] = engine.panda_bridge_wants() & 6
    panda.run_serial_cmd("/tmp/block-toggle; echo rc=$?")
    S["flushes_unregistered"] = flush_count()
    # Register one again: no flush, and it sees the loop.
    register("again")
    panda.enable_callback("again")
    runs.append(Counter(mode=None))
    runs[-1]["output"] = panda.run_serial_cmd("/tmp/block-toggle; echo rc=$?")
    S["flushes_again"] = flush_count()
    panda.end_analysis()


panda.run()

toggles = S.get("toggles_after", 0) - S.get("toggles_before", 0)
before, after = S.get("flushes_before"), S.get("flushes_after")
C.check("info jit reports the TB flush count", before is not None and after is not None, (before, after))
C.check(f"callbacks were enabled and disabled at least {MIN_TOGGLES} times during the runs",
        toggles >= MIN_TOGGLES, toggles)
none, unreg, again = S.get("flushes_none"), S.get("flushes_unregistered"), S.get("flushes_again")
C.check("with nothing registered the bridge instruments no block", S.get("shape_none") == 0, S.get("shape_none"))
C.check("the first registration flushes once", None not in (none, before) and before - none == 1, (none, before))
C.check("no TB flush during the runs", before is not None and after == before, (before, after, toggles))
C.check("unregistering every block callback keeps the instrumentation",
        S.get("shape_unregistered") == 6, S.get("shape_unregistered"))
C.check("no TB flush after unregistering, nor after registering one again",
        None not in (after, unreg, again) and unreg == after and again == after, (after, unreg, again))
C.check("no callback runs while disabled", not S["off_calls"], dict(S["off_calls"]))
C.check("the bridge instruments entry and end whatever is enabled", S["shapes"] == {6}, S["shapes"])
last = runs.pop() if runs and runs[-1]["mode"] is None else Counter()
C.check("a start_block_exec registered again sees the loop's entries",
        "rc=0" in last.get("output", "") and last["again"] == 1000, (last["again"], last.get("output")))
for i, run in enumerate(runs):
    entry_on, end_on = run["mode"]
    prefix = f"run {i} (entry {'on' if entry_on else 'off'}, end {'on' if end_on else 'off'})"
    C.check(prefix + ": the program exits 0", "rc=0" in run.get("output", ""), run.get("output"))
    C.check(prefix + ": entry sees the loop's entries", run["entry"] == (1000 if entry_on else 0), run["entry"])
    C.check(prefix + ": end sees the loop's exits", run["end"] == (1000 if end_on else 0), run["end"])
    C.check(prefix + ": ctx sees the loop's entries", run["ctx"] == 1000, run["ctx"])
sys.exit(C.report(f"BLOCK_TOGGLE arch={arch} runs={len(runs)} toggles={toggles} "
                  f"flushes={none}->{before}->{after}->{unreg}->{again}"))
