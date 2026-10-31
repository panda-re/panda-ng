"""Replay observers retain the recording's execution while mutation attempts fail.

Run: panda-python tests/replay_guard.py <image.json> [workdir]
Set RRCB_HANDOFF=1 only for fixtures whose snapshots contain handoff extensions.
Uses rrcb's guest call graph and compares block/instruction/memory hashes and
hypercall instruction counts against a clean replay. Attempts native writes,
faulting reads, syscall skip/argument/return rewrites, late mutator registration
and re-enabling a mutator registered before record/replay. A separate recording
checks that unlogged mutations are refused while recording is active.
"""
import json
import os
import subprocess
import sys
import tempfile

from testlib import Checks, progress


def rejected(call, error):
    try:
        call()
    except error:
        return True
    return False


def install(panda, name):
    result = {}
    for suffix, opts in [('mutator', {}), ('no_rr', dict(observe_only=True, modes=('analysis',)))]:
        @panda.cb_after_loadvm(name='unsafe', **opts)
        def unsafe(cpu):
            raise AssertionError('unsafe callback ran')
        result[suffix + '_python_entry'] = rejected(lambda: panda.run_replay(name), ValueError)
        cname = panda.ffi.new('char[]', name.encode())
        result[suffix + '_native_entry'] = panda.libpanda.panda_replay_begin(cname) != 0
        result[suffix + '_python_rr_preflight'] = rejected(panda._check_rr_observers, ValueError)
        result[suffix + '_native_record_entry'] = panda.libpanda.panda_record_begin(cname, panda.ffi.NULL) != 0
        panda.delete_callback('unsafe')

    @panda.cb_after_loadvm(name='disabled_mutator', enabled=False)
    def disabled_mutator(cpu):
        raise AssertionError('disabled mutator ran')
    @panda.cb_after_loadvm(name='disabled_no_rr', enabled=False, observe_only=True, modes=('analysis',))
    def disabled_no_rr(cpu):
        raise AssertionError('mode-incompatible callback ran')
    return result


def exercise(panda, cpu, result):
    ffi, C = panda.ffi, panda.libpanda
    pc = panda.arch.get_pc(cpu)
    before = bytes(panda.virtual_memory_read(cpu, pc, 8))
    bad = bytes(x ^ 0xff for x in before)
    data = ffi.new('uint8_t[]', bad)
    phys = panda.virt_to_phys(cpu, pc)
    result['python_virtual_write'] = rejected(lambda: panda.virtual_memory_write(cpu, pc, bad), PermissionError)
    result['python_physical_write'] = rejected(lambda: panda.physical_memory_write(phys, bad), PermissionError)
    result['python_pc'] = rejected(lambda: panda.arch.set_pc(cpu, 0), PermissionError)
    reg = 'RAX' if panda.arch_name == 'x86_64' else 'X0'
    rval = panda.arch.get_reg(cpu, reg)
    result['python_register'] = rejected(lambda: panda.arch.set_reg(cpu, reg, 123), PermissionError)
    C.panda_set_retval(cpu, 123)
    result['native_retval_unchanged'] = panda.arch.get_reg(cpu, reg) == rval
    arg = C.panda_get_syscall_arg(cpu, 1)
    C.panda_set_syscall_arg(cpu, 1, arg ^ 0xff)
    result['native_syscall_arg_unchanged'] = C.panda_get_syscall_arg(cpu, 1) == arg
    result['native_virtual_write'] = C.panda_virtual_memory_write_external(cpu, pc, ffi.cast('char *', data), 8) != 0
    result['native_physical_write'] = C.panda_physical_memory_write_external(phys, data, 8) != 0

    b = C.g_byte_array_new()
    C.g_byte_array_append(b, data, 8)
    try:
        result['plugin_virtual_write'] = not C.qemu_plugin_write_memory_vaddr(pc, b)
        result['plugin_physical_write'] = C.qemu_plugin_write_memory_hwaddr(phys, b) == C.QEMU_PLUGIN_HWADDR_OPERATION_ACCESS_DENIED
        regs = C.qemu_plugin_get_registers()
        try:
            desc = ffi.cast('qemu_plugin_reg_descriptor *', regs.data)
            handle = next(desc[i].handle for i in range(regs.len) if not desc[i].is_readonly)
            result['plugin_register_write'] = not C.qemu_plugin_write_register(handle, b)
            result['plugin_mutable_register_pointer'] = C.qemu_plugin_register_ptr(handle, ffi.NULL) == ffi.NULL
        finally:
            C.g_array_unref(regs)
        result['faulting_read'] = C.qemu_plugin_read_memory_vaddr_fault(pc, b, 8, C.QEMU_PLUGIN_VADDR_USER, True) == C.QEMU_PLUGIN_VADDR_BAD_MODE
        result['faulting_write'] = C.qemu_plugin_write_memory_vaddr_fault(pc, b, C.QEMU_PLUGIN_VADDR_USER, True) == C.QEMU_PLUGIN_VADDR_BAD_MODE
        # A normal RAM read is allowed; device reads must not consume unlogged input.
        result['plugin_ram_read'] = C.qemu_plugin_read_memory_hwaddr(phys, b, 8) == C.QEMU_PLUGIN_HWADDR_OPERATION_OK
        mmio = 0xfed00000 if panda.arch_name == 'x86_64' else 0x09000000
        result['plugin_device_read'] = C.qemu_plugin_read_memory_hwaddr(mmio, b, 1) == C.QEMU_PLUGIN_HWADDR_OPERATION_ACCESS_DENIED
    finally:
        C.g_byte_array_unref(b)
    result['plugin_pc'] = not C.qemu_plugin_try_set_pc(0)
    progress('guard: checking unchanged CPU and RAM')
    result['state_unchanged'] = (panda.arch.get_pc(cpu) == pc and
        panda.arch.get_reg(cpu, reg) == rval and bytes(panda.virtual_memory_read(cpu, pc, 8)) == before)

    progress('guard: checking late registrations')
    result['late_enable'] = rejected(lambda: panda.enable_callback('disabled_mutator'), ValueError)
    result['late_enable_stays_disabled'] = not panda.is_callback_enabled('disabled_mutator')
    result['late_mode_enable'] = rejected(lambda: panda.enable_callback('disabled_no_rr'), ValueError)
    result['late_hypercall_registration'] = rejected(lambda: panda.hypercall(0x1234)(lambda cpu: None), ValueError)
    result['late_registration'] = rejected(lambda: panda.cb_asid_changed(name='late_mutator')(lambda *a: False), ValueError)
    cb = ffi.new('panda_cb *', {'after_loadvm': panda.callback.after_loadvm(lambda cpu: None)})
    # No Python policy wrapper: exercise the native registry directly.
    owner = ffi.cast('void *', 0xfeed0001)
    result['native_late_registration'] = not C.panda_register_callback_modes_helper(owner, C.PANDA_CB_AFTER_LOADVM, cb, 7, False)
    result['unsupported_mode'] = not C.panda_register_callback_modes_helper(owner, C.PANDA_CB_ASID_CHANGED, cb, 8, True)

    @panda.cb_before_handle_exception(name='observer_exception', observe_only=True)
    def exception(cpu, index):
        return -1  # an observer's return must not swallow an exception

    # The syscall-event API has mutable args/skip/ret fields. Their effects must
    # be discarded even when a native consumer asks for them directly.
    counts = {'enter': 0, 'return': 0}
    result['syscalls'] = counts
    @ffi.callback('qemu_plugin_syscall_event_cb_t')
    def enter(ident, vcpu, sc, opaque):
        counts['enter'] += 1
        sc.args[0] = 0x12345678
        sc.skip = True
        sc.ret = -1
    @ffi.callback('qemu_plugin_syscall_event_cb_t')
    def ret(ident, vcpu, sc, opaque):
        counts['return'] += 1
        sc.ret = -1
    result['syscall_registration'] = bool(C.qemu_plugin_register_syscall_event_cb(0, enter, ret, ffi.NULL))
    abi = C.QEMU_PLUGIN_SYSCALL_ABI_X86_64 if panda.arch_name == 'x86_64' else C.QEMU_PLUGIN_SYSCALL_ABI_AARCH64
    result['syscall_watch'] = bool(C.qemu_plugin_syscall_watch(0, abi, -1, 3))
    panda._guard_refs = (enter, ret)  # retain cffi callbacks until the engine stops


def live(image):
    """A live mutating hypercall keeps its result, even when it removes itself."""
    from pandare2 import Panda
    from testlib import MAGIC, hypercall_args, in_program, upload
    capabilities = None
    if os.environ.get('RRCB_HANDOFF') == '1':
        capabilities = ('analysis', 'guest-hypercall', 'handoff')
    panda = Panda(generic=image, capabilities=capabilities)
    checks = Checks(panda.arch_name)
    reg = 'RAX' if panda.arch_name == 'x86_64' else 'X0'
    seen = {}

    @panda.cb_insn_translate(name='it', enabled=False, observe_only=True)
    def translate(cpu, pc):
        return in_program(pc)

    @panda.cb_insn_exec(name='ie', enabled=False, observe_only=True)
    def instruction(cpu, pc):
        if in_program(pc) and 'value' not in seen:
            seen['value'] = panda.arch.get_reg(cpu, reg)
            seen['observer_write'] = rejected(lambda: panda.arch.set_reg(cpu, reg, 0), PermissionError)
            panda.disable_callback('it')
            panda.disable_callback('ie')

    @panda.hypercall(MAGIC)
    def mutate(cpu):
        if hypercall_args(panda, cpu)[0] == 1:
            panda.arch.set_reg(cpu, reg, 0x12345678)
            panda.enable_callback('it')
            panda.enable_callback('ie')
            panda.disable_hypercall(mutate)

    checks.check('native bridge refuses replay with a live mutator',
                 panda.libpanda.panda_replay_begin(b'unused') != 0)

    @panda.queue_blocking
    def drive():
        panda.revert_sync('root')
        upload(panda, 'calls', panda.arch_name)
        seen['output'] = panda.run_serial_cmd('/tmp/calls; echo rc=$?')
        panda.end_analysis()
    panda.run()
    checks.check('guest sees the live hypercall return value', seen.get('value') == 0x12345678, seen.get('value'))
    checks.check('live observer rejects guest writes', seen.get('observer_write'))
    checks.check('live guest completes', 'rc=0' in seen.get('output', ''))
    sys.exit(checks.report('LIVE_HYPERCALL'))


def main():
    image = sys.argv[1]
    work = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix='replay-guard-')
    os.makedirs(work, exist_ok=True)
    name = os.path.join(work, 'calls')
    arch = json.load(open(image))['arch']
    if arch not in ('x86_64', 'aarch64'):
        print('SKIP replay_guard: the mutation fixtures cover x86_64 and aarch64')
        return
    checks = Checks(arch)
    runs = {}
    for mode, callbacks in [('record', '1'), ('clean', '1'), ('guard', 'guard')]:
        out = os.path.join(work, mode + '.json')
        progress(mode)
        proc = subprocess.run([sys.executable, '-u', os.path.join(os.path.dirname(__file__), 'rrcb.py'),
            '--child', 'record' if mode == 'record' else 'replay', image, name, out, callbacks], timeout=900)
        checks.check(mode + ' completes', proc.returncode == 0, proc.returncode)
        if proc.returncode:
            sys.exit(checks.report('REPLAY_GUARD'))
        runs[mode] = json.load(open(out))
    recorded_name = os.path.join(work, 'guarded-recording')
    for mode, action, callbacks in [('record-guard', 'record', 'guard'),
                                    ('record-guard-replay', 'replay', '1')]:
        out = os.path.join(work, mode + '.json')
        progress(mode)
        proc = subprocess.run([sys.executable, '-u', os.path.join(os.path.dirname(__file__), 'rrcb.py'),
            '--child', action, image, recorded_name, out, callbacks], timeout=900)
        checks.check(mode + ' completes', proc.returncode == 0, proc.returncode)
        if proc.returncode:
            sys.exit(checks.report('REPLAY_GUARD'))
        runs[mode] = json.load(open(out))
    clean, guard = runs['clean'], runs['guard']
    for key in ('hc', 'blocks', 'abe', 'insns', 'mem', 'asids', 'blk_hash', 'ins_hash', 'serial'):
        checks.check('mutation attempts preserve ' + key, clean[key] == guard[key], (clean[key], guard[key]) if key != 'mem' else None)
    recorded, replayed = runs['record-guard'], runs['record-guard-replay']
    for key in ('hc', 'blocks', 'abe', 'insns', 'mem', 'asids', 'blk_hash', 'ins_hash'):
        checks.check('guarded recording replays without mutator: ' + key,
                     recorded[key] == replayed[key])
    checks.check('guarded recording guest succeeds', 'rc=0' in recorded['output'])
    checks.check('guarded recording replay guest succeeds', replayed['serial'])
    for phase, run in [('replay', guard), ('record', recorded)]:
        for key, value in run['guard'].items():
            if key == 'syscalls':
                checks.check(phase + ': syscall observations fire at entry and return',
                             value['enter'] > 0 and value['return'] > 0, value)
            else:
                checks.check(phase + ': ' + key, value)
    checks.check('guest output still succeeds', guard['serial'])
    legacy = subprocess.run([sys.executable, '-u', os.path.join(os.path.dirname(__file__), 'rrcb.py'),
        '--child', 'replay', image, name, os.path.join(work, 'legacy.json'), 'legacy-pc'],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=900)
    checks.check('legacy noreturn PC API fails with a clear error', legacy.returncode == 1 and
        b'qemu_plugin_set_pc: guest mutation is forbidden during record/replay' in legacy.stdout, legacy.returncode)
    proc = subprocess.run([sys.executable, '-u', __file__, '--live', image], timeout=900)
    checks.check('live hypercall and observer policies', proc.returncode == 0, proc.returncode)
    sys.exit(checks.report('REPLAY_GUARD'))


if __name__ == '__main__':
    if sys.argv[1] == '--live':
        live(sys.argv[2])
    else:
        main()
