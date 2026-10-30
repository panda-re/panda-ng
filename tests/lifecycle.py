"""Public reset-hook ordering on ordinary/fast restore, refusal and hardware reset.

Run: python3 tests/lifecycle.py <image.json>
Uses the packaged pandare2 runtime. Set RRCB_HANDOFF=1 only for old fixtures
containing handoff snapshot extensions. Pending syscall slot coverage is in
static/lifecycle.py; this test exercises the real reset/load integration points.
"""
import os
import sys
import threading

from pandare2 import Panda
from testlib import Checks


RESET_PC = {'x86_64': 0xfffffff0, 'aarch64': 0, 'riscv64': 0x1000}


def main():
    capabilities = None
    if os.environ.get('RRCB_HANDOFF') == '1':
        capabilities = ('analysis', 'guest-hypercall', 'handoff')
    panda = Panda(generic=sys.argv[1], capabilities=capabilities)
    lib, ffi = panda.libpanda, panda.ffi
    # A real loaded owner is required; do not fabricate a plugin handle.
    panda.load_plugin('hypercaller')
    events, results = [], {}
    state = {'phase': 'initial', 'registered': False, 'armed': False}
    reset_seen = threading.Event()

    @ffi.callback('void(void *)')
    def cancel(opaque):
        events.append((state['phase'], 'cancel'))

    @panda.cb_after_loadvm(observe_only=True)
    def loaded(cpu):
        events.append((state['phase'], 'loaded'))

    @panda.cb_before_block_translate(observe_only=True)
    def translate(cpu, pc):
        if state['armed'] and pc == RESET_PC[panda.arch_name]:
            results['reset_vector'] = True
            reset_seen.set()
            panda.end_analysis()

    def stopped(fn):
        done = threading.Event()
        errors = []

        def run():
            try:
                fn()
            except Exception as error:
                errors.append(repr(error))
            finally:
                lib.panda_cont()
                done.set()
        # Off the vCPU threads, a stop request takes effect when the main loop
        # next checks its requests, after this iteration's queued functions. A
        # second hop runs fn on the next iteration, with every vCPU paused.
        panda.vm_stop()
        panda.queue_main_loop_wait_fn(lambda: panda.queue_main_loop_wait_fn(run))
        if not done.wait(120):
            raise TimeoutError('stopped main-loop operation')
        if errors:
            raise AssertionError(errors)

    def transition(phase, fn):
        state['phase'] = phase
        before = len(events)
        results[phase] = {'rc': fn()}
        results[phase]['events'] = [kind for _, kind in events[before:]]

    @panda.queue_blocking
    def drive():
        try:
            panda.revert_sync('root')

            def register():
                owner = lib.panda_get_plugin_by_name(b'hypercaller')
                assert owner != ffi.NULL
                assert lib.panda_register_transient_reset(owner, cancel, ffi.NULL)
                state['registered'] = True
                transition('fast-no-snapshot', lib.panda_fast_restore)
                transition('missing-load', lambda: lib.panda_revert(
                    ffi.new('char[]', b'lifecycle-does-not-exist')))
                transition('ordinary-load', lambda: lib.panda_revert(
                    ffi.new('char[]', b'root')))
                results['snap'] = lib.panda_fast_snap()
                transition('fast-restore', lib.panda_fast_restore)
                lib.panda_unregister_transient_reset(owner)
                lib.panda_unregister_transient_reset(owner)
                transition('unregistered-restore', lib.panda_fast_restore)
                assert lib.panda_register_transient_reset(owner, cancel, ffi.NULL)
            stopped(register)
            results['alive'] = 'lifecycle-alive' in panda.run_serial_cmd(
                "echo lifecycle-al''ive", timeout=120)
            state['phase'] = 'hardware-reset'
            panda.flush_tb()
            state['armed'] = True
            panda.reset()
            if not reset_seen.wait(120):
                raise TimeoutError('hardware reset vector')
        except Exception as error:
            results['error'] = repr(error)
            panda.end_analysis()

    panda.run()
    checks = Checks(panda.arch_name)
    checks.check('driver completed without error', 'error' not in results, results)
    checks.check('real owner registered', state['registered'])
    for phase in ('fast-no-snapshot', 'missing-load'):
        result = results.get(phase, {})
        checks.check(phase + ': rejected without cancellation or success notification',
                     result.get('rc') == -1 and result.get('events') == [], result)
    ordinary = results.get('ordinary-load', {})
    sequence = ordinary.get('events', [])
    checks.check('ordinary load: cancellation precedes exactly one success notification',
                 ordinary.get('rc') == 0 and sequence.count('cancel') >= 1
                 and sequence.count('loaded') == 1 and sequence[-1:] == ['loaded'], ordinary)
    checks.check('fast snapshot succeeds', results.get('snap') == 0, results)
    fast = results.get('fast-restore', {})
    checks.check('fast restore: exactly one cancellation then one success notification',
                 fast.get('rc') == 0 and fast.get('events') == ['cancel', 'loaded'], fast)
    unregistered = results.get('unregistered-restore', {})
    checks.check('unregister is idempotent and removes hook before another restore',
                 unregistered.get('rc') == 0 and unregistered.get('events') == ['loaded'],
                 unregistered)
    checks.check('guest executes after restores', results.get('alive'), results)
    hardware = [kind for phase, kind in events if phase == 'hardware-reset']
    checks.check('hardware reset cancels without load-success notification and reaches reset vector',
                 hardware == ['cancel'] and results.get('reset_vector'), hardware)
    print('LIFECYCLE events=' + repr(events), flush=True)
    sys.exit(checks.report('LIFECYCLE'))


if __name__ == '__main__':
    main()
