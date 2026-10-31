"""Reject malformed RAM and register effects in copies of an actual passing ND recording.

Run: panda-python tests/replay_effect_invalid.py IMAGE.json RECORDING_PREFIX [workdir]
The prefix must come from replay_effect.py, with record.json alongside it. Copies
retain its basename so replay selects the existing internal rr-<basename> snapshot.
Never edits the original log or creates a new guest snapshot.
"""
import json
from pathlib import Path
import resource
import struct
import subprocess
import sys
import tempfile

from testlib import Checks


def main():
    image = str(Path(sys.argv[1]).resolve())
    prefix = Path(sys.argv[2]).resolve()
    work = Path(sys.argv[3] if len(sys.argv) > 3 else
                tempfile.mkdtemp(prefix='replay-effect-invalid-')).resolve()
    work.mkdir(parents=True, exist_ok=True)
    recorded = json.loads((prefix.parent / 'record.json').read_text())
    address = recorded['observations'][0]['physical']
    original = Path(str(prefix) + '-rr-nondet.log').read_bytes()
    payload = bytes.fromhex('010100') + struct.pack('>QI', address, 8) + bytes.fromhex('8877665544332211')
    if original.count(payload) != 1:
        raise AssertionError('expected exactly one complete first RAM-effect payload')
    offset = original.index(payload)
    # cpu 0, gdbstub register 12 (r12), 8 bytes: replay_effect.py's register request.
    register = bytes.fromhex('010200') + struct.pack('>iiI', 0, 12, 8) + (0x1122334455667788).to_bytes(8, 'little')
    if original.count(register) != 1:
        raise AssertionError('expected exactly one complete register-effect payload')
    reg_at = original.index(register)
    variants = {
        'unknown-version': (original[:offset] + b'\xff' + original[offset + 1:],
                            'invalid analysis RAM effect payload'),
        'oversized-length': (original[:offset + 11] + struct.pack('>I', 65537) + original[offset + 15:],
                             'invalid analysis RAM effect payload'),
        'truncated-data': (original[:offset + len(payload) - 1], 'error reading the replay data'),
        'negative-register': (original[:reg_at + 7] + struct.pack('>i', -1) + original[reg_at + 11:],
                              'invalid analysis RAM effect payload'),
    }
    checks = Checks('x86_64')
    # Invalid-header rejection aborts through QEMU's sync-error path. Retain
    # diagnostics, not a potentially large core dump, for these expected failures.
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    for name, (data, diagnostic) in variants.items():
        directory = work / name
        directory.mkdir()
        copied = directory / prefix.name
        Path(str(copied) + '-rr-nondet.log').write_bytes(data)
        try:
            process = subprocess.run([
                sys.executable, '-u', str(Path(__file__).with_name('replay_effect.py')),
                '--child', 'replay', image, str(copied), str(directory / 'result.json')],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
            output = process.stdout.decode(errors='replace')
            (directory / 'run.log').write_text(output)
            checks.check(name + ': rejected with payload diagnostic',
                         process.returncode != 0 and diagnostic in output,
                         {'returncode': process.returncode, 'output': output[-2000:]})
            checks.check(name + ': no successful replay result', not (directory / 'result.json').exists())
        except subprocess.TimeoutExpired as error:
            (directory / 'run.log').write_bytes(error.stdout or b'')
            checks.check(name + ': rejected with payload diagnostic', False, 'timeout')
    checks.check('original recording unchanged',
                 Path(str(prefix) + '-rr-nondet.log').read_bytes() == original)
    sys.exit(checks.report('REPLAY_EFFECT_INVALID work=' + str(work)))


if __name__ == '__main__':
    main()
