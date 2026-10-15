#!/usr/bin/env python3
"""summary.py <outdir>: one markdown row per kernel from a full.sh run."""
import glob, os, re, subprocess, sys
d = sys.argv[1]
def rd(p):
    try:
        return open(p, errors='replace').read().replace('\r', '')
    except OSError:
        return ''
kernels = sorted({re.match(r'(.+?-\d+\.\d+)-', os.path.basename(f)).group(1)
                  for f in glob.glob(f'{d}/*-reads.con')})
def res(k, n, pat):
    return len(re.findall(pat, rd(f'{d}/{k}-{n}.log')))
def con(k, n, pat):
    return len(re.findall(pat, rd(f'{d}/{k}-{n}.con'), re.M))
def exits(k, n):
    return ','.join(re.findall(r'^EXIT \S+ (\d+)', rd(f'{d}/{k}-{n}.con'), re.M)) or '-'
v = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), 'verify.py'), d],
                   capture_output=True, text=True).stdout
def ver(k, n, key):
    m = re.search(rf'^{re.escape(k)}-{n}:.*\b{key}=(\d+)', v, re.M)
    return int(m.group(1)) if m else 0
print('| kernel | reads | writes | user hazards raw (exit codes) | +guard survived | write hazards raw | +guard survived (refused clean) | kern bad addr raw | contention raw | contention +guard (claims) | debug-API writes leaking |')
print('|---|---|---|---|---|---|---|---|---|---|---|')
for k in kernels:
    def cont(n):
        m = re.search(r'CONTEND ok=(\d+/\d+)', rd(f'{d}/{k}-{n}.con'))
        return m.group(1) if m else ('panic' if 'Kernel panic' in rd(f'{d}/{k}-{n}.con') else 'none')
    kraw = sum('Kernel panic' in rd(f'{d}/{k}-{n}.con') for n in ('kern-novma-raw', 'kern-wro-raw'))
    print(f"| {k} | {ver(k,'reads','read_ok')}/4 | {ver(k,'writes','write_ok')}/6 "
          f"| {exits(k,'haz-user-raw')} | {con(k,'haz-vext',r'^SURVIVED')}/5 "
          f"| {exits(k,'whaz-user-raw')} | {con(k,'whaz-vext',r'^SURVIVED')}/5 ({ver(k,'whaz-vext','refused_clean')}) "
          f"| {kraw}/2 panic | {cont('contend-raw')} | {cont('contend-vext')} ({res(k,'contend-vext','claimed')}) "
          f"| {ver(k,'debugw','debug_leak')}/2 |")
print()
print('verify.py: ' + v.strip().splitlines()[-1])
if not glob.glob(f'{d}/*-par.log'):
    sys.exit(0)
print()
print('| kernel | span reads (faults) | span writes | span write, last page read-only | multi: reads / writes / refused clean | multi, one write at a time: landed / refused | 2 vCPUs: OK / refused / wrong | most faults outstanding at once |')
print('|---|---|---|---|---|---|---|---|')
for k in kernels:
    span_f = ','.join(re.findall(r'op=read .*len=\d+ result=OK faults=(\d+)', rd(f'{d}/{k}-span.log'))) or '-'
    mp = re.search(r'max_concurrent_faults=(\d+)', rd(f'{d}/{k}-par.log'))
    pdone = 'done' if len(re.findall(r'^PAR done t0=(\d+) t1=\1 of \1', rd(f'{d}/{k}-par.con'), re.M)) == 2 else 'INCOMPLETE'
    print(f"| {k} | {ver(k,'span','read_ok')}/4 ({span_f}) | {ver(k,'wspan','write_ok')}/2 "
          f"| {ver(k,'wspan','refused_clean')}/2 refused clean "
          f"| {ver(k,'multi','read_ok')} / {ver(k,'multi','write_ok')} / {ver(k,'multi','refused_clean')} "
          f"| {ver(k,'multi-seq','write_ok')} / {ver(k,'multi-seq','refused_clean')} "
          f"| {ver(k,'par','par_ok')} / {ver(k,'par','par_refused')} / {ver(k,'par','par_bad')} ({pdone}) "
          f"| {mp.group(1) if mp else '-'} |")

