"""Check complete, source-pinned vanilla gate inventory; not a parity test."""
import hashlib
from pathlib import Path
import tomllib

ROOT = Path(__file__).resolve().parents[2]
inventory = tomllib.loads((ROOT / 'docs/vanilla-gates.toml').read_text())
series = [line.strip() for line in (ROOT / inventory['patch_series']).read_text().splitlines()
          if line.strip() and not line.lstrip().startswith('#')]
expected = {f'engine/patches/{name}' for name in series}
records = inventory['patch']
assert len(records) == len(expected), 'duplicate or missing patch records'
assert {record['file'] for record in records} == expected, 'inventory differs from series'
classes = {'build-only', 'inert-in-vanilla', 'runtime-gate', 'startup-gate',
           'upstream-fix-needs-off-path'}
for record in records:
    path = ROOT / record['file']
    assert record['id'] == path.name[:4], record['file']
    assert record['class'] in classes, record['id']
    assert hashlib.sha256(path.read_bytes()).hexdigest() == record['sha256'], record['id']
    for field in ('capability', 'paths', 'functions', 'current_gate', 'required_action'):
        assert record[field], (record['id'], field)
covered = {path for record in inventory['overlay'] for path in record['paths']}
actual = {str(path.relative_to(ROOT)) for path in (ROOT / 'engine/src').rglob('*')
          if path.is_file()}
assert covered == actual, f'overlay coverage differs: missing={actual - covered}, stale={covered - actual}'
assert not inventory['vanilla_parity_implemented'], 'inventory alone cannot establish parity'
print(f'PASS: {len(records)} patch classifications match the current source; vanilla parity NOT asserted')
