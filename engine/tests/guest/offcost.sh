#!/bin/sh
# offcost.sh STOCK_BUILD SERIES_BUILD [out]: the off-cost gate. "Built in but off" must cost nothing:
# the series with no plugin loaded must run the same guests as fast as stock QEMU built from the same
# tarball with the same configure line. The series launched rr-ready (-accel tcg,rr-ready=on, no
# recording) is measured against the series plain, with its own tolerance.
# env: ARCHS (x86_64 aarch64), REPS (5), TOL (0.05), RRTOL (0.10), TMO (600)
#   workloads: hk-count:1  boot plus a broad warm-up (find, gzip|md5sum, sha256sum, tar): translation
#              sc-bench:200000  3 x 200k getppid/getpgid/getsid: the syscall path
#              user-swap  a 64M swap round-trip over virtio-blk: memory and I/O
# Runs are interleaved (rep, workload, arch, then stock/series/rr) and one at a time, so a slow
# moment of the host lands on all three builds alike. A row fails when the median wall time of the
# series exceeds stock's by more than TOL (rr-ready: the series' by more than RRTOL), or when any
# run did not finish. Writes table.md and verdict.json; exits 1 on a failure.
H=$(cd "$(dirname "$0")" && pwd)
S=$(cd "$1" && pwd); B=$(cd "$2" && pwd)
O=${3:-$H/out/out-offcost}; mkdir -p $O; O=$(cd $O && pwd)
for r in $(seq ${REPS:-5}); do
  for w in hk-count:1 sc-bench:200000 user-swap; do
    for a in ${ARCHS:-x86_64 aarch64}; do
      for m in stock series rr; do
        q=$B; x=""
        [ $m = stock ] && q=$S
        [ $m = rr ] && x="-accel tcg,rr-ready=on"
        f=$O/$a.${w%%:*}.$m.$r.con
        s=$(date +%s.%N)
        QBUILD=$q QEXTRA="$x" TMO=${TMO:-600} $H/run.sh $a 6.13 $w < /dev/null > $f 2>&1
        echo "RC $? WALL $(echo "$(date +%s.%N) - $s" | bc)" >> $f
      done
    done
  done
done
python3 - $O ${TOL:-0.05} ${RRTOL:-0.10} <<'EOF'
import glob, json, os, re, statistics as st, sys
O, tol, rrtol = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
runs = {}
for f in glob.glob(O + "/*.con"):
    a, w, m, r = os.path.basename(f)[:-4].split(".")
    t = open(f, errors="replace").read()
    wall = float(re.search(r"WALL (\S+)", t).group(1))
    ok = "ALLDONE" in t and bool(re.search(r"EXIT \S+ 0\b", t))
    b = re.search(r"BENCH sc \S+ getppid_ns=(\S+)", t)
    runs.setdefault((a, w, m), []).append({"wall": wall, "ok": ok, "getppid_ns": float(b.group(1)) if b else None})
med = lambda v: st.median(x["wall"] for x in v)
rows, fail = [], False
for a, w in sorted({k[:2] for k in runs}):
    s, b, rr = (runs.get((a, w, m), []) for m in ("stock", "series", "rr"))
    if not (s and b and rr):
        continue
    ratio, rrratio = med(b) / med(s), med(rr) / med(b)
    ok = all(x["ok"] for x in s + b + rr)
    passed = ok and ratio <= 1 + tol and rrratio <= 1 + rrtol
    fail |= not passed
    gp = lambda v: st.median(x["getppid_ns"] for x in v) if all(x["getppid_ns"] for x in v) else None
    rows.append({"arch": a, "workload": w, "n": min(len(s), len(b), len(rr)),
                 "stock_s": med(s), "series_s": med(b), "rr_s": med(rr), "series_vs_stock": ratio,
                 "rr_vs_series": rrratio, "getppid_ns": [gp(s), gp(b), gp(rr)], "all_ok": ok,
                 "pass": passed})
rng = lambda k: "%.2f" % k
with open(O + "/table.md", "w") as f:
    f.write("| arch | workload | n | stock s | series s | rr-ready s | series/stock | rr/series "
            "| getppid ns (stock/series/rr) | verdict |\n|---|---|---|---|---|---|---|---|---|---|\n")
    for x in rows:
        g = "/".join("%.0f" % v if v else "-" for v in x["getppid_ns"])
        f.write("| %s | %s | %d | %s | %s | %s | %.3f | %.3f | %s | %s |\n" % (
            x["arch"], x["workload"], x["n"], rng(x["stock_s"]), rng(x["series_s"]), rng(x["rr_s"]),
            x["series_vs_stock"], x["rr_vs_series"], g, "PASS" if x["pass"] else "FAIL"))
json.dump({"tol": tol, "rrtol": rrtol, "pass": bool(rows) and not fail, "rows": rows},
          open(O + "/verdict.json", "w"), indent=1)
print(open(O + "/table.md").read())
print("offcost:", "PASS" if rows and not fail else "FAIL", "(tol %.0f%%, rr-ready %.0f%%)" % (tol * 100, rrtol * 100))
sys.exit(0 if rows and not fail else 1)
EOF
