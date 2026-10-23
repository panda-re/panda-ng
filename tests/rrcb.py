"""Record/replay with analysis callbacks: a replay sees what the recording saw.

Run: panda-python tests/rrcb.py <dir>/debian-13-x86_64.json [workdir]

Records tests/programs/calls_<arch>.S (see tests/calls.py) with Panda.record_cmd, then replays it
in fresh processes. The recording and the first replay run the same callbacks: between the
program's hypercalls 1 and 2, before/after_block_exec on every block (the kernel's too), insn_exec
on the program's instructions, and virt_mem_after_read/write on its accesses; asid_changed and
the hypercalls throughout. Checks that the replay sees exactly what the recording saw: the
instruction count at each hypercall, the number and order of blocks in the window (all of them),
the program's instructions, its memory accesses with their values, and the asid_changed sequence.
A second replay with only the hypercall callback must reach the hypercalls at the same instruction
counts (callbacks don't perturb a replay).
"""
import hashlib
import json
import os
import subprocess
import sys
import tempfile

from testlib import Checks, MAGIC, hypercall_args, in_program, progress, upload


def child(mode, image, name, out, with_cbs):
    from pandare2 import Panda
    panda = Panda(generic=image, rr=True)
    with_cbs = with_cbs == "1"
    seen = {"hc": [], "blocks": 0, "abe": 0, "insns": 0, "mem": [], "asids": []}
    blk = hashlib.sha256()
    ins = hashlib.sha256()

    @panda.cb_asid_changed
    def asid_changed(cpu, old, new):
        if panda.libpanda.rr_on():
            seen["asids"].append(new)
        return False

    if with_cbs:
        @panda.cb_before_block_exec(name="bbe", enabled=False)
        def bbe(cpu, tb):
            seen["blocks"] += 1
            blk.update(panda.arch.get_pc(cpu).to_bytes(8, "little"))

        @panda.cb_after_block_exec(name="abe", enabled=False)
        def abe(cpu, tb, exit_code):
            seen["abe"] += 1
            blk.update(bytes([exit_code]))

        @panda.cb_insn_translate(name="it", enabled=False)
        def it(cpu, pc):
            return in_program(pc)

        @panda.cb_insn_exec(name="ie", enabled=False)
        def ie(cpu, pc):
            if in_program(pc):
                seen["insns"] += 1
                ins.update(pc.to_bytes(8, "little"))
            return 0

        @panda.cb_virt_mem_after_read(name="vmar", enabled=False)
        def vmar(cpu, pc, addr, size, buf):
            if in_program(pc):
                seen["mem"].append(("r", pc, addr, size, bytes(panda.ffi.buffer(buf, size)).hex()))

        @panda.cb_virt_mem_after_write(name="vmaw", enabled=False)
        def vmaw(cpu, pc, addr, size, buf):
            if in_program(pc):
                seen["mem"].append(("w", pc, addr, size, bytes(panda.ffi.buffer(buf, size)).hex()))

    window = ["bbe", "abe", "it", "ie", "vmar", "vmaw"] if with_cbs else []

    @panda.hypercall(MAGIC)
    def hypercall(cpu):
        op = hypercall_args(panda, cpu)[0]
        if not panda.libpanda.rr_on():
            return
        seen["hc"].append((op, panda.rr_get_guest_instr_count()))
        progress(f"[{mode}] hypercall {op} at instruction {seen['hc'][-1][1]}")
        for n in window:
            (panda.enable_callback if op == 1 else panda.disable_callback)(n)

    if mode == "record":
        @panda.queue_blocking
        def drive():
            panda.revert_sync("root")
            upload(panda, "calls", panda.arch_name)
            progress("[record] recording /tmp/calls")
            seen["output"] = panda.run_serial_cmd("true")  # settle the prompt before recording
            panda.record_cmd("/tmp/calls; echo rc=$?", recording_name=name, snap_name=None, timeout=600)
            panda.end_analysis()
        panda.run()
    else:
        progress(f"[{mode}] replaying")
        panda.run_replay(name)
        seen["serial"] = b"rc=0" in panda.replay_serial
    seen["blk_hash"] = blk.hexdigest()
    seen["ins_hash"] = ins.hexdigest()
    with open(out, "w") as f:
        json.dump(seen, f)


def main():
    image = sys.argv[1]
    with open(image) as f:
        arch = json.load(f)["arch"]
    C = Checks(arch)
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix="rrcb-", dir="/tmp/apitest"
                                                                   if os.path.isdir("/tmp/apitest") else None)
    name = os.path.join(work, "calls")
    runs = {}
    for mode, cbs in [("record", "1"), ("replay", "1"), ("replay-hc-only", "0")]:
        out = os.path.join(work, mode + ".json")
        progress(f"{mode}: starting")
        try:
            rc = subprocess.run([sys.executable, "-u", __file__, "--child", mode.split("-")[0], image, name, out, cbs],
                                timeout=900).returncode
        except subprocess.TimeoutExpired:
            rc = "timeout"
        runs[mode] = json.load(open(out)) if rc == 0 and os.path.exists(out) else None
        r = runs[mode]
        progress(f"{mode}: rc={rc} " + (f"hc={r['hc']} blocks={r['blocks']} insns={r['insns']} mem={len(r['mem'])} "
                                        f"asids={len(r['asids'])}" if r else "no result"))
    rec, rep, rep0 = runs["record"], runs["replay"], runs["replay-hc-only"]
    ok3 = rec is not None and rep is not None
    C.check("record: both hypercalls, with the window's callbacks firing",
            rec is not None and [h[0] for h in rec["hc"]] == [1, 2] and rec["blocks"] > 0 and rec["insns"] > 0
            and len(rec["mem"]) > 0, rec and {k: rec[k] for k in ("hc", "blocks", "insns")})
    C.check("replay: both hypercalls, at the recording's instruction counts", ok3 and rep["hc"] == rec["hc"],
            ok3 and (rec["hc"], rep["hc"]))
    C.check("replay: the same blocks in the window, in the same order (kernel's too), same exit codes",
            ok3 and (rep["blocks"], rep["abe"], rep["blk_hash"]) == (rec["blocks"], rec["abe"], rec["blk_hash"]),
            ok3 and ((rec["blocks"], rec["abe"]), (rep["blocks"], rep["abe"])))
    C.check("replay: the program's instructions, same order", ok3 and (rep["insns"], rep["ins_hash"])
            == (rec["insns"], rec["ins_hash"]), ok3 and (rec["insns"], rep["insns"]))
    C.check("replay: the program's memory accesses, addresses and values", ok3 and rep["mem"] == rec["mem"],
            ok3 and (len(rec["mem"]), len(rep["mem"])))
    C.check("replay: the asid_changed sequence", ok3 and rep["asids"] == rec["asids"],
            ok3 and (len(rec["asids"]), len(rep["asids"])))
    C.check("replay: the program's output on the serial port", rep is not None and rep.get("serial"))
    C.check("replay without the analysis callbacks: same hypercall instruction counts",
            rep0 is not None and rec is not None and rep0["hc"] == rec["hc"],
            rep0 and rec and (rec["hc"], rep0["hc"]))
    sys.exit(C.report(f"RRCB arch={arch} work={work}"))


if __name__ == "__main__":
    if sys.argv[1] == "--child":
        child(*sys.argv[2:7])
    else:
        main()
