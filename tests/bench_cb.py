"""Callback attribution (not an end-to-end performance gate).

PANDA_PLUGIN_DIR=... panda-python tests/bench_cb.py IMAGE.json
N_BLOCK=2000 N_MEM=200 REPEAT=2 (all positive). Every repeat is retained as a
JSON record; summaries use medians, never the fastest repeat. Workload counts
are fixed, with no icount; callback counts must be positive in every repeat.
A private standalone qcow/description protects shared snapshots. Bound the whole
process externally (e.g. timeout 1800); each serial command is bounded at 600s.
The C plugin is loaded even in baseline (loaded-but-unsubscribed); missing C
phases are explicit, not a pass. Restore/activation time is outside dispatch
samples and is not measured by this attribution script. Use tests/perf/run.py
for rotated shared-launcher/vanilla A/A experiments and artifact manifests.
"""
import contextlib
import json
import math
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import sys
import tempfile
import time


def positive_int(value):
    n = int(value)
    if n <= 0:
        raise ValueError("counts and REPEAT must be positive")
    return n


def sample_record(tag, repeat, seconds, calls, subscribed):
    if not math.isfinite(seconds) or seconds <= 0:
        raise ValueError("phase duration must be finite and positive")
    if type(calls) is not int or calls < 0 or (subscribed and calls == 0):
        raise ValueError(f"{tag}: callback count must be positive for subscribed phases")
    return {"phase": tag, "repeat": repeat, "seconds": seconds, "calls": calls}


def summarize(samples, baseline):
    if len(samples) != len(baseline) or not samples:
        raise ValueError("incomplete repeats")
    costs = []
    for i, (sample, base) in enumerate(zip(samples, baseline)):
        if sample["repeat"] != i or base["repeat"] != i:
            raise ValueError("missing/duplicate repeat")
        sample_record(sample["phase"], i, sample["seconds"], sample["calls"], True)
        sample_record(base["phase"], i, base["seconds"], base["calls"], False)
        costs.append((sample["seconds"] - base["seconds"]) / sample["calls"] * 1e9)
    return {"paired_n": len(costs), "ns_per_call_all": costs,
            "median_ns_per_call": statistics.median(costs),
            "note": "phase repeats paired by index, not interleaved; attribution only"}


@contextlib.contextmanager
def private_image(image):
    """Copy full snapshot-bearing disk, never an overlay that loses internal snapshots."""
    source = Path(image).resolve()
    if source.suffix != ".json":
        raise ValueError("benchmark requires a JSON fixture")
    data = json.loads(source.read_text())
    import shlex
    extra = data.get("extra_args") or []
    extra = shlex.split(extra) if isinstance(extra, str) else extra
    if any("icount" in str(arg) for arg in extra):
        raise ValueError("icount is forbidden in benchmark fixtures")
    storage = ("-drive", "-blockdev", "-hda", "-hdb", "-cdrom", "-readconfig")
    if any(str(arg).split("=", 1)[0] in storage for arg in extra):
        raise ValueError("extra storage paths would bypass private fixture isolation")
    disk = (source.parent / data["qcow"]).resolve()
    info = subprocess.run(["qemu-img", "info", "--output=json", str(disk)],
                          check=True, capture_output=True, text=True, timeout=30)
    details = json.loads(info.stdout)
    if details.get("backing-filename") or details.get("format") != "qcow2":
        raise ValueError("private fixture requires standalone qcow2 (no shared backing chain)")
    # A short TMPDIR also keeps QMP/serial socket names below the Unix path limit.
    with tempfile.TemporaryDirectory(prefix="bcb-", dir="/tmp") as tmp:
        target = Path(tmp)
        shutil.copy2(disk, target / "disk.qcow2")
        data["qcow"] = "disk.qcow2"
        for key in ("isf", "kconf_file"):
            if data.get(key):
                original = (source.parent / data[key]).resolve()
                name = key + "".join(original.suffixes)
                shutil.copy2(original, target / name)
                data[key] = name
        (target / "image.json").write_text(json.dumps(data))
        yield str(target / "image.json")


def main(image):
    from cffi import FFI
    from pandare2 import Panda

    n_block = positive_int(os.environ.get("N_BLOCK", "2000"))
    n_mem = positive_int(os.environ.get("N_MEM", "200"))
    repeats = positive_int(os.environ.get("REPEAT", "2"))
    with private_image(image) as fixture:
        panda = Panda(generic=fixture)
        loop = "i=0; while [ $i -lt {n} ]; do i=$((i+1)); done; echo BENCH_CB_DONE"
        results, count, failures = {}, [0], []
        so = Path(panda.get_plugin_path()) / f"libpanda-cbcount_{panda.arch_name}-softmmu.so"
        cb = None
        if so.is_file():
            panda.load_plugin("cbcount")
            cffi = FFI()
            cffi.cdef("uint64_t cbcount_get(int); void cbcount_reset(void); void cbcount_enable(int, int);")
            cb = cffi.dlopen(str(so))
        else:
            print(json.dumps({"status": "skipped", "phases": ["bbe C", "vmar C"], "missing": str(so)}), flush=True)

        def timed(tag, n, subscribed=False, calls=lambda: count[0]):
            results[tag] = []
            for repeat in range(repeats):
                panda.revert_sync("root")
                count[0] = 0
                if cb:
                    cb.cbcount_reset()
                start = time.perf_counter()
                out = panda.run_serial_cmd(loop.format(n=n), timeout=600)
                seconds = time.perf_counter() - start
                if out.replace("\r", "").splitlines().count("BENCH_CB_DONE") != 1:
                    raise ValueError(f"workload did not complete: {out}")
                record = sample_record(tag, repeat, seconds, int(calls()), subscribed)
                results[tag].append(record)
                print(json.dumps({"sample": record}), flush=True)

        def bare_block(cpu, tb):
            count[0] += 1

        def bare_mem(cpu, pc, addr, size, buf):
            count[0] += 1

        @panda.queue_blocking
        def drive():
            try:
                timed("baseline", n_block)
                if cb:
                    cb.cbcount_enable(0, 1)
                    timed("bbe C", n_block, True, lambda: cb.cbcount_get(0))
                    cb.cbcount_enable(0, 0)
                panda.register_callback(panda.callback.before_block_exec,
                                        panda.callback.before_block_exec(bare_block), "raw_bbe")
                timed("bbe bare cffi", n_block, True)
                panda.delete_callback("raw_bbe")

                @panda.cb_before_block_exec(name="deco_bbe")
                def deco_bbe(cpu, tb):
                    count[0] += 1
                timed("bbe decorator", n_block, True)
                panda.delete_callback("deco_bbe")
                panda.enable_memcb()
                timed("memcb on, no cb", n_mem)
                if cb:
                    cb.cbcount_enable(1, 1)
                    timed("vmar C", n_mem, True, lambda: cb.cbcount_get(1))
                    cb.cbcount_enable(1, 0)
                panda.register_callback(panda.callback.virt_mem_after_read,
                                        panda.callback.virt_mem_after_read(bare_mem), "raw_vmar")
                timed("vmar bare cffi", n_mem, True)
                panda.delete_callback("raw_vmar")

                @panda.cb_virt_mem_after_read(name="deco_vmar")
                def deco_vmar(cpu, pc, addr, size, buf):
                    count[0] += 1
                timed("vmar decorator", n_mem, True)
                panda.delete_callback("deco_vmar")
            except BaseException as e:
                failures.append(e)
            finally:
                panda.end_analysis()

        panda.run()
        if failures:
            raise failures[0]
        expected = ["baseline", "memcb on, no cb", "bbe bare cffi", "bbe decorator",
                    "vmar bare cffi", "vmar decorator"] + (["bbe C", "vmar C"] if cb else [])
        if set(results) != set(expected) or any(len(xs) != repeats for xs in results.values()):
            raise ValueError("incomplete callback phases/repeats")
        summary = {tag: summarize(results[tag], results["baseline" if tag.startswith("bbe") else "memcb on, no cb"])
                   for tag in expected if tag.startswith(("bbe", "vmar"))}
        print(json.dumps({"raw_samples": results, "summary": summary}, allow_nan=False), flush=True)


if __name__ == "__main__":
    try:
        main(sys.argv[1])
    except BaseException:
        import traceback
        traceback.print_exc()
        sys.stdout.flush()
        sys.stderr.flush()
        os._exit(1)
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(0)  # Engine threads are process-lifetime; private fixture already cleaned.
