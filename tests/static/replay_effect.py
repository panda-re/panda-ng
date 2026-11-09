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
typedef struct CPUState { bool stopped; int gdb_num_regs; int cpu_index; int gdb_num_g_regs; } CPUState;
static CPUState cpus[]={{true,4,0,3},{true,4,1,3}}, *current_cpu;
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
typedef struct { uint8_t data[16]; unsigned len; } GByteArray;
static uint8_t regs[2][4][16];
static unsigned reg_writes;
static bool setter_refuses;  /* an ARM sysreg/pauth-style setter: returns 0, writes nothing */
static CPUState *qemu_get_cpu(int index) {return index>=0 && index<2 ? &cpus[index] : NULL;}
static GByteArray *g_byte_array_new(void) {return calloc(1,sizeof(GByteArray));}
static void g_byte_array_free(GByteArray *b,bool all) {(void)all;free(b);}
static int reg_size(CPUState *cpu,int reg) {return reg<0 || reg>=cpu->gdb_num_regs ? 0 : reg==3 ? 16 : 8;}
/* The vCPUs' env: 64 bytes; register 0 is its first 8 (a setter that writes CPU state). */
static uint8_t envs[2][64];
#define cpu_env(cpu) ((void *)envs[(cpu)-cpus])
static uint8_t *reg_slot(CPUState *cpu,int reg) {return reg ? regs[cpu-cpus][reg] : envs[cpu-cpus];}
static int gdb_read_register(CPUState *cpu,GByteArray *buf,int reg) {
    int n=reg_size(cpu,reg);memcpy(buf->data,reg_slot(cpu,reg),n);buf->len=n;return n;
}
static int gdb_write_register(CPUState *cpu,uint8_t *p,int reg) {
    int n=reg_size(cpu,reg);assert(n);reg_writes++;
    if(setter_refuses)return 0;
    memcpy(reg_slot(cpu,reg),p,n);return n;
}
static uint8_t log_bytes[70000];
static unsigned log_len, log_at;
static void replay_put_byte(uint8_t b) {log_bytes[log_len++]=b;}
static void replay_put_qword(uint64_t v) {for(int i=56;i>=0;i-=8)replay_put_byte(v>>i);}
static void replay_put_dword(uint32_t v) {for(int i=24;i>=0;i-=8)replay_put_byte(v>>i);}
static void replay_put_word(uint16_t v) {replay_put_byte(v>>8);replay_put_byte(v);}
static jmp_buf failure;
static _Noreturn void replay_sync_error(const char *s) {(void)s;longjmp(failure,1);}
static uint8_t replay_get_byte(void) {if(log_at==log_len)replay_sync_error("EOF");return log_bytes[log_at++];}
static uint64_t replay_get_qword(void) {uint64_t n=0;for(int i=0;i<8;i++)n=(n<<8)|replay_get_byte();return n;}
static uint32_t replay_get_dword(void) {uint32_t n=0;for(int i=0;i<4;i++)n=(n<<8)|replay_get_byte();return n;}
static uint16_t replay_get_word(void) {uint16_t n=replay_get_byte();return (n<<8)|replay_get_byte();}
/* Guest writes made by callbacks */
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define G_NORETURN _Noreturn
#define EVENT_GUEST_WRITE 0x5a
static unsigned tlb_flushes, syncs;
static void tlb_flush(CPUState *cpu) {(void)cpu;tlb_flushes++;}
#define g_new(T,n) ((T*)malloc(sizeof(T)*(n)))
#define g_renew(T,p,n) ((T*)realloc((p),sizeof(T)*(n)))
static void *g_malloc(size_t n) {allocations++;return malloc(n?n:1);}
typedef uint64_t vaddr;
static void cpu_synchronize_state(CPUState *cpu) {(void)cpu;}
static unsigned instruction_saves, accounts;
static void replay_save_instructions(void) {instruction_saves++;}
static void replay_put_event(uint8_t e) {replay_put_byte(e);}
static void replay_account_executed_instructions(void) {accounts++;}
static bool replay_next_event_is(int e) {
    if(log_at<log_len && log_bytes[log_at]==e){log_at++;return true;}return false;
}
static void replay_finish_event(void) {}
/* syscall_trap_access: guest-virtual writes through the TLB; vfills counts the
 * translation side effects (a TLB fill) replay must repeat. */
static uint8_t vram[32];
static unsigned vfills;
static bool vfail;
static bool syscall_trap_access(CPUState *cpu,vaddr a,void *buf,size_t n,bool w,vaddr *bad) {
    (void)cpu;assert(w);vfills++;
    if(vfail||a+n>sizeof(vram)){*bad=a;return false;}
    memcpy(vram+a,buf,n);return true;
}
'''
CHECKS = r'''
static void record_one(void *p) {
    replay_analysis_effect_prepare(p);
    replay_analysis_effect_save(p);
    replay_analysis_effect_run(p);
}
static void reset(void);
/* A handler as the embedding API's and PANDA's write APIs make one: admission, the write, the log. */
static bool panda_write(CPUState *cpu,vaddr a,const uint8_t *p,size_t n) {
    vaddr bad;bool ok;
    if(replay_guest_write_admitted(NULL,n)!=GUEST_WRITE_OK)return false;
    ok=syscall_trap_access(cpu,a,(void*)p,n,true,&bad);
    replay_guest_write_log_vaddr(cpu,a,p,n,ok?GUEST_WRITE_OK:GUEST_WRITE_EFAULT);return ok;
}
static int env_write(CPUState *cpu,const uint32_t *d,unsigned n,uint32_t off,const void *p,uint32_t len) {
    int r=replay_guest_write_admitted(cpu,len);
    return r!=GUEST_WRITE_OK ? r : replay_guest_write_env(cpu,d,n,off,p,len);
}
static int reg_write(CPUState *cpu,int reg,const uint8_t *p,int n) {
    int r=replay_guest_write_admitted(NULL,n);
    if(r!=GUEST_WRITE_OK)return r;
    r=gdb_write_register(cpu,(uint8_t*)p,reg)>0?GUEST_WRITE_OK:GUEST_WRITE_EACCES;
    replay_guest_write_log_reg(cpu,reg,p,n,r);return r;
}
/* The layout: env is 64 bytes, with a pointer at 40 to `elems` 4-byte scalars
 * (length entry 0) and one at 48 to an Inner (16 bytes, a host pointer at 8). */
static uint32_t arr_rec[4],arr_play[4];
static uint8_t inner_rec[16],inner_play[16];
static size_t elems=4;
static size_t layout_length(CPUState *cpu,int entry) {(void)cpu;assert(entry==0);return elems;}
static void layout_sync(CPUState *cpu) {(void)cpu;syncs++;}
static const GuestLayoutRange env_host[]={{40,16}},inner_host[]={{8,8}};
static const GuestLayoutPointer env_follow[]={{40,-1,4,0},{48,1,16,-1}};
static const GuestLayoutType types[]={{"CPUArchState",64,env_follow,2,env_host,1},
                                      {"Inner",16,NULL,0,inner_host,1}};
static const GuestLayoutField fields[]={{0,8,"pc"},{8,32,"gpr"},{40,8,"arr"},{48,8,"inner"},{56,8,"flags"}};
static const GuestLayout layout={0xfeed,types,2,fields,5,layout_length,layout_sync};
static const GuestLayout other_layout={0xbeef,types,2,fields,5,layout_length,layout_sync};
/* Each process has its own host addresses: the record and the replay pointees differ. */
static void machine(uint32_t *arr,uint8_t *inner) {
    memset(envs,0,sizeof(envs));memset(arr,0,16);memset(inner,0,16);
    for(int c=0;c<2;c++){memcpy(envs[c]+40,&arr,8);memcpy(envs[c]+48,&inner,8);}
}
static void guest_write_checks(void) {
    const uint8_t buf[]={0xa1,0xa2,0xa3},value[8]={9,9,9,9,9,9,9,9},pc[8]={0x40,0,0,0,0,0,0,0x80};
    const uint32_t gpr=0x01020304,elem=0x55667788,arr_path[]={40},inner_path[]={48};
    const uint32_t not_pointer[]={56},too_deep[]={40,0};
    const uint8_t field[8]={1,2,3,4,5,6,7,8};
    uint64_t ret=0;
    replay_guest_layout_register(&layout);
    /* Not recording: no scope, nothing admitted by it (callers apply their policy). */
    replay_mode=REPLAY_MODE_NONE;reset();
    assert(!replay_guest_write_begin(&cpus[0],REPLAY_GUEST_SITE_HYPERCALL));
    assert(replay_guest_write_admitted(NULL,1)==GUEST_WRITE_EPERM);
    /* Recording: memory, registers (this vCPU's register 0 is in its env), env
     * bytes, paths through both kinds of pointer, a sync; and the refusals. */
    replay_mode=REPLAY_MODE_RECORD;reset();machine(arr_rec,inner_rec);
    assert(replay_guest_write_begin(&cpus[0],REPLAY_GUEST_SITE_HYPERCALL));
    assert(replay_guest_write_admitted(&cpus[0],1)==GUEST_WRITE_OK);
    assert(replay_guest_write_admitted(&cpus[1],1)==GUEST_WRITE_EPERM);  /* another vCPU's env */
    assert(panda_write(&cpus[0],4,buf,3));
    assert(!panda_write(&cpus[0],30,buf,3));   /* refused the same way in replay */
    ram[20]=0x77;replay_guest_write_log_ram(20,(const uint8_t[]){0x77},1);
    assert(reg_write(&cpus[1],2,value,8)==GUEST_WRITE_OK);
    assert(reg_write(&cpus[0],0,pc,8)==GUEST_WRITE_OK);
    assert(env_write(&cpus[0],NULL,0,8,&gpr,4)==GUEST_WRITE_OK && !memcmp(envs[0]+8,&gpr,4));
    assert(env_write(&cpus[0],arr_path,1,4,&elem,4)==GUEST_WRITE_OK && arr_rec[1]==elem);
    assert(env_write(&cpus[0],inner_path,1,0,field,8)==GUEST_WRITE_OK && !memcmp(inner_rec,field,8));
    unsigned logged=guest_scope->nops;
    assert(env_write(&cpus[0],NULL,0,36,field,8)==GUEST_WRITE_EINVAL);    /* a host pointer */
    assert(env_write(&cpus[0],NULL,0,60,field,8)==GUEST_WRITE_EINVAL);    /* past env */
    assert(env_write(&cpus[0],arr_path,1,16,&elem,4)==GUEST_WRITE_EINVAL); /* past the array */
    assert(env_write(&cpus[0],inner_path,1,8,field,8)==GUEST_WRITE_EINVAL);/* Inner's host pointer */
    assert(env_write(&cpus[0],not_pointer,1,0,field,4)==GUEST_WRITE_EINVAL);
    assert(env_write(&cpus[0],too_deep,2,0,field,4)==GUEST_WRITE_EINVAL); /* scalars end a path */
    assert(env_write(&cpus[0],NULL,0,8,field,0)==GUEST_WRITE_EINVAL);
    assert(guest_scope->nops==logged && arr_rec[0]==0);     /* malformed: no change, no log */
    assert(replay_guest_write_sync(&cpus[0])==GUEST_WRITE_OK && syncs==1 && tlb_flushes==1);
    assert(replay_guest_write_admitted(NULL,REPLAY_GUEST_WRITE_BYTES_MAX)==GUEST_WRITE_E2BIG);
    unsigned writes_before=reg_writes;
    replay_guest_write_end(&cpus[0],true,0x1234);
    /* Ending the site repeats nothing; env was written, so the TLB was flushed. */
    assert(reg_writes==writes_before && syncs==1 && tlb_flushes==2);
    assert(instruction_saves==1 && vfills==2 && log_bytes[0]==EVENT_GUEST_WRITE);
    assert(replay_guest_write_admitted(NULL,1)==GUEST_WRITE_EPERM);
    /* Replay on a fresh machine with other host addresses: no handler, the same
     * writes along the same paths, the same translation side effects (vfills). */
    unsigned recorded=log_len;
    memset(vram,0,sizeof(vram));memset(ram,0,sizeof(ram));memset(regs,0,sizeof(regs));
    machine(arr_play,inner_play);vfills=0;reg_writes=0;tlb_flushes=0;syncs=0;
    replay_mode=REPLAY_MODE_PLAY;log_at=0;
    assert(replay_guest_write_replay(&cpus[0],REPLAY_GUEST_SITE_HYPERCALL,&ret) && ret==0x1234);
    assert(log_at==recorded && accounts==1 && vfills==2 && !memcmp(vram+4,buf,3) && !vram[30]);
    assert(ram[20]==0x77 && !memcmp(regs[1][2],value,8) && !memcmp(envs[0],pc,8));
    assert(!memcmp(envs[0]+8,&gpr,4) && arr_play[1]==elem && !memcmp(inner_play,field,8));
    assert(!arr_rec[2] && syncs==1 && tlb_flushes==2);
    assert(replay_guest_write_admitted(NULL,1)==GUEST_WRITE_EPERM);
    /* A recording that wrote env replays only with the same layout. */
    replay_guest_layout_register(&other_layout);log_at=0;
    if(!setjmp(failure)) {replay_guest_write_replay(&cpus[0],REPLAY_GUEST_SITE_HYPERCALL,&ret);
                          assert(!"another layout accepted");}
    replay_guest_layout_register(&layout);
    /* ... and only at the same kind of site. */
    log_at=0;
    if(!setjmp(failure)) {replay_guest_write_replay(&cpus[0],2,&ret);assert(!"another site accepted");}
    /* An unanswered hypercall is still one event, and replays as unanswered. */
    replay_mode=REPLAY_MODE_RECORD;reset();machine(arr_rec,inner_rec);
    assert(replay_guest_write_begin(&cpus[1],REPLAY_GUEST_SITE_HYPERCALL));
    replay_guest_write_end(&cpus[1],false,0);
    assert(!tlb_flushes);              /* no env write, no flush */
    replay_mode=REPLAY_MODE_PLAY;ret=7;
    assert(!replay_guest_write_replay(&cpus[1],REPLAY_GUEST_SITE_HYPERCALL,&ret) && !ret && log_at==log_len);
    /* A write whose result differs in replay, a missing event, a bad payload. */
    replay_mode=REPLAY_MODE_RECORD;reset();machine(arr_rec,inner_rec);
    assert(replay_guest_write_begin(&cpus[0],REPLAY_GUEST_SITE_HYPERCALL));
    assert(panda_write(&cpus[0],0,buf,3));
    replay_guest_write_end(&cpus[0],true,1);
    replay_mode=REPLAY_MODE_PLAY;vfail=true;
    if(!setjmp(failure)) {replay_guest_write_replay(&cpus[0],REPLAY_GUEST_SITE_HYPERCALL,&ret);
                          assert(!"diverged write accepted");}
    vfail=false;
    log_at=0;log_bytes[0]=0;
    if(!setjmp(failure)) {replay_guest_write_replay(&cpus[0],REPLAY_GUEST_SITE_HYPERCALL,&ret);
                          assert(!"missing event accepted");}
    log_bytes[0]=EVENT_GUEST_WRITE;
    /* version, site, op count, op kind, path length, data length (high bytes) */
    const unsigned bad[]={1,2,17,21,39,40};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        uint8_t saved=log_bytes[bad[i]];log_bytes[bad[i]]=0xff;log_at=0;
        if(!setjmp(failure)) {replay_guest_write_replay(&cpus[0],REPLAY_GUEST_SITE_HYPERCALL,&ret);
                              assert(!"bad guest-write payload accepted");}
        log_bytes[bad[i]]=saved;
    }
}
static void reset(void) {
    allocations=writes=dirty_invalidations=pending_count=log_len=log_at=reg_writes=0;
    instruction_saves=accounts=vfills=tlb_flushes=syncs=0;
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
    /* A setter that refuses the write is recorded as denied, changes nothing and
     * leaves the recording going; replay does not attempt it. */
    replay_mode=REPLAY_MODE_RECORD;reset();setter_refuses=true;
    assert(replay_analysis_reg_write_request(0,2,value,8));
    if(setjmp(failure)) assert(!"a refused register write ended the recording");
    record_one(pending[0]);
    assert(reg_writes==1 && log_bytes[2]==RAM_EFFECT_DENIED && !regs[0][2][0] && !ram_effect_pending);
    replay_mode=REPLAY_MODE_PLAY;effect=replay_analysis_effect_load();replay_analysis_effect_run(effect);
    assert(reg_writes==1 && !regs[0][2][0]);
    /* One the recording's setter accepted but replay's refuses is a divergence. */
    replay_mode=REPLAY_MODE_RECORD;reset();setter_refuses=false;
    assert(replay_analysis_reg_write_request(0,2,value,8));record_one(pending[0]);
    assert(log_bytes[2]==RAM_EFFECT_OK);
    replay_mode=REPLAY_MODE_PLAY;effect=replay_analysis_effect_load();setter_refuses=true;
    if(!setjmp(failure)) {replay_analysis_effect_run(effect);assert(!"refused replay write accepted");}
    replay_analysis_effect_cancel(effect);setter_refuses=false;
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
    guest_write_checks();
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
                         'system/analysis-policy.h', 'replay-internal.h', 'exec/gdbstub.h',
                         'system/hw_accel.h', 'system/syscall-trap.h', 'qemu/typedefs.h',
                         'exec/cputlb.h', 'qemu/error-report.h'):
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
