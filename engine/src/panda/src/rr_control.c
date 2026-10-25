/*
 * PANDA's record/replay API on the engine's own record/replay
 * (x-rr-begin-record, x-rr-begin-replay, x-rr-end-record).
 *
 * A recording <name> is the log <name>-rr-nondet.log plus the internal
 * snapshot "rr-<basename of name>" in the VM's disk image; a replay loads that
 * snapshot in a process started with the same command line and replays the log
 * from it, so the image must be opened writable (no -snapshot).
 *
 * The API calls only make a request: the work stops the VM, so it runs from a
 * main-loop bottom half.  They may be called from any thread.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "qapi/qapi-commands-replay.h"
#include "system/replay.h"
#include "system/runstate.h"
#include "migration/snapshot.h"
#include "panda/plugin.h"

enum { RR_REQ_RECORD, RR_REQ_RECORD_END, RR_REQ_REPLAY };

typedef struct RRRequest {
    int what;
    char *name;
    char *snapshot;     /* record: revert to this first */
} RRRequest;

static bool rr_pending;

static void rr_request_run(void *opaque)
{
    RRRequest *r = opaque;
    Error *err = NULL;
    g_autofree char *log = r->name ? g_strdup_printf("%s-rr-nondet.log", r->name) : NULL;
    g_autofree char *base = r->name ? g_path_get_basename(r->name) : NULL;
    g_autofree char *snap = base ? g_strdup_printf("rr-%s", base) : NULL;
    bool was_running = runstate_is_running();

    switch (r->what) {
    case RR_REQ_RECORD:
        if (r->snapshot) {
            vm_stop(RUN_STATE_RESTORE_VM);
            load_snapshot(r->snapshot, NULL, false, NULL, &err);
            if (was_running) {
                vm_start();
            }
        }
        if (!err) {
            qapi_free_RrBeginRecordInfo(
                qmp_x_rr_begin_record(snap, log, NULL, false, NULL, &err));
        }
        break;
    case RR_REQ_RECORD_END:
        qmp_x_rr_end_record(&err);
        break;
    case RR_REQ_REPLAY:
        /* x-rr-begin-replay wants a paused VM; it loads the snapshot and resumes */
        if (was_running) {
            vm_stop(RUN_STATE_PAUSED);
        }
        qapi_free_RrBeginRecordInfo(
            qmp_x_rr_begin_replay(snap, log, NULL, false, NULL, &err));
        if (err) {
            /* nothing to run without the replay */
            qemu_system_shutdown_request(SHUTDOWN_CAUSE_HOST_ERROR);
        }
        break;
    }
    if (err) {
        error_reportf_err(err, "PANDA record/replay %s: ", r->name ? r->name : "end");
    }
    g_free(r->name);
    g_free(r->snapshot);
    g_free(r);
    qatomic_set(&rr_pending, false);
}

static int rr_request(int what, const char *name, const char *snapshot)
{
    RRRequest *r;

    if (qatomic_xchg(&rr_pending, true)) {
        return RRCTRL_EPENDING;
    }
    r = g_new0(RRRequest, 1);
    r->what = what;
    r->name = g_strdup(name);
    r->snapshot = g_strdup(snapshot);
    aio_bh_schedule_oneshot(qemu_get_aio_context(), rr_request_run, r);
    return RRCTRL_OK;
}

bool rr_in_record(void)
{
    return replay_mode == REPLAY_MODE_RECORD;
}

bool rr_in_replay(void)
{
    return replay_mode == REPLAY_MODE_PLAY;
}

bool rr_on(void)
{
    return replay_mode != REPLAY_MODE_NONE;
}

uint64_t rr_get_guest_instr_count_external(void)
{
    return rr_on() ? replay_get_current_icount() : 0;
}

int panda_record_begin(const char *name, const char *snapshot)
{
    if (!name || rr_on()) {
        return RRCTRL_EINVALID;
    }
    return rr_request(RR_REQ_RECORD, name, snapshot);
}

int panda_record_end(void)
{
    if (!rr_in_record()) {
        return RRCTRL_EINVALID;
    }
    return rr_request(RR_REQ_RECORD_END, NULL, NULL);
}

int panda_replay_begin(const char *name)
{
    if (!name || rr_on()) {
        return RRCTRL_EINVALID;
    }
    return rr_request(RR_REQ_REPLAY, name, NULL);
}

static void shutdown_bh(void *opaque)
{
    qemu_system_shutdown_request((ShutdownCause)(uintptr_t)opaque);
}

/*
 * qemu_system_shutdown_request() for any thread.  While recording, the request
 * goes in the log, which needs the replay mutex: the main loop holds it.
 */
void panda_shutdown_request(int cause)
{
    if (rr_in_record()) {
        aio_bh_schedule_oneshot(qemu_get_aio_context(), shutdown_bh,
                                (void *)(uintptr_t)cause);
    } else {
        qemu_system_shutdown_request(cause);
    }
}

/* A replay can't continue past a stop; ending it ends the run. */
int panda_replay_end(void)
{
    if (!rr_in_replay()) {
        return RRCTRL_EINVALID;
    }
    qemu_system_shutdown_request(SHUTDOWN_CAUSE_HOST_QMP_QUIT);
    return RRCTRL_OK;
}
