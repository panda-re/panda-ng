#!/usr/bin/env python3
"""osicdef.py [--write]: penguin's PENGUIN_OSI_CDEF (compat/qemu_compat.py)
from QEMU's include/system/penguin-osi.h: its declarations, without comments or
preprocessor lines.  Without --write: exit 1 if penguin's differs
(a layout the two sides disagree on is read wrong, silently)."""
import os
import re
import sys
from pathlib import Path

# qemu_builder's own header, and penguin's cdef ($PENGUIN, a penguin checkout)
HEADER = Path(__file__).resolve().parents[2] / 'src' / 'include' / 'system' / 'penguin-osi.h'
COMPAT = Path(os.environ['PENGUIN']) / 'pyplugins' / 'compat' / 'qemu_compat.py'


def cdef(header: str) -> str:
    s = re.sub(r'/\*.*?\*/', '', header, flags=re.S)
    s = re.sub(r'//[^\n]*', '', s)
    s = re.sub(r'^\s*#[^\n]*$', '', s, flags=re.M)
    lines = [ln.rstrip() for ln in s.splitlines()]
    return '\n' + '\n'.join(ln for ln in lines if ln.strip()) + '\n'


def main() -> int:
    want = cdef(HEADER.read_text())
    src = COMPAT.read_text()
    m = re.search(r'(PENGUIN_OSI_CDEF = """)(.*?)(""")', src, flags=re.S)
    if not m:
        print('PENGUIN_OSI_CDEF not found in', COMPAT)
        return 1
    if m.group(2) == want:
        print('osicdef: in sync')
        return 0
    if '--write' in sys.argv:
        COMPAT.write_text(src[:m.start(2)] + want + src[m.end(2):])
        print('osicdef: written')
        return 0
    print('osicdef: penguin_osi.h and PENGUIN_OSI_CDEF differ (osicdef.py --write)')
    return 1


if __name__ == '__main__':
    sys.exit(main())
