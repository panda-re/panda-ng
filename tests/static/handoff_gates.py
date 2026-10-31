#!/usr/bin/env python3
"""Compile real snapshot/clock gate bodies; no engine build or guest.
Run: python3 -I tests/static/handoff_gates.py PATCHED_QEMU_TREE
"""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

TREE = Path(sys.argv[1]).resolve()
ROOT = Path(__file__).resolve().parents[2]


def function(text, name):
    match = re.search(r'^\w[^\n]*\b' + re.escape(name) + r'\([^;]*?\)\s*\{', text, re.M)
    assert match, name
    return text[match.start():text.index('\n}', match.end()) + 2]


def read(path):
    return (TREE / path).read_text()


arm = read('target/arm/machine.c')
gic = read('hw/intc/arm_gic_common.c')
timers = read('system/cpu-timers.c')
replay = read('replay/replay-snapshot.c')
assert 'analysis_policy_has(ANALYSIS_CAP_HANDOFF)' in function(read('hw/i386/pc.c'), 'kvmclock_sink_register')
assert read('target/i386/kvm/kvm.c').count('analysis_policy_has(ANALYSIS_CAP_HANDOFF) &&\n        (sregs.tr.type == 1 || sregs.tr.type == 9)') == 2
assert arm.count('fail | reg_fail : reg_fail') == 2  # preserve upstream assignment off-path
assert 'if (analysis_policy_has(ANALYSIS_CAP_HANDOFF)) {\n        arm_cpu_handoff_translate(cpu);' in arm
assert 'analysis_policy_has(ANALYSIS_CAP_HANDOFF) &&\n            (cpu->handoff_in || cpu->handoff_vcnt_valid)' in arm
assert 'analysis_policy_has(ANALYSIS_CAP_HANDOFF) &&\n        s->kvm_cpuif_in != gic_is_kvm(s)' in gic
assert '.pre_load = handoff_subsection_pre_load' in arm
assert '.pre_load = handoff_subsection_pre_load' in gic
assert 'analysis_policy_has(ANALYSIS_CAP_RR)' in function(read('replay/replay.c'), 'replay_check_timer_stall')
# Native upstream expired-timer conversion must remain independent of RR extension.
qtimer = read('util/qemu-timer.c') if (TREE / 'util/qemu-timer.c').exists() else ''
if qtimer:
    assert 'return replay_mode != REPLAY_MODE_NONE &&' in function(qtimer, 'timer_is_replay_virtual')

stubs = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#define ANALYSIS_CAP_ANALYSIS 1u
#define ANALYSIS_CAP_HANDOFF 16u
#define ANALYSIS_CAP_RR 4u
#define ANALYSIS_CAP_FUZZ 2u
static uint32_t caps;
static bool analysis_policy_has(uint32_t mask) { return (caps & mask) == mask; }
static bool icount_on, tcg_on = true;
static bool icount_enabled(void) { return icount_on; }
static bool tcg_enabled(void) { return tcg_on; }
#define qatomic_read(p) (*(p))
#define NANOSECONDS_PER_SECOND 1000000000
#define QEMU_CLOCK_VIRTUAL 0
#define ARM_FEATURE_AARCH64 1
#define ARM_CP_CONST 1
static int armed;
static int64_t icount_to_ns(int64_t count) { return count * 10; }
static int64_t qemu_clock_get_ns(int clock) { return 1000; }
static void timer_mod(void *t, int64_t value) { armed++; }
typedef struct TimersState {
    int64_t cpu_clock_offset, qemu_icount_bias, qemu_icount, vm_clock_warp_start;
    int64_t cpu_ticks_prev, cpu_ticks_offset;
    bool cpu_ticks_enabled;
    void *icount_vm_timer;
} TimersState;
static bool icount_state_loaded, clock_domain_changed;
typedef struct { int unused; } CPUARMState;
typedef struct { CPUARMState env; bool handoff_vcnt_valid; } ARMCPU;
static bool arm_feature(CPUARMState *env, int feature) { return true; }
typedef struct { unsigned type; const char *name; } ARMCPRegInfo;
static bool gic_kvm;
static bool gic_is_kvm(void *opaque) { return gic_kvm; }
#define DESC_P_MASK 128u
#define DESC_TYPE_SHIFT 8
struct { unsigned flags, limit; } tr;
typedef struct { struct { unsigned flags, limit; } tr; } CPUX86State;
'''
body = '\n'.join(function(timers, name) for name in ('timers_pre_load', 'icount_timers_post_load', 'timers_post_load'))
body += '\n' + function(arm, 'virt_count_needed') + '\n' + function(arm, 'handoff_subsection_pre_load')
body += '\n' + function(read('target/arm/helper.c'), 'cpreg_mismatch_is_icount_only')
body += '\n' + function(gic, 'gic_kvm_cpuif_needed')
# Extract the real TSS predicate, retaining its complete upstream type9 path.
seg = read('target/i386/tcg/system/seg_helper.c')
start = seg.index('    if (!(env->tr.flags & DESC_P_MASK)')
end = seg.index(' {', start)
predicate = seg[start + len('    if ('):end - 1]
body += '\nstatic bool tss_rejected(CPUX86State *env) { return ' + predicate + '; }\n'
tests = r'''
int main(void) {
    ARMCPU cpu = {0};
    ARMCPRegInfo ri = {.type=ARM_CP_CONST, .name="PMCEID0_EL0"};
    for (caps=0; caps<32; caps++) {
        assert(virt_count_needed(&cpu) == !!(caps & ANALYSIS_CAP_HANDOFF));
        assert((handoff_subsection_pre_load(&cpu) == 0) == !!(caps & ANALYSIS_CAP_HANDOFF));
        assert(cpreg_mismatch_is_icount_only(&ri, 0, 256) == !!(caps & ANALYSIS_CAP_RR));
        assert(!cpreg_mismatch_is_icount_only(&ri, 0, 512));
        gic_kvm=true;
        assert(gic_kvm_cpuif_needed(0) == !!(caps & ANALYSIS_CAP_HANDOFF));
        gic_kvm=false; assert(!gic_kvm_cpuif_needed(0));
        for (unsigned type=0; type<16; type++) {
            CPUX86State x = {.tr={.flags=DESC_P_MASK | (type << DESC_TYPE_SHIFT), .limit=103}};
            bool accepted = type==9 || (type==11 && (caps & ANALYSIS_CAP_HANDOFF));
            assert(tss_rejected(&x) == !accepted);
            x.tr.limit=102; assert(tss_rejected(&x));
            x.tr.limit=103; x.tr.flags &= ~DESC_P_MASK; assert(tss_rejected(&x));
        }
        for (int direction=0; direction<3; direction++) {
            TimersState s = {.cpu_clock_offset=100, .qemu_icount_bias=200,
                .qemu_icount=3, .vm_clock_warp_start=9, .cpu_ticks_prev=999,
                .cpu_ticks_offset=77, .icount_vm_timer=(void *)1};
            clock_domain_changed=false;
            icount_state_loaded=direction==1;
            icount_on=direction==0;
            armed=0;
            assert(!timers_post_load(&s, 2));
            bool rr = caps & ANALYSIS_CAP_RR;
            assert(clock_domain_changed == (rr && direction<2));
            assert(s.cpu_clock_offset == (rr && direction==1 ? 230 : 100));
            assert(s.qemu_icount_bias == (rr && direction==0 ? 70 : 200));
            assert(s.cpu_ticks_prev == (caps & ANALYSIS_CAP_ANALYSIS ? 77 : 999));
            assert(armed == (rr && direction==0));
        }
    }
    tcg_on=false;caps=ANALYSIS_CAP_HANDOFF;
    assert(!virt_count_needed(&cpu));
}
'''

with tempfile.TemporaryDirectory(prefix='handoff-gates-') as temp:
    out = Path(temp)
    (out / 'clock.c').write_text(stubs + body + tests)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', '-O2', str(out/'clock.c'), '-o', str(out/'clock')], check=True)
    subprocess.run([str(out/'clock')], check=True)
    # Actual replay load body with boundary assertions: native RR caps0 still
    # loads normally without the extension deferral bracket.
    replay_stubs = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
typedef struct Error Error;
#define ANALYSIS_CAP_RR 4
static unsigned caps, begun, ended, loads;
static bool success, replay_loading, replay_snapshot_reloaded;
static const char *replay_snapshot="start";
static struct { bool begin_start; } replay_state;
static bool analysis_policy_has(unsigned mask) { return (caps & mask)==mask; }
static void timer_replay_load_begin(void) { begun++; }
static void timer_replay_load_end(bool ok, bool convert) { assert(ok==success); ended++; }
#define g_strcmp0 strcmp
static bool load_snapshot(const char *name, void *v, bool b, void *l, Error **e) {
    assert(replay_loading); loads++; return success;
}
'''
    replay_tests = r'''
int main(void) {
    for (caps=0; caps<=4; caps+=4) {
        for (int ok=0; ok<2; ok++) {
            success=ok; begun=ended=loads=0;
            assert(replay_load_snapshot("start", 0)==success);
            assert(loads==1 && begun==!!caps && ended==!!caps && !replay_loading);
        }
    }
}
'''
    (out/'replay.c').write_text(replay_stubs+function(replay,'replay_load_snapshot')+replay_tests)
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O2',str(out/'replay.c'),'-o',str(out/'replay')],check=True)
    subprocess.run([str(out/'replay')],check=True)
print('PASS: actual clock/domain/analysis rewind and ARM subsection/PMCEID capability predicates; replay native off-path and extension bracket; source handoff checks')
