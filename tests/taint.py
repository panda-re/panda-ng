"""Taint mode (CF_TAINT pass, capability "taint") on a recording, end to end.

Run: panda-python tests/taint.py <dir>/debian-13-x86_64.json [workdir]

Records, in one process, a command that writes a marker file, drops the page cache, then `cat`s the
file (so its bytes come back from disk by DMA) and `echo`s a constant line typed on the serial
console. Then, each in a fresh process:
- plain: replays with taint off; reports the instruction count at echo's write;
- taint: replays with taint enabled at the first block and every device write into RAM labelled
  (taint_set_device_write_cb). Checks:
  - echo's write(1, ...) of the marker into the file is clean (the shell computed it);
  - cat's write(1, ...) of the marker carries taint on every marker byte (disk -> page cache ->
    user buffer -> syscall buffer, through the TCG shadow);
  - echo's write of the computed line is clean (never in a device write);
  - taint_label_ram / taint_query_ram / taint_count_ram round-trip on a scratch page, and
    taint_disable leaves taint_enabled() false;
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

CDEFS = """
bool taint_enable(void);
void taint_disable(void);
bool taint_enabled(void);
void taint_label_ram(uint64_t ram_addr, uint64_t len, uint8_t mask);
uint8_t taint_query_ram(uint64_t ram_addr);
uint64_t taint_count_ram(void);
uint64_t taint_va_to_ram(void *cpu, uint64_t va);
typedef void (*TaintDeviceWriteFn)(void *opaque, uint64_t ram_addr, uint64_t len);
void taint_set_device_write_cb(TaintDeviceWriteFn fn, void *opaque);
void taint_stats(char *buf, size_t len);
"""


def child(mode, image, name, out):
    import cffi
    panda = Panda(generic=image, rr=True, capabilities=CAPS)
    seen = {"icount": None, "writes": {}}

    if mode == "record":
        @panda.queue_blocking
        def drive():
            panda.revert_sync("root")
            panda.record_cmd(CMD, recording_name=name, snap_name=None)
            panda.end_analysis()

        panda.run()
        return

    ffi = cffi.FFI()
    ffi.cdef(CDEFS)
    lib = ffi.dlopen(panda.libpanda_path)
    keep = []

    if mode == "taint":
        @ffi.callback("void(void *, uint64_t, uint64_t)")
        def on_device_write(opaque, ram_addr, length):
            lib.taint_label_ram(ram_addr, length, 0xFF)
        keep.append(on_device_write)
        lib.taint_set_device_write_cb(on_device_write, ffi.NULL)

        @panda.cb_start_block_exec(observe_only=True)
        def first(cpu, tb):
            seen["enabled"] = bool(lib.taint_enable()) and bool(lib.taint_enabled())
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
            for i in range(len(text) if mode == "taint" else 0):
                ra = lib.taint_va_to_ram(ffi.cast("void *", int(panda.ffi.cast("uintptr_t", cpu))),
                                         buf + at + i)
                masks.append(-1 if ra == 2**64 - 1 else lib.taint_query_ram(ra))
            seen["writes"].setdefault(text, []).append(masks)

    panda.run_replay(name)
    if mode == "taint":
        buf = ffi.new("char[]", 1 << 16)
        lib.taint_stats(buf, 1 << 16)
        seen["stats"] = ffi.string(buf).decode().splitlines()[0]
        # scratch round-trip on RAM byte 0 (replay is over, nothing reads it any more)
        lib.taint_label_ram(0, 64, 0)
        before = lib.taint_count_ram()
        lib.taint_label_ram(0, 16, 0x0F)
        seen["roundtrip"] = [lib.taint_query_ram(0), lib.taint_query_ram(15), lib.taint_query_ram(16),
                             lib.taint_count_ram() - before]
        lib.taint_disable()
        seen["disabled"] = not lib.taint_enabled()
    with open(out, "w") as f:
        json.dump(seen, f)


def main():
    image = sys.argv[1]
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix="taint-test-")
    name = os.path.join(work, "taint")
    res = {}
    for mode in ("record", "plain", "taint"):
        out = os.path.join(work, f"{mode}.json")
        subprocess.run([sys.executable, __file__, "--child", mode, image, name, out], check=True)
        if mode != "record":
            res[mode] = json.load(open(out))
    t, p = res["taint"], res["plain"]
    marker, clean = t["writes"].get(MARKER, []), t["writes"].get(CLEAN, [])
    checks = [
        ("taint enabled at the first block", t.get("enabled") is True, ""),
        # fd 1 writes of the marker: echo's into the file (computed by the shell), then cat's
        ("echo's write of the computed marker into the file is clean",
         len(marker) == 2 and all(m == 0 for m in marker[0]), marker),
        ("cat's write of the disk-read marker is tainted on every byte",
         len(marker) == 2 and all(m > 0 for m in marker[1]), marker),
        ("echo's write of the computed line is clean",
         len(clean) == 1 and all(m == 0 for m in clean[0]), clean),
        ("label/query/count round-trip", t["roundtrip"] == [0x0F, 0x0F, 0, 16], t["roundtrip"]),
        ("taint_disable", t["disabled"] is True, ""),
        ("echo's write at the same instruction count as in the plain replay",
         t["icount"] is not None and t["icount"] == p["icount"],
         f"{t['icount']} vs {p['icount']}"),
    ]
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
