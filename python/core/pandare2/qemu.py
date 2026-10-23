"""`qemu-system-<arch>` console scripts (wheel installs): run the bundled engine as plain QEMU.

They exec pandare2/data/bin/panda-qemu, which loads the same libpanda-<arch>-softmmu.so that
`Panda(arch=...)` loads and calls its main().
"""
import os
import sys
from os.path import basename, dirname, join


def launcher():
    return join(dirname(__file__), "data", "bin", "panda-qemu")


def main():
    exe = launcher()
    if not os.path.isfile(exe):
        sys.exit(f"{basename(sys.argv[0])}: this pandare2 has no bundled engine ({exe} is missing)")
    if not os.access(exe, os.X_OK):
        os.chmod(exe, 0o755)
    name = basename(sys.argv[0])
    if not name.startswith(("qemu-system-", "panda-system-")):
        sys.exit(f"{name}: run as qemu-system-<arch>")
    # The launcher takes the arch from argv[0].
    os.execv(exe, [name] + sys.argv[1:])


if __name__ == "__main__":
    main()
