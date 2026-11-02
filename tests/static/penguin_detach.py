#!/usr/bin/env python3
"""Compile actual Penguin detach, public hook and core consumer-list bodies offline.

Penguin is one consumer of the syscall trap among others: its detach removes its own part of every
call (its runs aborted outside the locks) and leaves another consumer's calls, holds, injected
calls and watches as they were."""
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
trap=header[header.index('/* the same values'):header.index('#endif /* SYSTEM_SYSCALL_TRAP_H */')]
slots=(core[core.index('#define SC_SLOTS'):core.index('\nvoid syscall_trap_fault(')]+
       core[core.index('\nunsigned syscall_trap_forget_task('):core.index('\nbool syscall_trap_read(')])


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
#define unlikely(x) __builtin_expect(!!(x), 0)
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define qatomic_rcu_read(p) (*(p))
#define qatomic_rcu_set(p,v) (*(p)=(v))
#define RCU_READ_LOCK_GUARD()
struct rcu_head { int unused; };
#define g_free_rcu(p,field) g_free(p)
static bool replay_analysis_write_allowed(void) { return true; }
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
typedef uint64_t vaddr;
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
body=trap+slots+'\nstatic SyscallTrapConsumer *psc_trap;\n'
body+='\n'+'\n'.join(function(psc,n) for n in ('psc_run_free','psc_run_abort','psc_drop'))
body+='\nstatic const SyscallTrapOps psc_ops={.drop=psc_drop};\n'
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
static int old_opaque,aborts,foreign_drops;
static SyscallTrapConsumer *foreign;
static unsigned watches(void) {
    unsigned n=0;
    for(unsigned abi=0;abi<SYSCALL_ABI_COUNT;abi++) {
        n+=sc_watch_all[abi];
        for(unsigned num=0;num<SYSCALL_TRAP_NUMS;num++) n+=sc_watch[abi][num];
    }
    return n;
}
static void foreign_drop(SyscallTrap *sc,void *opaque) { assert(opaque==&foreign_drops);g_free(sc->data);foreign_drops++; }
static const SyscallTrapOps foreign_ops={.drop=foreign_drop};
/* Penguin's part (a run, or none) and, if @other, the foreign consumer's (some bytes); @holder holds it. */
static void add_run_at(unsigned index,int state,bool with_data,bool other,SyscallTrapConsumer *holder) {
    uint64_t key=1;
    while(sc_home(key,2,0x4000)!=index) key++;
    assert(!sc_slots[index].pc);
    PRun *r=with_data?g_new0(PRun,1):NULL;
    if(r) { r->running=true;r->wdata=g_malloc(8);r->rbuf=g_malloc(8); }
    SCPart parts[2]={{.c=psc_trap,.data=r,.want_return=true},{.c=foreign,.data=g_malloc(4),.want_return=true}};
    SCCall call={.holder=holder->order,.n=other?2:1,.cap=2,.part=parts};
    if(!other) g_free(parts[1].data);
    sc_add(key,2,0x4000,4,&call,state);
    assert(sc_slots[index].pc && sc_slots[index].state==state);
}
static unsigned penguin_parts(void) {
    unsigned n=0;
    for(unsigned i=0;i<SC_SLOTS;i++)
        for(unsigned j=0;sc_slots[i].pc && j<sc_slots[i].call.n;j++) n+=sc_slots[i].call.part[j].c==psc_trap;
    return n;
}
static void boot(CPUState *cpu,void *opaque) {}
static uint64_t kernel(CPUState *cpu,uint64_t pc,void *opaque) { return 0; }
static void callback(CPUState *cpu,struct penguin_sc_event *ev,struct penguin_sc_cmd *cmd,void *opaque) {
    assert(!cpu && opaque==&old_opaque && cmd->op==PENGUIN_SC_OP_ABORT);
    assert(g_mutex_trylock(&psc_lock));g_mutex_unlock(&psc_lock);
    assert(g_mutex_trylock(&sc_lock));g_mutex_unlock(&sc_lock);
    assert(g_mutex_trylock(&sc_cons_lock));g_mutex_unlock(&sc_cons_lock);
    assert(g_mutex_trylock(&psc_boot_lock));g_mutex_unlock(&psc_boot_lock);
    /* Penguin's parts left every call before its first ABORT; the trap no longer lists it. */
    assert(!penguin_parts() && psc_trap->removing);
    assert(foreign ? sc_list && sc_list->n==1 && sc_list->c[0]==foreign : !sc_list);
    assert(psc_detaching && psc_consumer && psc_cb==callback && psc_cb_opaque==&old_opaque);
    assert(psc_boot_cb==boot && psc_boot_opaque==&old_opaque && psc_booted);
    assert(!set_penguin_syscall_callback(NULL,NULL));
    assert(!penguin_syscall_detach());
    assert(!penguin_kernel_hook(1,kernel,&old_opaque));
    assert(!syscall_trap_watch(psc_trap,0,1,3));
    set_penguin_syscall_boot_callback(boot,NULL);
    assert(psc_boot_opaque==&old_opaque);
    penguin_syscall_set_scope(false);penguin_syscall_set_current_source(false);
    assert(psc_scope && psc_cur_cpu);
    unsigned w=watches();psc_mark_dirty();assert(watches()==w);
    aborts++;
}
int main(void) {
    /* Another consumer, with its own watch: Penguin attaches beside it. */
    foreign=syscall_trap_add_consumer(&foreign_ops,&foreign_drops,0);
    assert(syscall_trap_watch(foreign,0,7,3) && watches()==3);
    assert(penguin_syscall_detach() && sc_list->n==1);            /* not attached: nothing to do */
    assert(set_penguin_syscall_callback(callback,&old_opaque));
    assert(psc_trap && psc_trap!=foreign && sc_list->n==2 && sc_list->c[0]==foreign && sc_list->c[1]==psc_trap);
    assert(set_penguin_syscall_callback(callback,&old_opaque) && sc_list->n==2);   /* idempotent */
    unsigned old_watches=watches();
    assert(old_watches>3);
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
        add_run_at(0,SC_PENDING,true,true,psc_trap);
        add_run_at(1,SC_HELD_ENTER,true,false,psc_trap);
        add_run_at(2,SC_HELD_RETURN,true,false,psc_trap);
        add_run_at(3,state,false,true,foreign);        /* the other consumer's injection */
        add_run_at(SC_SLOTS-2,state,true,false,psc_trap);
        add_run_at(SC_SLOTS-1,SC_PENDING,false,false,psc_trap);
        cpus[0].syscall_current=cpus[1].syscall_current=42;
        cpus[0].syscall_kmmu=cpus[1].syscall_kmmu=7;
        cpus[0].syscall_inject_abis=cpus[1].syscall_inject_abis=3;
        sc_fault_addr=123;sc_fault_write=true;
        static SCSlot before[SC_SLOTS];
        static SCPart parts[SC_SLOTS][2];
        CPUState cpu_before[2];
        static uint8_t watch_before[sizeof(sc_watch)];
        uint8_t all_before[sizeof(sc_watch_all)];
        memcpy(before,sc_slots,sizeof(before));memcpy(cpu_before,cpus,sizeof(cpus));
        for(unsigned i=0;i<SC_SLOTS;i++) if(sc_slots[i].pc) memcpy(parts[i],sc_slots[i].call.part,sc_slots[i].call.n*sizeof(SCPart));
        memcpy(watch_before,sc_watch,sizeof(sc_watch));memcpy(all_before,sc_watch_all,sizeof(sc_watch_all));
        int old_aborts=aborts,old_foreign=foreign_drops;unsigned old_freed=freed;
        bool old_dirty=psc_dirty;
        uint64_t seq=sc_seq,full=sc_full,stale=sc_stale;
        SCList *list=sc_list;
        for(unsigned retry=0;retry<2;retry++) {
            assert(!penguin_syscall_detach());
            assert(!memcmp(before,sc_slots,sizeof(before)) && !memcmp(cpu_before,cpus,sizeof(cpus)));
            for(unsigned i=0;i<SC_SLOTS;i++)
                assert(!sc_slots[i].pc || !memcmp(parts[i],sc_slots[i].call.part,sc_slots[i].call.n*sizeof(SCPart)));
            assert(!memcmp(watch_before,sc_watch,sizeof(sc_watch)) && !memcmp(all_before,sc_watch_all,sizeof(sc_watch_all)));
            assert(syscall_trap_pending==6 && sc_held==4 && syscall_trap_on);
            assert(sc_seq==seq && sc_full==full && sc_stale==stale && sc_list==list && list->n==2);
            assert(!psc_trap->removing && !psc_detaching);
            assert(psc_consumer && psc_cb==callback && psc_cb_opaque==&old_opaque);
            assert(psc_boot_cb==boot && psc_boot_opaque==&old_opaque && psc_booted);
            assert(psc_scope && psc_cur_cpu && psc_dirty==old_dirty);
            assert(sc_fault_addr==123 && sc_fault_write);
            assert(aborts==old_aborts && freed==old_freed && foreign_drops==old_foreign);
            assert(!sc_dropped || !sc_dropped->len);
        }
        /* Fixture models Penguin's completed injection; the other's is still running. */
        sc_slots[SC_SLOTS-2].state=SC_HELD_RETURN;
        assert(penguin_syscall_detach());
        assert(aborts==old_aborts+4 && freed==old_freed+4 && foreign_drops==old_foreign);
        /* The other consumer's calls stay with their data; Penguin-held calls stay held, for the
         * consumers after it; its only pending call without data goes. */
        assert(syscall_trap_pending==5 && sc_held==4 && !penguin_parts());
        assert(sc_slots[0].call.n==1 && sc_slots[0].call.part[0].c==foreign);
        assert(sc_slots[3].state==state && sc_slots[3].call.n==1 && sc_slots[3].call.part[0].c==foreign);
        assert(!sc_slots[SC_SLOTS-1].pc);
        assert(sc_list && sc_list->n==1 && sc_list->c[0]==foreign);
        assert(watches()==3 && sc_watch[0][7]==3 && syscall_trap_on);   /* the other's watch only */
        assert(!psc_consumer && !psc_trap && !psc_cb && !psc_cb_opaque && !psc_boot_cb && !psc_boot_opaque && !psc_booted);
        assert(!psc_detaching);
        psc_mark_dirty();assert(psc_dirty && watches()==3); // inert persistent state
        assert(penguin_syscall_detach() && aborts==old_aborts+4 && freed==old_freed+4);
        assert(set_penguin_syscall_callback(callback,&old_opaque));assert(watches()>3 && psc_consumer);
        assert(sc_list->n==2 && sc_list->c[1]==psc_trap);
        assert(syscall_trap_cancel_all()==5 && foreign_drops==old_foreign+2);
    }
    /* The last consumer to go takes every call with it. */
    assert(syscall_trap_remove_consumer(foreign) && sc_list->n==1 && sc_list->c[0]==psc_trap);
    foreign=NULL;
    set_penguin_syscall_boot_callback(boot,&old_opaque);
    psc_booted=true;
    penguin_syscall_set_scope(true);penguin_syscall_set_current_source(true);
    add_run_at(0,SC_HELD_ENTER,true,false,psc_trap);
    int old_aborts=aborts;
    assert(penguin_syscall_detach() && aborts==old_aborts+1);
    assert(!sc_list && !syscall_trap_pending && !sc_held && !syscall_trap_on && !watches());
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
print('PASS: actual Penguin/core detach/cancel/watch/drop bodies; mixed late-injection refusal without partial changes; outside-lock mixed drain retains callback/opaque; another consumer\'s calls, injection and watches untouched; unsafe/reentrant/idempotent cases and inert configuration')
