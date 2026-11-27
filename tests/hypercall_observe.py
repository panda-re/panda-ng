"""after_guest_hypercall: observers see every hypercall and its answer, live, recording and replay.

Run: panda-python tests/hypercall_observe.py <debian-13-x86_64|aarch64|riscv64 image> [workdir]
The answers program (tests/programs/answers_<arch>.S, see tests/hypercall_rr.py) makes N
hypercalls (MAGIC, 1, &buf, i), then (MAGIC, 2, checksum). An embedding-API consumer
(panda_set_guest_hypercall_callback, as Penguin answers its hypercalls) answers the op 1 calls:
it writes a quad into buf through the guest-write API and returns a value. It declines op 2,
which a pandare2 hypercall handler (a guest_hypercall callback) answers instead. An observe-only
after_guest_hypercall callback logs (instruction count, number, arguments, answerer, return
value, logged writes) for every hypercall, and an observe-only guest_hypercall callback counts
them. The run is made live, recorded, then replayed with only the observers: the replay must see
the recording's sequence exactly, and the live run the same calls and answers (no writes logged).
"""
import json
import os
import subprocess
import sys
import tempfile

from testlib import Checks, MAGIC, progress, upload

N = 16
PLUGIN_RET = 0x77


def answer(i):
    """(quad written into buf, return value) for the program's i-th hypercall."""
    return 0x1000 + 3 * i, 7 * i + 1


EXPECTED = sum(sum(answer(i)) for i in range(N))


def embedder(panda, result):
    """Answer op 1 through the embedding API, from a cffi handle of our own (like Penguin)."""
    from cffi import FFI
    ffi = FFI()
    ffi.cdef("""
        typedef struct CPUState CPUState;
        typedef int (*panda_guest_hypercall_cb_t)(CPUState *cs, uint64_t nr, uint64_t a0,
            uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t *ret,
            void *opaque);
        void panda_set_guest_hypercall_callback(panda_guest_hypercall_cb_t cb, void *opaque);
        void panda_register_guest_hypercall(uint64_t nr);
        int panda_guest_write_virt(CPUState *cpu, uint64_t addr, const void *buf, size_t len);
    """)
    lib = ffi.dlopen(panda.libpanda_path)

    @ffi.callback("panda_guest_hypercall_cb_t")
    def on_hypercall(cpu, nr, op, addr, i, a3, a4, a5, ret, opaque):
        if nr != MAGIC or op != 1:
            return 1
        quad, value = answer(int(i))
        data = quad.to_bytes(8, 'little')
        if lib.panda_guest_write_virt(cpu, addr, data, len(data)) != 0:
            return 1
        ret[0] = value
        result['answered'] += 1
        return 0

    lib.panda_register_guest_hypercall(MAGIC)
    lib.panda_set_guest_hypercall_callback(on_hypercall, ffi.NULL)
    return ffi, lib, on_hypercall      # keep alive


def child(mode, image, name, output):
    from pandare2 import Panda
    panda = Panda(generic=image, capabilities=['record'] if mode == 'record' else None)
    result = {'answered': 0, 'plugin': 0, 'seen': [], 'before': 0, 'checksum': None}
    keep = None

    @panda.cb_after_guest_hypercall(observe_only=True)
    def after(cpu, hc):
        if hc.nr != MAGIC or panda.libpanda.rr_on() != (mode != 'live'):
            return
        args = [int(hc.args[i]) for i in range(6)]
        writes = [[int(w.kind), int(w.result), int(w.reg), int(w.addr),
                   bytes(panda.ffi.buffer(w.data, w.len)).hex() if w.len else '']
                  for w in (hc.writes[i] for i in range(hc.nwrites))]
        icount = panda.rr_get_guest_instr_count() if mode != 'live' else None
        result['seen'].append([icount, int(hc.nr), args, int(hc.answerer), bool(hc.handled),
                               int(hc.ret), bool(hc.writes_known), writes])
        if args[0] == 2:
            result['checksum'] = args[1]

    @panda.cb_guest_hypercall(observe_only=True)
    def before(cpu):
        if panda.arch.get_arg(cpu, 0, convention='syscall') == MAGIC and \
                panda.libpanda.rr_on() == (mode != 'live'):
            result['before'] += 1
        return False

    if mode != 'replay':
        keep = embedder(panda, result)

        @panda.hypercall(MAGIC, mutates=True)
        def op2(cpu):
            panda.arch.set_retval(cpu, PLUGIN_RET)
            result['plugin'] += 1

        @panda.queue_blocking
        def drive():
            panda.revert_sync('root')
            upload(panda, 'answers', panda.arch_name)
            panda.run_serial_cmd('true')
            if mode == 'record':
                panda.record(name)
                panda.wait_rr('record')
            result['output'] = panda.run_serial_cmd('/tmp/answers; echo rc=$?', timeout=600)
            if mode == 'record':
                panda.end_record()
                panda.wait_rr('off')
            panda.end_analysis()
        panda.run()
    else:
        panda.run_replay(name)
        result['output'] = 'rc=0' if b'rc=0' in panda.replay_serial else ''
    del keep
    with open(output, 'w') as stream:
        json.dump(result, stream)


def main():
    image = sys.argv[1]
    arch = json.load(open(image))['arch']
    if arch not in ('x86_64', 'aarch64', 'riscv64'):
        print(f'SKIP hypercall_observe: no answers program for {arch}')
        return
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix='hypercall-observe-')
    os.makedirs(work, exist_ok=True)
    name = os.path.join(work, 'answers')
    checks = Checks(arch)
    runs = {}
    for mode in ('live', 'record', 'replay'):
        out = os.path.join(work, mode + '.json')
        progress(mode)
        process = subprocess.run([sys.executable, '-u', __file__, '--child', mode,
                                  image, name, out], timeout=900)
        checks.check(mode + ': completes', process.returncode == 0, process.returncode)
        if process.returncode:
            sys.exit(checks.report('HYPERCALL_OBSERVE'))
        runs[mode] = json.load(open(out))
    record, replay, live = runs['record'], runs['replay'], runs['live']
    for mode in ('live', 'record'):
        run = runs[mode]
        checks.check(mode + ': the embedder answered every op 1 call', run['answered'] == N,
                     run['answered'])
        checks.check(mode + ': the plugin answered op 2', run['plugin'] == 1, run['plugin'])
    for mode, run in runs.items():
        seen = run['seen']
        checks.check(mode + ': the program completed', 'rc=0' in run.get('output', ''))
        checks.check(mode + ': the observer saw every hypercall', len(seen) == N + 1, len(seen))
        checks.check(mode + ': observe-only guest_hypercall saw every hypercall',
                     run['before'] == N + 1, run['before'])
        checks.check(mode + ': the program saw the answers', run['checksum'] == EXPECTED,
                     (run['checksum'], EXPECTED))
        ok = len(seen) == N + 1
        for i, (_, nr, args, answerer, handled, ret, known, writes) in enumerate(seen[:N]):
            quad, value = answer(i)
            ok &= (args[0], args[2], answerer, handled, ret) == (1, i, 1, True, value)
            ok &= known == (mode != 'live')
            # a virtual write is logged per page, by guest physical address (kind PHYS)
            ok &= (not known) or (len(writes) == 1 and writes[0][0] == 2 and writes[0][1] == 0
                                  and writes[0][4] == quad.to_bytes(8, 'little').hex())
        if seen:
            _, nr, args, answerer, handled, ret, known, writes = seen[-1]
            ok &= (args[0], answerer, handled, ret) == (2, 2, True, PLUGIN_RET)
        checks.check(mode + ': numbers, arguments, answerers, answers and writes as expected', ok,
                     seen[:2])
    checks.check('replay: the recording\'s sequence, at the same instruction counts and values',
                 replay['seen'] == record['seen'], (replay['seen'][:2], record['seen'][:2]))
    strip = lambda seen: [s[1:6] for s in seen]
    checks.check('live: the same calls and answers as the recording',
                 strip(live['seen']) == strip(record['seen']))
    checks.check('replay: no consumer ran', replay['answered'] == 0 and replay['plugin'] == 0)
    sys.exit(checks.report('HYPERCALL_OBSERVE work=' + work))


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--child':
        child(*sys.argv[2:6])
    else:
        main()
