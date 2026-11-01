"""Guest writes in record/replay: a mutating hypercall handler is recorded, replay applies its writes.

Run: panda-python tests/hypercall_rr.py <debian-13-x86_64|aarch64|riscv64 image> [workdir]
The answers program (tests/programs/answers_<arch>.S) makes N hypercalls (MAGIC, 1, &buf, i);
the recording's handler writes a quad into buf (a guest memory write) and sets the return
register, and the program folds both into a checksum it reports with hypercall (MAGIC, 2,
checksum). Each answer is one hypercall event in the log. Replay runs without the handler,
then with it registered (it must not run), then again: each must reach the recording's checksum
at the same guest instruction counts, seen by an observe-only guest_hypercall callback. The
handler writes through the guest-write API, setting the return register with
panda.arch.set_retval and panda.guest.set_retval on alternate calls, so the program checks both.
"""
import json
import os
import subprocess
import sys
import tempfile

from testlib import Checks, MAGIC, hypercall_args, progress, upload

N = 16


def answer(i):
    """(quad written into buf, return value) for the program's i-th hypercall."""
    return 0x1000 + 3 * i, 7 * i + 1


EXPECTED = sum(sum(answer(i)) for i in range(N))


def child(mode, image, name, output):
    from pandare2 import Panda
    panda = Panda(generic=image, rr=True)
    result = {'answered': 0, 'seen': [], 'checksum': None}

    def observe(cpu):
        op, a, b, _ = hypercall_args(panda, cpu)
        if panda.libpanda.rr_on():
            result['seen'].append([int(op), panda.rr_get_guest_instr_count()])
        if op == 2:
            result['checksum'] = int(a)
        return op, a, b

    if mode in ('record', 'replay-mutator'):
        @panda.hypercall(MAGIC, mutates=True)
        def hypercall(cpu):
            op, a, b = observe(cpu) if mode == 'record' else hypercall_args(panda, cpu)[:3]
            if op == 1:
                quad, ret = answer(int(b))
                panda.virtual_memory_write(cpu, a, quad.to_bytes(8, 'little'))
                # Both return paths: pandare2's register names, and the engine's
                # panda_guest_set_retval (C's panda_set_retval, Rust's GuestWriter)
                if int(b) % 2:
                    panda.guest.set_retval(cpu, ret)
                else:
                    panda.arch.set_retval(cpu, ret)
                result['answered'] += 1
    if mode != 'record':
        # Observers run in replay before the recorded answer is applied, whatever the mutators
        @panda.cb_guest_hypercall(observe_only=True)
        def seen(cpu):
            if panda.arch.get_arg(cpu, 0, convention='syscall') == MAGIC:
                observe(cpu)
            return False

    if mode == 'record':
        @panda.queue_blocking
        def drive():
            panda.revert_sync('root')
            upload(panda, 'answers', panda.arch_name)
            panda.run_serial_cmd('true')
            panda.record(name)
            panda.wait_rr('record')
            result['output'] = panda.run_serial_cmd('/tmp/answers; echo rc=$?', timeout=600)
            panda.end_record()
            panda.wait_rr('off')
            panda.end_analysis()
        panda.run()
    else:
        panda.run_replay(name)
        result['serial'] = b'rc=0' in panda.replay_serial
    with open(output, 'w') as stream:
        json.dump(result, stream)


def main():
    image = sys.argv[1]
    arch = json.load(open(image))['arch']
    if arch not in ('x86_64', 'aarch64', 'riscv64'):
        print(f'SKIP hypercall_rr: no answers program for {arch}')
        return
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix='hypercall-rr-')
    os.makedirs(work, exist_ok=True)
    name = os.path.join(work, 'answers')
    checks = Checks(arch)
    runs = {}
    for mode in ('record', 'replay', 'replay-mutator', 'replay-again'):
        out = os.path.join(work, mode + '.json')
        progress(mode)
        process = subprocess.run([sys.executable, '-u', __file__, '--child', mode,
                                  image, name, out], timeout=900)
        checks.check(mode + ': completes', process.returncode == 0, process.returncode)
        if process.returncode:
            sys.exit(checks.report('HYPERCALL_RR'))
        runs[mode] = json.load(open(out))
    record = runs['record']
    checks.check('record: the handler answered every hypercall', record['answered'] == N,
                 record['answered'])
    checks.check('record: the program saw the answers', record['checksum'] == EXPECTED,
                 (record['checksum'], EXPECTED))
    checks.check('record: the program completed', 'rc=0' in record.get('output', ''))
    for mode in ('replay', 'replay-mutator', 'replay-again'):
        replay = runs[mode]
        checks.check(mode + ': no handler ran', replay['answered'] == 0, replay['answered'])
        checks.check(mode + ': the recorded answers reached the program',
                     replay['checksum'] == EXPECTED, (replay['checksum'], EXPECTED))
        checks.check(mode + ': the same hypercalls at the same instruction counts',
                     replay['seen'] == record['seen'], (replay['seen'][:4], record['seen'][:4]))
        checks.check(mode + ': the program completed', replay.get('serial'))
    sys.exit(checks.report('HYPERCALL_RR work=' + work))


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--child':
        child(*sys.argv[2:6])
    else:
        main()
