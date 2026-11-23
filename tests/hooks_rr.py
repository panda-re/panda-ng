"""Hooks in record/replay: the same hits, in the same order, on replay.

Run: panda-python tests/hooks_rr.py <debian-13-x86_64|ubuntu-24.04-aarch64 image> [workdir]

tests/hooks.py's program (tests/programs/hooks_<arch>.S) runs once to find its labels, then is
recorded with observe-only hooks (panda.hook) at h0, h1 (adjacent instructions in one block) and x1
(on the second page of a block that crosses a page), and replayed twice with the same hooks. Each
hit notes the hook and the program's accumulator register, which every instruction of the
hooked blocks updates; each of the program's hypercalls notes the instruction count. Every replay
must see exactly what the recording saw: 3R hits per hook, in the same order, with the same
register values, and the hypercalls at the same instruction counts.
"""
import json
import os
import subprocess
import sys
import tempfile

from testlib import Checks, MAGIC, progress, upload

PANDA_TEST = dict(area="hooks", tier="guest", fixture="cloud-debian-13", arches=["x86_64"],
                  plugins=["hooks"], args=["{fixture}", "{work}"])

R = 100
LABELS = ["h0", "h1", "h2", "p0", "x0", "xs", "x1"]
ACC = {"x86_64": "R14", "aarch64": "X22"}
HOOKED = ["h0", "h1", "x1"]


def child(mode, image, name, output, labels):
    from pandare2 import Panda
    panda = Panda(generic=image, capabilities=['record'] if mode == 'record' else None)
    arch = panda.arch_name
    labels = json.loads(labels)
    st = {"asid": None, "seq": [], "labels": labels}

    def on_hypercall(cpu):
        op = panda.arch.get_arg(cpu, 2, convention="syscall")
        if op == 1 and not st["labels"]:
            table = panda.arch.get_arg(cpu, 3, convention="syscall")
            raw = bytes(panda.virtual_memory_read(cpu, table, 8 * len(LABELS)))
            st["labels"] = {k: int.from_bytes(raw[8 * i:8 * i + 8], "little")
                            for i, k in enumerate(LABELS)}
        if op == 1:
            st["asid"] = panda.current_asid(cpu)
        if panda.libpanda.rr_on():
            st["seq"].append(["hc", int(op), panda.rr_get_guest_instr_count()])

    def hook(k):
        @panda.hook(labels[k], kernel=False, observe_only=True)
        def h(cpu, tb, hook):
            # the recording or a replay, not the run before the recording starts
            if panda.libpanda.rr_on() and panda.current_asid(cpu) == st["asid"]:
                st["seq"].append([k, panda.arch.get_reg(cpu, ACC[arch])])

    if labels:
        for k in HOOKED:
            hook(k)
    if mode == 'record':
        panda.hypercall(MAGIC, observe_only=True)(on_hypercall)

        @panda.queue_blocking
        def drive():
            panda.revert_sync('root')
            upload(panda, 'hooks', arch)
            st["first"] = panda.run_serial_cmd('/tmp/hooks; echo rc=$?', timeout=600)
            if labels:
                panda.record(name)
                panda.wait_rr('record')
                st['output'] = panda.run_serial_cmd('/tmp/hooks; echo rc=$?', timeout=600)
                panda.end_record()
                panda.wait_rr('off')
            panda.end_analysis()
        panda.run()
    else:
        # In replay the hypercall handlers don't run: observe the hypercalls instead.
        @panda.cb_guest_hypercall(observe_only=True)
        def seen(cpu):
            if panda.arch.get_arg(cpu, 0, convention='syscall') == MAGIC:
                on_hypercall(cpu)
            return False
        panda.run_replay(name)
        st['output'] = 'rc=0' if b'rc=0' in panda.replay_serial else ''
    with open(output, 'w') as stream:
        json.dump(st, stream)


def run(mode, image, name, out, labels):
    p = subprocess.run([sys.executable, '-u', __file__, '--child', mode, image, name, out,
                        json.dumps(labels)], timeout=1200)
    return p.returncode, (json.load(open(out)) if p.returncode == 0 else None)


def main():
    image = sys.argv[1]
    arch = json.load(open(image))['arch']
    if arch not in ACC:
        print(f'SKIP hooks_rr: no hooks program for {arch}')
        return
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix='hooks-rr-')
    os.makedirs(work, exist_ok=True)
    name = os.path.join(work, 'hooks')
    checks = Checks(arch)
    progress('labels')
    rc, first = run('record', image, name, os.path.join(work, 'labels.json'), {})
    checks.check('the program runs', rc == 0 and first and 'rc=0' in first.get('first', ''), rc)
    if not (first and first.get('labels')):
        sys.exit(checks.report('HOOKS_RR'))
    labels = first['labels']
    runs = {}
    for mode in ('record', 'replay', 'replay-again'):
        progress(mode)
        rc, runs[mode] = run(mode, image, name, os.path.join(work, mode + '.json'), labels)
        checks.check(mode + ': completes', rc == 0, rc)
        if rc:
            sys.exit(checks.report('HOOKS_RR'))
    rec = runs['record']['seq']
    hits = {k: sum(1 for x in rec if x[0] == k) for k in HOOKED}
    checks.check('record: 3R hits per hook', hits == {k: 3 * R for k in HOOKED}, hits)
    checks.check('record: hypercalls 1 3 4 2', [x[1] for x in rec if x[0] == 'hc'] == [1, 3, 4, 2],
                 [x for x in rec if x[0] == 'hc'])
    checks.check('record: the program completed', 'rc=0' in runs['record'].get('output', ''))
    for mode in ('replay', 'replay-again'):
        rep = runs[mode]['seq']
        diff = next(((i, a, b) for i, (a, b) in enumerate(zip(rep, rec)) if a != b), None)
        checks.check(mode + ': the same hits in the same order with the same register values, '
                     'and the hypercalls at the same instruction counts',
                     rep == rec, (len(rep), len(rec), diff))
        checks.check(mode + ': the program completed', runs[mode].get('output'))
    sys.exit(checks.report(f'HOOKS_RR hits={hits} work={work}'))


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--child':
        child(*sys.argv[2:7])
    else:
        main()
