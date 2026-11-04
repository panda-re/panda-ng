/* A QEMU plugin on the syscall-event bridge (qemu_plugin_register_syscall_event_cb), for
 * tests/syscall_events.py's "bridge" mode: it counts the entries and returns of one number
 * (arg num=, default 63: x86_64 uname) beside the PANDA syscall events, and prints them at exit. */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static int64_t num = 63;
static uint64_t enters, returns;

static void on_enter(qemu_plugin_id_t id, unsigned int vcpu, struct qemu_plugin_syscall *sc,
                     void *u)
{
    if (sc->num == num) {
        enters++;
        sc->want_return = true;
    }
}

static void on_return(qemu_plugin_id_t id, unsigned int vcpu, struct qemu_plugin_syscall *sc,
                      void *u)
{
    if (sc->num == num) {
        returns++;
    }
}

/* 0: entries, 1: returns (tests/syscall_events.py reads them through ctypes) */
uint64_t scbridge_get(int which) { return which ? returns : enters; }

static void at_exit(void *u)
{
    printf("SCBRIDGE enters=%" PRIu64 " returns=%" PRIu64 "\n", enters, returns);
    fflush(stdout);
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                                           int argc, char **argv)
{
    for (int i = 0; i < argc; i++) {
        if (!strncmp(argv[i], "num=", 4)) {
            num = strtoll(argv[i] + 4, NULL, 0);
        }
    }
    if (!qemu_plugin_register_syscall_event_cb(id, on_enter, on_return, NULL) ||
        !qemu_plugin_syscall_watch(id, QEMU_PLUGIN_SYSCALL_ABI_X86_64, num,
                                   QEMU_PLUGIN_SYSCALL_ENTER | QEMU_PLUGIN_SYSCALL_RETURN)) {
        fprintf(stderr, "scbridge: the syscall-event bridge refused\n");
        return -1;
    }
    qemu_plugin_register_atexit_cb(id, at_exit, NULL);
    return 0;
}
