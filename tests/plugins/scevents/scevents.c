/* syscall.enter / syscall.return from a C plugin, for tests/syscall_events.py.
 *
 * An observer on one number (SCEVENTS_NUM, default 63: x86_64 uname) at entry and return, with
 * instance data: the entry stores the call id there, the return checks it. With SCEVENTS_SKIP
 * set, a mutator skips that number with SCEVENTS_SKIP as the return value, and the observer
 * (after it) checks that it sees the skipped call and its return. The observer also tries to
 * skip the call itself, which must be refused. Python reads the counts through scevents_get(). */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "panda/events.h"
#include "panda/syscall-events.h"

#define SC_MODES 1      /* PANDA_MODE_ANALYSIS (panda/plugin.h, which needs the engine headers) */

enum { ENTERS, RETURNS, PAIRED, OTHER, SKIPPED_SEEN, SKIP_RET_OK, OBS_REFUSED, MUT_SKIPS,
       RESTARTS, N };

static uint64_t counts[N];
static int64_t num, skip_ret;
static bool skip;
static char refusal[160];

static void observe(void *ctx, void *args, void *data)
{
    PandaSyscall *sc = args;
    uint64_t *tag = data;

    if (sc->num != num || sc->abi != 0) {
        counts[OTHER]++;
        return;
    }
    if (!(sc->flags & PANDA_SC_AT_RETURN)) {
        counts[ENTERS]++;
        *tag = sc->call_id;
        if (sc->flags & PANDA_SC_SKIPPED) {
            counts[SKIPPED_SEEN]++;
        }
        if (!panda_syscall_skip(sc, -1) && panda_syscall_refusal()) {
            counts[OBS_REFUSED]++;
            snprintf(refusal, sizeof(refusal), "%s", panda_syscall_refusal());
        }
        return;
    }
    counts[RETURNS]++;
    if (*tag == sc->call_id) {
        counts[PAIRED]++;
    }
    if (sc->flags & PANDA_SC_RESTART) {
        counts[RESTARTS]++;
    }
    if (skip && (sc->flags & PANDA_SC_SKIPPED) && sc->ret == skip_ret) {
        counts[SKIP_RET_OK]++;
    }
}

static void mutate(void *ctx, void *args, void *data)
{
    PandaSyscall *sc = args;

    if (sc->num == num && !(sc->flags & PANDA_SC_AT_RETURN) && panda_syscall_skip(sc, skip_ret)) {
        counts[MUT_SKIPS]++;
    }
}

uint64_t scevents_get(int which) { return which >= 0 && which < N ? counts[which] : 0; }
const char *scevents_refusal(void) { return refusal; }

bool init_plugin(void *self)
{
    const char *n = getenv("SCEVENTS_NUM"), *s = getenv("SCEVENTS_SKIP");
    panda_sub obs, mut;

    num = n ? strtoll(n, NULL, 0) : 63;
    skip = s != NULL;
    skip_ret = s ? strtoll(s, NULL, 0) : 0;
    obs = panda_subscribe(self, panda_syscall_event(PANDA_SC_ENTER), observe, NULL,
                          SC_MODES, PANDA_SUB_OBSERVE);
    if (!obs || !panda_sub_set_data_size(obs, sizeof(uint64_t)) ||
        !panda_syscall_watch(obs, 0, num, PANDA_SC_ENTER | PANDA_SC_RETURN)) {
        fprintf(stderr, "scevents: observer: %s\n", panda_syscall_refusal());
        return false;
    }
    if (skip) {
        mut = panda_subscribe(self, panda_syscall_event(PANDA_SC_ENTER), mutate, NULL,
                              SC_MODES, PANDA_SUB_MUTATE);
        if (!mut || !panda_syscall_watch(mut, 0, num, PANDA_SC_ENTER)) {
            fprintf(stderr, "scevents: mutator: %s\n", panda_syscall_refusal());
            return false;
        }
    }
    return true;
}

void uninit_plugin(void *self)
{
}
