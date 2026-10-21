#!/usr/bin/env python3
"""osibench.py <out dir> [arch...]: osibench.sh's runs, per arch and set: the
median write(2) in ns (sc-fdbench's BENCH line), what it adds over no hook
("empty"), and the engine's events (a rejecting set must deliver none, the
passing one every write).  Exit 1 if a run is missing or the events are wrong."""
import glob
import os
import re
import statistics
import sys

BENCH = re.compile(r'BENCH fd n=(\d+) write_ns=([\d.]+) getppid_ns=([\d.]+)')
EVENTS = re.compile(r'events=(\d+)')
WHAT = {
    'empty': 'no hook',
    'wnum': 'fd number filter, rejects',
    'wfd': 'fd path endswith, rejects at the leaf',
    'wfdx': 'fd path exact, rejects at the leaf',
    'wfdnear': 'fd path endswith, leaf matches, parent rejects',
    'wfdstart': 'fd path startswith, rejects (whole path)',
    'wexe': 'exe path endswith, rejects at the leaf',
    'wfdpass': 'fd path endswith, passes: Python callback',
}


def main() -> int:
    out = sys.argv[1]
    archs = sys.argv[2:] or sorted({os.path.basename(f).split('-')[0]
                                    for f in glob.glob(f'{out}/*-empty-*.con')})
    bad = 0
    for a in archs:
        base = None
        print(f'{a}: ns per write(2), median of runs (added over no hook)')
        for n in WHAT:
            ws, evs, lost = [], [], 0
            for con in sorted(glob.glob(f'{out}/{a}-{n}-*.con')):
                m = BENCH.search(open(con, errors='replace').read())
                e = EVENTS.findall(open(con[:-4] + '.log', errors='replace').read())
                if not m:
                    lost += 1
                    continue
                ws.append(float(m[2]))
                evs.append(int(e[-1]) if e else -1)
                writes = int(m[1])
            if not ws:
                continue
            med = statistics.median(ws)
            if n == 'empty':
                base = med
            want_ok = (all(e >= writes for e in evs) if n == 'wfdpass'
                       else all(e == 0 for e in evs))
            bad += lost + (not want_ok)
            added = f'{med - base:+8.1f}' if base is not None else '       ?'
            spread = f'{min(ws):.0f}-{max(ws):.0f}'
            print(f'  {n:9} {med:8.1f} {added}  runs={len(ws)}{f" lost={lost}" if lost else ""} '
                  f'range={spread:11} events={"ok" if want_ok else evs}  {WHAT[n]}')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
