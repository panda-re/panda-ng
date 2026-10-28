#!/usr/bin/env python3
"""Validate series paths and patch numbering before extracting or applying QEMU."""
import re
from pathlib import Path
import sys


def check(patches, version):
    series = patches / version / "series"
    numbers = set()
    count = 0
    for lineno, line in enumerate(series.read_text().splitlines(), 1):
        item = line.strip()
        if not item or item.startswith("#"):
            continue
        match = re.fullmatch(re.escape(version) + r"/([0-9]{4})-[A-Za-z0-9_.+-]+\.patch", item)
        if not match:
            raise ValueError(f"{series}:{lineno}: malformed patch path: {item!r}")
        number = int(match.group(1))
        if number in numbers:
            raise ValueError(f"{series}:{lineno}: duplicate patch number {number:04d}")
        if number != count + 1:
            raise ValueError(f"{series}:{lineno}: expected patch {count + 1:04d}, got {number:04d}")
        path = patches / item
        if not path.is_file() or path.is_symlink():
            raise ValueError(f"{series}:{lineno}: missing or symlinked patch: {item}")
        overlay = patches.parent / "src"
        for patchline in path.read_text().splitlines():
            if patchline.startswith(("+++ b/", "--- a/")):
                target = patchline[6:].split("\t", 1)[0]
                if (overlay / target).exists():
                    raise ValueError(f"{path}: {target} is overlay-owned; patch changes would be silently overwritten")
        numbers.add(number)
        count += 1
    if not count:
        raise ValueError(f"{series}: empty series")
    return count


if __name__ == "__main__":
    try:
        print(f"PASS: series format and numbering ({check(Path(sys.argv[1]), sys.argv[2])} patches)")
    except (ValueError, OSError) as exc:
        sys.exit(f"FAIL: {exc}")
