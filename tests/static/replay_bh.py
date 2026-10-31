"""Standalone compiled regression for replay BH queue coalescing, without guests.

Run: panda-python tests/static/replay_bh.py /path/to/replay/replay-events.c
Alternatively set PANDA_REPLAY_EVENTS to that patched QEMU source file. The
source tree must include include/qemu/queue.h. Uses CC (default cc) and temporary
files only. Cancellation/deletion semantics are deliberately not simulated:
replay's direct aio_bh_call does not honor normal AIO BH cancellation flags.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
import unittest


SOURCE = Path(sys.argv.pop(1) if len(sys.argv) > 1 and not sys.argv[1].startswith('-')
              else os.environ.get('PANDA_REPLAY_EVENTS', 'replay/replay-events.c')).resolve()


def definition(text, name):
    """Keep the original signature/body; balance braces outside comments/strings."""
    match = re.search(r'^(?:static )?(?:void|bool|Event \*)\s*' + re.escape(name)
                      + r'\s*\([^;{}]*\)\s*\{', text, re.M)
    if match is None:
        raise AssertionError(f'function definition not found: {name}')
    start = match.end() - 1
    tokens = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.S)
    depth = 0
    for token in tokens.finditer(text, start):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return text[match.start():token.end()] + '\n'
    raise AssertionError(f'unclosed function body: {name}')


STUBS = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "qemu/queue.h"
#define g_assert assert
#define g_free free
#define g_new0(t,n) ((t *)calloc(n, sizeof(t)))
#define error_report(...) fprintf(stderr, __VA_ARGS__)
enum { REPLAY_MODE_NONE, REPLAY_MODE_RECORD, REPLAY_MODE_PLAY };
typedef enum {
    REPLAY_ASYNC_EVENT_BH, REPLAY_ASYNC_EVENT_BH_ONESHOT,
    REPLAY_ASYNC_EVENT_INPUT, REPLAY_ASYNC_EVENT_INPUT_SYNC,
    REPLAY_ASYNC_EVENT_CHAR_READ, REPLAY_ASYNC_EVENT_BLOCK,
    REPLAY_ASYNC_EVENT_NET, REPLAY_ASYNC_EVENT_ANALYSIS_EFFECT, REPLAY_ASYNC_COUNT
} ReplayAsyncEventKind;
enum { EVENT_ASYNC = 100, EVENT_ASYNC_LAST = EVENT_ASYNC + REPLAY_ASYNC_COUNT - 1 };
typedef void QEMUBHFunc(void *);
typedef struct { QEMUBHFunc *cb; void *opaque; } QEMUBH;
typedef int AioContext;
typedef int QemuInputEvent;
static int replay_mode, first_cpu;
static bool replay_file = true;
static int kicks, live_schedules, live_oneshots;
static uint64_t icount = 42;
static struct { int data_kind; uint64_t read_event_id; } replay_state;
static struct { int kind; uint64_t id; } log_events[32];
static unsigned log_count, log_at;
static bool replay_mutex_locked(void) { return true; }
static uint64_t replay_get_current_icount(void) { return icount; }
static void cpu_exit(int cpu) { (void)cpu; kicks++; }
static void qemu_bh_schedule(QEMUBH *bh) { (void)bh; live_schedules++; }
static void aio_bh_schedule_oneshot(AioContext *c, QEMUBHFunc *cb, void *p) {
    (void)c; (void)cb; (void)p; live_oneshots++;
}
static void aio_bh_call(QEMUBH *bh) { bh->cb(bh->opaque); }
static void qemu_input_event_send_impl(void *p, QemuInputEvent *e) { (void)p; (void)e; abort(); }
static void qemu_input_event_sync_impl(void) { abort(); }
static void replay_event_char_read_run(void *p) { (void)p; abort(); }
static void replay_event_net_run(void *p) { (void)p; abort(); }
static void replay_put_event(int kind) {
    assert(log_count < 32); log_events[log_count++].kind = kind;
}
static void replay_put_qword(uint64_t id) { assert(log_count); log_events[log_count - 1].id = id; }
static uint64_t replay_get_qword(void) { assert(log_at < log_count); return log_events[log_at].id; }
static void replay_finish_event(void) {
    log_at++;
    replay_state.data_kind = log_at < log_count ? log_events[log_at].kind : 0;
}
static void replay_save_input_event(void *p) { (void)p; abort(); }
static void replay_event_char_read_save(void *p) { (void)p; abort(); }
static void replay_event_net_save(void *p) { (void)p; abort(); }
static void *replay_read_input_event(void) { abort(); }
static void *replay_event_char_read_load(void) { abort(); }
static void *replay_event_net_load(void) { abort(); }
/* Recorded analysis RAM effects are covered by tests/static/replay_effect.py. */
static void replay_analysis_effect_run(void *p) { (void)p; abort(); }
static void replay_analysis_effect_cancel(void *p) { (void)p; abort(); }
static void replay_analysis_effect_prepare(void *p) { (void)p; abort(); }
static void replay_analysis_effect_save(void *p) { (void)p; abort(); }
static void *replay_analysis_effect_load(void) { abort(); }
'''


CHECKS = r'''
static unsigned pending(void) {
    unsigned n = 0;
    Event *event;
    QTAILQ_FOREACH(event, &events_list, events) n++;
    return n;
}
static void add_log(ReplayAsyncEventKind kind, uint64_t id) {
    replay_put_event(EVENT_ASYNC + kind); replay_put_qword(id);
}
static unsigned a_calls, b_calls, pending_during_a, trace;
static bool reschedule_self, reenter;
static int drain_kind;
static QEMUBH a, b;
static void drain(void) {
    if (drain_kind == 0) replay_save_events();
    else if (drain_kind == 1) replay_flush_events();
    else replay_read_events();
}
static void a_cb(void *p) {
    assert(p == &a);
    a_calls++;
    trace = trace * 10 + 1;
    if (reschedule_self && a_calls == 1) {
        /* The running node must already be absent, including on a nested drain. */
        assert(pending() == pending_during_a);
        replay_bh_schedule_event(&a);
        assert(pending() == pending_during_a + 1);
        if (reenter) drain();
    }
}
static void b_cb(void *p) { assert(p == &b); b_calls++; trace = trace * 10 + 2; }
static void oneshot_cb(void *p) { assert(p == &a_calls); a_calls++; }
static void start_playback(void) {
    replay_state.data_kind = log_events[0].kind;
    replay_state.read_event_id = (uint64_t)-1;
}
int main(int argc, char **argv) {
    assert(argc == 4);
    replay_mode = atoi(argv[1]);
    drain_kind = atoi(argv[2]);
    int scenario = atoi(argv[3]);
    a = (QEMUBH){ a_cb, &a };
    b = (QEMUBH){ b_cb, &b };
    replay_enable_events();
    if (replay_mode == REPLAY_MODE_NONE) {
        replay_bh_schedule_event(&a);
        replay_bh_schedule_event(&a);
        replay_bh_schedule_oneshot_event(NULL, oneshot_cb, &a_calls);
        replay_bh_schedule_oneshot_event(NULL, oneshot_cb, &a_calls);
        assert(live_schedules == 2 && live_oneshots == 2);
        assert(!pending() && !kicks && !a_calls);
        return 0;
    }
    if (scenario == 0) {
        replay_bh_schedule_event(&a);
        icount++; /* pending identity, not instruction count, defines coalescing */
        replay_bh_schedule_event(&a);
        assert(pending() == 1 && kicks == 1);
        if (drain_kind == 2) { add_log(REPLAY_ASYNC_EVENT_BH, 42); start_playback(); }
        drain(); assert(a_calls == 1);
    } else if (scenario == 1) {
        replay_bh_schedule_event(&a); replay_bh_schedule_event(&b);
        assert(pending() == 2 && kicks == 2);
        if (drain_kind == 2) {
            add_log(REPLAY_ASYNC_EVENT_BH, 42); add_log(REPLAY_ASYNC_EVENT_BH, 42);
            start_playback();
        }
        drain(); assert(a_calls == 1 && b_calls == 1);
    } else if (scenario == 2 || scenario == 3) {
        reschedule_self = true; reenter = scenario == 3;
        replay_bh_schedule_event(&a);
        if (drain_kind == 2) {
            add_log(REPLAY_ASYNC_EVENT_BH, 42); add_log(REPLAY_ASYNC_EVENT_BH, 42);
            start_playback();
        }
        drain(); assert(a_calls == 2 && kicks == 2);
    } else if (scenario == 4) {
        replay_bh_schedule_event(&a);
        if (drain_kind == 2) { add_log(REPLAY_ASYNC_EVENT_BH, 42); start_playback(); }
        drain(); assert(a_calls == 1 && !pending());
        replay_bh_schedule_event(&a);
        if (drain_kind == 2) {
            add_log(REPLAY_ASYNC_EVENT_BH, 42);
            replay_state.data_kind = log_events[log_at].kind;
        }
        drain(); assert(a_calls == 2 && kicks == 2);
    } else if (scenario == 5) {
        replay_bh_schedule_oneshot_event(NULL, oneshot_cb, &a_calls);
        replay_bh_schedule_oneshot_event(NULL, oneshot_cb, &a_calls);
        assert(pending() == 2 && kicks == 2);
        if (drain_kind == 2) {
            add_log(REPLAY_ASYNC_EVENT_BH_ONESHOT, 42);
            add_log(REPLAY_ASYNC_EVENT_BH_ONESHOT, 42); start_playback();
        }
        drain(); assert(a_calls == 2);
    } else if (scenario == 6) {
        /* A BLOCK event with the same BH pointer is not a pending BH event. */
        replay_block_event(&a, 7); replay_bh_schedule_event(&a);
        assert(pending() == 2);
        if (drain_kind == 2) {
            add_log(REPLAY_ASYNC_EVENT_BLOCK, 7); add_log(REPLAY_ASYNC_EVENT_BH, 42);
            start_playback();
        }
        drain(); assert(a_calls == 2);
    } else if (scenario == 7) {
        /* Duplicating A must neither move it behind B nor duplicate it. A's
         * self-reschedule belongs behind B, including a nested queue drain. */
        reschedule_self = reenter = true;
        pending_during_a = 1;
        replay_bh_schedule_event(&a); replay_bh_schedule_event(&b);
        replay_bh_schedule_event(&a);
        assert(pending() == 2);
        if (drain_kind == 2) {
            add_log(REPLAY_ASYNC_EVENT_BH, 42); add_log(REPLAY_ASYNC_EVENT_BH, 42);
            add_log(REPLAY_ASYNC_EVENT_BH, 42); start_playback();
        }
        drain(); assert(a_calls == 2 && b_calls == 1 && trace == 121);
    } else abort();
    assert(!pending());
    if (drain_kind == 0 && replay_mode == REPLAY_MODE_RECORD) {
        assert(log_count == a_calls + b_calls);
    }
    if (drain_kind == 1 || (drain_kind == 0 && replay_mode == REPLAY_MODE_PLAY)) {
        assert(!log_count); /* Flush, and save while playing, must not emit records. */
    }
    if (drain_kind == 2) assert(log_at == log_count);
    return 0;
}
'''


def harness(source):
    text = source.read_text()
    # Include the actual Event layout/list globals; no QEMU implementation copied.
    start = text.index('typedef struct Event {')
    end = text.index('static bool events_enabled;', start) + len('static bool events_enabled;')
    names = ('replay_run_event', 'replay_enable_events', 'replay_flush_events',
             'replay_add_event', 'replay_bh_schedule_event',
             'replay_bh_schedule_oneshot_event', 'replay_block_event',
             'replay_save_event', 'replay_save_events', 'replay_read_event',
             'replay_read_events')
    return STUBS + text[start:end] + '\n' + ''.join(definition(text, n) for n in names) + CHECKS


class ReplayBH(unittest.TestCase):
    def test_coalescing_and_reentrancy(self):
        self.assertTrue(SOURCE.is_file(),
                        'pass replay-events.c or set PANDA_REPLAY_EVENTS to patched source')
        with tempfile.TemporaryDirectory(prefix='replay-bh-') as directory:
            directory = Path(directory)
            source = directory / 'bh.c'
            executable = directory / 'bh'
            source.write_text(harness(SOURCE))
            command = shlex.split(os.environ.get('CC', 'cc'))
            subprocess.run(command + ['-std=gnu11', '-O2', '-Wall', '-Werror',
                                      '-I', str(SOURCE.parents[1] / 'include'),
                                      str(source), '-o', str(executable)], check=True)
            cases = ('duplicate', 'different', 'self-reschedule', 'nested-drain',
                     'after-completion', 'oneshot', 'block-not-bh', 'queue-order')
            for mode, name in enumerate(('NONE', 'RECORD', 'PLAY')):
                drains = ((0, 'save'), (1, 'flush'))
                if mode == 2:
                    drains += ((2, 'read'),)
                for drain, drain_name in drains:
                    for scenario, case in enumerate(cases):
                        with self.subTest(mode=name, drain=drain_name, case=case):
                            subprocess.run([str(executable), str(mode), str(drain),
                                            str(scenario)], check=True)


if __name__ == '__main__':
    unittest.main()
