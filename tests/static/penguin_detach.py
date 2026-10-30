#!/usr/bin/env python3
"""Compile actual Penguin detach, public hook and core release bodies offline."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
psc=(ROOT/'engine/src/system/penguin-syscalls.c').read_text()
core=(ROOT/'engine/src/system/syscall-trap.c').read_text()
header=(ROOT/'engine/src/include/system/syscall-trap.h').read_text()
trap=header[header.index('typedef struct SyscallTrap {'):header.index('/* one consumer')]
slots=core[core.index('#define SC_SLOTS'):core.index('/*\n * Run the call the consumer')]


def function(text,name):
    match=re.search(r'^.*\b'+re.escape(name)+r'\([^;]*?\)\s*\{',text,re.M)
    if match and text[match.start():match.end()].startswith(name+'('):
        start=text.rfind('\n',0,match.start()-1)+1
    else:
        start=match.start() if match else 0
    assert match,name
    return text[start:text.index('\n}',match.end())+2]


stubs=r'''
#include <glib.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define CONFIG_TCG 1
#define SYSCALL_ABI_COUNT 10
#define SYSCALL_ABI_I386 1
#define SYSCALL_ABI_ARM 3
#define SYSCALL_ABI_ARM_OABI 4
#define SYSCALL_ABI_PPC 6
#define SYSCALL_ABI_RISCV32 8
#define SYSCALL_TRAP_NUMS 1024
#define SYSCALL_TRAP_ENTER 1
#define SYSCALL_TRAP_RETURN 2
#define SYSCALL_TRAP_FAULT 2
#define qatomic_read(p) (*(p))
#define qatomic_set(p,v) (*(p)=(v))
#define qatomic_inc(p) (++*(p))
#define qatomic_dec(p) (--*(p))
#define PENGUIN_SC_OP_ABORT 9
#define QEMU_LOCK_GUARD(lock) g_autoptr(GMutexLocker) GUARD = g_mutex_locker_new(lock)
typedef GMutex QemuMutex;
#define qemu_mutex_init g_mutex_init
#define qemu_mutex_lock g_mutex_lock
#define qemu_mutex_unlock g_mutex_unlock
typedef struct CPUState {
    bool stopped;
    uint64_t syscall_current;
    uint32_t syscall_kmmu, syscall_inject_abis;
} CPUState;
static CPUState cpus[2]={{true},{true}}, *current_cpu;
static bool bql=true, main_thread=true;
#define CPU_FOREACH(cpu) for (cpu=cpus;cpu<cpus+2;cpu++)
static bool bql_locked(void) { return bql; }
static bool qemu_in_main_thread(void) { return main_thread; }
static bool cpu_is_stopped(CPUState *cpu) { return cpu->stopped; }
typedef uint64_t vaddr;
struct penguin_sc_event { int unused; };
struct penguin_sc_cmd { int op; };
typedef void (*penguin_syscall_cb_t)(CPUState *,struct penguin_sc_event *,struct penguin_sc_cmd *,void *);
typedef void (*penguin_syscall_boot_cb_t)(CPUState *,void *);
typedef uint64_t (*penguin_kernel_hook_cb_t)(CPUState *,uint64_t,void *);
typedef struct { bool running; struct penguin_sc_event ev; struct penguin_sc_cmd cmd; void *wdata,*rbuf; } PRun;
static unsigned freed;
static void psc_fault_resolved(PRun *r) { freed++; }
static QemuMutex psc_lock,psc_boot_lock;
static bool psc_consumer,psc_detaching,psc_dirty,psc_booted,psc_scope,psc_cur_cpu;
static unsigned psc_kernel_hooks;
static penguin_syscall_cb_t psc_cb;
static void *psc_cb_opaque;
static penguin_syscall_boot_cb_t psc_boot_cb;
static void *psc_boot_opaque;
typedef struct TCGHook { int unused; } TCGHook;
static TCGHook one_hook;
static bool insert_fails;
static TCGHook *tcg_hook_insert(uint64_t addr, void (*cb)(CPUState *,vaddr,void *),void *opaque) { return insert_fails?NULL:&one_hook; }
static void tcg_hook_remove(TCGHook *h) { assert(h==&one_hook); }
static void pkh_fn(CPUState *cpu,vaddr pc,void *opaque) {}
'''
# Use actual slot storage, cancellation, watches and release too: the public
# detach acceptance must not depend on a second implementation of the drain.
body=trap+slots+'\n'+function(core,'syscall_trap_cancel_all')
body+='\n'.join(function(core,n) for n in ('syscall_trap_watch','syscall_trap_unwatch_all'))
body+='\n'+'\n'.join(function(psc,n) for n in ('psc_run_free','psc_run_abort','psc_drop'))
body+='\nstatic const SyscallTrapOps psc_ops={.drop=psc_drop};\n'
body+='\n'.join(function(core,n) for n in ('syscall_trap_set_consumer','syscall_trap_clear_consumer'))
body+='\n'+function(psc,'psc_mark_dirty')
body+='\n'+function(psc,'set_penguin_syscall_callback')
body+='\n'+function(psc,'set_penguin_syscall_boot_callback')
body+='\n'+function(psc,'penguin_syscall_set_scope')
body+='\n'+function(psc,'penguin_syscall_set_current_source')
body+='\n'+function(psc,'psc_detach_quiescent')+'\n'+function(psc,'penguin_syscall_detach')
start=psc.index('typedef struct PKHook');end=psc.index('static void pkh_fn',start)
body+='\n'+psc[start:end]
body+='\n'+function(psc,'penguin_kernel_hook')+'\n'+function(psc,'penguin_kernel_unhook')
tests=r'''
static int old_opaque,aborts;
static unsigned watches(void) {
    unsigned n=0;
    for(unsigned abi=0;abi<SYSCALL_ABI_COUNT;abi++) {
        n+=sc_watch_all[abi];
        for(unsigned num=0;num<SYSCALL_TRAP_NUMS;num++) n+=sc_watch[abi][num];
    }
    return n;
}
static void add_run_at(unsigned index,int state,bool with_data) {
    uint64_t key=1;
    while(sc_home(key,2,0x4000)!=index) key++;
    assert(!sc_slots[index].pc);
    PRun *r=with_data?g_new0(PRun,1):NULL;
    if(r) { r->running=true;r->wdata=g_malloc(8);r->rbuf=g_malloc(8); }
    SyscallTrap sc={.data=r};
    sc_add(key,2,0x4000,4,&sc,state);
    assert(sc_slots[index].pc && sc_slots[index].state==state);
}
static void boot(CPUState *cpu,void *opaque) {}
static uint64_t kernel(CPUState *cpu,uint64_t pc,void *opaque) { return 0; }
static void callback(CPUState *cpu,struct penguin_sc_event *ev,struct penguin_sc_cmd *cmd,void *opaque) {
    assert(!cpu && opaque==&old_opaque && cmd->op==PENGUIN_SC_OP_ABORT);
    assert(g_mutex_trylock(&psc_lock));g_mutex_unlock(&psc_lock);
    assert(g_mutex_trylock(&sc_lock));g_mutex_unlock(&sc_lock);
    assert(g_mutex_trylock(&psc_boot_lock));g_mutex_unlock(&psc_boot_lock);
    assert(!syscall_trap_pending && !sc_held && !sc_fault_addr && !sc_fault_write);
    static const SCSlot zero={0};
    for(unsigned i=0;i<SC_SLOTS;i++) assert(!memcmp(&sc_slots[i],&zero,sizeof(zero)));
    for(unsigned i=0;i<2;i++) assert(!cpus[i].syscall_current && !cpus[i].syscall_kmmu && !cpus[i].syscall_inject_abis);
    assert(sc_ops==&psc_ops && !sc_opaque && sc_detaching && psc_detaching);
    assert(psc_consumer && psc_cb==callback && psc_cb_opaque==&old_opaque);
    assert(psc_boot_cb==boot && psc_boot_opaque==&old_opaque && psc_booted);
    assert(!syscall_trap_on && !watches());
    assert(!set_penguin_syscall_callback(NULL,NULL));
    assert(!penguin_syscall_detach());
    assert(!penguin_kernel_hook(1,kernel,&old_opaque));
    set_penguin_syscall_boot_callback(boot,NULL);
    assert(psc_boot_opaque==&old_opaque);
    penguin_syscall_set_scope(false);penguin_syscall_set_current_source(false);
    assert(psc_scope && psc_cur_cpu);
    psc_mark_dirty();assert(!watches());
    aborts++;
}
int main(void) {
    static const SyscallTrapOps foreign={0};sc_ops=&foreign;
    assert(!set_penguin_syscall_callback(callback,&old_opaque));
    assert(penguin_syscall_detach() && sc_ops==&foreign);
    sc_ops=NULL;assert(set_penguin_syscall_callback(callback,&old_opaque));
    unsigned old_watches=watches();
    bql=false;assert(!penguin_syscall_detach());bql=true;
    current_cpu=cpus;assert(!penguin_syscall_detach());current_cpu=NULL;
    cpus[1].stopped=false;assert(!penguin_syscall_detach());cpus[1].stopped=true;
    assert(psc_consumer && psc_cb==callback && watches()==old_watches);
    insert_fails=true;assert(!penguin_kernel_hook(1,kernel,&old_opaque));assert(!psc_kernel_hooks);
    insert_fails=false;uint64_t h=penguin_kernel_hook(1,kernel,&old_opaque);assert(h && psc_kernel_hooks==1);
    assert(!penguin_syscall_detach() && watches()==old_watches);
    assert(penguin_kernel_unhook(h));assert(!penguin_kernel_unhook(h) && !psc_kernel_hooks);
    for(int state=SC_INJECT;state<=SC_INJECT_RET;state++) {
        set_penguin_syscall_boot_callback(boot,&old_opaque);
        psc_booted=true;
        penguin_syscall_set_scope(true);penguin_syscall_set_current_source(true);
        add_run_at(0,SC_PENDING,true);
        add_run_at(1,SC_HELD_ENTER,true);
        add_run_at(2,SC_HELD_RETURN,true);
        add_run_at(SC_SLOTS-2,state,true);
        add_run_at(SC_SLOTS-1,SC_PENDING,false);
        cpus[0].syscall_current=cpus[1].syscall_current=42;
        cpus[0].syscall_kmmu=cpus[1].syscall_kmmu=7;
        cpus[0].syscall_inject_abis=cpus[1].syscall_inject_abis=3;
        sc_fault_addr=123;sc_fault_write=true;
        SCSlot before[SC_SLOTS];
        CPUState cpu_before[2];
        uint8_t watch_before[sizeof(sc_watch)],all_before[sizeof(sc_watch_all)];
        memcpy(before,sc_slots,sizeof(before));memcpy(cpu_before,cpus,sizeof(cpus));
        memcpy(watch_before,sc_watch,sizeof(sc_watch));memcpy(all_before,sc_watch_all,sizeof(sc_watch_all));
        int old_aborts=aborts;unsigned old_freed=freed;
        bool old_dirty=psc_dirty;
        uint64_t seq=sc_seq,full=sc_full,stale=sc_stale;
        for(unsigned retry=0;retry<2;retry++) {
            assert(!penguin_syscall_detach());
            assert(!memcmp(before,sc_slots,sizeof(before)) && !memcmp(cpu_before,cpus,sizeof(cpus)));
            assert(!memcmp(watch_before,sc_watch,sizeof(sc_watch)) && !memcmp(all_before,sc_watch_all,sizeof(sc_watch_all)));
            assert(syscall_trap_pending==5 && sc_held==3 && syscall_trap_on);
            assert(sc_seq==seq && sc_full==full && sc_stale==stale);
            assert(sc_ops==&psc_ops && !sc_opaque && !sc_detaching && !psc_detaching);
            assert(psc_consumer && psc_cb==callback && psc_cb_opaque==&old_opaque);
            assert(psc_boot_cb==boot && psc_boot_opaque==&old_opaque && psc_booted);
            assert(psc_scope && psc_cur_cpu && psc_dirty==old_dirty);
            assert(sc_fault_addr==123 && sc_fault_write);
            assert(aborts==old_aborts && freed==old_freed && (!sc_dropped || !sc_dropped->len));
        }
        /* Fixture models completed injection; pending/held ownership can drain. */
        sc_slots[SC_SLOTS-2].state=SC_HELD_RETURN;
        assert(penguin_syscall_detach());
        assert(aborts==old_aborts+4 && freed==old_freed+4);
        assert(!syscall_trap_pending && !sc_held && !watches() && !syscall_trap_on && !sc_ops && !sc_opaque);
        assert(!psc_consumer && !psc_cb && !psc_cb_opaque && !psc_boot_cb && !psc_boot_opaque && !psc_booted);
        assert(!sc_detaching && !psc_detaching);
        psc_mark_dirty();assert(psc_dirty && !watches()); // inert persistent state
        assert(penguin_syscall_detach() && aborts==old_aborts+4 && freed==old_freed+4);
        assert(set_penguin_syscall_callback(callback,&old_opaque));assert(watches() && psc_consumer);
    }
}
'''
with tempfile.TemporaryDirectory(prefix='penguin-detach-') as temp:
    out=Path(temp);(out/'test.c').write_text(stubs+body+tests)
    flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','glib-2.0'],text=True))
    subprocess.run([os.environ.get('CC','cc'),'-std=gnu11','-O2',str(out/'test.c'),'-o',str(out/'test'),*flags],check=True)
    subprocess.run([str(out/'test')],check=True)
# Every opaque-bearing/public mutation path rejects drain reentry; refresh and
# internal hooks must not revive watches or affect a replacement core consumer.
for name in ('set_penguin_syscall_boot_callback','penguin_syscall_set_name','penguin_syscall_set_layout','penguin_syscall_set_guard','penguin_syscall_set_reaper','penguin_syscall_set_exec','penguin_syscall_register','penguin_syscall_unregister','penguin_syscall_set_scope','penguin_syscall_set_current_source'):
    assert 'psc_detaching' in function(psc,name),name
for name in ('psc_refresh','psc_guard_cb','psc_reap_cb','psc_exec_cb'):
    assert 'qatomic_read(&psc_consumer)' in function(psc,name),name
print('PASS: actual Penguin/core detach/cancel/watch/drop bodies; mixed late-injection refusal without partial changes; outside-lock mixed drain retains callback/opaque; unsafe/conflict/reentrant/idempotent cases and inert configuration')
