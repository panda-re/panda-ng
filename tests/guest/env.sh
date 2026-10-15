# env.sh: where the guest harness finds its inputs; every script sources it
# after setting H (this directory).  Each setting can be overridden from the
# environment; the defaults are where prepare.sh puts things.
#   QBUILD        a QEMU build directory: qemu-system-*, libqemu-system-*.so, pc-bios
#                 (run.sh requires it)
#   QSRC          that build's source tree, for the plugins' qemu-plugin.h
#   KERNEL_DIR    linux_builder's kernels, extracted: <ver>/<kernel>, cosi.<arch>.json.xz
#   BUSYBOX_DIST  rehosting/busybox #dist: <arch>/busybox
#   ZCC           a cross C compiler driver taking -target (zig cc)
#   PENGUIN       a penguin checkout (penhost.py, pyhost.py's pmem, osicdef.py)
#   PLUGINS       the TCG plugins built from plugins/*.c
#   GEN           built inputs: initrds, pagetest builds, syms-*.env, sysprof-*.txt, ours-*
#   OUTROOT       where runs write out-*/ directories
export QSRC=${QSRC:-${QBUILD:+$QBUILD/..}}
export KERNEL_DIR=${KERNEL_DIR:-$H/dl/kernels}
export BUSYBOX_DIST=${BUSYBOX_DIST:-$H/dl/busybox-dist}
export ZCC=${ZCC:-"$H/dl/zig/bin/python -m ziglang cc"}
export PENGUIN=${PENGUIN:-$H/../../../penguin}
export GEN=${GEN:-$H/gen}
export PLUGINS=${PLUGINS:-$GEN/plugins}
export OUTROOT=${OUTROOT:-$H/out}
