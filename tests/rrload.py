"""Snapshot loads and a recording: refused while recording, and the recording replays both ways.

Run: panda-python tests/rrload.py <dir>/debian-13-x86_64.json [workdir]

A recording is the log plus its start snapshot; a load during the recording is not in the log, so
its replay could not repeat it (engine 0073). The recording process reverts before
x-rr-begin-record (allowed), then, while recording, tries a loadvm (Panda.revert_sync), a
panda_revert from the main loop and a savevm that would replace the start snapshot rr-<name>: each
must be refused with the VM still running (a command after them runs), and the recording must
replay with x-rr-begin-replay (Panda.run_replay) and print both commands' output again.

The same recording is then replayed in a fresh process through QEMU's startup path, -icount
rr=replay,rrfile=,rrsnapshot=rr-<name>, which cannot replay a recording begun at run time (its log
has nothing of machine init, nor of the snapshot load's block requests: before 0141 the x86 load
hung for ever in blkreplay's flush). It must stop at once with an error that says to use
x-rr-begin-replay: x86 when it loads the start snapshot (marked as x-rr-begin-record's),
aarch64/riscv64 virt already at machine init (the fdt rng-seed's random draw).

The mark is x-rr-begin-record's alone: after the recording ends, the recording process loads the
start snapshot again with rr off (an ordinary loadvm) and saves rrload-after-<name>. That snapshot
must not carry the "replay/begin" subsection (before the fix the load left the mark set and every
later savevm in the process inherited it). The test reads both snapshots' vmstate out of the qcow2
(the start snapshot must carry the mark, the new one must not).
"""
import json
import os
import struct
import subprocess
import sys
import tempfile
import threading

from pandare2 import Panda

LOAD_REFUSED = "does not allow loading a snapshot while recording"
SAVE_REFUSED = "does not allow replacing snapshot"
STARTUP_HINT = "x-rr-begin-replay"


def child(mode, image, name, out):
    base = os.path.basename(name)
    extra = None
    if mode == "startup":
        extra = ["-icount", f"shift=auto,rr=replay,rrfile={name}-rr-nondet.log,rrsnapshot=rr-{base}"]
    panda = Panda(generic=image, rr=True, extra_args=extra)
    lib = panda.libpanda
    seen = {}

    if mode == "record":
        @panda.queue_blocking
        def drive():
            seen["revert_before"] = panda.revert_sync("root")
            panda.record(name)
            panda.wait_rr("record")
            seen["a"] = panda.run_serial_cmd("echo rrload-a-$((6*7))", timeout=120)
            seen["loadvm"] = panda.revert_sync("root")
            ev = threading.Event()

            def revert():
                seen["panda_revert"] = lib.panda_revert(panda.ffi.new("char[]", b"root"))
                ev.set()
            panda.queue_main_loop_wait_fn(revert)
            seen["revert_ran"] = ev.wait(120)
            seen["savevm_start"] = panda.run_monitor_cmd(f"savevm rr-{base}")
            seen["b"] = panda.run_serial_cmd("echo rrload-b-$((6*7))", timeout=120)
            seen["in_record"] = bool(lib.rr_in_record())
            panda.end_record()
            panda.wait_rr("off")
            # rr off: an ordinary load of the (marked) start snapshot, then an ordinary savevm
            seen["load_start_off"] = panda.revert_sync(f"rr-{base}")
            seen["savevm_after"] = panda.run_monitor_cmd(f"savevm rrload-after-{base}")
            panda.end_analysis()
        panda.run()
    elif mode == "replay":
        panda.run_replay(name)
        seen["serial"] = panda.replay_serial.decode(errors="replace")
    else:
        # startup replay: expected to exit during panda_init with the error; if it ran, record that
        panda.run()
        seen["ran"] = True
    with open(out, "w") as f:
        json.dump(seen, f)


MARK = b"replay/begin"


def vmstate_has(qcow, snap, needle=MARK):
    """Whether internal snapshot `snap` of qcow2 file `qcow` has `needle` in its vmstate
    (None: no such snapshot). Reads the vmstate area through the snapshot's L1 table."""
    with open(qcow, "rb") as f:
        h = f.read(104)
        assert h[:4] == b"QFI\xfb", "not a qcow2"
        version, = struct.unpack(">I", h[4:8])
        cluster_bits, = struct.unpack(">I", h[20:24])
        size, = struct.unpack(">Q", h[24:32])
        nb_snapshots, = struct.unpack(">I", h[60:64])
        snap_off, = struct.unpack(">Q", h[64:72])
        if version >= 3:
            incompat, = struct.unpack(">Q", h[72:80])
            assert not incompat & (1 << 4), "extended L2 entries are not handled"
        csize = 1 << cluster_bits
        l2_bits = cluster_bits - 3
        span = 1 << (cluster_bits + l2_bits)
        vm_off = ((size + span - 1) // span) * span
        mask = 0x00fffffffffffe00
        f.seek(snap_off)
        for _ in range(nb_snapshots):
            e = f.read(40)
            l1_off, l1_size, id_len, name_len, _, _, _, vm_size, extra_len = \
                struct.unpack(">QIHHIIQII", e)
            extra = f.read(extra_len)
            if extra_len >= 8:
                vm_size = struct.unpack(">Q", extra[:8])[0] or vm_size
            f.read(id_len)
            name = f.read(name_len).decode(errors="replace")
            pad = (-(40 + extra_len + id_len + name_len)) % 8
            f.read(pad)
            if name == snap:
                break
        else:
            return None
        here = f.tell()
        f.seek(l1_off)
        l1 = struct.unpack(f">{l1_size}Q", f.read(8 * l1_size))
        tail = b""
        pos = vm_off
        while pos < vm_off + vm_size:
            n = min(csize - pos % csize, vm_off + vm_size - pos)
            l1i, l2i = pos // span, (pos % span) // csize
            data = bytes(n)
            l2 = l1[l1i] & mask if l1i < len(l1) else 0
            if l2:
                f.seek(l2 + 8 * l2i)
                ent, = struct.unpack(">Q", f.read(8))
                assert not ent & (1 << 62), "compressed vmstate cluster"
                if ent & mask and not ent & 1:
                    f.seek((ent & mask) + pos % csize)
                    data = f.read(n)
            buf = tail + data
            if needle in buf:
                return True
            tail = buf[-len(needle):]
            pos += n
        f.seek(here)
        return False


def run_child(mode, image, name, out, log):
    with open(log, "w") as f:
        try:
            rc = subprocess.run([sys.executable, "-u", __file__, "--child", mode, image, name, out],
                                stdout=f, stderr=subprocess.STDOUT,
                                timeout=120 if mode == "startup" else 900).returncode
        except subprocess.TimeoutExpired:
            rc = "timeout"
    res = json.load(open(out)) if rc == 0 and os.path.exists(out) else None
    print(f"{mode}: rc={rc}" + ("" if res else " no result") + f" (log {log})")
    return rc, res, open(log, errors="replace").read()


def main():
    image = sys.argv[1]
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix="rrload-")
    name = os.path.join(work, "r")
    arch = json.load(open(image)).get("arch", os.path.basename(image))
    _, rec, _ = run_child("record", image, name, os.path.join(work, "record.json"),
                          os.path.join(work, "record.log"))
    _, rep, _ = run_child("replay", image, name, os.path.join(work, "replay.json"),
                          os.path.join(work, "replay.log"))
    src, start, slog = run_child("startup", image, name, os.path.join(work, "startup.json"),
                                 os.path.join(work, "startup.log"))
    rec = rec or {}
    marks = ("rrload-a-42", "rrload-b-42")
    checks = {
        "a revert before x-rr-begin-record succeeds": rec.get("revert_before", "x").strip() == "",
        "loadvm while recording is refused": LOAD_REFUSED in (rec.get("loadvm") or ""),
        "panda_revert while recording is refused (-1)": rec.get("panda_revert") == -1 and rec.get("revert_ran"),
        "savevm over the start snapshot while recording is refused": SAVE_REFUSED in (rec.get("savevm_start") or ""),
        "the guest runs on after the refusals, still recording":
            marks[0] in (rec.get("a") or "") and marks[1] in (rec.get("b") or "") and rec.get("in_record"),
        "the recording replays (x-rr-begin-replay) and prints both outputs again":
            rep is not None and all(m in rep["serial"] for m in marks),
    }
    checks[f"startup replay (-icount rr=replay,rrsnapshot=) is refused with the x-rr-begin-replay hint ({arch})"] = \
        src not in (0, "timeout") and STARTUP_HINT in slog
    qcow = os.path.join(os.path.dirname(os.path.abspath(image)), json.load(open(image))["qcow"])
    base = os.path.basename(name)
    start_marked = vmstate_has(qcow, f"rr-{base}")
    after_marked = vmstate_has(qcow, f"rrload-after-{base}")
    print(f"  vmstate mark: rr-{base}={start_marked} rrload-after-{base}={after_marked}")
    checks["with rr off, a loadvm of the start snapshot and a savevm succeed"] = \
        rec.get("load_start_off", "x").strip() == "" and "Error" not in rec.get("savevm_after", "Error")
    checks["the start snapshot carries the replay/begin mark"] = start_marked is True
    checks["a savevm after an ordinary load of the start snapshot is not marked"] = after_marked is False
    for k, ok in checks.items():
        print(f"  {'PASS' if ok else 'FAIL'} {k}")
    sys.exit(0 if all(checks.values()) else 1)


if __name__ == "__main__":
    if sys.argv[1] == "--child":
        child(*sys.argv[2:6])
    else:
        main()
