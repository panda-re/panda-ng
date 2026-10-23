#!/usr/bin/env python3
"""Check every hook access against the guest's own view.

For each run <dir>/<name>.{con,log}: the guest prints, per test,
    TEST <name>, PRE ... addr=<a>, TRUTH:<hex> (what its mapping holds after
    the hook), and for write tests ORIG:<hex> (before) and OTHER:<hex>
    (another view of the same memory: the file via read(2), or a fresh
    zero page).
Newer tests print one line per checked range instead:
    AT addr=<a> len=<n> TRUTH:<hex> TRUTHFNV:<h> [ORIG:.. ORIGFNV:..]
       [OTHER:.. OTHERFNV:..]
(hex is at most the first 256 bytes; FNV-1a covers the whole range).
The plugin logs PAGEIN ... op=<op> addr=<a> len=<n> result=<r> ...
[data=<hex, first 256 bytes>] [fnv=<h>], and PAGEIN-TEST-BOUNDARY when a
test starts, which splits the log per test (older logs have no boundaries:
accesses are then matched by address over the whole run).

  read OK      data == TRUTH
  write OK     TRUTH == pattern, and OTHER == ORIG (nothing leaked into
               memory shared with other mappings)
  write refused  TRUTH == ORIG and OTHER == ORIG (nothing written)
  NOACCESS     a read the guest must refuse
Contention runs (many reads) are checked against the data.bin pattern.
Parallel runs (*-par*): every access to a test's PROT_NONE page must be
refused, every other access must read span.bin bytes.
Exit status 1 if any read is wrong, or any write via the fault API is wrong
or leaks.  Leaks through wapi=debug are expected and only counted.
"""
import glob
import os
import re
import sys

out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), 'out')

data = b''
for pg in range(8):
    data += ''.join(f'[pg{pg} off{o:04x}]' for o in range(0, 4096, 16))[:4096].encode()
span = b''.join(''.join(f'[sp{b:02d} off{o:04x}]..' for o in range(0, 4096, 16)).encode()
                for b in range(64))
PATTERN = b'<PAGEIN-WROTE-ME>'
KEYS = ('read_ok', 'read_bad', 'read_refused', 'write_ok', 'write_bad', 'leak', 'refused_clean',
        'refused_dirty', 'debug_leak', 'par_ok', 'par_refused', 'par_bad', 'unmatched')


def pattern(n):
    return (PATTERN * (n // len(PATTERN) + 1))[:n]


def fnv(b):
    h = 0xcbf29ce484222325
    for x in b:
        h = ((h ^ x) * 0x100000001b3) & 0xffffffffffffffff
    return h


def parse_con(con):
    tests, cur = [], None
    for line in open(con, errors='replace'):
        line = line.strip('\r\n')
        m = re.match(r'TEST (\S+)', line)
        if m:
            cur = dict(name=m.group(1), at={}, bad=set(), noaccess=set())
            tests.append(cur)
            continue
        if cur is None:
            continue
        m = re.match(r'PRE .*addr=(0x[0-9a-f]+)', line)
        if m:
            cur['addr'] = int(m.group(1), 16)
        m = re.match(r'NOACCESS addr=(0x[0-9a-f]+)', line)
        if m:
            cur['noaccess'].add(int(m.group(1), 16))
        m = re.match(r'PAR bad t=\d+ addr=(0x[0-9a-f]+)', line)
        if m:
            cur['bad'].add(int(m.group(1), 16))
        m = re.match(r'AT addr=(0x[0-9a-f]+) len=(\d+)(.*)', line)
        if m:
            c = dict(len=int(m.group(2)))
            for tag, v in re.findall(r' (\w+):([0-9a-f]+)', m.group(3)):
                c[tag] = int(v, 16) if tag.endswith('FNV') else bytes.fromhex(v)
            cur['at'][int(m.group(1), 16)] = c
            continue
        for tag in ('TRUTH', 'ORIG', 'OTHER'):
            m = re.match(tag + r':([0-9a-f]+)$', line)
            if m:
                cur[tag] = bytes.fromhex(m.group(1))
    return tests


ACC = re.compile(r'PAGEIN (?:.*op=(\w+) )?.*addr=0x([0-9a-f]+) len=(\d+) result=(\w+)'
                 r'(?:.* data=([0-9a-f]+))?(?: fnv=([0-9a-f]+))?')


def parse_log(log):
    """List of segments (one per test boundary, or one for the whole log)."""
    segs, cur, bounded = [], [], False
    for line in open(log, errors='replace'):
        if line.startswith('PAGEIN-TEST-BOUNDARY'):
            if bounded or cur:
                segs.append(cur)
            cur, bounded = [], True
            continue
        m = ACC.search(line)
        if m:
            cur.append(dict(op=m.group(1) or 'read', addr=int(m.group(2), 16),
                            len=int(m.group(3)), res=m.group(4),
                            data=bytes.fromhex(m.group(5)) if m.group(5) else None,
                            fnv=int(m.group(6), 16) if m.group(6) else None))
    segs.append(cur)
    return segs, bounded


def check(r, a, c):
    """Access a (last logged for its address) against AT check c."""
    truth = c.get('TRUTH', b'')
    leaked = 'ORIGFNV' in c and 'OTHERFNV' in c and c['ORIGFNV'] != c['OTHERFNV']
    if a['op'] == 'read':
        if a['res'] != 'OK':
            return
        d = a['data'] or b''
        n = min(len(d), len(truth))
        good = d[:n] == truth[:n]
        if a['fnv'] is not None and 'TRUTHFNV' in c:
            good = good and a['fnv'] == c['TRUTHFNV']
        r['read_ok' if good else 'read_bad'] += 1
        return
    if a['res'] == 'OK':
        n = a['len']
        landed = c.get('TRUTHFNV') == fnv(pattern(n)) and \
            truth == pattern(n)[:len(truth)]
        if a['op'] == 'debugwrite':
            r['debug_leak'] += leaked
            return
        r['write_ok' if landed and not leaked else 'write_bad'] += 1
        r['leak'] += leaked
    elif 'ORIGFNV' in c:
        clean = c.get('TRUTHFNV') == c['ORIGFNV'] and not leaked
        r['refused_clean' if clean else 'refused_dirty'] += 1


def check_old(r, t, op, n, res, d):
    truth = t['TRUTH']
    if op == 'read':
        if res == 'OK':
            r['read_ok' if d == truth[:len(d)] else 'read_bad'] += 1
        return
    leaked = 'OTHER' in t and 'ORIG' in t and t['OTHER'] != t['ORIG']
    if res == 'OK':
        landed = truth[:n] == pattern(n)
        if op == 'debugwrite':
            r['debug_leak'] += leaked
            return
        r['write_ok' if landed and not leaked else 'write_bad'] += 1
        r['leak'] += leaked
    elif 'ORIG' in t:
        clean = truth == t['ORIG'] and not leaked
        r['refused_clean' if clean else 'refused_dirty'] += 1


def span_read_ok(a):
    d = a['data'] or b''
    off = a['addr'] & 0xfff
    return any(d == span[k * 4096 + off:k * 4096 + off + len(d)] for k in range(64))


tot = dict.fromkeys(KEYS, 0)
for con in sorted(glob.glob(os.path.join(out, '*.con'))):
    log = con[:-4] + '.log'
    if not os.path.exists(log):
        continue
    name = os.path.basename(con)[:-4]
    tests = parse_con(con)
    segs, bounded = parse_log(log)
    if not any(segs):
        continue
    r = dict.fromkeys(KEYS, 0)
    if 'contend' in name:
        for a in (a for s in segs for a in s):
            if a['res'] != 'OK':
                continue
            d, off = a['data'], a['addr'] & 0xfff
            if any(d == data[k * 4096 + off:k * 4096 + off + len(d)] for k in range(4)):
                r['read_ok'] += 1
            else:
                r['read_bad'] += 1
    else:
        if bounded:
            pairs = list(zip(tests, segs))
        else:
            pairs = [(t, segs[0]) for t in tests]
        for t, seg in pairs:
            if t['bad']:
                for a in seg:
                    if a['addr'] in t['bad']:
                        r['par_refused' if a['res'] == 'UNAVAILABLE' else 'par_bad'] += 1
                    else:
                        r['par_ok' if a['res'] == 'OK' and span_read_ok(a) else 'par_bad'] += 1
                continue
            last = {}
            for a in seg:
                last[a['addr']] = a
            for addr, c in t['at'].items():
                if addr in last:
                    check(r, last[addr], c)
            for addr in t['noaccess']:
                if addr in last:
                    r['read_refused' if last[addr]['res'] != 'OK' else 'read_bad'] += 1
            if 'addr' in t and t['addr'] in last and 'TRUTH' in t:
                a = last[t['addr']]
                check_old(r, t, a['op'], a['len'], a['res'], a['data'])
            if bounded:
                known = set(t['at']) | t['noaccess'] | ({t['addr']} if 'addr' in t else set())
                r['unmatched'] += sum(a['addr'] not in known for a in seg)
    print(f'{name}: ' + ' '.join(f'{k}={v}' for k, v in r.items() if v))
    for k in tot:
        tot[k] += r[k]
print('TOTAL: ' + ' '.join(f'{k}={v}' for k, v in tot.items()))
bad = tot['read_bad'] + tot['write_bad'] + tot['leak'] + tot['refused_dirty'] + tot['par_bad']
sys.exit(1 if bad else 0)
