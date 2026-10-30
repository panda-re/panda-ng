"""Standalone actual RAM-effect implementation tests; no engine/guest build.
Run: panda-python tests/static/replay_effect.py (CC defaults to cc).
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / 'engine/src'
STUBS = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <limits.h>
#define g_assert assert
#define g_free free
#define RCU_READ_LOCK_GUARD()
#define MEMTXATTRS_UNSPECIFIED 0
#define MEMTX_OK 0
#define ANALYSIS_CAP_RR 4
#define REPLAY_ASYNC_EVENT_ANALYSIS_EFFECT 7
#define CPU_FOREACH(cpu) for (int i=0;i<2 && ((cpu)=&cpus[i],1);i++)
enum {REPLAY_MODE_NONE,REPLAY_MODE_RECORD,REPLAY_MODE_PLAY};
typedef uint64_t hwaddr;
typedef struct { bool stopped; int gdb_num_regs; } CPUState;
static CPUState cpus[]={{true,4},{true,4}}, *current_cpu;
static int replay_mode=REPLAY_MODE_RECORD;
static bool main_thread=true,bql=true,locked=true,enabled=true,cap=true;
static void *replay_file=(void*)1;
static unsigned allocations, writes, dirty_invalidations;
static void *g_malloc0(size_t n) { allocations++;return calloc(1,n); }
static bool qemu_in_main_thread(void) {return main_thread;}
static bool bql_locked(void) {return bql;}
static bool replay_mutex_locked(void) {return locked;}
static bool replay_events_enabled(void) {return enabled;}
static bool analysis_policy_has(int mask) {(void)mask;return cap;}
static bool cpu_is_stopped(CPUState *cpu) {return cpu->stopped;}
typedef struct {bool ram,readonly,device,romd;} MemoryRegion;
static MemoryRegion regions[]={{true,false,false,false},{true,false,false,false}};
static int address_space_memory;
static uint8_t ram[32];
static MemoryRegion *address_space_translate(void *as,uint64_t addr,hwaddr *off,
                                            hwaddr *len,bool wr,int attrs) {
    (void)as;(void)wr;(void)attrs;
    *off=addr%16;
    if (*len>16-*off) *len=16-*off;
    return &regions[addr<16?0:1];
}
static bool memory_region_is_ram(MemoryRegion *m) {return m->ram;}
static bool memory_region_is_ram_device(MemoryRegion *m) {return m->device;}
static bool memory_region_is_romd(MemoryRegion *m) {return m->romd;}
static bool memory_access_is_direct(MemoryRegion *m,bool w,int a) {(void)w;(void)a;return m->ram;}
static int address_space_write(void *as,uint64_t addr,int attrs,const uint8_t *p,uint32_t n) {
    (void)as;(void)attrs;assert(addr+n<=sizeof(ram));
    writes++;dirty_invalidations++;memcpy(ram+addr,p,n);return MEMTX_OK;
}
static void *pending[64];
static unsigned pending_count;
static void replay_add_event(int kind,void *effect,void *other,uint64_t id) {
    assert(kind==7 && !other && !id);pending[pending_count++]=effect;
}
/* gdbstub registers: 0-2 are 8 bytes, 3 is 16; reg_writes counts setter calls. */
typedef struct { uint8_t *data; unsigned len; } GByteArray;
static uint8_t regs[2][4][16];
static unsigned reg_writes;
static CPUState *qemu_get_cpu(int index) {return index>=0 && index<2 ? &cpus[index] : NULL;}
static GByteArray *g_byte_array_new(void) {return calloc(1,sizeof(GByteArray));}
static void g_byte_array_free(GByteArray *b,bool all) {(void)all;free(b);}
static int reg_size(CPUState *cpu,int reg) {return reg<0 || reg>=cpu->gdb_num_regs ? 0 : reg==3 ? 16 : 8;}
static int gdb_read_register(CPUState *cpu,GByteArray *buf,int reg) {(void)buf;return reg_size(cpu,reg);}
static int gdb_write_register(CPUState *cpu,uint8_t *p,int reg) {
    int n=reg_size(cpu,reg);assert(n);reg_writes++;memcpy(regs[cpu-cpus][reg],p,n);return n;
}
static uint8_t log_bytes[70000];
static unsigned log_len, log_at;
static void replay_put_byte(uint8_t b) {log_bytes[log_len++]=b;}
static void replay_put_qword(uint64_t v) {for(int i=56;i>=0;i-=8)replay_put_byte(v>>i);}
static void replay_put_dword(uint32_t v) {for(int i=24;i>=0;i-=8)replay_put_byte(v>>i);}
static jmp_buf failure;
static _Noreturn void replay_sync_error(const char *s) {(void)s;longjmp(failure,1);}
static uint8_t replay_get_byte(void) {if(log_at==log_len)replay_sync_error("EOF");return log_bytes[log_at++];}
static uint64_t replay_get_qword(void) {uint64_t n=0;for(int i=0;i<8;i++)n=(n<<8)|replay_get_byte();return n;}
static uint32_t replay_get_dword(void) {uint32_t n=0;for(int i=0;i<4;i++)n=(n<<8)|replay_get_byte();return n;}
'''
CHECKS = r'''
static void record_one(void *p) {
    replay_analysis_effect_prepare(p);
    replay_analysis_effect_save(p);
    replay_analysis_effect_run(p);
}
static void reset(void) {
    allocations=writes=dirty_invalidations=pending_count=log_len=log_at=reg_writes=0;
    memset(ram,0,sizeof(ram));memset(regs,0,sizeof(regs));
}
int main(void) {
    const uint8_t data[]={1,2,3,4};
    assert(!replay_analysis_ram_write_request(0,data,0));
    assert(!replay_analysis_ram_write_request(UINT64_MAX,data,2));
    assert(!replay_analysis_ram_write_request(0,data,REPLAY_RAM_EFFECT_MAX+1));
    assert(!allocations && !writes);
    for(int mode=0;mode<3;mode++) {
        replay_mode=mode;
        bool ok=replay_analysis_ram_write_request(14,data,4);
        assert(ok==(mode==REPLAY_MODE_RECORD));
        if(ok) replay_analysis_effect_cancel(pending[--pending_count]);
    }
    replay_mode=REPLAY_MODE_RECORD;
    bool *gates[]={&main_thread,&bql,&locked,&enabled,&cap,&cpus[1].stopped};
    for(unsigned i=0;i<sizeof(gates)/sizeof(*gates);i++) {
        *gates[i]=false;assert(!replay_analysis_ram_write_request(14,data,4));*gates[i]=true;
    }
    current_cpu=&cpus[0];assert(!replay_analysis_ram_write_request(0,data,4));current_cpu=NULL;
    reset();
    uint8_t source[]={1,2,3,4};
    assert(replay_analysis_ram_write_request(14,source,4));
    source[0]=99;assert(!writes && !ram[14]);
    record_one(pending[0]);
    assert(writes==1 && dirty_invalidations==1 && ram[14]==1 && ram[17]==4);
    assert(!ram_effect_pending);
    /* Replay loads/applies from ND payload with no mutator and no producer. */
    memset(ram,0,sizeof(ram));replay_mode=REPLAY_MODE_PLAY;
    unsigned before=allocations;
    cap=false;
    if(!setjmp(failure)) {replay_analysis_effect_load();assert(!"unprovisioned effect accepted");}
    assert(allocations==before && log_at==0);
    cap=true;
    void *effect=replay_analysis_effect_load();
    replay_analysis_effect_run(effect);
    assert(ram[14]==1 && ram[17]==4 && log_at==log_len);
    /* Every rejected region shape denies the WHOLE cross-region write. */
    replay_mode=REPLAY_MODE_RECORD;
    for(int reject=0;reject<4;reject++) {
        reset();regions[1]=(MemoryRegion){true,false,false,false};
        if(reject==0)regions[1].ram=false;
        if(reject==1)regions[1].readonly=true;
        if(reject==2)regions[1].device=true;
        if(reject==3)regions[1].romd=true;
        assert(replay_analysis_ram_write_request(14,data,4));record_one(pending[0]);
        assert(!writes && !ram[14] && log_bytes[2]==RAM_EFFECT_DENIED);
        replay_mode=REPLAY_MODE_PLAY;
        effect=replay_analysis_effect_load();replay_analysis_effect_run(effect);
        assert(!writes);replay_mode=REPLAY_MODE_RECORD;
    }
    regions[1]=(MemoryRegion){true,false,false,false};reset();
    /* Ordered requests, bounded pending memory, cancellation never applies. */
    for(int i=0;i<64;i++)assert(replay_analysis_ram_write_request(0,data,4));
    assert(!replay_analysis_ram_write_request(0,data,4));
    for(int i=0;i<64;i++)replay_analysis_effect_cancel(pending[i]);
    assert(!ram_effect_pending && !writes);
    reset();assert(replay_analysis_ram_write_request(0,data,4));
    const uint8_t later[]={8,9};
    assert(replay_analysis_ram_write_request(1,later,2));
    record_one(pending[0]);record_one(pending[1]);
    assert(ram[0]==1 && ram[1]==8 && ram[2]==9 && ram[3]==4);
    replay_mode=REPLAY_MODE_PLAY;memset(ram,0,sizeof(ram));
    effect=replay_analysis_effect_load();replay_analysis_effect_run(effect);
    effect=replay_analysis_effect_load();replay_analysis_effect_run(effect);
    assert(ram[0]==1 && ram[1]==8 && ram[2]==9 && ram[3]==4);
    replay_mode=REPLAY_MODE_RECORD;
    reset();assert(replay_analysis_ram_write_request(0,data,4));
    record_one(pending[0]);
    /* Validate payload headers and address/length before allocating. */
    const unsigned bad_fields[]={0,1,2,11,12};
    for(unsigned i=0;i<sizeof(bad_fields)/sizeof(*bad_fields);i++) {
        unsigned field=bad_fields[i];uint8_t saved=log_bytes[field];
        log_bytes[field]=255;log_at=0;unsigned before=allocations;
        if(!setjmp(failure)) {effect=replay_analysis_effect_load();assert(!"bad payload accepted");}
        assert(allocations==before);log_bytes[field]=saved;
    }
    log_at=0;effect=replay_analysis_effect_load();
    regions[0].readonly=true;
    if(!setjmp(failure)) {replay_analysis_effect_run(effect);assert(!"preflight mismatch accepted");}
    replay_analysis_effect_cancel(effect);
    regions[0].readonly=false;
    /* Register effects: shape is refused before allocation. */
    replay_mode=REPLAY_MODE_RECORD;reset();
    const uint8_t value[]={0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
    assert(!replay_analysis_reg_write_request(-1,0,value,8));
    assert(!replay_analysis_reg_write_request(0,-1,value,8));
    assert(!replay_analysis_reg_write_request(0,0,value,0));
    assert(!replay_analysis_reg_write_request(0,0,value,REPLAY_REG_EFFECT_MAX+1));
    assert(!replay_analysis_reg_write_request(0,0,NULL,8));
    for(unsigned i=0;i<sizeof(gates)/sizeof(*gates);i++) {
        *gates[i]=false;assert(!replay_analysis_reg_write_request(1,2,value,8));*gates[i]=true;
    }
    assert(!allocations);
    /* Admission copies; the drain writes through the gdbstub setter. */
    uint8_t copy[8];memcpy(copy,value,8);
    assert(replay_analysis_reg_write_request(1,2,copy,8));
    copy[0]=0;assert(!reg_writes);
    record_one(pending[0]);
    assert(reg_writes==1 && !memcmp(regs[1][2],value,8) && !writes);
    assert(log_bytes[1]==2 && log_bytes[2]==RAM_EFFECT_OK && !ram_effect_pending);
    replay_mode=REPLAY_MODE_PLAY;memset(regs,0,sizeof(regs));
    effect=replay_analysis_effect_load();replay_analysis_effect_run(effect);
    assert(reg_writes==2 && !memcmp(regs[1][2],value,8) && log_at==log_len);
    /* Missing vCPU, missing register and wrong size are recorded as denied. */
    const int denied[][3]={{5,0,8},{0,4,8},{0,3,8},{0,0,4}};
    for(unsigned i=0;i<4;i++) {
        replay_mode=REPLAY_MODE_RECORD;reset();
        assert(replay_analysis_reg_write_request(denied[i][0],denied[i][1],value,denied[i][2]));
        record_one(pending[0]);
        assert(!reg_writes && log_bytes[2]==RAM_EFFECT_DENIED);
        replay_mode=REPLAY_MODE_PLAY;
        effect=replay_analysis_effect_load();replay_analysis_effect_run(effect);
        assert(!reg_writes);
    }
    /* A 16-byte register takes a 16-byte payload. */
    replay_mode=REPLAY_MODE_RECORD;reset();
    const uint8_t wide[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    assert(replay_analysis_reg_write_request(0,3,wide,16));record_one(pending[0]);
    assert(reg_writes==1 && !memcmp(regs[0][3],wide,16));
    /* Replay against a machine without the register is a sync error. */
    reset();assert(replay_analysis_reg_write_request(0,1,value,8));record_one(pending[0]);
    replay_mode=REPLAY_MODE_PLAY;effect=replay_analysis_effect_load();
    cpus[0].gdb_num_regs=1;
    if(!setjmp(failure)) {replay_analysis_effect_run(effect);assert(!"register mismatch accepted");}
    replay_analysis_effect_cancel(effect);cpus[0].gdb_num_regs=4;
    /* Header (version, opcode, result), negative cpu/reg and length fields. */
    const unsigned bad_reg[]={0,1,2,3,7,11};
    for(unsigned i=0;i<sizeof(bad_reg)/sizeof(*bad_reg);i++) {
        unsigned field=bad_reg[i];uint8_t saved=log_bytes[field];
        log_bytes[field]=255;log_at=0;unsigned before=allocations;
        if(!setjmp(failure)) {effect=replay_analysis_effect_load();assert(!"bad register payload accepted");}
        assert(allocations==before);log_bytes[field]=saved;
    }
    /* RAM and register requests share one ordered queue and one pending cap. */
    replay_mode=REPLAY_MODE_RECORD;reset();
    for(int i=0;i<63;i++)assert(replay_analysis_ram_write_request(0,data,4));
    assert(replay_analysis_reg_write_request(0,0,value,8));
    assert(!replay_analysis_reg_write_request(0,0,value,8));
    assert(!replay_analysis_ram_write_request(0,data,4));
    for(int i=0;i<64;i++)replay_analysis_effect_cancel(pending[i]);
    assert(!ram_effect_pending && !writes && !reg_writes);
    return 0;
}
'''


class ReplayEffect(unittest.TestCase):
    def test_public_binding_header(self):
        plugin = (SRC / 'include/panda/plugin.h').read_text()
        self.assertIn('#include "panda/analysis-effect.h"', plugin)

    def test_bounded_deferred_ram_and_register_writes(self):
        with tempfile.TemporaryDirectory(prefix='ram-effect-') as tmp:
            tmp = Path(tmp)
            for name in ('qemu/osdep.h', 'qemu/main-loop.h', 'qemu/rcu.h', 'hw/core/cpu.h',
                         'system/cpus.h', 'system/address-spaces.h', 'system/memory.h', 'system/replay.h',
                         'system/analysis-policy.h', 'replay-internal.h', 'exec/gdbstub.h'):
                path = tmp / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('')
            source = tmp / 'test.c'
            source.write_text(STUBS + '\n#include "' + str(SRC / 'replay/replay-effect.c') + '"\n' + CHECKS)
            exe = tmp / 'test'
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O2', '-Wall', '-Werror',
                            '-I', str(tmp), '-I', str(SRC / 'include'), str(source),
                            '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    unittest.main()
