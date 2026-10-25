#!/usr/bin/env bash
# tests/modules/at91-extboard.sh INTREE_BUILD ENGINE_BUILD SDK BOARD_C [OUT]
# The at91sam9260 board (qemu_builder's at91sam9260-board branch, src/hw/arm/at91sam9260.c) in-tree
# vs as an external board .so built with the SDK, on the same bare-metal guest (at91-guest.S: DBGU
# chip id, five PIT ticks through the AIC, semihosting exit), under icount:
#   intree: INTREE_BUILD/qemu-system-arm -M at91sam9260      (the board's wiring patch applied)
#   so:     ENGINE_BUILD/qemu-system-arm -M at91sam9260.so   (engine without it)
#   qhost:  the same .so through a penguin-style libqemu host (dlopen libqemu-system-arm.so RTLD_GLOBAL,
#           then qemu_init; extboard probe's qhost.c), -M ./at91sam9260.so:at91sam9260
# PASS when the console output and libbb's block and instruction counts are identical.
set -u
IN=$(cd "$1" && pwd); EN=$(cd "$2" && pwd); SDK=$(cd "$3" && pwd); BOARD=$4; OUT=${5:-$(mktemp -d)}
mkdir -p "$OUT"; H=$(cd "$(dirname "$0")" && pwd); cd "$OUT"
AS=${AS:-arm-none-eabi-as}; LD=${LD:-arm-none-eabi-ld}
$AS -mcpu=arm926ej-s "$H/at91-guest.S" -o guest.o && $LD -z max-page-size=4096 -Ttext=0x20008000 -e _start guest.o -o guest.elf
s=$(date +%s.%N); "$SDK/bin/panda-board-cc" arm-softmmu -o at91sam9260.so "$BOARD" 2> board-cc.err || { cat board-cc.err; exit 1; }
echo "board .so built in $(echo "$(date +%s.%N) - $s" | bc) s ($(stat -c %s at91sam9260.so) bytes)"
cat > qhost.c <<'C'
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv)
{
    if (!dlopen(getenv("QHOST_ENGINE"), RTLD_NOW | RTLD_GLOBAL)) { fprintf(stderr, "%s\n", dlerror()); return 2; }
    void (*init)(int, char **) = dlsym(RTLD_DEFAULT, "qemu_init");
    int (*loop)(void) = dlsym(RTLD_DEFAULT, "qemu_main_loop");
    void (*cleanup)(int) = dlsym(RTLD_DEFAULT, "qemu_cleanup");
    init(argc, argv);
    int status = loop();
    cleanup(status);
    return status;
}
C
cc -O2 -o qhost qhost.c -ldl
BB=$EN/tests/tcg/plugins/libbb.so
COMMON=(-kernel guest.elf -icount shift=0,sleep=off -semihosting -display none -monitor none -serial none
        -plugin "$BB" -d plugin -D)
run() { n=$1; shift; timeout 60 "$@" "${COMMON[@]}" $n.plugin > $n.console 2> $n.err; echo "$n rc=$?"; }
run intree "$IN/qemu-system-arm" -M at91sam9260
run so "$EN/qemu-system-arm" -M ./at91sam9260.so
QHOST_ENGINE=$EN/libqemu-system-arm.so run qhost ./qhost -M ./at91sam9260.so:at91sam9260
for n in intree so qhost; do echo "$n: console $(md5sum < $n.console | cut -c1-12), $(tail -1 $n.plugin)"; done
sed 's/^/  | /' intree.console
fail=0
for n in so qhost; do
    if [ -s $n.plugin ] && cmp -s intree.console $n.console && cmp -s intree.plugin $n.plugin; then
        echo "PASS $n boots the same as in-tree"; else echo "FAIL $n differs from in-tree"; fail=1; fi
done
exit $fail
