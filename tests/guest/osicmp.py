#!/usr/bin/env python3
"""osicmp.py <con> <log>: apis/hostosi.py's answers (OSI got, in the log)
against the guest's own view (OSI truth, on the console), per key, at the
entry and at the return, and osiref.py's (OSI ref); then the cost lines."""
import sys
from collections import defaultdict


def keyed(lines):
    d = defaultdict(list)
    for v in lines:
        k, _, rest = v.partition(' ')
        if k in ('fd', 'map'):
            k2, _, rest2 = rest.partition(' ')
            d[f'{k} {k2}'].append(rest2)
        else:
            d[k].append(rest)
    return d


con, log = sys.argv[1:3]
truth = [ln.split('OSI truth ', 1)[1].rstrip('\n') for ln in open(con, errors='replace')
         if 'OSI truth ' in ln]
got = defaultdict(list)
cost = []
for ln in open(log, errors='replace'):
    if ln.startswith('OSI got '):
        at, _, rest = ln[8:].rstrip('\n').partition(' ')
        got[at].append(rest)
    elif ln.startswith('OSI ref '):
        got['ref'].append(ln[8:].rstrip('\n'))
    elif ln.startswith('OSI cost '):
        cost.append(ln.strip())
if not truth or not got['enter']:
    print(f'RESULT no answers (truth lines={len(truth)}, enter lines={len(got["enter"])}): '
          'the guest test or the engine did not run')
    sys.exit(1)
t = keyed(truth)
# the child's arguments are the parent's (fork, no exec)
t['childargs'] = t['args']
bad = 0
for at in ('enter', 'return', 'ref'):
    g = keyed(got[at])
    for k in sorted(set(t) | set(g)):
        if t.get(k) != g.get(k):
            bad += 1
            print(f'DIFF {at} {k}: truth={t.get(k)} got={g.get(k)}')
    print(f'{at}: {len(g)} keys, {sum(1 for k in t if t.get(k) == g.get(k))}/{len(t)} match')
# the engine's OSI filters (penhost.py's hooks): calls each should pass
counts = {}
for ln in open(log, errors='replace'):
    if ln.startswith('SC count tag='):
        f = dict(kv.split('=', 1) for kv in ln.split()[2:])
        counts[f['tag']] = int(f['enter'])
want = {'fd_null': 5, 'fd_exact': 5, 'fd_pipe': 4, 'fd_miss': 0, 'exe_yes': 1, 'exe_no': 0,
        'exe_exact': 1 if t.get('exe') == ['/pagetest'] else 0}
for k, v in want.items():
    if counts.get(k) != v:
        bad += 1
        print(f'DIFF filter {k}: want={v} got={counts.get(k)}')
print(f'filters: {sum(counts.get(k) == v for k, v in want.items())}/{len(want)} as expected')
mc = [ln.split() for ln in open(log, errors='replace') if ln.startswith('OSI matchcheck ')]
if not mc:
    bad += 1
    print('DIFF matchcheck: not run')
else:
    f = dict(kv.split('=') for kv in mc[-1][2:])
    print(f'matcher: {int(f["n"]) - int(f["bad"])}/{f["n"]} as Python decides')
    if int(f['bad']):
        bad += 1
        for ln in open(log, errors='replace'):
            if ln.startswith('OSI matchbad '):
                print(ln.rstrip())
for c in cost:
    print(c)
print('RESULT', 'ok' if not bad else f'{bad} diffs')
sys.exit(1 if bad else 0)
