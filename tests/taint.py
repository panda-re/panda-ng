"""Taint mode (CF_TAINT pass, capability "taint") on a recording, end to end.

Run: panda-python tests/taint.py <dir>/debian-13-x86_64.json [workdir]

Records, in one process, a command that writes a marker file, drops the page cache, then `cat`s the
file (so its bytes come back from disk by DMA) and `echo`s a constant line typed on the serial
console. Then, each in a fresh process:
- plain: replays with taint off; reports the instruction count at echo's write;
- clean: replays with taint enabled and nothing labelled;
- taint: replays with taint and label sets enabled at the first block and every device write into
  RAM labelled, each with its own label (Panda.taint_device_write). Checks:
  - echo's write(1, ...) of the marker into the file is clean (the shell computed it);
  - cat's write(1, ...) of the marker carries taint on every marker byte (disk -> page cache ->
    user buffer -> syscall buffer, through the TCG shadow), and every marker byte carries exactly
    one label, the same one (the disk read that brought the block in);
  - echo's write of the computed line is clean (never in a device write);
  - taint_label_ram / taint_query_ram / taint_count_ram round-trip on a scratch page,
    taint_label_reg / taint_query_reg round-trip on gdb register 1 (riscv64's x0 is hardwired),
    and taint_disable leaves taint_enabled() false;
  - tainted-branch events fire (disk data reaches compares) and none fire in the clean replay,
    nor tainted-jump events;
  - echo's write comes at the same instruction count as in the plain replay (taint never changes
    execution).
"""
import json
import os
import subprocess
import sys
import tempfile

from pandare2 import Panda

# The shell computes both lines, so neither appears in the typed command (or its echo).
MARKER = "taintmarker-4242"
CLEAN = "cleanline-1764"
CMD = ("echo taintmarker-$((2*2121)) > /root/taint.txt; sync; echo 3 > /proc/sys/vm/drop_caches; "
       "cat /root/taint.txt; echo cleanline-$((42*42))")
CAPS = ["analysis", "rr", "taint"] + os.environ.get("TAINT_TEST_CAPS", "").split()


def child(mode, image, name, out):
    panda = Panda(generic=image, rr=True, capabilities=CAPS)
    seen = {"icount": None, "writes": {}, "branch": 0, "pc": 0}

    if mode == "record":
        @panda.queue_blocking
        def drive():
            panda.revert_sync("root")
            panda.record_cmd(CMD, recording_name=name, snap_name=None)
            panda.end_analysis()

        panda.run()
        return

    if mode == "taint":
        @panda.taint_device_write
        def on_device_write(ram_addr, length):
            seen["device_writes"] = seen.get("device_writes", 0) + 1
            panda.taint_label_ram(ram_addr, length, 0xFF, label=seen["device_writes"])

    if mode in ("taint", "clean"):
        @panda.cb_taint_branch
        def on_branch(cpu, pc, mask):
            seen["branch"] += 1

        @panda.cb_taint_pc
        def on_pc(cpu, pc, mask):
            seen["pc"] += 1

        @panda.cb_start_block_exec(observe_only=True)
        def first(cpu, tb):
            if mode == "taint":
                panda.taint_enable_labels()
            seen["enabled"] = panda.taint_enable() and panda.taint_enabled()
            # register round-trip between blocks, cleared again before the guest runs
            n = panda.taint_label_reg(cpu, 1, 0xA5)
            got = panda.taint_query_reg(cpu, 1)
            panda.taint_label_reg(cpu, 1, 0)
            seen["reg"] = [n, got == [0xA5] * n, panda.taint_query_reg(cpu, 1) == [0] * n]
            panda.disable_callback("first")

    @panda.ppp("syscalls2", "on_sys_write_enter", observe_only=True)
    def write(cpu, pc, fd, buf, count):
        if fd != 1 or count > 4096:
            return
        try:
            data = panda.virtual_memory_read(cpu, buf, count)
        except ValueError:
            return
        for text in (MARKER, CLEAN):
            at = data.find(text.encode())
            if at < 0:
                continue
            if text == CLEAN:
                seen["icount"] = panda.rr_get_guest_instr_count()
            masks = []
            if mode != "plain":
                masks = [-1 if m is None else m
                         for m in panda.taint_query_va(cpu, buf + at, len(text))]
            seen["writes"].setdefault(text, []).append(masks)
            if mode == "taint":
                seen.setdefault("labels", {}).setdefault(text, []).append(
                    panda.taint_labels_va(cpu, buf + at, len(text)))

    panda.run_replay(name)
    if mode != "plain":
        seen["stats"] = {k: v for k, v in panda.taint_stats().items() if k != "h1"}
        # scratch round-trip on RAM byte 0 (replay is over, nothing reads it any more)
        panda.taint_label_ram(0, 64, 0)
        before = panda.taint_count_ram()
        panda.taint_label_ram(0, 16, 0x0F)
        seen["roundtrip"] = [panda.taint_query_ram(0), panda.taint_query_ram(15),
                             panda.taint_query_ram(16), panda.taint_count_ram() - before]
        panda.taint_disable()
        seen["disabled"] = not panda.taint_enabled()
    with open(out, "w") as f:
        json.dump(seen, f)


def main():
    image = sys.argv[1]
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix="taint-test-")
    name = os.path.join(work, "taint")
    res = {}
    for mode in ("record", "plain", "clean", "taint"):
        out = os.path.join(work, f"{mode}.json")
        subprocess.run([sys.executable, __file__, "--child", mode, image, name, out], check=True)
        if mode != "record":
            res[mode] = json.load(open(out))
    t, p, c = res["taint"], res["plain"], res["clean"]
    marker, clean = t["writes"].get(MARKER, []), t["writes"].get(CLEAN, [])
    checks = [
        ("taint enabled at the first block", t.get("enabled") is True, ""),
        # fd 1 writes of the marker: echo's into the file (computed by the shell), then cat's
        ("echo's write of the computed marker into the file is clean",
         len(marker) == 2 and all(m == 0 for m in marker[0]), marker),
        ("cat's write of the disk-read marker is tainted on every byte",
         len(marker) == 2 and all(m > 0 for m in marker[1]), marker),
        ("cat's marker bytes carry one label, the same for all",
         len(t.get("labels", {}).get(MARKER, [])) == 2 and
         all(ls is not None and len(ls) == 1 for ls in t["labels"][MARKER][1]) and
         len({ls[0] for ls in t["labels"][MARKER][1]}) == 1,
         t.get("labels", {}).get(MARKER, [None, None])[1:]),
        ("echo's write of the computed line is clean",
         len(clean) == 1 and all(m == 0 for m in clean[0]), clean),
        ("label/query/count round-trip", t["roundtrip"] == [0x0F, 0x0F, 0, 16], t["roundtrip"]),
        ("register label/query round-trip", t["reg"][0] > 0 and all(t["reg"][1:]), t["reg"]),
        ("taint_disable", t["disabled"] is True, ""),
        ("tainted-branch events with device input labelled", t["branch"] > 0,
         f"{t['branch']} (stats {t['stats'].get('branch_events')})"),
        ("no tainted-branch or tainted-jump events with nothing labelled",
         c["branch"] == 0 and c["pc"] == 0,
         f"branch {c['branch']} pc {c['pc']}"),
        ("echo's write at the same instruction count as in the plain replay",
         t["icount"] is not None and t["icount"] == p["icount"],
         f"{t['icount']} vs {p['icount']}"),
        ("the same in the clean replay", c["icount"] == p["icount"], f"{c['icount']} vs {p['icount']}"),
    ]
    print(f"events with device input labelled: {t['branch']} tainted branches, {t['pc']} tainted jumps")
    bad = 0
    for what, ok, detail in checks:
        print(f"{'ok  ' if ok else 'FAIL'} {what}" + ("" if ok else f": {detail}"))
        bad += not ok
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    if sys.argv[1] == "--child":
        child(*sys.argv[2:6])
    else:
        main()
