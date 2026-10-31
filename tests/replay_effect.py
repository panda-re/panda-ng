"""Real ND-stream deferred RAM and register effect fixture (record producer, fresh replay).

Run: panda-python tests/replay_effect.py <debian-13-x86_64.json> [workdir]
No native compilation. Uses the existing calls guest; modifies only the first
read-only label-table quad after it is resident, not program instructions.
The recording queues two overlapping physical writes and an r12 write (the calls
guest leaves r12 alone between hypercalls 1 and 2) while main-thread/stopped;
replay installs observations only, never calls the effect request API.
"""
import json
import os
import subprocess
import sys
import tempfile

from testlib import Checks, MAGIC, hypercall_args, progress, upload

BASE = bytes.fromhex('8877665544332211')
TAIL = bytes.fromhex('aabbccdd')
EXPECTED = BASE[:2] + TAIL + BASE[6:]
R12 = 0x1122334455667788
R12_GDB = 12    # x86_64 gdbstub order: rax rbx rcx rdx rsi rdi rbp rsp r8..r15


def child(mode, image, name, output):
    from pandare2 import Panda
    panda = Panda(generic=image, rr=True)
    if panda.arch_name != 'x86_64':
        raise RuntimeError('effect fixture currently specifies x86_64 calls guest')
    # Exercise the generated public binding, not a ctypes fallback that could
    # hide a missing SDK declaration. Do not acquire replay/BQL manually.
    request = panda.libpanda.panda_analysis_ram_write
    request_reg = panda.libpanda.panda_analysis_reg_write
    result = {'observations': [], 'requests': 0, 'admitted': [], 'deferred': False}
    waiting = {'done': False}

    def observe(cpu, op, addr):
        physical = int(panda.virt_to_phys(cpu, addr))
        value = bytes(panda.physical_memory_read(physical, 8))
        result['observations'].append({
            'op': op, 'icount': panda.rr_get_guest_instr_count(),
            'physical': physical, 'value': value.hex(),
            'r12': int(panda.arch.get_reg(cpu, 'R12'))})
        return physical, value

    @panda.hypercall(MAGIC, observe_only=True)
    def hypercall(cpu):
        if not panda.libpanda.rr_on():
            return
        op, labels, _, _ = hypercall_args(panda, cpu)
        physical, value = observe(cpu, op, labels)
        if mode != 'record' or op != 1:
            return
        # vCPU stop request is acted on before main_loop_wait callbacks. The
        # main-loop callback then runs with main/BQL/replay lock and every CPU
        # stopped; request admission itself verifies these facts. No locks are
        # taken by the Python thread and no mutation is made in this hypercall.
        panda.vm_stop()

        def enqueue_stopped():
            first = panda.ffi.new('uint8_t[]', BASE)
            second = panda.ffi.new('uint8_t[]', TAIL)
            reg = panda.ffi.new('uint8_t[]', R12.to_bytes(8, 'little'))
            result['requests'] += 3
            result['admitted'] = [bool(request(physical, first, len(BASE))),
                                  bool(request(physical + 2, second, len(TAIL))),
                                  bool(request_reg(0, R12_GDB, reg, 8))]
            result['deferred'] = bytes(panda.physical_memory_read(physical, 8)) == value
            waiting['done'] = True
            # Resume only after both ordered events have entered the real ND
            # queue. Existing scheduler drains effects before its next budget.
            panda.libpanda.panda_cont()

        panda.queue_main_loop_wait_fn(enqueue_stopped)

    if mode == 'record':
        @panda.queue_blocking
        def drive():
            panda.revert_sync('root')
            upload(panda, 'calls', panda.arch_name)
            panda.run_serial_cmd('true')
            panda.record(name)
            panda.wait_rr('record')
            result['output'] = panda.run_serial_cmd('/tmp/calls; echo rc=$?', timeout=600)
            panda.end_record()
            panda.wait_rr('off')
            panda.end_analysis()
        panda.run()
        if not waiting['done']:
            raise AssertionError('stopped main-loop request callback never ran')
    else:
        # No queue callback, producer request or stop is installed in replay.
        # Calls op2 must nevertheless see ordered recorded payload application.
        panda.run_replay(name)
        result['serial'] = b'rc=0' in panda.replay_serial
    with open(output, 'w') as stream:
        json.dump(result, stream)


def main():
    image = sys.argv[1]
    arch = json.load(open(image))['arch']
    if arch != 'x86_64':
        print('SKIP replay_effect: fixture currently specifies x86_64 calls guest')
        return
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix='replay-effect-')
    os.makedirs(work, exist_ok=True)
    name = os.path.join(work, 'effect')
    checks = Checks(arch)
    runs = {}
    for mode in ('record', 'replay', 'replay-again'):
        out = os.path.join(work, mode + '.json')
        progress(mode + ': real ordered RAM and register effects')
        process = subprocess.run([sys.executable, '-u', __file__, '--child',
                                  'record' if mode == 'record' else 'replay',
                                  image, name, out], timeout=900)
        checks.check(mode + ': completes', process.returncode == 0, process.returncode)
        if process.returncode:
            sys.exit(checks.report('REPLAY_EFFECT'))
        runs[mode] = json.load(open(out))
    record = runs['record']
    checks.check('all ordered requests admitted while stopped',
                 record['requests'] == 3 and record['admitted'] == [True, True, True])
    checks.check('admission does not synchronously mutate RAM', record['deferred'])
    observations = record['observations']
    checks.check('record observes both guest boundaries',
                 [o['op'] for o in observations] == [1, 2], observations)
    checks.check('ordered overlapping writes visible before next boundary',
                 len(observations) == 2 and observations[1]['value'] == EXPECTED.hex(),
                 observations)
    checks.check('register write applied before next boundary',
                 len(observations) == 2 and observations[0]['r12'] != R12 and
                 observations[1]['r12'] == R12, observations)
    checks.check('record guest completes', 'rc=0' in record.get('output', ''))
    for mode in ('replay', 'replay-again'):
        replay = runs[mode]
        checks.check(mode + ': no producer requests', replay['requests'] == 0 and not replay['admitted'])
        checks.check(mode + ': same RAM/physical addresses/guest instruction counts',
                     replay['observations'] == observations, replay['observations'])
        checks.check(mode + ': guest completes', replay.get('serial'))
    sys.exit(checks.report('REPLAY_EFFECT work=' + work))


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--child':
        child(*sys.argv[2:6])
    else:
        main()
