/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Taint: a TCG pass that adds a bit-precise shadow computation after translation.
 *
 * The pass runs once per translated block with CF_TAINT in its cflags, after plugin injection
 * and before tcg_optimize. It walks the op list and emits, next to each guest op, ops that
 * compute the op's output shadow from its inputs' shadows (Memcheck's V-bit rules).
 *
 * Shadow state:
 *   - every TCG global at env+off has a shadow global at senv+off, where senv is a per-vCPU
 *     buffer the size of the CPU state; explicit ld/st against env map to the same offsets;
 *   - every other temp gets a TEMP_TB shadow temp of the same type;
 *   - guest RAM has one shadow byte per byte, indexed by ram_addr_t (one sparse mapping);
 *   - a per-vCPU, per-mmu_idx shadow TLB (page -> shadow addend) sits after the shadow env,
 *     so a guest load/store's shadow access is inline on a hit and a helper call on a miss.
 * Helpers: the result shadow is the OR of the scalar input shadows (H1), except for the hand
 * models below; calls whose inputs are tainted are counted per helper.
 */
#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "qemu/bswap.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "qemu/madvise.h"
#include "qemu/memalign.h"
#include "qemu/main-loop.h"
#include "qom/object.h"
#include "hw/core/cpu.h"
#include "exec/cpu-common.h"
#include "exec/target_page.h"
#include "exec/translation-block.h"
#include "exec/tb-flush.h"
#include "exec/memop.h"
#include "tcg/tcg-gvec-desc.h"
#include "exec/cputlb.h"
#include "exec/helper-head.h.inc"
#include "accel/tcg/cpu-ldst-common.h"
#include "system/memory.h"
#include "system/ramblock.h"
#include "system/ramlist.h"
#include "system/replay.h"
#include "system/analysis-policy.h"
#include "tcg/tcg.h"
#include "tcg/tcg-temp-internal.h"
#include "tcg/tcg-op-common.h"
#include "tcg/tcg-internal.h"
#include "exec/taint.h"
#include "accel/tcg/hooks.h"
#include "accel/tcg/probe.h"
#include "exec/tlb-flags.h"
#include "accel/tcg/cpu-loop.h"
#include <dlfcn.h>

/* panda/plugin.h is target-specific; these two are not (panda/src/callbacks.c). */
bool panda_register_transient_reset(void *owner, void (*reset)(void *opaque), void *opaque);
void panda_unregister_transient_reset(void *owner);

bool taint_active;

/* ---------------------------------------------------------------- shadow state */

#define STLB_BITS 10
#define TAINT_MAX_ARGS 16   /* more than any non-call op has */
#define STLB_N (1u << STLB_BITS)

typedef struct STLBEntry {
    uint64_t tag;          /* guest page address, or -1 */
    intptr_t saddend;      /* shadow host address - guest vaddr */
} STLBEntry;

static size_t env_size;    /* bytes of CPU state from env on (instance size - env offset) */
static size_t stlb_off;    /* offset of the shadow TLB in the shadow env buffer */
static size_t ctlb_off;    /* ... and of the check variant's store TLB (see ctlb_fill) */
static size_t tick_off;         /* elastic: track TBs left before the next all-clean scan */
static size_t senv_bytes;
static intptr_t senv_field_off;   /* env-relative offset of CPUState::taint_senv */
static int senv_idx;              /* temp index of the senv pointer global */
static int nb_guest_globals;
static uint16_t shadow_global[TCG_MAX_TEMPS];    /* guest global idx -> shadow global idx */

static uint8_t *ram_shadow;
static uint64_t ram_shadow_size;

/*
 * The page bit: one byte per 4 KiB of RAM shadow, set before anything can write a nonzero
 * shadow byte there (a track-variant shadow TLB entry allows inline stores, so filling one
 * sets it). Never cleared. A check-variant store to a page without it has nothing to clean.
 */
#define DIRTY_BITS 12
static uint8_t *ram_dirty;
/* where a check-variant store to a clean page puts its zero shadow; never read */
static uint8_t ctlb_sink[64 * 1024] __attribute__((aligned(64)));

static TaintDeviceWriteFn device_write_fn;
static void *device_write_opaque;

static TaintMmioReadFn mmio_fn;
static void *mmio_opaque;

/* Events; the pass emits their checks only while a function is set. */
static TaintEventFn branch_fn, pc_fn;
static void *branch_opaque, *pc_opaque;
static ptrdiff_t pc_off = -1;   /* the PC's field in the CPU state (found by find_pc) */
static unsigned pc_size;

static struct {
    uint64_t tbs, ops, untracked_ld, untracked_st, ptr_calls, ld_slow, st_slow,
             ram_oob, unknown_ops, h1_tainted, device_writes, device_bytes,
             ptr_tainted, branch_events, pc_events, lab_calls, lab_many, mmio_tainted,
             helper_mem_bytes, h2b_calls, to_track, to_check, chk_slow, chk_st_slow;
} stats;
static GHashTable *h1_hits;     /* helper name -> tainted-call count */

static STLBEntry *stlb(CPUState *cpu, int mmu_idx)
{
    return (STLBEntry *)((uint8_t *)cpu->taint_senv + stlb_off) + (size_t)mmu_idx * STLB_N;
}

static STLBEntry *ctlb(CPUState *cpu, int mmu_idx)
{
    return (STLBEntry *)((uint8_t *)cpu->taint_senv + ctlb_off) + (size_t)mmu_idx * STLB_N;
}

static void ctlb_flush(CPUState *cpu)
{
    if (cpu->taint_senv) {
        memset(ctlb(cpu, 0), -1, sizeof(STLBEntry) * STLB_N * NB_MMU_MODES);
    }
}

static void stlb_flush_all(CPUState *cpu)
{
    if (cpu->taint_senv) {
        memset(stlb(cpu, 0), -1, sizeof(STLBEntry) * STLB_N * NB_MMU_MODES);
        ctlb_flush(cpu);
    }
}

void taint_tlb_flush_mmuidx(CPUState *cpu, int mmu_idx)
{
    if (cpu->taint_senv) {
        memset(stlb(cpu, mmu_idx), -1, sizeof(STLBEntry) * STLB_N);
        memset(ctlb(cpu, mmu_idx), -1, sizeof(STLBEntry) * STLB_N);
    }
}

void taint_tlb_flush_page(CPUState *cpu, int mmu_idx, vaddr page)
{
    if (cpu->taint_senv) {
        size_t i = (page >> TARGET_PAGE_BITS) & (STLB_N - 1);

        stlb(cpu, mmu_idx)[i].tag = -1;
        ctlb(cpu, mmu_idx)[i].tag = -1;
    }
}

static uint64_t ram_extent(void)
{
    uint64_t end = 0;
    RAMBlock *rb;

    RCU_READ_LOCK_GUARD();
    RAMBLOCK_FOREACH(rb) {
        end = MAX(end, qemu_ram_get_offset(rb) + qemu_ram_get_max_length(rb));
    }
    return end;
}

/* [0]: track blocks left before the next scan; [1]: the countdown it restarts from;
 * [2]: the check store TLB has sink entries (ctlb_fill) */
static uint32_t *tick(CPUState *cpu)
{
    return (uint32_t *)((uint8_t *)cpu->taint_senv + tick_off);
}

/* The page bit for [ra, ra + len) of RAM shadow; a check store TLB sink entry for it goes. */
static void ram_mark(uint64_t ra, uint64_t len)
{
    bool fresh = false;
    CPUState *cpu;

    if (!len || ra >= ram_shadow_size) {
        return;
    }
    for (uint64_t i = ra >> DIRTY_BITS; i <= (ra + len - 1) >> DIRTY_BITS; i++) {
        fresh |= !ram_dirty[i];
        ram_dirty[i] = 1;
    }
    if (!fresh) {
        return;
    }
    CPU_FOREACH(cpu) {
        if (cpu->taint_senv && tick(cpu)[2]) {
            tick(cpu)[2] = 0;
            ctlb_flush(cpu);
        }
    }
}

static bool ram_clean(uint64_t ra, uint64_t len)
{
    for (uint64_t i = ra >> DIRTY_BITS; i <= (ra + len - 1) >> DIRTY_BITS; i++) {
        if (ram_dirty[i]) {
            return false;
        }
    }
    return true;
}

static void ram_shadow_alloc(void)
{
    CPUState *cpu;
    uint64_t size = ROUND_UP(ram_extent(), qemu_real_host_page_size());

    if (ram_shadow && size <= ram_shadow_size) {
        return;
    }
    if (ram_shadow) {
        munmap(ram_shadow, ram_shadow_size);
    }
    ram_shadow = mmap(NULL, size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (ram_shadow == MAP_FAILED) {
        error_report("taint: cannot map %" PRIu64 " bytes of RAM shadow", size);
        abort();
    }
    ram_shadow_size = size;
    g_free(ram_dirty);
    ram_dirty = g_new0(uint8_t, (size >> DIRTY_BITS) + 1);
    CPU_FOREACH(cpu) {
        stlb_flush_all(cpu);    /* entries into the old mapping */
    }
}

static void ram_shadow_clear(void)
{
    if (ram_shadow) {
        qemu_madvise(ram_shadow, ram_shadow_size, QEMU_MADV_DONTNEED);
    }
}

static void senv_clear(CPUState *cpu)
{
    if (cpu->taint_senv) {
        memset(cpu->taint_senv, 0, stlb_off);
        stlb_flush_all(cpu);
    }
}

/* ---------------------------------------------------------------- slow-path helpers */

static uint8_t *ram_shadow_at(CPUState *cpu, vaddr addr, int mmu_idx, MMUAccessType type)
{
    uint64_t ra;

    if (!tlb_peek_ram_addr(cpu, addr, mmu_idx, type, &ra)) {
        return NULL;
    }
    if (ra >= ram_shadow_size) {
        stats.ram_oob++;
        return NULL;
    }
    return ram_shadow + ra;
}

/* The shadow of @addr's page if it is RAM all the way through. */
static uint8_t *ram_shadow_page(CPUState *cpu, vaddr addr, int mmu_idx, MMUAccessType type)
{
    vaddr page = addr & TARGET_PAGE_MASK;
    uint8_t *sp = ram_shadow_at(cpu, page, mmu_idx, type);

    if (sp && ram_shadow_at(cpu, page + TARGET_PAGE_SIZE - 1, mmu_idx, type)
              == sp + TARGET_PAGE_SIZE - 1) {
        return sp;
    }
    return NULL;
}

/*
 * Fill the shadow TLB for the page of @addr if it is RAM. A track block may then store taint
 * there inline, so the page bit is set; a check block only reads through it, and leaving the
 * check variant flushes it (taint_to_track).
 */
static void stlb_fill(CPUState *cpu, vaddr addr, int mmu_idx, MMUAccessType type)
{
    vaddr page = addr & TARGET_PAGE_MASK;
    uint8_t *sp = ram_shadow_page(cpu, addr, mmu_idx, type);

    if (sp) {
        STLBEntry *e = stlb(cpu, mmu_idx) + ((page >> TARGET_PAGE_BITS) & (STLB_N - 1));

        if (!(cpu->tcg_cflags & CF_TAINT_CHECK)) {
            ram_mark(sp - ram_shadow, TARGET_PAGE_SIZE);
        }
        e->saddend = (intptr_t)sp - (intptr_t)page;
        e->tag = page;
    }
}

/*
 * Fill the check variant's store TLB, which only ever stores zero: a page without the page bit
 * has no taint to clean, so its entry points at a sink and the clean store costs no shadow page.
 */
static void ctlb_fill(CPUState *cpu, vaddr addr, int mmu_idx)
{
    vaddr page = addr & TARGET_PAGE_MASK;
    uint8_t *sp = ram_shadow_page(cpu, addr, mmu_idx, MMU_DATA_STORE);
    STLBEntry *e = ctlb(cpu, mmu_idx) + ((page >> TARGET_PAGE_BITS) & (STLB_N - 1));

    if (!sp) {
        return;
    }
    if (ram_clean(sp - ram_shadow, TARGET_PAGE_SIZE)) {
        sp = ctlb_sink;
        tick(cpu)[2] = 1;
    }
    e->saddend = (intptr_t)sp - (intptr_t)page;
    e->tag = page;
}

static bool guest_big_endian(MemOp mop)
{
    return !!(mop & MO_BSWAP) != HOST_BIG_ENDIAN;
}

/*
 * Recent MMIO loads that the source callback tainted, for the same load's label step (which may
 * run at the end of the instruction). One vCPU runs at a time under rr.
 */
static struct {
    CPUState *cpu;
    uint64_t addr;
    unsigned size;
    uint32_t label;
} mmio_ring[8];
static unsigned mmio_next;

static void mmio_note(CPUState *cpu, uint64_t addr, unsigned size, uint32_t label)
{
    mmio_ring[mmio_next++ % ARRAY_SIZE(mmio_ring)] =
        (typeof(mmio_ring[0])){ cpu, addr, size, label };
}

static uint32_t mmio_label_at(CPUState *cpu, uint64_t addr)
{
    for (unsigned i = 1; i <= ARRAY_SIZE(mmio_ring); i++) {
        typeof(mmio_ring[0]) *e = &mmio_ring[(mmio_next - i) % ARRAY_SIZE(mmio_ring)];

        if (e->cpu == cpu && addr - e->addr < e->size) {
            return e->label;
        }
    }
    return 0;
}

static uint64_t taint_ld_slow(CPUArchState *env, uint64_t addr, uint32_t oi)
{
    CPUState *cpu = env_cpu(env);
    MemOp mop = get_memop(oi);
    int mmu_idx = get_mmuidx(oi);
    unsigned size = memop_size(mop);
    uint64_t v = 0;
    uint32_t mmio = 0;

    stats.ld_slow++;
    if (mmio_fn && !ram_shadow_at(cpu, addr, mmu_idx, MMU_DATA_LOAD) &&
        (mmio = mmio_fn(mmio_opaque, cpu, addr, size))) {
        stats.mmio_tainted++;
        mmio_note(cpu, addr, size, mmio);
    }
    for (unsigned i = 0; i < size; i++) {
        uint8_t *sp = ram_shadow_at(cpu, addr + i, mmu_idx, MMU_DATA_LOAD);
        uint64_t b = sp ? *sp : mmio ? 0xff : 0;
        unsigned shift = guest_big_endian(mop) ? 8 * (size - 1 - i) : 8 * i;

        v |= b << shift;
    }
    if ((mop & MO_SIGN) && size < 8) {
        v = sextract64(v, 0, size * 8);
    }
    if (((addr & ~TARGET_PAGE_MASK) + size) <= TARGET_PAGE_SIZE) {
        stlb_fill(cpu, addr, mmu_idx, MMU_DATA_LOAD);
    }
    return v;
}

static void taint_st_slow(CPUArchState *env, uint64_t addr, uint32_t oi, uint64_t v)
{
    CPUState *cpu = env_cpu(env);
    MemOp mop = get_memop(oi);
    int mmu_idx = get_mmuidx(oi);
    unsigned size = memop_size(mop);

    stats.st_slow++;
    for (unsigned i = 0; i < size; i++) {
        uint8_t *sp = ram_shadow_at(cpu, addr + i, mmu_idx, MMU_DATA_STORE);
        unsigned shift = guest_big_endian(mop) ? 8 * (size - 1 - i) : 8 * i;

        if (sp) {
            if ((uint8_t)(v >> shift)) {
                ram_mark(sp - ram_shadow, 1);
            }
            *sp = v >> shift;
        }
    }
    if (((addr & ~TARGET_PAGE_MASK) + size) <= TARGET_PAGE_SIZE) {
        stlb_fill(cpu, addr, mmu_idx, MMU_DATA_STORE);
    }
}

/* A check-variant store missed its store TLB: clean what it wrote, then fill the TLB. */
static void taint_chk_st_slow(CPUArchState *env, uint64_t addr, uint32_t oi)
{
    CPUState *cpu = env_cpu(env);
    int mmu_idx = get_mmuidx(oi);
    unsigned size = memop_size(get_memop(oi));

    stats.chk_st_slow++;
    for (unsigned i = 0; i < size; i++) {
        uint8_t *sp = ram_shadow_at(cpu, addr + i, mmu_idx, MMU_DATA_STORE);

        if (sp && *sp) {
            *sp = 0;        /* a clean byte is left alone: no shadow page for it */
        }
    }
    if (((addr & ~TARGET_PAGE_MASK) + size) <= TARGET_PAGE_SIZE) {
        ctlb_fill(cpu, addr, mmu_idx);
    }
}

/*
 * H2b: shadow helpers generated at build time by the LLVM pass in engine/h2b. A library holds
 * `taint_h2b_table`; each entry takes the helper's arguments, their shadows and the shadow env's
 * offset from env, staged in the shadow env at h2b_off, and returns the shadow of the result.
 * - pure helpers (no pointer arguments): the entry only computes shadows; it runs beside the
 *   helper, and only when an input is tainted;
 * - helpers with pointers into the CPU state: the entry is an instrumented clone that does the
 *   helper's work too (its stores and their shadows), and replaces the helper's call.
 */
#define H2B_MAX_ARGS 8
typedef struct H2bRow {
    const char *name;
    uint64_t (*fn)(const uint64_t *in);
    int32_t nargs;
    uint32_t ptrmask;           /* which arguments are pointers */
} H2bRow;
static GHashTable *h2b_rows;    /* helper name -> const H2bRow * */
static size_t h2b_off;          /* the staged arguments and shadows, in the shadow env */

static void h2b_load(void)
{
    const char *path = getenv("PANDA_TAINT_H2B");
    const H2bRow *t;
    void *lib;

    h2b_rows = g_hash_table_new(g_str_hash, g_str_equal);
    if (!path || !*path) {
        return;
    }
    lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    t = lib ? dlsym(lib, "taint_h2b_table") : NULL;
    if (!t) {
        warn_report("taint: no shadow helpers from %s: %s", path, dlerror());
        return;
    }
    for (; t->name; t++) {
        if (t->nargs <= H2B_MAX_ARGS) {
            g_hash_table_insert(h2b_rows, (gpointer)t->name, (gpointer)t);
        }
    }
}

static uint64_t taint_h2b_call(CPUArchState *env, const H2bRow *row)
{
    uint8_t *senv = env_cpu(env)->taint_senv;
    uint64_t *in = (uint64_t *)(senv + h2b_off);

    stats.h2b_calls++;
    in[2 * row->nargs] = (uintptr_t)senv - (uintptr_t)env;
    return row->fn(in);
}

static TCGHelperInfo h2b_call_info = {
    .func = taint_h2b_call, .name = "taint_h2b_call", .flags = TCG_CALL_NO_RWG,
    .typemask = dh_typemask(i64, 0) | dh_typemask(ptr, 1) | dh_typemask(ptr, 2),
};
/* in place of a helper: globals are synced around it, as for any helper that may touch them */
static TCGHelperInfo h2b_replace_info = {
    .func = taint_h2b_call, .name = "taint_h2b_replace", .flags = 0,  /* result unused */
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1) | dh_typemask(ptr, 2),
};

static void taint_h1_hit(const TCGHelperInfo *info)
{
    const char *name = info->name ? info->name : "(unnamed)";
    gpointer n = g_hash_table_lookup(h1_hits, name);

    stats.h1_tainted++;
    g_hash_table_insert(h1_hits, (gpointer)name, GSIZE_TO_POINTER(GPOINTER_TO_SIZE(n) + 1));
}

/*
 * Helpers with pointers into the CPU state (vector and FP registers): until their write sets are
 * known (Phase 3), every pointed region of @size bytes, and the result, is tainted entirely if any
 * byte of any region or any scalar input (@acc) is. Sound, and coarse: input regions get it too.
 */
static uint64_t taint_ptr_model(CPUArchState *env, void *a, void *b, void *c, void *d,
                                uint32_t size, uint64_t acc)
{
    uint8_t *lo = env_cpu(env)->taint_senv, *hi = lo + env_size;
    uint8_t *ptrs[4] = { a, b, c, d };
    bool any = acc != 0;

    for (int i = 0; i < 4 && !any; i++) {
        if (ptrs[i] >= lo && ptrs[i] + size <= hi) {
            for (uint32_t k = 0; k < size; k++) {
                any |= ptrs[i][k] != 0;
            }
        }
    }
    if (!any) {
        return 0;
    }
    stats.ptr_tainted++;
    for (int i = 0; i < 4; i++) {
        if (ptrs[i] >= lo && ptrs[i] + size <= hi) {
            memset(ptrs[i], 0xff, size);
        }
    }
    return -1;
}

static TCGHelperInfo ptr_model_info = {
    .func = taint_ptr_model, .name = "taint_ptr_model", .flags = TCG_CALL_NO_RWG,
    .typemask = dh_typemask(i64, 0) | dh_typemask(ptr, 1) | dh_typemask(ptr, 2) |
                dh_typemask(ptr, 3) | dh_typemask(ptr, 4) | dh_typemask(ptr, 5) |
                dh_typemask(i32, 6) | dh_typemask(i64, 7),
};

/*
 * Elastic TBs (PLAN §3c, DECAF++): while no CPU state is tainted, blocks run as a check variant
 * that only tests each load's shadow (and clears each store's). A tainted load, before it reads,
 * switches the vCPU to the track variant and restarts the instruction, as a page fault would.
 * Track blocks count down; at zero, an all-clean CPU state switches back to check. Each return
 * to track doubles the countdown (up to ELASTIC_MAX), so code that keeps touching tainted memory
 * settles in track instead of bouncing. Opt-in for now: PANDA_TAINT_ELASTIC=1.
 */
#define ELASTIC_PERIOD 1024
#define ELASTIC_MAX (64 * 1024)
static bool elastic;
static bool addr_taint;         /* address mode: see taint_set_address_mode */

static bool senv_any(CPUState *cpu)
{
    const uint64_t *w = cpu->taint_senv;
    size_t n = env_size / 8;
    uint64_t acc = 0;

    for (size_t i = 0; i < n; i++) {
        acc |= w[i];
    }
    for (size_t i = n * 8; i < env_size; i++) {
        acc |= ((uint8_t *)cpu->taint_senv)[i];
    }
    return acc != 0;
}

static G_NORETURN void taint_to_track(CPUState *cpu, uintptr_t ra)
{
    uint32_t *t = tick(cpu);

    stats.to_track++;
    cpu->tcg_cflags &= ~CF_TAINT_CHECK;
    stlb_flush_all(cpu);        /* check-variant fills did not set the page bit */
    t[1] = MIN(MAX(t[1], ELASTIC_PERIOD / 2) * 2, ELASTIC_MAX);
    t[0] = t[1];
    cpu_loop_exit_restore(cpu, ra);
}

/* A check-variant load found its shadow tainted in the shadow TLB. */
static void taint_chk_hit(CPUArchState *env)
{
    taint_to_track(env_cpu(env), GETPC());
}

/*
 * A check-variant load missed the shadow TLB. It runs before the load, so the TLB is filled here
 * as the load is about to (no fault: a faulting load restarts and comes back); then the shadow is
 * read as taint_ld_slow reads it, the MMIO source included.
 */
static void taint_chk_slow(CPUArchState *env, uint64_t addr, uint32_t oi)
{
    MemOp mop = get_memop(oi);
    int mmu_idx = get_mmuidx(oi);
    uint64_t last = addr + memop_size(mop) - 1;
    void *host;

    stats.chk_slow++;
    if (probe_access_flags(env, addr, 0, MMU_DATA_LOAD, mmu_idx, true, &host, GETPC()) &
        TLB_INVALID_MASK) {
        return;
    }
    if ((last & TARGET_PAGE_MASK) != (addr & TARGET_PAGE_MASK) &&
        (probe_access_flags(env, last, 0, MMU_DATA_LOAD, mmu_idx, true, &host, GETPC()) &
         TLB_INVALID_MASK)) {
        return;
    }
    if (taint_ld_slow(env, addr, oi)) {
        taint_to_track(env_cpu(env), GETPC());
    }
}

/* A track block's countdown reached zero. */
static void taint_track_tick(CPUArchState *env)
{
    CPUState *cpu = env_cpu(env);

    tick(cpu)[0] = MAX(tick(cpu)[1], ELASTIC_PERIOD);
    if (!senv_any(cpu)) {
        stats.to_check++;
        cpu->tcg_cflags |= CF_TAINT_CHECK;  /* from the next block looked up */
    }
}

/* The API tainted CPU state: leave the check variant at the next block. */
static void elastic_tainted_cpu(CPUState *cpu)
{
    if (cpu->tcg_cflags & CF_TAINT_CHECK) {
        cpu->tcg_cflags &= ~CF_TAINT_CHECK;
        stlb_flush_all(cpu);
        tick(cpu)[0] = MAX(tick(cpu)[1], ELASTIC_PERIOD);
        cpu_exit(cpu);
    }
}

/* restoring needs the guest globals in env: NO_WG (they are read, not written) */
static TCGHelperInfo chk_hit_info = {
    .func = taint_chk_hit, .name = "taint_chk_hit", .flags = TCG_CALL_NO_WG,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1),
};
static TCGHelperInfo chk_slow_info = {
    .func = taint_chk_slow, .name = "taint_chk_slow", .flags = TCG_CALL_NO_WG,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1) | dh_typemask(i64, 2) |
                dh_typemask(i32, 3),
};
static TCGHelperInfo track_tick_info = {
    .func = taint_track_tick, .name = "taint_track_tick", .flags = TCG_CALL_NO_RWG,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1),
};

static TCGHelperInfo ld_slow_info = {
    .func = taint_ld_slow, .name = "taint_ld_slow", .flags = TCG_CALL_NO_RWG,
    .typemask = dh_typemask(i64, 0) | dh_typemask(ptr, 1) | dh_typemask(i64, 2) |
                dh_typemask(i32, 3),
};
static TCGHelperInfo chk_st_slow_info = {
    .func = taint_chk_st_slow, .name = "taint_chk_st_slow", .flags = TCG_CALL_NO_RWG,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1) | dh_typemask(i64, 2) |
                dh_typemask(i32, 3),
};

static TCGHelperInfo st_slow_info = {
    .func = taint_st_slow, .name = "taint_st_slow", .flags = TCG_CALL_NO_RWG,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1) | dh_typemask(i64, 2) |
                dh_typemask(i32, 3) | dh_typemask(i64, 4),
};
static TCGHelperInfo h1_hit_info = {
    .func = taint_h1_hit, .name = "taint_h1_hit", .flags = TCG_CALL_NO_RWG,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1),
};

/* The guest address of an instruction from its insn_start word (page offset for PC-relative code). */
static uint64_t insn_pc(CPUState *cpu, uint64_t pc, bool pcrel)
{
    if (pcrel) {
        return (cpu->cc->get_pc(cpu) & TARGET_PAGE_MASK) | (pc & ~TARGET_PAGE_MASK);
    }
    return pc;
}

static void taint_branch_ev(CPUArchState *env, uint64_t pc, uint32_t pcrel, uint64_t mask)
{
    stats.branch_events++;
    if (branch_fn) {
        branch_fn(branch_opaque, env_cpu(env), insn_pc(env_cpu(env), pc, pcrel), mask);
    }
}

/*
 * Before an indirect jump's TB lookup: the PC already holds the target. A tainted target is
 * reported, then its shadow is cleared, since taint is data flow only: PC-relative updates after
 * the jump would otherwise keep reporting it.
 */
static void taint_pc_ev(CPUArchState *env)
{
    CPUState *cpu = env_cpu(env);
    uint8_t *sp = (uint8_t *)cpu->taint_senv + pc_off;
    uint64_t m = pc_size == 8 ? ldq_he_p(sp) : ldl_he_p(sp);

    if (m) {
        stats.pc_events++;
        if (pc_fn) {
            pc_fn(pc_opaque, cpu, cpu->cc->get_pc(cpu), m);
        }
        memset(sp, 0, pc_size);
    }
}

/* Reads globals (the callback may look at registers); writes none. */
static TCGHelperInfo branch_ev_info = {
    .func = taint_branch_ev, .name = "taint_branch_ev", .flags = TCG_CALL_NO_WG,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1) | dh_typemask(i64, 2) |
                dh_typemask(i32, 3) | dh_typemask(i64, 4),
};
/* Writes the PC's shadow global. */
static TCGHelperInfo pc_ev_info = {
    .func = taint_pc_ev, .name = "taint_pc_ev", .flags = 0,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1),
};

/* ---------------------------------------------------------------- tier B: labels */

/*
 * Opt-in (taint_enable_labels): besides its tier-A bit mask, every tainted byte carries a label
 * set, an interned u32 id (0 = none, LSET_MANY = more than LSET_MAX labels). Ids live in a
 * per-vCPU label file (one per CPU-state byte, then LSLOT per TCG temp of the current block) and
 * one per RAM byte. An id is meaningful only where the byte's tier-A mask is nonzero, so nothing
 * is ever cleared: the pass emits, after each op's shadow computation, a branch on the output
 * mask and, only when it is nonzero, a call that computes the output's ids from the inputs'.
 * Input masks are passed through a staging area in the shadow buffer.
 */
#define LSLOT 32                    /* ids per temp: the widest vector */
#define LSET_MAX 256
#define LSET_MANY 1u
#define NOSLOT UINT32_MAX
/* The staging area is 64-byte aligned; vector masks need their own size's alignment. */
#define STAGE_IN 16                 /* staged input masks, 32 bytes each */
#define STAGE_ADDR (STAGE_IN * 32)
#define STAGE_PTR (STAGE_ADDR + 32) /* four pointers */
#define STAGE_DST (STAGE_PTR + 32)  /* a vector output mask, for the gate (32-byte aligned) */
#define STAGE_BYTES (STAGE_DST + 32)
QEMU_BUILD_BUG_ON(STAGE_DST % 32 || STAGE_BYTES % 32);
#define LAB_BATCH 128               /* staging entries for batched instructions (after entry 0) */
#define LAB_BATCH_OPS 48            /* instructions with more ops than this are batched */

static bool labels_on;
static size_t stage_off, lab_off, lab_carry;    /* lab_carry: the carry flag's slot */
static uint32_t *ram_lab;
static uint64_t ram_lab_size;       /* entries */

typedef struct LSet {
    uint32_t n, hash;
    uint32_t v[];
} LSet;
static GPtrArray *lsets;            /* id -> LSet *; 0 and 1 are placeholders */
static GHashTable *lset_ids;        /* LSet * -> id */
static GHashTable *lunions;         /* (a << 32 | b), a < b -> id + 1 */

static guint lset_hash(gconstpointer k)
{
    return ((const LSet *)k)->hash;
}

static gboolean lset_equal(gconstpointer a, gconstpointer b)
{
    const LSet *x = a, *y = b;

    return x->n == y->n && !memcmp(x->v, y->v, x->n * sizeof(uint32_t));
}

static uint32_t lset_intern(const uint32_t *v, uint32_t n)
{
    gpointer id;
    LSet *k;

    if (!n) {
        return 0;
    }
    if (n > LSET_MAX) {
        stats.lab_many++;
        return LSET_MANY;
    }
    k = g_malloc(sizeof(LSet) + n * sizeof(uint32_t));
    k->n = n;
    k->hash = 2166136261u;
    for (uint32_t i = 0; i < n; i++) {
        k->v[i] = v[i];
        k->hash = (k->hash ^ v[i]) * 16777619u;
    }
    if (g_hash_table_lookup_extended(lset_ids, k, NULL, &id)) {
        g_free(k);
        return GPOINTER_TO_UINT(id);
    }
    g_ptr_array_add(lsets, k);
    g_hash_table_insert(lset_ids, k, GUINT_TO_POINTER(lsets->len - 1));
    return lsets->len - 1;
}

static uint32_t lunion(uint32_t a, uint32_t b)
{
    uint64_t key;
    gpointer r;

    if (a == b || !b) {
        return a;
    }
    if (!a) {
        return b;
    }
    if (a == LSET_MANY || b == LSET_MANY) {
        return LSET_MANY;
    }
    if (a > b) {
        uint32_t t = a;
        a = b;
        b = t;
    }
    key = (uint64_t)a << 32 | b;
    r = g_hash_table_lookup(lunions, GSIZE_TO_POINTER(key));
    if (r) {
        return GPOINTER_TO_UINT(r) - 1;
    }
    const LSet *x = g_ptr_array_index(lsets, a), *y = g_ptr_array_index(lsets, b);
    g_autofree uint32_t *v = g_new(uint32_t, x->n + y->n);
    uint32_t i = 0, j = 0, n = 0, id;

    while (i < x->n || j < y->n) {
        if (j == y->n || (i < x->n && x->v[i] < y->v[j])) {
            v[n++] = x->v[i++];
        } else if (i == x->n || y->v[j] < x->v[i]) {
            v[n++] = y->v[j++];
        } else {
            v[n++] = x->v[i++];
            j++;
        }
    }
    id = lset_intern(v, n);
    g_hash_table_insert(lunions, GSIZE_TO_POINTER(key), GUINT_TO_POINTER(id + 1));
    return id;
}

static uint32_t *labfile(CPUState *cpu)
{
    return (uint32_t *)((uint8_t *)cpu->taint_senv + lab_off);
}

static uint8_t *stage(CPUState *cpu)
{
    return (uint8_t *)cpu->taint_senv + stage_off;
}

static void ram_lab_alloc(void)
{
    if (ram_lab && ram_lab_size >= ram_shadow_size) {
        return;
    }
    if (ram_lab) {
        munmap(ram_lab, ram_lab_size * sizeof(uint32_t));
    }
    ram_lab = mmap(NULL, ram_shadow_size * sizeof(uint32_t), PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (ram_lab == MAP_FAILED) {
        error_report("taint: cannot map the RAM label table");
        abort();
    }
    ram_lab_size = ram_shadow_size;
}

/* The id of a RAM byte's labels, or 0 when the byte is clean. */
static uint32_t ram_lab_at(uint64_t ra)
{
    return ra < ram_lab_size && ram_shadow[ra] ? ram_lab[ra] : 0;
}

/* LM_SHIFTV: a shift by a run-time amount (staged at STAGE_ADDR); map holds kind | width << 8 */
enum { LM_BYTE, LM_PREFIX, LM_ALL, LM_MAP, LM_SHIFTV };
enum { SH_SHL, SH_SHR, SH_SAR, SH_ROTL, SH_ROTR };

/* The input bit that output bit @i of a @w-bit shift of kind @kind by @c comes from, or -1. */
static int shift_src(int kind, int w, int c, int i)
{
    switch (kind) {
    case SH_SHL: return i >= c ? i - c : -1;
    case SH_SHR: return i + c < w ? i + c : -1;
    case SH_SAR: return MIN(i + c, w - 1);
    case SH_ROTL: return (i - c + w) % w;
    default: return (i + c) % w;
    }
}

static uint64_t shift_map(int kind, int w, int c)
{
    uint64_t map = 0;

    for (int i = 0; i < w; i++) {
        int src = shift_src(kind, w, c, i);

        if (src >= 0) {
            map |= 1ull << (8 * (i / 8) + src / 8);
        }
    }
    return map;
}
enum { LOP_PLAIN, LOP_CALL, LOP_LD, LOP_ST, LOP_ENVLD, LOP_ENVST };

typedef struct LabIn {
    uint32_t slot;
    uint8_t size, mode;
    uint64_t map;           /* LM_MAP: bit 8*i + j means output byte i takes input byte j */
} LabIn;

typedef struct LabDesc {
    uint8_t op, nd, nin, dsize, carry_out, dyn;
    uint32_t off;           /* LOP_LD/ST: the MemOpIdx; ENV: the offset; CALL: region size */
    uint32_t dslot[8];
    uint8_t dsz[8];         /* LOP_CALL: bytes of each output */
    LabIn in[STAGE_IN];
} LabDesc;

static GHashTable *lab_descs;

static guint desc_hash(gconstpointer k)
{
    const uint8_t *b = k;
    guint h = 2166136261u;

    for (size_t i = 0; i < sizeof(LabDesc); i++) {
        h = (h ^ b[i]) * 16777619u;
    }
    return h;
}

static gboolean desc_equal(gconstpointer a, gconstpointer b)
{
    return !memcmp(a, b, sizeof(LabDesc));
}

/* Descriptors are shared by every block that needs the same one, and never freed. */
static const LabDesc *desc_intern(const LabDesc *d)
{
    LabDesc *k = g_hash_table_lookup(lab_descs, d);

    if (!k) {
        k = g_memdup2(d, sizeof(LabDesc));
        g_hash_table_add(lab_descs, k);
    }
    return k;
}

static bool staged(const uint8_t *st, int k, int j)
{
    return st[k * 32 + j] != 0;
}

/* RAM label ids of a guest access, through the TLB (only reached when the access is tainted). */
static uint32_t *ram_lab_ptr(CPUState *cpu, vaddr addr, int mmu_idx, MMUAccessType type)
{
    uint64_t ra;

    if (!ram_lab || !tlb_peek_ram_addr(cpu, addr, mmu_idx, type, &ra) || ra >= ram_lab_size) {
        return NULL;
    }
    return ram_lab + ra;
}

/* Run one label step with its inputs staged at @st. */
static void lab_run(CPUState *cpu, const LabDesc *d, const uint8_t *st)
{
    uint32_t *lf = labfile(cpu);
    uint32_t out[LSLOT], all = 0;

    stats.lab_calls++;
    switch (d->op) {
    case LOP_LD: {
        uint64_t addr = ldq_he_p(st + STAGE_ADDR);
        MemOp mop = get_memop(d->off);
        unsigned size = memop_size(mop);

        for (unsigned i = 0; i < size; i++) {
            unsigned m = guest_big_endian(mop) ? size - 1 - i : i;
            uint32_t *rl, lab;

            /* the loaded shadow byte says whether the RAM byte's id is valid */
            out[i] = 0;
            if (st[STAGE_DST + i]) {
                rl = ram_lab_ptr(cpu, addr + m, get_mmuidx(d->off), MMU_DATA_LOAD);
                if (rl) {
                    out[i] = *rl;
                } else if ((lab = mmio_label_at(cpu, addr + m))) {
                    out[i] = lset_intern(&lab, 1);     /* a device register (MMIO source) */
                }
            }
        }
        for (unsigned i = 0; i < d->dsize; i++) {
            lf[d->dslot[0] + i] = out[MIN(i, size - 1)];
        }
        return;
    }
    case LOP_ST: {
        uint64_t addr = ldq_he_p(st + STAGE_ADDR);
        MemOp mop = get_memop(d->off);
        unsigned size = memop_size(mop);

        for (unsigned i = 0; i < size; i++) {
            unsigned m = guest_big_endian(mop) ? size - 1 - i : i;
            uint32_t *rl;

            if (staged(st, 0, i) &&
                (rl = ram_lab_ptr(cpu, addr + m, get_mmuidx(d->off), MMU_DATA_STORE))) {
                *rl = lf[d->in[0].slot + i];
            }
        }
        return;
    }
    case LOP_ENVLD:
    case LOP_ENVST: {
        uintptr_t off = d->off;
        unsigned msize = d->in[0].size;

        if (d->dyn & 1) {
            off += (uintptr_t)ldq_he_p(st + STAGE_PTR) - (uintptr_t)cpu->taint_senv;
        }
        if (off + msize > env_size) {
            return;
        }
        if (d->op == LOP_ENVLD) {
            for (unsigned i = 0; i < d->dsize; i++) {
                out[i] = lf[off + (i < msize ? i : (d->dyn & 2 ? i % msize : msize - 1))];
            }
            for (unsigned i = 0; i < d->dsize; i++) {
                lf[d->dslot[0] + i] = out[i];
            }
        } else {
            for (unsigned i = 0; i < msize; i++) {
                if (staged(st, 0, i)) {
                    lf[off + i] = lf[d->in[0].slot + i];
                }
            }
        }
        return;
    }
    default:
        break;
    }

    for (int k = 0; k < d->nin; k++) {
        if (d->in[k].slot != NOSLOT && (d->in[k].mode == LM_ALL || d->op == LOP_CALL)) {
            for (int j = 0; j < d->in[k].size; j++) {
                if (staged(st, k, j)) {
                    all = lunion(all, lf[d->in[k].slot + j]);
                }
            }
        }
    }
    if (d->op == LOP_CALL) {
        /* pointer arguments into the CPU state: their regions join the union and receive it */
        uint8_t *lo = cpu->taint_senv, *hi = lo + env_size;
        bool any = all != 0;

        for (int r = 0; r < 4; r++) {
            uint8_t *ptr = (uint8_t *)(uintptr_t)ldq_he_p(st + STAGE_PTR + 8 * r);

            if (ptr >= lo && ptr + d->off <= hi) {
                for (uint32_t j = 0; j < d->off; j++) {
                    if (ptr[j]) {
                        any = true;
                        all = lunion(all, lf[ptr - lo + j]);
                    }
                }
            }
        }
        if (!any) {
            return;
        }
        for (int r = 0; r < 4; r++) {
            uint8_t *ptr = (uint8_t *)(uintptr_t)ldq_he_p(st + STAGE_PTR + 8 * r);

            if (ptr >= lo && ptr + d->off <= hi) {
                for (uint32_t j = 0; j < d->off; j++) {
                    lf[ptr - lo + j] = all;
                }
            }
        }
    }
    uint64_t emap[STAGE_IN];

    for (int k = 0; d->op == LOP_PLAIN && k < d->nin; k++) {
        const LabIn *in = &d->in[k];
        int w = in->map >> 8;

        emap[k] = in->mode != LM_SHIFTV ? in->map :
                  shift_map(in->map & 0xff, w, ldq_he_p(st + STAGE_ADDR) & (w - 1));
    }
    for (int i = 0; i < d->dsize; i++) {
        uint32_t u = all;

        for (int k = 0; d->op == LOP_PLAIN && k < d->nin; k++) {
            const LabIn *in = &d->in[k];

            if (in->slot == NOSLOT) {
                continue;
            }
            switch (in->mode) {
            case LM_BYTE:
                if (i < in->size && staged(st, k, i)) {
                    u = lunion(u, lf[in->slot + i]);
                }
                break;
            case LM_PREFIX:
                for (int j = 0; j <= i && j < in->size; j++) {
                    if (staged(st, k, j)) {
                        u = lunion(u, lf[in->slot + j]);
                    }
                }
                break;
            case LM_MAP:
            case LM_SHIFTV:
                for (int j = 0; j < 8 && j < in->size; j++) {
                    if (((emap[k] >> (8 * i + j)) & 1) && staged(st, k, j)) {
                        u = lunion(u, lf[in->slot + j]);
                    }
                }
                break;
            }
        }
        out[i] = u;
    }
    for (int k = 0; k < d->nd; k++) {
        if (d->dslot[k] != NOSLOT) {
            int n = d->op == LOP_CALL ? d->dsz[k] : d->dsize;

            for (int i = 0; i < n; i++) {
                lf[d->dslot[k] + i] = out[MIN(i, d->dsize - 1)];
            }
        }
    }
    if (d->carry_out) {
        lf[lab_carry] = out[d->dsize - 1];
    }
}

static void taint_lab(CPUArchState *env, const void *d)
{
    lab_run(env_cpu(env), d, stage(env_cpu(env)));
}

/* A batched instruction's steps: each with its staging entry and output mask size (0: always). */
typedef struct LabStep {
    const LabDesc *d;
    uint16_t idx;
    uint8_t msize;
} LabStep;

typedef struct LabProg {
    uint32_t n;
    LabStep s[];
} LabProg;

static void taint_lab_prog(CPUArchState *env, const void *pp)
{
    const LabProg *prog = pp;
    CPUState *cpu = env_cpu(env);

    for (uint32_t i = 0; i < prog->n; i++) {
        const LabStep *s = &prog->s[i];
        const uint8_t *st = stage(cpu) + s->idx * STAGE_BYTES;
        bool any = !s->msize;

        for (int j = 0; j < s->msize && !any; j++) {
            any = st[STAGE_DST + j] != 0;
        }
        if (any) {
            lab_run(cpu, s->d, st);
        }
    }
}

static GHashTable *lab_progs;

static const LabProg *prog_intern(const LabStep *steps, int n)
{
    size_t sz = sizeof(LabProg) + n * sizeof(LabStep);
    g_autofree LabProg *k = g_malloc0(sz);
    GBytes *key;
    LabProg *v;

    k->n = n;
    for (int i = 0; i < n; i++) {
        k->s[i] = steps[i];
    }
    key = g_bytes_new(k, sz);
    v = g_hash_table_lookup(lab_progs, key);
    if (v) {
        g_bytes_unref(key);
        return v;
    }
    v = (LabProg *)g_bytes_get_data(key, NULL);
    g_hash_table_insert(lab_progs, key, v);
    return v;
}

static TCGHelperInfo lab_info = {
    .func = taint_lab, .name = "taint_lab", .flags = TCG_CALL_NO_RWG,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1) | dh_typemask(ptr, 2),
};
static TCGHelperInfo lab_prog_info = {
    .func = taint_lab_prog, .name = "taint_lab_prog", .flags = TCG_CALL_NO_RWG,
    .typemask = dh_typemask(void, 0) | dh_typemask(ptr, 1) | dh_typemask(ptr, 2),
};

/* ---------------------------------------------------------------- init */

/*
 * The PC's field in the CPU state: set it to a value that differs from the current one in every
 * byte and see which bytes change (a 32-bit PC truncates), then restore the whole state.
 */
static void find_pc(CPUState *cpu)
{
    uint8_t *env = (uint8_t *)cpu_env(cpu);
    g_autofree uint8_t *save = g_memdup2(env, env_size);
    size_t lo = SIZE_MAX, hi = 0;

    cpu->cc->set_pc(cpu, cpu->cc->get_pc(cpu) ^ 0x5a5a5a5a5a5a5a50ull);
    for (size_t i = 0; i < env_size; i++) {
        if (env[i] != save[i]) {
            lo = MIN(lo, i);
            hi = i;
        }
    }
    memcpy(env, save, env_size);
    if (lo == SIZE_MAX || hi - lo >= 8) {
        warn_report("taint: cannot locate the PC in the CPU state; no indirect-jump events");
        return;
    }
    pc_size = hi - lo < 4 ? 4 : 8;
    pc_off = lo & ~(ptrdiff_t)(pc_size - 1);
}

void taint_tcg_init(CPUState *cpu)
{
    TCGContext *s = tcg_ctx;
    TCGTemp *env_ts = tcgv_ptr_temp(tcg_env);
    TCGv_ptr senv;

    if (!analysis_policy_has(ANALYSIS_CAP_TAINT)) {
        return;
    }
    env_size = object_class_get_instance_size(OBJECT_CLASS(cpu->cc)) -
               ((uintptr_t)cpu_env(cpu) - (uintptr_t)cpu);
    stlb_off = ROUND_UP(env_size, 64);
    ctlb_off = stlb_off + sizeof(STLBEntry) * STLB_N * NB_MMU_MODES;
    stage_off = ROUND_UP(ctlb_off + sizeof(STLBEntry) * STLB_N * NB_MMU_MODES, 64);
    h2b_off = ROUND_UP(stage_off + STAGE_BYTES * (1 + LAB_BATCH), 64);
    tick_off = h2b_off + (H2B_MAX_ARGS * 2 + 1) * sizeof(uint64_t);
    lab_off = ROUND_UP(tick_off + 3 * sizeof(uint32_t), 64);
    lab_carry = env_size + TCG_MAX_TEMPS * LSLOT;
    senv_bytes = lab_off + (lab_carry + 1) * sizeof(uint32_t);
    senv_field_off = (uintptr_t)&cpu->taint_senv - (uintptr_t)cpu_env(cpu);

    nb_guest_globals = s->nb_globals;
    senv = tcg_global_mem_new_ptr(tcg_env, senv_field_off, "taint_senv");
    senv_idx = temp_idx(tcgv_ptr_temp(senv));
    for (int i = 0; i < nb_guest_globals; i++) {
        TCGTemp *ts = &s->temps[i];
        TCGTemp *sh;

        if (ts->kind != TEMP_GLOBAL || ts->mem_base != env_ts || ts->indirect_reg) {
            continue;
        }
        if (ts->mem_offset < 0 || ts->mem_offset >= env_size) {
            continue;   /* CPUState fields (negative offsets) carry no guest data */
        }
        if (ts->base_type == TCG_TYPE_I32) {
            sh = tcgv_i32_temp(tcg_global_mem_new_i32(senv, ts->mem_offset,
                                                      g_strdup_printf("t_%s", ts->name)));
        } else {
            sh = tcgv_i64_temp(tcg_global_mem_new_i64(senv, ts->mem_offset,
                                                      g_strdup_printf("t_%s", ts->name)));
        }
        shadow_global[i] = temp_idx(sh);
    }
    h1_hits = g_hash_table_new(g_str_hash, g_str_equal);
    h2b_load();
    lsets = g_ptr_array_new();
    g_ptr_array_add(lsets, NULL);       /* 0: no labels */
    g_ptr_array_add(lsets, NULL);       /* 1: LSET_MANY */
    lset_ids = g_hash_table_new(lset_hash, lset_equal);
    lunions = g_hash_table_new(NULL, NULL);
    lab_descs = g_hash_table_new(desc_hash, desc_equal);
    lab_progs = g_hash_table_new(g_bytes_hash, g_bytes_equal);
    find_pc(cpu);
}

void taint_vcpu_init(CPUState *cpu)
{
    if (!analysis_policy_has(ANALYSIS_CAP_TAINT) || !senv_bytes) {
        return;
    }
    cpu->taint_senv = qemu_memalign(64, senv_bytes);
    memset(cpu->taint_senv, 0, senv_bytes);
    stlb_flush_all(cpu);
}

/* ---------------------------------------------------------------- the pass */

typedef struct Pass {
    TCGContext *s;
    int nb_temps;           /* temps that existed before the pass */
    TCGTemp **sh;           /* shadow of each non-global temp */
    TCGTemp **sptr;         /* for a ptr temp equal to env + x: senv + x */
    TCGOp *chain;           /* first op of the current carry chain */
    TCGTemp *scarry;        /* 1 if the carry/borrow bit is tainted */
    /*
     * TB temps that cross the pass's own labels but live only within one guest op: one set per
     * TB, reused by every op, since each TB temp costs a spill slot in TCG's small frame.
     */
    TCGv_i64 res, res2, addr, addr2, acc, any, sval, ash, sv1, sv2;
    /* scratch outputs for untracked globals, per op (so repeated D() calls agree) */
    TCGTemp *dkey[4], *dval[4];
    int nd;
    /* copies of aliased input shadows for the label step: TB temps, by type, reused per op */
    TCGTemp *prepool[TCG_TYPE_COUNT][4];
    bool batch;             /* this instruction's label steps are batched */
    int nbatch;
    struct LabStep bent[LAB_BATCH];
    uint64_t pc;            /* insn_start word of the current guest instruction */
    bool pcrel;
    bool check;             /* the check variant of an elastic pair (CF_TAINT_CHECK) */
    bool ticked;            /* a track TB's mode tick is emitted */
} Pass;

static TCGv_i64 pool(TCGv_i64 *slot)
{
    if (!*slot) {
        *slot = temp_tcgv_i64(tcg_temp_new_internal(TCG_TYPE_I64, TEMP_TB));
    }
    return *slot;
}

static TCGTemp *zero(TCGType t)
{
    return tcg_constant_internal(t, 0);
}

static TCGTemp *senv_temp(Pass *p)
{
    return &p->s->temps[senv_idx];
}

static bool is_env(TCGTemp *ts)
{
    return ts == tcgv_ptr_temp(tcg_env);
}

static TCGTemp *tmp(TCGType ty);

/* The shadow of @ts, allocating one for a temp that has none yet. */
static TCGTemp *S(Pass *p, TCGTemp *ts)
{
    size_t i = temp_idx(ts);

    switch (ts->kind) {
    case TEMP_CONST:
    case TEMP_FIXED:
        return zero(ts->type);
    case TEMP_GLOBAL:
        if (i < nb_guest_globals && shadow_global[i]) {
            return &p->s->temps[shadow_global[i]];
        }
        return zero(ts->type);
    default:
        break;
    }
    if (i >= p->nb_temps) {
        return zero(ts->type);
    }
    if (!p->sh[i]) {
        if (ts->base_type == TCG_TYPE_I128) {
            TCGTemp *first = ts - ts->temp_subindex;
            TCGTemp *sh = tcg_temp_new_internal(TCG_TYPE_I128, TEMP_TB);

            p->sh[temp_idx(first)] = sh;
            p->sh[temp_idx(first) + 1] = sh + 1;
        } else {
            p->sh[i] = tcg_temp_new_internal(ts->base_type, TEMP_TB);
        }
    }
    return p->sh[i];
}

/* The shadow to write for output @ts: a scratch temp for an untracked global. */
static TCGTemp *D(Pass *p, TCGTemp *ts)
{
    size_t i = temp_idx(ts);

    if (ts->kind == TEMP_GLOBAL && !(i < nb_guest_globals && shadow_global[i])) {
        for (int k = 0; k < p->nd; k++) {
            if (p->dkey[k] == ts) {
                return p->dval[k];
            }
        }
        TCGTemp *t = tmp(ts->type);

        if (p->nd < ARRAY_SIZE(p->dkey)) {
            p->dkey[p->nd] = ts;
            p->dval[p->nd++] = t;
        }
        return t;
    }
    return S(p, ts);
}

/* Type-generic emitters on temps. Vector ops use element size @vece. */
#define I32(t) temp_tcgv_i32(t)
#define I64(t) temp_tcgv_i64(t)
#define VEC(t) temp_tcgv_vec(t)

#define GEN_BIN(name)                                                         \
static void g_##name(TCGType ty, unsigned vece, TCGTemp *d, TCGTemp *a, TCGTemp *b) \
{                                                                             \
    switch (ty) {                                                             \
    case TCG_TYPE_I32: tcg_gen_##name##_i32(I32(d), I32(a), I32(b)); break;   \
    case TCG_TYPE_I64: tcg_gen_##name##_i64(I64(d), I64(a), I64(b)); break;   \
    default: tcg_gen_##name##_vec(vece, VEC(d), VEC(a), VEC(b)); break;       \
    }                                                                         \
}
GEN_BIN(or)
GEN_BIN(and)
GEN_BIN(andc)
GEN_BIN(xor)

static void g_mov(TCGType ty, TCGTemp *d, TCGTemp *a)
{
    if (d == a) {
        return;
    }
    switch (ty) {
    case TCG_TYPE_I32: tcg_gen_mov_i32(I32(d), I32(a)); break;
    case TCG_TYPE_I64: tcg_gen_mov_i64(I64(d), I64(a)); break;
    default: tcg_gen_mov_vec(VEC(d), VEC(a)); break;
    }
}

static void g_not(TCGType ty, TCGTemp *d, TCGTemp *a)
{
    switch (ty) {
    case TCG_TYPE_I32: tcg_gen_not_i32(I32(d), I32(a)); break;
    case TCG_TYPE_I64: tcg_gen_not_i64(I64(d), I64(a)); break;
    default: tcg_gen_not_vec(0, VEC(d), VEC(a)); break;
    }
}

/*
 * Scratch temps: EBB temps freed after each guest op, so a large instruction does not run out of
 * TCG_MAX_TEMPS. Only values that live past the op or across the pass's own labels are TB temps.
 */
static __thread TCGTemp *scratch[256];
static __thread int nscratch;

static TCGTemp *tmp(TCGType ty)
{
    TCGTemp *t = tcg_temp_new_internal(ty, TEMP_EBB);

    if (nscratch < ARRAY_SIZE(scratch)) {
        scratch[nscratch++] = t;
    }
    return t;
}

static TCGv_i64 s64(void)
{
    return temp_tcgv_i64(tmp(TCG_TYPE_I64));
}

static TCGv_i32 s32(void)
{
    return temp_tcgv_i32(tmp(TCG_TYPE_I32));
}

static TCGv_ptr sptrtmp(void)
{
    return temp_tcgv_ptr(tmp(TCG_TYPE_PTR));
}

static void free_scratch(void)
{
    while (nscratch) {
        tcg_temp_free_internal(scratch[--nscratch]);
    }
}

/* Left smear: every bit at and above the lowest set bit (carry propagation). */
static void g_lsmear(TCGType ty, unsigned vece, TCGTemp *d, TCGTemp *a)
{
    TCGTemp *t = tmp(ty);

    switch (ty) {
    case TCG_TYPE_I32: tcg_gen_neg_i32(I32(t), I32(a)); break;
    case TCG_TYPE_I64: tcg_gen_neg_i64(I64(t), I64(a)); break;
    default: tcg_gen_neg_vec(vece, VEC(t), VEC(a)); break;
    }
    g_or(ty, vece, d, a, t);
}

/* Pessimistic cast: all ones if any bit of @a is set (per element for vectors). */
static void g_pcast(TCGType ty, unsigned vece, TCGTemp *d, TCGTemp *a)
{
    switch (ty) {
    case TCG_TYPE_I32: tcg_gen_negsetcondi_i32(TCG_COND_NE, I32(d), I32(a), 0); break;
    case TCG_TYPE_I64: tcg_gen_negsetcondi_i64(TCG_COND_NE, I64(d), I64(a), 0); break;
    default:
        tcg_gen_cmp_vec(TCG_COND_NE, vece, VEC(d), VEC(a),
                        tcg_constant_vec(ty, vece, 0));
        break;
    }
}

/* OR of the shadows of @n inputs of type @ty, into a new temp. */
static TCGTemp *or_inputs(Pass *p, TCGType ty, unsigned vece, TCGTemp **in, int n)
{
    TCGTemp *acc = NULL;

    for (int i = 0; i < n; i++) {
        TCGTemp *si = S(p, in[i]);

        if (si->kind == TEMP_CONST) {
            continue;
        }
        if (!acc) {
            acc = si;
        } else {
            TCGTemp *t = tmp(ty);

            g_or(ty, vece, t, acc, si);
            acc = t;
        }
    }
    return acc ? acc : zero(ty);
}

/* Emit a copy of @op with its temp args replaced by @args (constants copied as is). */
static void emit_clone(TCGOp *op, TCGArg *args)
{
    TCGOp *n = tcg_emit_op(op->opc, op->nargs);

    n->param1 = op->param1;
    n->param2 = op->param2;
    memcpy(n->args, args, sizeof(TCGArg) * op->nargs);
}

/* Clone @op onto shadows: outputs and inputs become shadows, constant args stay. */
static void clone_on_shadows(Pass *p, TCGOp *op)
{
    const TCGOpDef *def = &tcg_op_defs[op->opc];
    TCGArg args[TAINT_MAX_ARGS];
    int k = 0;

    for (int i = 0; i < def->nb_oargs; i++, k++) {
        args[k] = temp_arg(D(p, arg_temp(op->args[k])));
    }
    for (int i = 0; i < def->nb_iargs; i++, k++) {
        args[k] = temp_arg(S(p, arg_temp(op->args[k])));
    }
    for (; k < op->nargs; k++) {
        args[k] = op->args[k];
    }
    emit_clone(op, args);
}

/* All outputs := pcast(OR of all inputs). The sound default. */
static void rule_pcast_all(Pass *p, TCGOp *op, TCGType ty, unsigned vece)
{
    const TCGOpDef *def = &tcg_op_defs[op->opc];
    TCGTemp *in[TAINT_MAX_ARGS];
    int ni = 0;

    for (int i = 0; i < def->nb_iargs; i++) {
        TCGTemp *ts = arg_temp(op->args[def->nb_oargs + i]);

        if (ts->type == ty) {
            in[ni++] = ts;
        }
    }
    TCGTemp *acc = or_inputs(p, ty, vece, in, ni);
    for (int i = 0; i < def->nb_oargs; i++) {
        TCGTemp *o = arg_temp(op->args[i]);

        if (o->type == ty) {
            g_pcast(ty, vece, D(p, o), acc);
        } else {
            g_mov(o->type, D(p, o), zero(o->type));
        }
    }
    /* inputs of another type: any taint taints the outputs */
    for (int i = 0; i < def->nb_iargs; i++) {
        TCGTemp *ts = arg_temp(op->args[def->nb_oargs + i]);

        if (ts->type != ty && ts->type <= TCG_TYPE_I64 && ty <= TCG_TYPE_I64) {
            TCGTemp *any = tmp(ty);

            if (ts->type == TCG_TYPE_I32) {
                TCGv_i32 b = s32();

                tcg_gen_negsetcondi_i32(TCG_COND_NE, b, I32(S(p, ts)), 0);
                if (ty == TCG_TYPE_I64) {
                    tcg_gen_ext_i32_i64(I64(any), b);
                } else {
                    tcg_gen_mov_i32(I32(any), b);
                }
            } else {
                TCGv_i64 b = s64();

                tcg_gen_negsetcondi_i64(TCG_COND_NE, b, I64(S(p, ts)), 0);
                tcg_gen_extrl_i64_i32(I32(any), b);
            }
            for (int j = 0; j < def->nb_oargs; j++) {
                TCGTemp *o = arg_temp(op->args[j]);

                if (o->type == ty) {
                    g_or(ty, vece, D(p, o), D(p, o), any);
                }
            }
        }
    }
}

/* and: a bit is clean if both are clean, or either is a clean 0. */
static void rule_and(Pass *p, TCGType ty, unsigned vece, TCGTemp *d,
                     TCGTemp *a, TCGTemp *b, bool not_b)
{
    TCGTemp *sa = S(p, a), *sb = S(p, b);
    TCGTemp *bv = b;

    if (not_b) {
        bv = tmp(ty);
        g_not(ty, bv, b);
    }
    TCGTemp *t1 = tmp(ty), *t2 = tmp(ty), *t3 = tmp(ty);
    g_and(ty, vece, t1, sa, sb);
    g_and(ty, vece, t2, sa, bv);
    g_and(ty, vece, t3, a, sb);
    g_or(ty, vece, t1, t1, t2);
    g_or(ty, vece, D(p, d), t1, t3);
}

/* or: a bit is clean if both are clean, or either is a clean 1. */
static void rule_or(Pass *p, TCGType ty, unsigned vece, TCGTemp *d,
                    TCGTemp *a, TCGTemp *b, bool not_b)
{
    TCGTemp *sa = S(p, a), *sb = S(p, b);
    TCGTemp *t1 = tmp(ty), *t2 = tmp(ty), *t3 = tmp(ty);

    g_and(ty, vece, t1, sa, sb);
    if (not_b) {
        g_and(ty, vece, t2, sa, b);       /* ~(~b) */
    } else {
        g_andc(ty, vece, t2, sa, b);
    }
    g_andc(ty, vece, t3, sb, a);
    g_or(ty, vece, t1, t1, t2);
    g_or(ty, vece, D(p, d), t1, t3);
}

/* d = OP(sa, b) | pcast(sb): a shift/rotate by a tainted amount taints everything. */
/*
 * Scalar mul: a change in a's tainted bits moves the product only from bit
 * lowbit(sa) + ctz(any value b can take) up, i.e. from lowbit(sa) << ctz(b | sb); the same for b.
 * left() of the two. Exact for a clean factor of 0 (x * 0 is clean), and never below the
 * add-style rule's lowest bit, so sound where it is.
 */
static void rule_mul64(TCGv_i64 d, TCGv_i64 a, TCGv_i64 sa, TCGv_i64 b, TCGv_i64 sb)
{
    TCGv_i64 r = s64(), t = s64(), z = s64(), l = s64(), zero = tcg_constant_i64(0);
    TCGv_i64 xs[2][3] = { { sa, b, sb }, { sb, a, sa } };

    /* d may be the shadow of an input (mul t0, t0, t1): written only at the end */
    tcg_gen_movi_i64(r, 0);
    for (int i = 0; i < 2; i++) {
        TCGv_i64 s = xs[i][0], v = xs[i][1], sv = xs[i][2];

        tcg_gen_or_i64(z, v, sv);               /* bits the other factor can have set */
        tcg_gen_ctzi_i64(t, z, 0);
        tcg_gen_neg_i64(l, s);
        tcg_gen_and_i64(l, l, s);               /* lowest tainted bit */
        tcg_gen_shl_i64(t, l, t);
        tcg_gen_movcond_i64(TCG_COND_EQ, t, z, zero, zero, t);
        tcg_gen_or_i64(r, r, t);
    }
    tcg_gen_neg_i64(t, r);
    tcg_gen_or_i64(d, r, t);
}

static void rule_mul(Pass *p, TCGType ty, TCGTemp *d, TCGTemp *a, TCGTemp *b)
{
    if (ty == TCG_TYPE_I64) {
        rule_mul64(I64(D(p, d)), I64(a), I64(S(p, a)), I64(b), I64(S(p, b)));
        return;
    }
    TCGv_i64 r = s64(), va = s64(), vb = s64(), wa = s64(), wb = s64();

    tcg_gen_extu_i32_i64(va, I32(a));
    tcg_gen_extu_i32_i64(vb, I32(b));
    tcg_gen_extu_i32_i64(wa, I32(S(p, a)));
    tcg_gen_extu_i32_i64(wb, I32(S(p, b)));
    rule_mul64(r, va, wa, vb, wb);
    tcg_gen_extrl_i64_i32(I32(D(p, d)), r);
}

/*
 * mulu2/muls2: the low half as rule_mul (signed or not, the low half is the same product); the
 * high half all ones when a tainted bit can reach the product at all, i.e. when a factor has a
 * tainted bit and the other can be nonzero. Both computed before either output is written: the
 * outputs may be the inputs.
 */
static void rule_mul2(Pass *p, TCGType ty, TCGTemp *d, TCGTemp *hi, TCGTemp *x, TCGTemp *y)
{
    TCGv_i64 a = s64(), b = s64(), sa = s64(), sb = s64(), lo = s64(), h = s64(), t = s64();

    if (ty == TCG_TYPE_I64) {
        tcg_gen_mov_i64(a, I64(x));
        tcg_gen_mov_i64(b, I64(y));
        tcg_gen_mov_i64(sa, I64(S(p, x)));
        tcg_gen_mov_i64(sb, I64(S(p, y)));
    } else {
        tcg_gen_extu_i32_i64(a, I32(x));
        tcg_gen_extu_i32_i64(b, I32(y));
        tcg_gen_extu_i32_i64(sa, I32(S(p, x)));
        tcg_gen_extu_i32_i64(sb, I32(S(p, y)));
    }
    rule_mul64(lo, a, sa, b, sb);
    tcg_gen_or_i64(t, b, sb);
    tcg_gen_movcond_i64(TCG_COND_NE, h, t, tcg_constant_i64(0), sa, tcg_constant_i64(0));
    tcg_gen_or_i64(t, a, sa);
    tcg_gen_movcond_i64(TCG_COND_NE, t, t, tcg_constant_i64(0), sb, tcg_constant_i64(0));
    tcg_gen_or_i64(h, h, t);
    tcg_gen_negsetcondi_i64(TCG_COND_NE, h, h, 0);
    if (ty == TCG_TYPE_I64) {
        tcg_gen_mov_i64(I64(D(p, hi)), h);
        tcg_gen_mov_i64(I64(D(p, d)), lo);
    } else {
        tcg_gen_extrl_i64_i32(I32(D(p, hi)), h);
        tcg_gen_extrl_i64_i32(I32(D(p, d)), lo);
    }
}

static void rule_shift(Pass *p, TCGOp *op, TCGType ty)
{
    TCGTemp *d = arg_temp(op->args[0]), *a = arg_temp(op->args[1]), *b = arg_temp(op->args[2]);
    TCGTemp *t = tmp(ty), *m = tmp(ty);
    TCGArg args[3] = { temp_arg(t), temp_arg(S(p, a)), temp_arg(b) };

    emit_clone(op, args);
    g_pcast(ty, 0, m, S(p, b));
    g_or(ty, 0, D(p, d), t, m);
}

/* ---- tier B: label propagation code */

/* The label slot of a temp: its CPU-state offset for a global, its own slot for a temp. */
static uint32_t lslot(Pass *p, TCGTemp *ts)
{
    size_t i = temp_idx(ts);

    switch (ts->kind) {
    case TEMP_CONST:
    case TEMP_FIXED:
        return NOSLOT;
    case TEMP_GLOBAL:
        return i < nb_guest_globals && shadow_global[i] ? ts->mem_offset : NOSLOT;
    default:
        return i < p->nb_temps ? env_size + i * LSLOT : NOSLOT;
    }
}

static void stage_put(Pass *p, TCGTemp *v, intptr_t off)
{
    TCGv_ptr b = temp_tcgv_ptr(senv_temp(p));

    off += stage_off;
    switch (v->type) {
    case TCG_TYPE_I32: tcg_gen_st_i32(I32(v), b, off); break;
    case TCG_TYPE_I64: tcg_gen_st_i64(I64(v), b, off); break;
    default: tcg_gen_st_vec(VEC(v), b, off); break;
    }
}

/* A label that the code branches to when shadow @m is zero; NULL if it is a constant zero. */
static TCGLabel *lab_gate(Pass *p, TCGTemp *m)
{
    TCGLabel *skip;

    if (m->kind == TEMP_CONST) {
        return NULL;
    }
    skip = gen_new_label();
    switch (m->type) {
    case TCG_TYPE_I32:
        tcg_gen_brcondi_i32(TCG_COND_EQ, I32(m), 0, skip);
        break;
    case TCG_TYPE_I64:
        tcg_gen_brcondi_i64(TCG_COND_EQ, I64(m), 0, skip);
        break;
    default: {
        TCGv_ptr b = temp_tcgv_ptr(senv_temp(p));
        TCGv_i64 acc = s64(), t = s64();

        stage_put(p, m, STAGE_DST);
        tcg_gen_ld_i64(acc, b, stage_off + STAGE_DST);
        for (int o = 8; o < tcg_type_size(m->type); o += 8) {
            tcg_gen_ld_i64(t, b, stage_off + STAGE_DST + o);
            tcg_gen_or_i64(acc, acc, t);
        }
        tcg_gen_brcondi_i64(TCG_COND_EQ, acc, 0, skip);
        break;
    }
    }
    return skip;
}

/*
 * One label step. Normally: a branch on the output mask @m around a call, with the inputs staged
 * in staging entry 0. In a batched instruction (one with many ops): no branch; the step's inputs
 * and output mask go to their own staging entry, and one call at the end of the instruction (or
 * before any guest branch or label in it) runs the steps whose output mask is nonzero, in order.
 */
typedef struct LabCtx {
    intptr_t base;          /* the staging entry's offset in the staging area */
    TCGLabel *skip;
    int idx;
    uint8_t msize;
} LabCtx;

static void lab_flush(Pass *p);

/* False if there is nothing to do (@m is a constant zero). @m NULL: no gate. */
static bool lab_begin(Pass *p, TCGTemp *m, LabCtx *c)
{
    if (m && m->kind == TEMP_CONST) {
        return false;
    }
    if (!p->batch) {
        c->base = 0;
        c->skip = m ? lab_gate(p, m) : NULL;
        return true;
    }
    if (p->nbatch == LAB_BATCH) {
        lab_flush(p);
    }
    c->idx = 1 + p->nbatch;
    c->base = c->idx * STAGE_BYTES;
    c->skip = NULL;
    c->msize = m ? tcg_type_size(m->type) : 0;
    if (m) {
        stage_put(p, m, c->base + STAGE_DST);
    }
    return true;
}

static void lab_put(Pass *p, LabCtx *c, TCGTemp *v, intptr_t off)
{
    stage_put(p, v, c->base + off);
}

static void lab_end(Pass *p, LabCtx *c, const LabDesc *d)
{
    if (p->batch) {
        p->bent[p->nbatch++] = (LabStep){ desc_intern(d), c->idx, c->msize };
        return;
    }
    tcg_gen_call2(taint_lab, &lab_info, NULL, tcgv_ptr_temp(tcg_env),
                  tcg_constant_internal(TCG_TYPE_PTR, (uintptr_t)desc_intern(d)));
    if (c->skip) {
        gen_set_label(c->skip);
    }
}

/* Run the batched steps so far (at the current emit position). */
static void lab_flush(Pass *p)
{
    if (!p->nbatch) {
        return;
    }
    tcg_gen_call2(taint_lab_prog, &lab_prog_info, NULL, tcgv_ptr_temp(tcg_env),
                  tcg_constant_internal(TCG_TYPE_PTR,
                                        (uintptr_t)prog_intern(p->bent, p->nbatch)));
    p->nbatch = 0;
}

static int shift_kind(TCGOpcode opc)
{
    switch (opc) {
    case INDEX_op_shl: return SH_SHL;
    case INDEX_op_shr: return SH_SHR;
    case INDEX_op_sar: return SH_SAR;
    case INDEX_op_rotl: return SH_ROTL;
    default: return SH_ROTR;
    }
}

/*
 * For ops that only move bits, with constant parameters: which input bytes each output byte's
 * bits come from (bit 8*i + j: output byte i from input byte j). False for anything else.
 */
static bool byte_map(TCGOp *op, TCGType ty, int k, uint64_t *map)
{
    const TCGOpDef *def = &tcg_op_defs[op->opc];
    int w = ty == TCG_TYPE_I32 ? 32 : 64, dw = w;
    TCGTemp *b = def->nb_iargs > 1 ? arg_temp(op->args[def->nb_oargs + 1]) : NULL;
    int src[64];

    if (ty > TCG_TYPE_I64) {
        return false;
    }
    switch (op->opc) {
    case INDEX_op_shl: case INDEX_op_shr: case INDEX_op_sar:
    case INDEX_op_rotl: case INDEX_op_rotr: {
        if (k != 0 || b->kind != TEMP_CONST) {
            return false;
        }
        *map = shift_map(shift_kind(op->opc), w, b->val & (w - 1));
        return true;
    }
    case INDEX_op_extract: case INDEX_op_sextract: {
        int pos = op->args[2], len = op->args[3];

        for (int i = 0; i < w; i++) {
            src[i] = i < len ? pos + i : op->opc == INDEX_op_sextract ? pos + len - 1 : -1;
        }
        break;
    }
    case INDEX_op_deposit: {
        int pos = op->args[3], len = op->args[4];

        for (int i = 0; i < w; i++) {
            bool in = i >= pos && i < pos + len;

            src[i] = in ? (k == 1 ? i - pos : -1) : (k == 0 ? i : -1);
        }
        break;
    }
    case INDEX_op_extract2: {
        int pos = op->args[3];

        for (int i = 0; i < w; i++) {
            int j = pos + i;

            src[i] = j < w ? (k == 0 ? j : -1) : (k == 1 ? j - w : -1);
        }
        break;
    }
    case INDEX_op_bswap16: case INDEX_op_bswap32: case INDEX_op_bswap64: {
        int n = op->opc == INDEX_op_bswap16 ? 2 : op->opc == INDEX_op_bswap32 ? 4 : 8;
        bool os = op->opc != INDEX_op_bswap64 && (op->args[2] & TCG_BSWAP_OS);

        for (int i = 0; i < w; i++) {
            int byte = i / 8;

            src[i] = byte < n ? (n - 1 - byte) * 8 + i % 8 : os ? 7 : -1;
        }
        break;
    }
    case INDEX_op_ext_i32_i64: case INDEX_op_extu_i32_i64:
        dw = 64;
        for (int i = 0; i < 64; i++) {
            src[i] = i < 32 ? i : op->opc == INDEX_op_ext_i32_i64 ? 31 : -1;
        }
        break;
    case INDEX_op_extrl_i64_i32: case INDEX_op_extrh_i64_i32:
        dw = 32;
        for (int i = 0; i < 32; i++) {
            src[i] = op->opc == INDEX_op_extrh_i64_i32 ? i + 32 : i;
        }
        break;
    default:
        return false;
    }
    *map = 0;
    for (int i = 0; i < dw; i++) {
        if (src[i] >= 0) {
            *map |= 1ull << (8 * (i / 8) + src[i] / 8);
        }
    }
    return true;
}

static int lab_mode(TCGOp *op, TCGType ty, int out, int k, uint64_t *map)
{
    switch (op->opc) {
    case INDEX_op_mov: case INDEX_op_mov_vec: case INDEX_op_not: case INDEX_op_not_vec:
    case INDEX_op_xor: case INDEX_op_xor_vec: case INDEX_op_eqv: case INDEX_op_eqv_vec:
    case INDEX_op_and: case INDEX_op_and_vec: case INDEX_op_nand: case INDEX_op_nand_vec:
    case INDEX_op_andc: case INDEX_op_andc_vec: case INDEX_op_or: case INDEX_op_or_vec:
    case INDEX_op_nor: case INDEX_op_nor_vec: case INDEX_op_orc: case INDEX_op_orc_vec:
    case INDEX_op_bitsel_vec:
        return LM_BYTE;
    case INDEX_op_add: case INDEX_op_add_vec: case INDEX_op_sub: case INDEX_op_sub_vec:
    case INDEX_op_mul: case INDEX_op_mul_vec: case INDEX_op_neg: case INDEX_op_neg_vec:
        return LM_PREFIX;
    case INDEX_op_mulu2: case INDEX_op_muls2:
        return out == 0 ? LM_PREFIX : LM_ALL;
    case INDEX_op_movcond: case INDEX_op_cmpsel_vec:
        return k >= 2 ? LM_BYTE : LM_ALL;
    case INDEX_op_shl: case INDEX_op_shr: case INDEX_op_sar:
    case INDEX_op_rotl: case INDEX_op_rotr:
        if (k == 0 && ty <= TCG_TYPE_I64 &&
            arg_temp(op->args[2])->kind != TEMP_CONST) {
            *map = shift_kind(op->opc) | (ty == TCG_TYPE_I32 ? 32 : 64) << 8;
            return LM_SHIFTV;
        }
        return byte_map(op, ty, k, map) ? LM_MAP : LM_ALL;
    default:
        return byte_map(op, ty, k, map) ? LM_MAP : LM_ALL;
    }
}

/*
 * Copies of the shadows of @op's inputs that are also its outputs, taken before the op's shadow
 * code overwrites them: the label step needs the inputs' masks, not the output's.
 */
static void lab_save_aliased(Pass *p, TCGOp *op, int first_in, int nin, TCGTemp **pre)
{
    const TCGOpDef *def = &tcg_op_defs[op->opc];

    for (int k = 0; k < nin; k++) {
        TCGTemp *it = arg_temp(op->args[first_in + k]);

        pre[k] = NULL;
        for (int o = 0; o < def->nb_oargs; o++) {
            if (arg_temp(op->args[o]) == it && lslot(p, it) != NOSLOT && k < 4) {
                TCGTemp *si = S(p, it);
                TCGTemp **slot = &p->prepool[si->type][k];

                /* a TB temp: an op's later outputs are staged after an earlier one's label */
                if (!*slot) {
                    *slot = tcg_temp_new_internal(si->type, TEMP_TB);
                }
                pre[k] = *slot;
                g_mov(si->type, pre[k], si);
                break;
            }
        }
    }
}

/*
 * For and/or-type ops, the bits of input @k that reach the output (tier A's improvement rule:
 * a clean 0 under and, or a clean 1 under or, blocks the other input). NULL for other ops.
 */
static TCGTemp *lab_contrib(Pass *p, TCGOp *op, TCGType ty, TCGTemp **pre, int k)
{
    bool is_and, inv_b;
    unsigned vece = TCGOP_VECE(op);
    TCGTemp *a, *b, *sa, *sb, *bv, *t, *c;

    switch (op->opc) {
    case INDEX_op_movcond: case INDEX_op_cmpsel_vec: {
        /* only the selected value contributes byte-wise (the compared values count as ALL) */
        TCGTemp *c1 = arg_temp(op->args[1]), *c2 = arg_temp(op->args[2]);
        TCGTemp *sv = pre[k] ? pre[k] : S(p, arg_temp(op->args[1 + k]));
        TCGTemp *z = op->opc == INDEX_op_movcond ? zero(ty) : NULL;
        TCGCond cond = op->args[5];

        if (k < 2) {
            return NULL;
        }
        c = tmp(ty);
        if (op->opc == INDEX_op_cmpsel_vec) {
            z = tmp(ty);
            tcg_gen_dupi_vec(vece, VEC(z), 0);
            tcg_gen_cmpsel_vec(cond, vece, VEC(c), VEC(c1), VEC(c2),
                               VEC(k == 2 ? sv : z), VEC(k == 2 ? z : sv));
        } else if (ty == TCG_TYPE_I32) {
            tcg_gen_movcond_i32(cond, I32(c), I32(c1), I32(c2), I32(k == 2 ? sv : z),
                                I32(k == 2 ? z : sv));
        } else {
            tcg_gen_movcond_i64(cond, I64(c), I64(c1), I64(c2), I64(k == 2 ? sv : z),
                                I64(k == 2 ? z : sv));
        }
        return c;
    }
    case INDEX_op_and: case INDEX_op_and_vec: case INDEX_op_nand: case INDEX_op_nand_vec:
        is_and = true, inv_b = false;
        break;
    case INDEX_op_andc: case INDEX_op_andc_vec:
        is_and = true, inv_b = true;
        break;
    case INDEX_op_or: case INDEX_op_or_vec: case INDEX_op_nor: case INDEX_op_nor_vec:
        is_and = false, inv_b = false;
        break;
    case INDEX_op_orc: case INDEX_op_orc_vec:
        is_and = false, inv_b = true;
        break;
    default:
        return NULL;
    }
    a = arg_temp(op->args[1]);
    b = arg_temp(op->args[2]);
    sa = pre[0] ? pre[0] : S(p, a);
    sb = pre[1] ? pre[1] : S(p, b);
    /* the other input's value as the op sees it, and its shadow */
    TCGTemp *ov = k == 0 ? b : a, *os = k == 0 ? sb : sa, *me = k == 0 ? sa : sb;

    bv = ov;
    if ((k == 0 && inv_b) != !is_and) {
        bv = tmp(ty);
        g_not(ty, bv, ov);      /* under or, a clean 0 of the other passes this input */
    }
    t = tmp(ty);
    c = tmp(ty);
    g_or(ty, vece, t, bv, os);
    g_and(ty, vece, c, me, t);
    return c;
}

/* Emit the label computation for each output of @op (after its shadow, before the op). */
static void lab_op(Pass *p, TCGOp *op, TCGType ty, TCGTemp **pre)
{
    const TCGOpDef *def = &tcg_op_defs[op->opc];

    for (int o = 0; o < def->nb_oargs; o++) {
        TCGTemp *dt = arg_temp(op->args[o]);
        TCGTemp *ins[STAGE_IN];
        LabDesc d;
        LabCtx c;

        memset(&d, 0, sizeof(d));
        d.op = LOP_PLAIN;
        d.nd = 1;
        d.dslot[0] = lslot(p, dt);
        d.dsize = tcg_type_size(dt->type);
        if (d.dslot[0] == NOSLOT || d.dsize > LSLOT) {
            continue;
        }
        for (int k = 0; k < def->nb_iargs && d.nin < STAGE_IN; k++) {
            TCGTemp *it = arg_temp(op->args[def->nb_oargs + k]);
            LabIn *in = &d.in[d.nin];

            in->slot = lslot(p, it);
            in->size = MIN(tcg_type_size(it->type), LSLOT);
            in->mode = lab_mode(op, ty, o, k, &in->map);
            ins[d.nin++] = it;
        }
        if (!lab_begin(p, D(p, dt), &c)) {
            continue;
        }
        for (int k = 0; k < d.nin; k++) {
            if (d.in[k].slot != NOSLOT) {
                TCGTemp *m = lab_contrib(p, op, ty, pre, k);

                lab_put(p, &c, m ? m : pre[k] ? pre[k] : S(p, ins[k]), k * 32);
            }
            if (d.in[k].mode == LM_SHIFTV) {
                lab_put(p, &c, arg_temp(op->args[2]), STAGE_ADDR);
            }
        }
        lab_end(p, &c, &d);
    }
}

/* Carry-chain ops: the sum's bytes from the lower input bytes, plus the carry flag's labels. */
static void lab_carry_op(Pass *p, TCGOp *op, bool cin, bool cout, TCGTemp **pre)
{
    TCGTemp *dt = arg_temp(op->args[0]);
    TCGTemp *ins[2] = { arg_temp(op->args[1]), arg_temp(op->args[2]) };
    LabDesc d;
    LabCtx c;

    memset(&d, 0, sizeof(d));
    d.op = LOP_PLAIN;
    d.nd = 1;
    d.dslot[0] = lslot(p, dt);
    d.dsize = tcg_type_size(dt->type);
    d.carry_out = cout;
    if (d.dslot[0] == NOSLOT) {
        return;
    }
    for (int k = 0; k < 2; k++) {
        d.in[k] = (LabIn){ .slot = lslot(p, ins[k]), .size = tcg_type_size(ins[k]->type),
                           .mode = LM_PREFIX };
    }
    d.nin = 2;
    if (cin && p->scarry) {
        d.in[2] = (LabIn){ .slot = lab_carry, .size = 1, .mode = LM_ALL };
        d.nin = 3;
    }
    if (!lab_begin(p, D(p, dt), &c)) {
        return;
    }
    for (int k = 0; k < 2; k++) {
        if (d.in[k].slot != NOSLOT) {
            lab_put(p, &c, pre[k] ? pre[k] : S(p, ins[k]), k * 32);
        }
    }
    if (d.nin == 3) {
        lab_put(p, &c, p->scarry, 2 * 32);
    }
    lab_end(p, &c, &d);
}

/* A guest load: the loaded value's labels from RAM (when its shadow @res is nonzero). */
static void lab_ld(Pass *p, TCGTemp *v, TCGv_i64 res, TCGv_i64 a, MemOpIdx oi)
{
    LabDesc d;
    LabCtx c;

    memset(&d, 0, sizeof(d));
    d.op = LOP_LD;
    d.nd = 1;
    d.dslot[0] = lslot(p, v);
    d.dsize = MIN(tcg_type_size(v->type), 8);
    d.off = oi;
    if (d.dslot[0] == NOSLOT || !lab_begin(p, tcgv_i64_temp(res), &c)) {
        return;
    }
    /* the loaded shadow, byte by byte, says which RAM label ids are valid */
    lab_put(p, &c, tcgv_i64_temp(res), STAGE_DST);
    lab_put(p, &c, tcgv_i64_temp(a), STAGE_ADDR);
    lab_end(p, &c, &d);
}

/* A guest store: RAM labels from the value's, for the bytes whose shadow @sv is nonzero. */
static void lab_st(Pass *p, TCGTemp *v, TCGv_i64 sv, TCGv_i64 a, MemOpIdx oi)
{
    LabDesc d;
    LabCtx c;

    memset(&d, 0, sizeof(d));
    d.op = LOP_ST;
    d.nin = 1;
    d.in[0] = (LabIn){ .slot = lslot(p, v), .size = memop_size(get_memop(oi)) };
    d.off = oi;
    if (d.in[0].slot == NOSLOT || !lab_begin(p, tcgv_i64_temp(sv), &c)) {
        return;
    }
    lab_put(p, &c, tcgv_i64_temp(sv), 0);
    lab_put(p, &c, tcgv_i64_temp(a), STAGE_ADDR);
    lab_end(p, &c, &d);
}

/* ld/st against the CPU state (a constant offset from env, or from a tracked pointer). */
static void lab_env(Pass *p, TCGOp *op, bool load, TCGTemp *v, TCGTemp *sbase)
{
    LabDesc d;
    LabCtx c;
    unsigned msize;
    TCGTemp *m;

    switch (op->opc) {
    case INDEX_op_ld8u: case INDEX_op_ld8s: case INDEX_op_st8: msize = 1; break;
    case INDEX_op_ld16u: case INDEX_op_ld16s: case INDEX_op_st16: msize = 2; break;
    case INDEX_op_ld32u: case INDEX_op_ld32s: case INDEX_op_st32: msize = 4; break;
    case INDEX_op_dupm_vec: msize = 1 << TCGOP_VECE(op); break;
    default: msize = tcg_type_size(v->type); break;
    }
    memset(&d, 0, sizeof(d));
    d.op = load ? LOP_ENVLD : LOP_ENVST;
    d.off = op->args[2];
    d.dyn = (sbase != senv_temp(p)) | (op->opc == INDEX_op_dupm_vec ? 2 : 0);
    d.in[0] = (LabIn){ .slot = load ? NOSLOT : lslot(p, v), .size = MIN(msize, LSLOT) };
    d.nin = 1;
    if (load) {
        d.nd = 1;
        d.dslot[0] = lslot(p, v);
        d.dsize = MIN(tcg_type_size(v->type), LSLOT);
        if (d.dslot[0] == NOSLOT) {
            return;
        }
        m = D(p, v);
    } else {
        if (d.in[0].slot == NOSLOT) {
            return;
        }
        m = S(p, v);
    }
    if (!lab_begin(p, m, &c)) {
        return;
    }
    if (!load) {
        lab_put(p, &c, m, 0);
    }
    if (d.dyn & 1) {
        lab_put(p, &c, sbase, STAGE_PTR);
    }
    lab_end(p, &c, &d);
}

/*
 * A helper call (H1): every output gets the union of all tainted input bytes, and of the regions
 * that pointer arguments into the CPU state reach (which receive it too). @gate is the call's
 * any-taint shadow; NULL for a call with such pointers, whose helper finds that out itself.
 */
static void lab_callop(Pass *p, TCGOp *op, TCGTemp **ins, int nin, TCGTemp **writes, int nw,
                       TCGTemp **sps, int nsp, uint32_t size, TCGv_i64 gate)
{
    int no = TCGOP_CALLO(op);
    LabDesc d;
    LabCtx c;

    memset(&d, 0, sizeof(d));
    d.op = LOP_CALL;
    d.dsize = 1;
    d.off = nsp ? size : 0;
    for (int i = 0; i < no + nw && d.nd < 8; i++) {
        TCGTemp *o = i < no ? arg_temp(op->args[i]) : writes[i - no];

        if (o->type <= TCG_TYPE_I64 && lslot(p, o) != NOSLOT) {
            d.dslot[d.nd] = lslot(p, o);
            d.dsz[d.nd++] = tcg_type_size(o->type);
        }
    }
    for (int k = 0; k < nin && d.nin < STAGE_IN; k++) {
        d.in[d.nin++] = (LabIn){ .slot = lslot(p, ins[k]), .size = tcg_type_size(ins[k]->type),
                                 .mode = LM_ALL };
    }
    if ((!d.nd && !nsp) || !lab_begin(p, gate ? tcgv_i64_temp(gate) : NULL, &c)) {
        return;
    }
    for (int k = 0; k < d.nin; k++) {
        if (d.in[k].slot != NOSLOT) {
            lab_put(p, &c, S(p, ins[k]), k * 32);
        }
    }
    for (int r = 0; nsp && r < 4; r++) {
        lab_put(p, &c, r < nsp ? sps[r] : tcg_constant_internal(TCG_TYPE_I64, 0), STAGE_PTR + 8 * r);
    }
    lab_end(p, &c, &d);
}

/* ---- guest memory */

static TCGv_ptr senv_ptr(Pass *p)
{
    return temp_tcgv_ptr(senv_temp(p));
}

/* Shadow TLB entry pointer for @a; returns the base offset of the mmu_idx's table. */
static TCGv_ptr stlb_entry(Pass *p, TCGv_i64 a, int mmu_idx, intptr_t *off)
{
    TCGv_i64 idx = s64();
    TCGv_ptr ent = sptrtmp();

    tcg_gen_extract_i64(idx, a, TARGET_PAGE_BITS, STLB_BITS);
    tcg_gen_shli_i64(idx, idx, 4);
    tcg_gen_add_ptr(ent, senv_ptr(p), temp_tcgv_ptr(tcgv_i64_temp(idx)));
    *off = stlb_off + (intptr_t)mmu_idx * STLB_N * sizeof(STLBEntry);
    return ent;
}

static void bswap_shadow(TCGv_i64 v, unsigned size)
{
    switch (size) {
    case 2: tcg_gen_bswap16_i64(v, v, TCG_BSWAP_IZ | TCG_BSWAP_OZ); break;
    case 4: tcg_gen_bswap32_i64(v, v, TCG_BSWAP_IZ | TCG_BSWAP_OZ); break;
    case 8: tcg_gen_bswap64_i64(v, v); break;
    }
}

/* The shadow of a guest load of @oi at @a, as an i64 (sign-extended like the load). */
static TCGv_i64 shadow_load(Pass *p, TCGv_i64 a, MemOpIdx oi, TCGv_i64 res)
{
    MemOp mop = get_memop(oi);
    unsigned size = memop_size(mop);
    intptr_t off;
    TCGv_i64 tag = s64(), end = s64();
    TCGv_ptr ent = stlb_entry(p, a, get_mmuidx(oi), &off), hp = sptrtmp();
    TCGLabel *slow = gen_new_label(), *done = gen_new_label();

    tcg_gen_ld_i64(tag, ent, off + offsetof(STLBEntry, tag));
    tcg_gen_addi_i64(end, a, size - 1);
    tcg_gen_andi_i64(end, end, TARGET_PAGE_MASK);
    tcg_gen_brcond_i64(TCG_COND_NE, tag, end, slow);
    tcg_gen_ld_ptr(hp, ent, off + offsetof(STLBEntry, saddend));
    tcg_gen_add_ptr(hp, hp, temp_tcgv_ptr(tcgv_i64_temp(a)));
    switch (size) {
    case 1: tcg_gen_ld8u_i64(res, hp, 0); break;
    case 2: tcg_gen_ld16u_i64(res, hp, 0); break;
    case 4: tcg_gen_ld32u_i64(res, hp, 0); break;
    default: tcg_gen_ld_i64(res, hp, 0); break;
    }
    if (guest_big_endian(mop) != HOST_BIG_ENDIAN) {
        bswap_shadow(res, size);
    }
    if ((mop & MO_SIGN) && size < 8) {
        tcg_gen_sextract_i64(res, res, 0, size * 8);
    }
    tcg_gen_br(done);
    gen_set_label(slow);
    tcg_gen_call3(taint_ld_slow, &ld_slow_info, tcgv_i64_temp(res), tcgv_ptr_temp(tcg_env),
                  tcgv_i64_temp(a), tcg_constant_internal(TCG_TYPE_I32, oi));
    gen_set_label(done);
    return res;
}

static void shadow_store(Pass *p, TCGv_i64 a, MemOpIdx oi, TCGv_i64 sv_in)
{
    /* the value is used after the slow-path label, so it must be a TB temp */
    TCGv_i64 sv = pool(&p->sval);
    MemOp mop = get_memop(oi);
    unsigned size = memop_size(mop);
    intptr_t off;
    TCGv_i64 tag = s64(), end = s64(), v = s64();
    TCGv_ptr ent = stlb_entry(p, a, get_mmuidx(oi), &off), hp = sptrtmp();
    TCGLabel *slow = gen_new_label(), *done = gen_new_label();

    tcg_gen_mov_i64(sv, sv_in);
    tcg_gen_ld_i64(tag, ent, off + offsetof(STLBEntry, tag));
    tcg_gen_addi_i64(end, a, size - 1);
    tcg_gen_andi_i64(end, end, TARGET_PAGE_MASK);
    tcg_gen_brcond_i64(TCG_COND_NE, tag, end, slow);
    tcg_gen_ld_ptr(hp, ent, off + offsetof(STLBEntry, saddend));
    tcg_gen_add_ptr(hp, hp, temp_tcgv_ptr(tcgv_i64_temp(a)));
    tcg_gen_mov_i64(v, sv);
    if (guest_big_endian(mop) != HOST_BIG_ENDIAN) {
        bswap_shadow(v, size);
    }
    switch (size) {
    case 1: tcg_gen_st8_i64(v, hp, 0); break;
    case 2: tcg_gen_st16_i64(v, hp, 0); break;
    case 4: tcg_gen_st32_i64(v, hp, 0); break;
    default: tcg_gen_st_i64(v, hp, 0); break;
    }
    tcg_gen_br(done);
    gen_set_label(slow);
    tcg_gen_call4(taint_st_slow, &st_slow_info, NULL, tcgv_ptr_temp(tcg_env),
                  tcgv_i64_temp(a), tcg_constant_internal(TCG_TYPE_I32, oi), tcgv_i64_temp(sv));
    gen_set_label(done);
}

/* The guest address of a qemu_ld/st as an i64 temp (copied, since the op may overwrite it). */
static TCGv_i64 copy_addr(TCGTemp *addr, TCGv_i64 a)
{

    if (addr->type == TCG_TYPE_I32) {
        tcg_gen_extu_i32_i64(a, I32(addr));
    } else {
        tcg_gen_mov_i64(a, I64(addr));
    }
    return a;
}

static void set_from_i64(Pass *p, TCGTemp *d, TCGv_i64 v)
{
    TCGTemp *sd = D(p, d);

    if (sd->type == TCG_TYPE_I32) {
        tcg_gen_extrl_i64_i32(I32(sd), v);
    } else {
        tcg_gen_mov_i64(I64(sd), v);
    }
}

static TCGv_i64 as_i64(Pass *p, TCGTemp *v)
{
    TCGTemp *sv = S(p, v);

    if (sv->type == TCG_TYPE_I32) {
        TCGv_i64 t = s64();

        tcg_gen_extu_i32_i64(t, I32(sv));
        return t;
    }
    return I64(sv);
}

static MemOpIdx half_oi(MemOpIdx oi)
{
    return make_memop_idx(MO_64 | (get_memop(oi) & MO_BSWAP), get_mmuidx(oi));
}

/* qemu_ld/st: the address copy goes before the op, the shadow access after it. */
static void taint_qemu_ldst(Pass *p, TCGOp *op, TCGOp *next)
{
    TCGContext *s = p->s;
    bool two = op->opc == INDEX_op_qemu_ld2 || op->opc == INDEX_op_qemu_st2;
    TCGTemp *v0 = arg_temp(op->args[0]);
    TCGTemp *v1 = two ? arg_temp(op->args[1]) : NULL;
    TCGTemp *addr = arg_temp(op->args[two ? 2 : 1]);
    MemOpIdx oi = op->args[two ? 3 : 2];
    TCGv_i64 a, as = NULL;

    s->emit_before_op = op;
    a = copy_addr(addr, pool(&p->addr));
    if (addr_taint) {
        /* address mode: all ones if any bit of the address is tainted (copied before the op,
         * which may overwrite the address register) */
        as = pool(&p->ash);
        tcg_gen_negsetcondi_i64(TCG_COND_NE, as, as_i64(p, addr), 0);
    }
    s->emit_before_op = next;

    if (!two) {
        if (op->opc == INDEX_op_qemu_ld) {
            TCGv_i64 r = shadow_load(p, a, oi, pool(&p->res));
            MemOp mop = get_memop(oi);
            unsigned size = memop_size(mop);

            if (as) {
                TCGv_i64 m = s64();

                tcg_gen_andi_i64(m, as, size < 8 && !(mop & MO_SIGN) ? MAKE_64BIT_MASK(0, 8 * size)
                                                                      : -1);
                tcg_gen_or_i64(r, r, m);
            }
            set_from_i64(p, v0, r);
            if (labels_on) {
                lab_ld(p, v0, p->res, a, oi);
            }
        } else {
            TCGv_i64 sv = as_i64(p, v0);

            if (as) {                   /* a TB temp: shadow_store branches */
                TCGv_i64 t = pool(&p->sv1);

                tcg_gen_or_i64(t, sv, as);
                sv = t;
            }
            shadow_store(p, a, oi, sv);
            if (labels_on) {
                lab_st(p, v0, p->sval, a, oi);
            }
        }
        return;
    }
    /* 128 bits: two 64-bit halves; the low half is at the lower address unless big-endian */
    TCGv_i64 a2 = pool(&p->addr2);
    bool be = guest_big_endian(get_memop(oi));
    TCGTemp *lo_ts = v0, *hi_ts = v1;

    tcg_gen_addi_i64(a2, a, 8);
    if (op->opc == INDEX_op_qemu_ld2) {
        TCGv_i64 rl = shadow_load(p, be ? a2 : a, half_oi(oi), pool(&p->res));
        TCGv_i64 rh = shadow_load(p, be ? a : a2, half_oi(oi), pool(&p->res2));

        if (as) {
            tcg_gen_or_i64(rl, rl, as);
            tcg_gen_or_i64(rh, rh, as);
        }
        set_from_i64(p, lo_ts, rl);
        set_from_i64(p, hi_ts, rh);
        if (labels_on) {
            lab_ld(p, lo_ts, p->res, be ? a2 : a, half_oi(oi));
            lab_ld(p, hi_ts, p->res2, be ? a : a2, half_oi(oi));
        }
    } else {
        /* each half's shadow just before its store: scratch temps end at the store's branches */
        TCGv_i64 sl = as_i64(p, lo_ts), shi;

        if (as) {                       /* a TB temp: shadow_store branches */
            tcg_gen_or_i64(pool(&p->sv1), sl, as);
            sl = p->sv1;
        }
        shadow_store(p, be ? a2 : a, half_oi(oi), sl);
        if (labels_on) {
            lab_st(p, lo_ts, p->sval, be ? a2 : a, half_oi(oi));
        }
        shi = as_i64(p, hi_ts);
        if (as) {
            tcg_gen_or_i64(pool(&p->sv2), shi, as);
            shi = p->sv2;
        }
        shadow_store(p, be ? a : a2, half_oi(oi), shi);
        if (labels_on) {
            lab_st(p, hi_ts, p->sval, be ? a : a2, half_oi(oi));
        }
    }
}

/* ld/st against host memory: env-relative ones map to the shadow env. */
static void taint_host_ldst(Pass *p, TCGOp *op, bool load)
{
    TCGTemp *v = arg_temp(op->args[0]), *base = arg_temp(op->args[1]);
    TCGTemp *sbase = NULL;
    TCGArg args[TAINT_MAX_ARGS];

    if (is_env(base)) {
        /* CPUState fields (negative offsets, e.g. the icount decrementer) carry no guest data */
        intptr_t off = op->args[2];

        if (off >= 0 && off < env_size) {
            sbase = senv_temp(p);
        }
    } else if (temp_idx(base) < p->nb_temps && p->sptr[temp_idx(base)]) {
        sbase = p->sptr[temp_idx(base)];
    }
    if (!sbase) {
        if (load) {
            stats.untracked_ld++;
            g_mov(v->type, D(p, v), zero(v->type));
        } else {
            stats.untracked_st++;
        }
        return;
    }
    args[0] = temp_arg(load ? D(p, v) : S(p, v));
    args[1] = temp_arg(sbase);
    for (int k = 2; k < op->nargs; k++) {
        args[k] = op->args[k];
    }
    emit_clone(op, args);
    if (labels_on) {
        lab_env(p, op, load, v, sbase);
    }
}

/* ---- calls */

static const char *const control_prefixes[] = {
    "lookup_tb_ptr", "raise_exception", "exception", "panda_", "plugin_", "debug",
    "wfi", "wfe", "yield", "hlt", "pause", "rechecking_single_step", "check_bxj_trap",
    "exit_atomic", "rr_", "sc_", "tcg_hook", "taint_",
};

static bool is_control(const char *name)
{
    if (!name) {
        return false;
    }
    for (int i = 0; i < ARRAY_SIZE(control_prefixes); i++) {
        if (g_str_has_prefix(name, control_prefixes[i])) {
            return true;
        }
    }
    return false;
}

/* Hand models: helpers whose result has known clean bits. x86's lazy flags: the result of
 * cc_compute_all holds only CF PF AF ZF SF OF, and cc_compute_c only CF. */
static uint64_t result_mask(const char *name)
{
    if (!name) {
        return -1;
    }
    if (!strcmp(name, "cc_compute_all")) {
        return 0x8d5;
    }
    if (!strcmp(name, "cc_compute_c") || !strcmp(name, "cc_compute_nz")) {
        return 1;
    }
    return -1;
}

/* Helpers that read and write guest registers held in TCG globals (by global name). */
static const struct {
    const char *helper;
    const char *globals[4];
} global_sets[] = {
    { "divb_AL", { "rax", "eax" } }, { "idivb_AL", { "rax", "eax" } },
    { "divw_AX", { "rax", "rdx", "eax", "edx" } }, { "idivw_AX", { "rax", "rdx", "eax", "edx" } },
    { "divl_EAX", { "rax", "rdx", "eax", "edx" } }, { "idivl_EAX", { "rax", "rdx", "eax", "edx" } },
    { "divq_EAX", { "rax", "rdx" } }, { "idivq_EAX", { "rax", "rdx" } },
};

static TCGTemp *global_named(Pass *p, const char *name)
{
    for (int i = 0; i < nb_guest_globals; i++) {
        TCGTemp *ts = &p->s->temps[i];

        if (ts->kind == TEMP_GLOBAL && ts->name && !strcmp(ts->name, name)) {
            return ts;
        }
    }
    return NULL;
}

/* Stage a call's arguments and their shadows for an H2b entry. */
static void h2b_stage(Pass *p, TCGOp *op, int no, int ni, uint32_t ptrmask)
{
    for (int i = 0; i < ni; i++) {
        TCGTemp *ts = arg_temp(op->args[no + i]);
        TCGv_i64 v;

        if (ts->type == TCG_TYPE_I32) {
            v = s64();
            tcg_gen_extu_i32_i64(v, temp_tcgv_i32(ts));
        } else {
            v = temp_tcgv_i64(ts);
        }
        tcg_gen_st_i64(v, senv_ptr(p), h2b_off + 8 * i);
        tcg_gen_st_i64(ts->kind == TEMP_CONST || (ptrmask >> i & 1) ? tcg_constant_i64(0)
                                                                    : as_i64(p, ts),
                       senv_ptr(p), h2b_off + 8 * (ni + i));
    }
}

/*
 * Replace a call by its H2b clone when every pointer argument points into the CPU state (the
 * clone maps those to the shadow env). Not with labels on: their step for pointer helpers expects
 * the coarse model.
 */
static bool h2b_replace(Pass *p, TCGOp *op, const H2bRow *row, int no, int ni)
{
    if (no || labels_on || row->nargs != ni) {
        return false;
    }
    for (int i = 0; i < ni; i++) {
        TCGTemp *ts = arg_temp(op->args[no + i]);

        if ((row->ptrmask >> i & 1) &&
            !(is_env(ts) || (temp_idx(ts) < p->nb_temps && p->sptr[temp_idx(ts)]))) {
            return false;
        }
    }
    h2b_stage(p, op, no, ni, row->ptrmask);
    tcg_gen_call2(taint_h2b_call, &h2b_replace_info, NULL, tcgv_ptr_temp(tcg_env),
                  tcg_constant_internal(TCG_TYPE_PTR, (uintptr_t)row));
    tcg_op_remove(p->s, op);
    return true;
}

static void taint_call(Pass *p, TCGOp *op)
{
    const TCGHelperInfo *info = tcg_call_info(op);
    int no = TCGOP_CALLO(op), ni = TCGOP_CALLI(op);
    TCGv_i64 acc = NULL;
    bool control = is_control(info->name);
    TCGTemp *ins[STAGE_IN];
    int nin = 0;

    if (pc_fn && pc_off >= 0 && info->name && !strcmp(info->name, "lookup_tb_ptr")) {
        tcg_gen_call1(taint_pc_ev, &pc_ev_info, NULL, tcgv_ptr_temp(tcg_env));
    }

    const H2bRow *row = control || !info->name ? NULL : g_hash_table_lookup(h2b_rows, info->name);
    if (row && row->ptrmask) {
        if (h2b_replace(p, op, row, no, ni)) {
            return;
        }
        row = NULL;
    }

    if (!control) {
        for (int i = 0; i < ni; i++) {
            TCGTemp *ts = arg_temp(op->args[no + i]);
            TCGv_i64 si;

            if (is_env(ts) || ts->kind == TEMP_CONST) {
                continue;
            }
            if (ts->type == TCG_TYPE_I64 && temp_idx(ts) < p->nb_temps &&
                p->sptr[temp_idx(ts)]) {
                stats.ptr_calls++;      /* points into env: no write-set model yet */
            }
            si = as_i64(p, ts);
            if (nin < STAGE_IN) {
                ins[nin++] = ts;
            }
            if (!acc) {
                acc = pool(&p->acc);
                tcg_gen_mov_i64(acc, si);
            } else {
                tcg_gen_or_i64(acc, acc, si);
            }
        }
    }
    /* declared global read/write set */
    TCGTemp *writes[4];
    int nw = 0;
    for (int k = 0; info->name && k < ARRAY_SIZE(global_sets); k++) {
        if (strcmp(info->name, global_sets[k].helper)) {
            continue;
        }
        for (int j = 0; j < 4 && global_sets[k].globals[j]; j++) {
            TCGTemp *g = global_named(p, global_sets[k].globals[j]);

            if (g) {
                if (!acc) {
                    acc = pool(&p->acc);
                    tcg_gen_movi_i64(acc, 0);
                }
                tcg_gen_or_i64(acc, acc, as_i64(p, g));
                writes[nw++] = g;
                if (nin < STAGE_IN) {
                    ins[nin++] = g;
                }
            }
        }
    }
    /* pointers into the CPU state */
    TCGTemp *sps[4] = { NULL, NULL, NULL, NULL };
    int nsp = 0;
    uint32_t size = 32;
    for (int i = 0; !control && i < ni; i++) {
        TCGTemp *ts = arg_temp(op->args[no + i]);

        if (ts->type == TCG_TYPE_I64 && temp_idx(ts) < p->nb_temps && p->sptr[temp_idx(ts)] &&
            nsp < 4) {
            sps[nsp++] = p->sptr[temp_idx(ts)];
        }
    }
    if (nsp) {
        TCGTemp *last = arg_temp(op->args[no + ni - 1]);

        if (last->kind == TEMP_CONST && last->type == TCG_TYPE_I32 && last->val > 0) {
            size = MIN(MAX(simd_maxsz(last->val), 16), 256);    /* a gvec descriptor */
        }
        TCGv_i64 any = pool(&p->any);
        TCGTemp *null = tcg_constant_internal(TCG_TYPE_PTR, 0);

        if (labels_on) {
            lab_callop(p, op, ins, nin, writes, nw, sps, nsp, size, NULL);
        }
        tcg_gen_call7(taint_ptr_model, &ptr_model_info, tcgv_i64_temp(any),
                      tcgv_ptr_temp(tcg_env), sps[0] ? sps[0] : null, sps[1] ? sps[1] : null,
                      sps[2] ? sps[2] : null, sps[3] ? sps[3] : null,
                      tcg_constant_internal(TCG_TYPE_I32, size),
                      acc ? tcgv_i64_temp(acc) : tcg_constant_internal(TCG_TYPE_I64, 0));
        acc = any;
    }
    if (!acc || nsp || nw || (row && row->nargs != ni)) {
        row = NULL;
    }
    if (row) {
        /* H2b: the shadow helper computes the result's shadow, only when an input is tainted */
        TCGLabel *skip = gen_new_label();

        tcg_gen_brcondi_i64(TCG_COND_EQ, acc, 0, skip);
        h2b_stage(p, op, no, ni, 0);
        tcg_gen_call2(taint_h2b_call, &h2b_call_info, tcgv_i64_temp(acc),
                      tcgv_ptr_temp(tcg_env), tcg_constant_internal(TCG_TYPE_PTR, (uintptr_t)row));
        gen_set_label(skip);
        tcg_gen_andi_i64(acc, acc, result_mask(info->name));
        if (labels_on) {
            lab_callop(p, op, ins, nin, writes, nw, sps, 0, 0, acc);
        }
    } else if (acc) {
        TCGLabel *skip = gen_new_label();

        tcg_gen_brcondi_i64(TCG_COND_EQ, acc, 0, skip);
        tcg_gen_call1(taint_h1_hit, &h1_hit_info, NULL,
                      tcg_constant_internal(TCG_TYPE_PTR, (uintptr_t)info));
        gen_set_label(skip);
        tcg_gen_negsetcondi_i64(TCG_COND_NE, acc, acc, 0);
        tcg_gen_andi_i64(acc, acc, result_mask(info->name));
        if (labels_on && !nsp) {
            lab_callop(p, op, ins, nin, writes, nw, sps, 0, 0, acc);
        }
        for (int k = 0; k < nw; k++) {
            TCGTemp *sw = D(p, writes[k]);

            if (sw->type == TCG_TYPE_I32) {
                tcg_gen_extrl_i64_i32(I32(sw), acc);
            } else {
                tcg_gen_mov_i64(I64(sw), acc);
            }
        }
    }
    for (int i = 0; i < no; i++) {
        TCGTemp *o = arg_temp(op->args[i]);
        TCGTemp *so = D(p, o);

        switch (o->type) {
        case TCG_TYPE_I32:
            if (acc) {
                tcg_gen_extrl_i64_i32(I32(so), acc);
            } else {
                tcg_gen_movi_i32(I32(so), 0);
            }
            break;
        case TCG_TYPE_I64:
            if (acc) {
                tcg_gen_mov_i64(I64(so), acc);
            } else {
                tcg_gen_movi_i64(I64(so), 0);
            }
            break;
        default:
            break;
        }
    }
}

/* ---- the op walk */

static bool is_carry_op(TCGOpcode opc)
{
    switch (opc) {
    case INDEX_op_addco: case INDEX_op_addci: case INDEX_op_addcio: case INDEX_op_addc1o:
    case INDEX_op_subbo: case INDEX_op_subbi: case INDEX_op_subbio: case INDEX_op_subb1o:
        return true;
    default:
        return false;
    }
}

static void taint_carry(Pass *p, TCGOp *op, TCGType ty)
{
    TCGTemp *d = arg_temp(op->args[0]), *a = arg_temp(op->args[1]), *b = arg_temp(op->args[2]);
    bool cin = op->opc == INDEX_op_addci || op->opc == INDEX_op_addcio ||
               op->opc == INDEX_op_subbi || op->opc == INDEX_op_subbio;
    bool cout = op->opc != INDEX_op_addci && op->opc != INDEX_op_subbi;
    TCGTemp *pre[2] = { NULL, NULL };
    TCGTemp *t;

    if (labels_on) {
        lab_save_aliased(p, op, 1, 2, pre);
    }
    t = tmp(ty);

    g_or(ty, 0, t, S(p, a), S(p, b));
    if (cin && p->scarry) {
        TCGTemp *c = tmp(ty);

        if (ty == TCG_TYPE_I32) {
            tcg_gen_extrl_i64_i32(I32(c), I64(p->scarry));
        } else {
            tcg_gen_mov_i64(I64(c), I64(p->scarry));
        }
        g_or(ty, 0, t, t, c);
    }
    if (cout) {
        if (!p->scarry) {
            p->scarry = tcg_temp_new_internal(TCG_TYPE_I64, TEMP_TB);
        }
        if (ty == TCG_TYPE_I32) {
            TCGv_i32 c = s32();

            tcg_gen_setcondi_i32(TCG_COND_NE, c, I32(t), 0);
            tcg_gen_extu_i32_i64(I64(p->scarry), c);
        } else {
            tcg_gen_setcondi_i64(TCG_COND_NE, I64(p->scarry), I64(t), 0);
        }
    }
    g_lsmear(ty, 0, D(p, d), t);
    if (labels_on) {
        lab_carry_op(p, op, cin, cout, pre);
    }
}

static void ptr_track(Pass *p, TCGOp *op, TCGType ty)
{
    const TCGOpDef *def = &tcg_op_defs[op->opc];

    if (def->nb_oargs == 0 || op->opc == INDEX_op_call) {
        return;
    }
    TCGTemp *d = arg_temp(op->args[0]);
    size_t di = temp_idx(d);

    if (di >= p->nb_temps || d->kind == TEMP_GLOBAL) {
        return;
    }
    TCGTemp *base = NULL;
    TCGTemp *a = def->nb_iargs > 0 ? arg_temp(op->args[1]) : NULL;
    TCGTemp *b = def->nb_iargs > 1 ? arg_temp(op->args[2]) : NULL;

    if (ty == TCG_TYPE_PTR && (op->opc == INDEX_op_add || op->opc == INDEX_op_mov)) {
        TCGTemp *other = NULL;

        if (a && is_env(a)) {
            base = senv_temp(p), other = b;
        } else if (b && is_env(b)) {
            base = senv_temp(p), other = a;
        } else if (a && temp_idx(a) < p->nb_temps && p->sptr[temp_idx(a)]) {
            base = p->sptr[temp_idx(a)], other = b;
        }
        if (base) {
            TCGTemp *sp = tcg_temp_new_internal(TCG_TYPE_PTR, TEMP_TB);

            if (other) {
                tcg_gen_add_ptr(temp_tcgv_ptr(sp), temp_tcgv_ptr(base), temp_tcgv_ptr(other));
            } else {
                tcg_gen_mov_ptr(temp_tcgv_ptr(sp), temp_tcgv_ptr(base));
            }
            p->sptr[di] = sp;
            return;
        }
    }
    p->sptr[di] = NULL;
}

/* A conditional branch: report it if either compared value is tainted. */
static void taint_brcond(Pass *p, TCGTemp *a, TCGTemp *b)
{
    TCGv_i64 m = s64();
    TCGLabel *skip = gen_new_label();

    tcg_gen_or_i64(m, as_i64(p, a), as_i64(p, b));
    tcg_gen_brcondi_i64(TCG_COND_EQ, m, 0, skip);
    tcg_gen_call4(taint_branch_ev, &branch_ev_info, NULL, tcgv_ptr_temp(tcg_env),
                  tcg_constant_internal(TCG_TYPE_I64, p->pc),
                  tcg_constant_internal(TCG_TYPE_I32, p->pcrel), tcgv_i64_temp(m));
    gen_set_label(skip);
}

/* Elastic, check variant: test a load's shadow before it reads; leave for track if tainted. */
static void chk_load(Pass *p, TCGv_i64 a, MemOpIdx oi)
{
    unsigned size = memop_size(get_memop(oi));
    intptr_t off;
    TCGv_i64 tag = s64(), end = s64(), v = s64();
    TCGv_ptr ent = stlb_entry(p, a, get_mmuidx(oi), &off), hp = sptrtmp();
    TCGLabel *slow = gen_new_label(), *done = gen_new_label();

    tcg_gen_ld_i64(tag, ent, off + offsetof(STLBEntry, tag));
    tcg_gen_addi_i64(end, a, size - 1);
    tcg_gen_andi_i64(end, end, TARGET_PAGE_MASK);
    tcg_gen_brcond_i64(TCG_COND_NE, tag, end, slow);
    tcg_gen_ld_ptr(hp, ent, off + offsetof(STLBEntry, saddend));
    tcg_gen_add_ptr(hp, hp, temp_tcgv_ptr(tcgv_i64_temp(a)));
    switch (size) {
    case 1: tcg_gen_ld8u_i64(v, hp, 0); break;
    case 2: tcg_gen_ld16u_i64(v, hp, 0); break;
    case 4: tcg_gen_ld32u_i64(v, hp, 0); break;
    default: tcg_gen_ld_i64(v, hp, 0); break;
    }
    tcg_gen_brcondi_i64(TCG_COND_EQ, v, 0, done);
    tcg_gen_call1(taint_chk_hit, &chk_hit_info, NULL, tcgv_ptr_temp(tcg_env));
    tcg_gen_br(done);
    gen_set_label(slow);
    tcg_gen_call3(taint_chk_slow, &chk_slow_info, NULL, tcgv_ptr_temp(tcg_env),
                  tcgv_i64_temp(a), tcg_constant_internal(TCG_TYPE_I32, oi));
    gen_set_label(done);
}

/* Elastic, check variant: a store writes a clean value, so its destination's shadow is zero. */
static void chk_store(Pass *p, TCGv_i64 a, MemOpIdx oi)
{
    unsigned size = memop_size(get_memop(oi));
    intptr_t off;
    TCGv_i64 tag = s64(), end = s64(), zero = tcg_constant_i64(0);
    TCGv_ptr ent = stlb_entry(p, a, get_mmuidx(oi), &off), hp = sptrtmp();
    TCGLabel *slow = gen_new_label(), *done = gen_new_label();

    off += ctlb_off - stlb_off;
    tcg_gen_ld_i64(tag, ent, off + offsetof(STLBEntry, tag));
    tcg_gen_addi_i64(end, a, size - 1);
    tcg_gen_andi_i64(end, end, TARGET_PAGE_MASK);
    tcg_gen_brcond_i64(TCG_COND_NE, tag, end, slow);
    tcg_gen_ld_ptr(hp, ent, off + offsetof(STLBEntry, saddend));
    tcg_gen_add_ptr(hp, hp, temp_tcgv_ptr(tcgv_i64_temp(a)));
    switch (size) {
    case 1: tcg_gen_st8_i64(zero, hp, 0); break;
    case 2: tcg_gen_st16_i64(zero, hp, 0); break;
    case 4: tcg_gen_st32_i64(zero, hp, 0); break;
    default: tcg_gen_st_i64(zero, hp, 0); break;
    }
    tcg_gen_br(done);
    gen_set_label(slow);
    tcg_gen_call3(taint_chk_st_slow, &chk_st_slow_info, NULL, tcgv_ptr_temp(tcg_env),
                  tcgv_i64_temp(a), tcg_constant_internal(TCG_TYPE_I32, oi));
    gen_set_label(done);
}

/* The check variant instruments only guest memory: CPU state is clean while it runs. */
static void check_op(Pass *p, TCGOp *op, TCGOp *next)
{
    TCGContext *s = p->s;
    bool two = op->opc == INDEX_op_qemu_ld2 || op->opc == INDEX_op_qemu_st2;
    bool load = op->opc == INDEX_op_qemu_ld || op->opc == INDEX_op_qemu_ld2;
    TCGv_i64 a, a2 = NULL;
    MemOpIdx oi;

    if (!load && op->opc != INDEX_op_qemu_st && op->opc != INDEX_op_qemu_st2) {
        return;
    }
    stats.ops++;
    oi = op->args[two ? 3 : 2];
    s->emit_before_op = op;
    a = copy_addr(arg_temp(op->args[two ? 2 : 1]), pool(&p->addr));
    if (two) {
        a2 = pool(&p->addr2);
        tcg_gen_addi_i64(a2, a, 8);
        oi = half_oi(oi);
    }
    if (load) {
        chk_load(p, a, oi);
        if (two) {
            chk_load(p, a2, oi);
        }
        return;
    }
    s->emit_before_op = next;       /* a clean value overwrites whatever was there */
    chk_store(p, a, oi);
    if (two) {
        chk_store(p, a2, oi);
    }
}

/* Elastic, track variant: count blocks down to the next all-clean scan. */
static void gen_tick(Pass *p)
{
    TCGv_i32 t = s32();
    TCGLabel *skip = gen_new_label();

    tcg_gen_ld_i32(t, senv_ptr(p), tick_off);
    tcg_gen_subi_i32(t, t, 1);
    tcg_gen_st_i32(t, senv_ptr(p), tick_off);
    tcg_gen_brcondi_i32(TCG_COND_NE, t, 0, skip);
    tcg_gen_call1(taint_track_tick, &track_tick_info, NULL, tcgv_ptr_temp(tcg_env));
    gen_set_label(skip);
}

static void taint_op(Pass *p, TCGOp *op, TCGOp *next)
{
    TCGContext *s = p->s;
    TCGOpcode opc = op->opc;
    TCGType ty = TCGOP_TYPE(op);
    unsigned vece = TCGOP_VECE(op);
    const TCGOpDef *def = &tcg_op_defs[opc];
    TCGTemp *d = def->nb_oargs ? arg_temp(op->args[0]) : NULL;
    TCGTemp *a = def->nb_iargs > 0 ? arg_temp(op->args[def->nb_oargs]) : NULL;
    TCGTemp *b = def->nb_iargs > 1 ? arg_temp(op->args[def->nb_oargs + 1]) : NULL;

    if (p->check) {
        check_op(p, op, next);
        return;
    }
    /* Shadow work goes before the op, so value-dependent rules see the inputs. */
    if (is_carry_op(opc)) {
        if (!p->chain) {
            p->chain = op;
        }
        s->emit_before_op = p->chain;
        taint_carry(p, op, ty);
        return;
    }
    p->chain = NULL;
    s->emit_before_op = op;
    stats.ops++;

    switch (opc) {
    case INDEX_op_set_label:
        lab_flush(p);
        memset(p->sptr, 0, sizeof(TCGTemp *) * p->nb_temps);
        return;
    case INDEX_op_insn_start:
        if (elastic && !p->ticked) {
            p->ticked = true;
            gen_tick(p);
        }
        lab_flush(p);
        p->pc = tcg_get_insn_start_param(op, 0);
        if (labels_on) {
            int n = 0;

            for (TCGOp *o = next; o && o->opc != INDEX_op_insn_start; o = QTAILQ_NEXT(o, link)) {
                n++;
            }
            p->batch = n > LAB_BATCH_OPS;
        }
        return;
    case INDEX_op_brcond:
        lab_flush(p);
        if (branch_fn) {
            taint_brcond(p, a, b);
        }
        return;
    case INDEX_op_br: case INDEX_op_exit_tb: case INDEX_op_goto_tb: case INDEX_op_goto_ptr:
        lab_flush(p);
        return;
    case INDEX_op_discard: case INDEX_op_mb:
    case INDEX_op_plugin_cb: case INDEX_op_plugin_mem_cb:
        return;
    case INDEX_op_call:
        taint_call(p, op);
        return;
    case INDEX_op_qemu_ld: case INDEX_op_qemu_st:
    case INDEX_op_qemu_ld2: case INDEX_op_qemu_st2:
        taint_qemu_ldst(p, op, next);
        return;
    case INDEX_op_ld8u: case INDEX_op_ld8s: case INDEX_op_ld16u: case INDEX_op_ld16s:
    case INDEX_op_ld32u: case INDEX_op_ld32s: case INDEX_op_ld: case INDEX_op_ld_vec:
    case INDEX_op_dupm_vec:
        taint_host_ldst(p, op, true);
        ptr_track(p, op, ty);
        return;
    case INDEX_op_st8: case INDEX_op_st16: case INDEX_op_st32: case INDEX_op_st:
    case INDEX_op_st_vec:
        taint_host_ldst(p, op, false);
        return;
    default:
        break;
    }

    ptr_track(p, op, ty);

    TCGTemp *pre[TAINT_MAX_ARGS];

    if (labels_on) {
        lab_save_aliased(p, op, def->nb_oargs, MIN(def->nb_iargs, STAGE_IN), pre);
    }
    if (a && a == b && (opc == INDEX_op_xor || opc == INDEX_op_sub || opc == INDEX_op_eqv ||
                        opc == INDEX_op_andc || opc == INDEX_op_xor_vec || opc == INDEX_op_sub_vec ||
                        opc == INDEX_op_eqv_vec || opc == INDEX_op_andc_vec)) {
        /* x ^ x, x - x, ~(x ^ x), x & ~x: a constant */
        g_mov(ty, D(p, d), zero(ty));
        return;
    }

    switch (opc) {
    case INDEX_op_mov: case INDEX_op_mov_vec:
        g_mov(ty, D(p, d), S(p, a));
        break;
    case INDEX_op_xor: case INDEX_op_eqv: case INDEX_op_xor_vec: case INDEX_op_eqv_vec:
        g_or(ty, vece, D(p, d), S(p, a), S(p, b));
        break;
    case INDEX_op_not: case INDEX_op_not_vec:
        g_mov(ty, D(p, d), S(p, a));
        break;
    case INDEX_op_and: case INDEX_op_nand: case INDEX_op_and_vec: case INDEX_op_nand_vec:
        rule_and(p, ty, vece, d, a, b, false);
        break;
    case INDEX_op_andc: case INDEX_op_andc_vec:
        rule_and(p, ty, vece, d, a, b, true);
        break;
    case INDEX_op_or: case INDEX_op_nor: case INDEX_op_or_vec: case INDEX_op_nor_vec:
        rule_or(p, ty, vece, d, a, b, false);
        break;
    case INDEX_op_orc: case INDEX_op_orc_vec:
        rule_or(p, ty, vece, d, a, b, true);
        break;
    case INDEX_op_mul:
        if (ty <= TCG_TYPE_I64) {
            rule_mul(p, ty, d, a, b);
            break;
        }
        /* fall through */
    case INDEX_op_add: case INDEX_op_sub:
    case INDEX_op_add_vec: case INDEX_op_sub_vec: case INDEX_op_mul_vec: {
        TCGTemp *t = tmp(ty);

        g_or(ty, vece, t, S(p, a), S(p, b));
        g_lsmear(ty, vece, D(p, d), t);
        break;
    }
    case INDEX_op_neg: case INDEX_op_neg_vec:
        g_lsmear(ty, vece, D(p, d), S(p, a));
        break;
    case INDEX_op_shl: case INDEX_op_shr: case INDEX_op_sar:
    case INDEX_op_rotl: case INDEX_op_rotr:
        rule_shift(p, op, ty);
        break;
    case INDEX_op_shli_vec: case INDEX_op_shri_vec: case INDEX_op_sari_vec:
    case INDEX_op_rotli_vec:
    case INDEX_op_bswap16: case INDEX_op_bswap32: case INDEX_op_bswap64:
    case INDEX_op_extract: case INDEX_op_sextract: case INDEX_op_deposit:
    case INDEX_op_extract2: case INDEX_op_ext_i32_i64: case INDEX_op_extu_i32_i64:
    case INDEX_op_extrl_i64_i32: case INDEX_op_extrh_i64_i32: case INDEX_op_dup_vec:
        clone_on_shadows(p, op);
        break;
    case INDEX_op_shls_vec: case INDEX_op_shrs_vec: case INDEX_op_sars_vec:
    case INDEX_op_rotls_vec: {
        TCGTemp *t = tmp(ty), *m = tmp(ty);
        TCGv_i32 any = s32();
        TCGArg args[3] = { temp_arg(t), temp_arg(S(p, a)), temp_arg(b) };

        emit_clone(op, args);
        tcg_gen_negsetcondi_i32(TCG_COND_NE, any, I32(S(p, b)), 0);
        tcg_gen_dup_i32_vec(MO_32, VEC(m), any);
        g_or(ty, vece, D(p, d), t, m);
        break;
    }
    case INDEX_op_shlv_vec: case INDEX_op_shrv_vec: case INDEX_op_sarv_vec:
    case INDEX_op_rotlv_vec: case INDEX_op_rotrv_vec: {
        TCGTemp *t = tmp(ty), *m = tmp(ty);
        TCGArg args[3] = { temp_arg(t), temp_arg(S(p, a)), temp_arg(b) };

        emit_clone(op, args);
        g_pcast(ty, vece, m, S(p, b));
        g_or(ty, vece, D(p, d), t, m);
        break;
    }
    case INDEX_op_setcond: case INDEX_op_negsetcond: {
        TCGTemp *t = tmp(ty);

        g_or(ty, 0, t, S(p, a), S(p, b));
        g_pcast(ty, 0, D(p, d), t);
        if (opc == INDEX_op_setcond) {
            if (ty == TCG_TYPE_I32) {
                tcg_gen_andi_i32(I32(D(p, d)), I32(D(p, d)), 1);
            } else {
                tcg_gen_andi_i64(I64(D(p, d)), I64(D(p, d)), 1);
            }
        }
        break;
    }
    case INDEX_op_movcond: {
        /* d = cond(c1, c2) ? v1 : v2: the chosen shadow, plus all bits if the condition is tainted */
        TCGTemp *c1 = a, *c2 = b;
        TCGTemp *v1 = arg_temp(op->args[3]), *v2 = arg_temp(op->args[4]);
        TCGCond cond = op->args[5];
        TCGTemp *t = tmp(ty), *m = tmp(ty);

        if (ty == TCG_TYPE_I32) {
            tcg_gen_movcond_i32(cond, I32(t), I32(c1), I32(c2), I32(S(p, v1)), I32(S(p, v2)));
        } else {
            tcg_gen_movcond_i64(cond, I64(t), I64(c1), I64(c2), I64(S(p, v1)), I64(S(p, v2)));
        }
        g_or(ty, 0, m, S(p, c1), S(p, c2));
        g_pcast(ty, 0, m, m);
        g_or(ty, 0, D(p, d), t, m);
        break;
    }
    case INDEX_op_cmp_vec: {
        TCGTemp *t = tmp(ty);

        g_or(ty, vece, t, S(p, a), S(p, b));
        g_pcast(ty, vece, D(p, d), t);
        break;
    }
    case INDEX_op_bitsel_vec: {
        /* d = (a & b) | (~a & c) */
        TCGTemp *c = arg_temp(op->args[3]);
        TCGTemp *t = tmp(ty), *u = tmp(ty);

        tcg_gen_bitsel_vec(vece, VEC(t), VEC(a), VEC(S(p, b)), VEC(S(p, c)));
        g_xor(ty, vece, u, b, c);
        g_or(ty, vece, u, u, S(p, b));
        g_or(ty, vece, u, u, S(p, c));
        g_and(ty, vece, u, u, S(p, a));
        g_or(ty, vece, D(p, d), t, u);
        break;
    }
    case INDEX_op_cmpsel_vec: {
        TCGTemp *v1 = arg_temp(op->args[3]), *v2 = arg_temp(op->args[4]);
        TCGCond cond = op->args[5];
        TCGTemp *t = tmp(ty), *m = tmp(ty);

        tcg_gen_cmpsel_vec(cond, vece, VEC(t), VEC(a), VEC(b), VEC(S(p, v1)), VEC(S(p, v2)));
        g_or(ty, vece, m, S(p, a), S(p, b));
        g_pcast(ty, vece, m, m);
        g_or(ty, vece, D(p, d), t, m);
        break;
    }
    case INDEX_op_mulu2: case INDEX_op_muls2:
        rule_mul2(p, ty, d, arg_temp(op->args[1]), arg_temp(op->args[2]), arg_temp(op->args[3]));
        break;
    default:
        /* div/rem/mulh/clz/ctz/ctpop/abs/saturating/min/max and anything new */
        if (!(def->flags & TCG_OPF_VECTOR) &&
            !(opc == INDEX_op_divs || opc == INDEX_op_divu || opc == INDEX_op_rems ||
              opc == INDEX_op_remu || opc == INDEX_op_divs2 || opc == INDEX_op_divu2 ||
              opc == INDEX_op_mulsh || opc == INDEX_op_muluh || opc == INDEX_op_clz ||
              opc == INDEX_op_ctz || opc == INDEX_op_ctpop)) {
            stats.unknown_ops++;
        }
        rule_pcast_all(p, op, ty, vece);
        break;
    }
    if (labels_on) {
        lab_op(p, op, ty, pre);
    }
}

void taint_gen_tb(CPUState *cpu, TranslationBlock *tb)
{
    TCGContext *s = tcg_ctx;
    Pass p = { .s = s, .nb_temps = s->nb_temps, .pcrel = tb_cflags(tb) & CF_PCREL,
               .check = tb_cflags(tb) & CF_TAINT_CHECK };
    TCGOp *op, *next;
    const TCGOpcode *old_list = tcg_swap_vecop_list(NULL);

    if (!senv_idx) {
        return;
    }
    stats.tbs++;
    p.sh = tcg_malloc(sizeof(TCGTemp *) * p.nb_temps);
    p.sptr = tcg_malloc(sizeof(TCGTemp *) * p.nb_temps);
    memset(p.sh, 0, sizeof(TCGTemp *) * p.nb_temps);
    memset(p.sptr, 0, sizeof(TCGTemp *) * p.nb_temps);

    /*
     * The pass adds branches (shadow TLB miss, helper counters). Guest EBB temps may be live
     * across them, so they become TB temps; free EBB temps are not reused for shadows.
     */
    for (int i = s->nb_globals; i < p.nb_temps; i++) {
        if (s->temps[i].kind == TEMP_EBB) {
            s->temps[i].kind = TEMP_TB;
        }
    }
    memset(s->free_temps, 0, sizeof(s->free_temps));

    nscratch = 0;
    QTAILQ_FOREACH_SAFE(op, &s->ops, link, next) {
        p.nd = 0;
        taint_op(&p, op, next);
        free_scratch();
    }
    s->emit_before_op = NULL;
    tcg_swap_vecop_list(old_list);
}

/* ---------------------------------------------------------------- devices and API */

void taint_device_write(uint64_t ram_addr, uint64_t len)
{
    stats.device_writes++;
    stats.device_bytes += len;
    if (ram_shadow && ram_addr < ram_shadow_size) {
        memset(ram_shadow + ram_addr, 0, MIN(len, ram_shadow_size - ram_addr));
    }
    if (device_write_fn) {
        device_write_fn(device_write_opaque, ram_addr, len);
    }
}

/*
 * Helpers that write guest memory themselves report each write: aarch64 FEAT_MOPS CPY*, SET*
 * (glibc's memcpy/memset on a CPU that has them), DC ZVA and RISC-V cbo.zero. A write never
 * crosses a page on either side, and its pages are in the TLB (the helper just accessed them).
 */
void taint_helper_copy(CPUState *cpu, vaddr to, int to_idx, vaddr from, int from_idx,
                       uint64_t len)
{
    uint8_t *d = ram_shadow_at(cpu, to, to_idx, MMU_DATA_STORE);
    uint8_t *s = ram_shadow_at(cpu, from, from_idx, MMU_DATA_LOAD);

    if (!d) {
        return;     /* not RAM: a device's register keeps no shadow */
    }
    len = MIN(len, ram_shadow_size - (d - ram_shadow));
    stats.helper_mem_bytes += len;
    if (!s || s + len > ram_shadow + ram_shadow_size) {
        memset(d, 0, len);  /* from a device: clean, as a load from it with no source cb */
        return;
    }
    ram_mark(d - ram_shadow, len);
    memmove(d, s, len);
    if (ram_lab && d - ram_shadow + len <= ram_lab_size && s - ram_shadow + len <= ram_lab_size) {
        memmove(ram_lab + (d - ram_shadow), ram_lab + (s - ram_shadow), len * sizeof(uint32_t));
    }
}

/* ... and a fill of [to, to + len) with the low byte of the CPU-state field at @data_off
 * (negative: a constant). */
void taint_helper_fill(CPUState *cpu, vaddr to, int idx, uint64_t len, ptrdiff_t data_off)
{
    uint8_t *d = ram_shadow_at(cpu, to, idx, MMU_DATA_STORE);
    uint8_t m = 0;
    uint32_t id = 0;

    if (!d) {
        return;
    }
    len = MIN(len, ram_shadow_size - (d - ram_shadow));
    stats.helper_mem_bytes += len;
    if (data_off >= 0 && data_off < env_size) {
        m = ((uint8_t *)cpu->taint_senv)[data_off];
        id = m ? labfile(cpu)[data_off] : 0;
    }
    if (m) {
        ram_mark(d - ram_shadow, len);
    }
    memset(d, m, len);
    if (ram_lab && d - ram_shadow + len <= ram_lab_size) {
        for (uint64_t i = 0; i < len; i++) {
            ram_lab[d - ram_shadow + i] = id;
        }
    }
}

void taint_set_device_write_cb(TaintDeviceWriteFn fn, void *opaque)
{
    device_write_fn = fn;
    device_write_opaque = opaque;
}

static void taint_reset(void *opaque)
{
    CPUState *cpu;

    ram_shadow_clear();
    CPU_FOREACH(cpu) {
        senv_clear(cpu);
    }
}

static int taint_reset_owner;

static void set_cflags(bool on)
{
    CPUState *cpu;

    CPU_FOREACH(cpu) {
        stlb_flush_all(cpu);
        tcg_cflags_clear(cpu, CF_TAINT_CHECK);      /* start in track; elastic finds clean */
        if (cpu->cflags_next_tb != -1) {
            cpu->cflags_next_tb &= ~CF_TAINT_CHECK;
        }
        if (on) {
            tick(cpu)[0] = tick(cpu)[1] = ELASTIC_PERIOD;
            tcg_cflags_set(cpu, CF_TAINT);
            if (cpu->cflags_next_tb != -1) {
                cpu->cflags_next_tb |= CF_TAINT;
            }
        } else {
            tcg_cflags_clear(cpu, CF_TAINT);
            if (cpu->cflags_next_tb != -1) {
                cpu->cflags_next_tb &= ~CF_TAINT;
            }
        }
    }
    tb_flush__exclusive_or_serial();
}

static void commit_work(CPUState *cpu, run_on_cpu_data data)
{
    set_cflags(data.host_int);
}

/* From a vCPU (a callback) the flush waits for a safe point; otherwise it is immediate. */
static void commit(bool on)
{
    if (current_cpu) {
        async_safe_run_on_cpu(current_cpu, commit_work, RUN_ON_CPU_HOST_INT(on));
        cpu_exit(current_cpu);
    } else {
        set_cflags(on);
    }
}

bool taint_enable(void)
{
    CPUState *cpu;

    if (!analysis_policy_has(ANALYSIS_CAP_TAINT) || !senv_idx ||
        replay_mode != REPLAY_MODE_PLAY) {
        return false;
    }
    if (taint_active) {
        return true;
    }
    elastic = g_strcmp0(getenv("PANDA_TAINT_ELASTIC"), "1") == 0;
    ram_shadow_alloc();
    CPU_FOREACH(cpu) {
        senv_clear(cpu);
    }
    panda_register_transient_reset(&taint_reset_owner, taint_reset, NULL);
    taint_active = true;
    commit(true);
    return true;
}

void taint_disable(void)
{
    if (!taint_active) {
        return;
    }
    taint_active = false;
    commit(false);
    panda_unregister_transient_reset(&taint_reset_owner);
    taint_reset(NULL);
}

bool taint_enabled(void)
{
    return taint_active;
}

void taint_label_ram(uint64_t ram_addr, uint64_t len, uint8_t mask)
{
    if (!ram_shadow) {
        ram_shadow_alloc();
    }
    if (ram_addr < ram_shadow_size) {
        len = MIN(len, ram_shadow_size - ram_addr);
        if (mask) {
            ram_mark(ram_addr, len);
        }
        memset(ram_shadow + ram_addr, mask, len);
        if (ram_lab) {
            memset(ram_lab + ram_addr, 0, len * sizeof(uint32_t));
        }
    }
}

uint8_t taint_query_ram(uint64_t ram_addr)
{
    return ram_shadow && ram_addr < ram_shadow_size ? ram_shadow[ram_addr] : 0;
}

uint64_t taint_count_ram(void)
{
    uint64_t n = 0;

    if (!ram_shadow) {
        return 0;
    }
    /* only resident shadow pages can be nonzero; mincore keeps this cheap on a sparse map */
    size_t pg = qemu_real_host_page_size();
    size_t npages = ram_shadow_size / pg;
    g_autofree unsigned char *vec = g_malloc(npages);

    if (mincore(ram_shadow, ram_shadow_size, vec) != 0) {
        memset(vec, 1, npages);
    }
    for (size_t i = 0; i < npages; i++) {
        if (!(vec[i] & 1)) {
            continue;
        }
        const uint64_t *w = (const uint64_t *)(ram_shadow + i * pg);
        for (size_t j = 0; j < pg / 8; j++) {
            if (w[j]) {
                for (int k = 0; k < 8; k++) {
                    n += ((w[j] >> (8 * k)) & 0xff) != 0;
                }
            }
        }
    }
    return n;
}

uint64_t taint_va_to_ram(CPUState *cpu, uint64_t va)
{
    TranslateForDebugResult t;
    hwaddr xlat, len = 1;
    MemoryRegion *mr;
    uint64_t ra = -1;

    if (!cpu_translate_for_debug(cpu, va, &t)) {
        return -1;
    }
    RCU_READ_LOCK_GUARD();
    mr = address_space_translate(cpu_get_address_space(cpu, cpu_asidx_from_attrs(cpu, t.attrs)),
                                 t.physaddr, &xlat, &len, false, t.attrs);
    if (memory_region_is_ram(mr)) {
        ra = memory_region_get_ram_addr(mr) + xlat;
    }
    return ra;
}

int taint_label_reg(CPUState *cpu, int regno, uint8_t mask)
{
    ptrdiff_t off;
    unsigned size;

    if (!cpu->taint_senv || !tcg_register_location(cpu, regno, &off, &size) ||
        off < 0 || off + size > env_size) {
        return -1;
    }
    memset((uint8_t *)cpu->taint_senv + off, mask, size);
    memset(labfile(cpu) + off, 0, size * sizeof(uint32_t));
    if (mask) {
        elastic_tainted_cpu(cpu);
    }
    return size;
}

int taint_query_reg(CPUState *cpu, int regno, uint8_t *out, int len)
{
    ptrdiff_t off;
    unsigned size;

    if (!cpu->taint_senv || !tcg_register_location(cpu, regno, &off, &size) ||
        off < 0 || off + size > env_size) {
        return -1;
    }
    memcpy(out, (uint8_t *)cpu->taint_senv + off, MIN(size, (unsigned)MAX(len, 0)));
    return size;
}

/*
 * Address mode (opt-in): a load through a tainted address is tainted, and so is a store through
 * one (table lookups, pointer chasing). Tier A only: label sets have no id for such bytes.
 */
bool taint_set_address_mode(bool on)
{
    if (!senv_idx || (on && labels_on)) {
        return false;
    }
    if (on != addr_taint) {
        addr_taint = on;
        if (taint_active) {
            commit(true);
        }
    }
    return true;
}

bool taint_enable_labels(bool on)
{
    if (!senv_idx || (on && addr_taint)) {
        return false;
    }
    if (on) {
        ram_shadow_alloc();
        ram_lab_alloc();
    }
    if (on != labels_on) {
        labels_on = on;
        if (taint_active) {
            commit(true);
        }
    }
    return true;
}

static int lset_expand(uint32_t id, uint32_t *out, int max)
{
    const LSet *set;

    if (!id) {
        return 0;
    }
    if (id == LSET_MANY) {
        return -1;
    }
    set = g_ptr_array_index(lsets, id);
    memcpy(out, set->v, MIN(set->n, (uint32_t)MAX(max, 0)) * sizeof(uint32_t));
    return set->n;
}

void taint_label_ram_l(uint64_t ram_addr, uint64_t len, uint8_t mask, uint32_t label)
{
    uint32_t id = label ? lset_intern(&label, 1) : 0;

    taint_label_ram(ram_addr, len, mask);
    ram_lab_alloc();
    for (uint64_t i = ram_addr; i < ram_addr + len && i < ram_lab_size; i++) {
        ram_lab[i] = id;
    }
}

int taint_query_ram_labels(uint64_t ram_addr, uint32_t *out, int max)
{
    return ram_shadow && ram_addr < ram_shadow_size ? lset_expand(ram_lab_at(ram_addr), out, max)
                                                     : 0;
}

int taint_label_reg_l(CPUState *cpu, int regno, uint8_t mask, uint32_t label)
{
    ptrdiff_t off;
    unsigned size;
    uint32_t id = label ? lset_intern(&label, 1) : 0;

    if (!cpu->taint_senv || !tcg_register_location(cpu, regno, &off, &size) ||
        off < 0 || off + size > env_size) {
        return -1;
    }
    memset((uint8_t *)cpu->taint_senv + off, mask, size);
    for (unsigned i = 0; i < size; i++) {
        labfile(cpu)[off + i] = id;
    }
    if (mask) {
        elastic_tainted_cpu(cpu);
    }
    return size;
}

int taint_query_reg_labels(CPUState *cpu, int regno, int byte, uint32_t *out, int max)
{
    ptrdiff_t off;
    unsigned size;

    if (!cpu->taint_senv || !tcg_register_location(cpu, regno, &off, &size) ||
        off < 0 || off + size > env_size || byte < 0 || byte >= size) {
        return -1;
    }
    if (!((uint8_t *)cpu->taint_senv)[off + byte]) {
        return 0;
    }
    return lset_expand(labfile(cpu)[off + byte], out, max);
}

/* Checks are emitted only while a function is set: re-translate when that changes. */
static void set_event(TaintEventFn *slot, void **opq, TaintEventFn fn, void *opaque)
{
    bool changed = !*slot != !fn;

    *slot = fn;
    *opq = opaque;
    if (changed && taint_active) {
        commit(true);
    }
}

void taint_set_mmio_read_cb(TaintMmioReadFn fn, void *opaque)
{
    mmio_fn = fn;
    mmio_opaque = opaque;
}

void taint_set_branch_cb(TaintEventFn fn, void *opaque)
{
    set_event(&branch_fn, &branch_opaque, fn, opaque);
}

void taint_set_pc_cb(TaintEventFn fn, void *opaque)
{
    set_event(&pc_fn, &pc_opaque, fn, opaque);
}

void taint_stats(char *buf, size_t len)
{
    GString *out = g_string_new(NULL);
    GHashTableIter it;
    gpointer k, v;

    g_string_append_printf(out,
        "tbs %" PRIu64 " ops %" PRIu64 " untracked_ld %" PRIu64 " untracked_st %" PRIu64
        " ptr_calls %" PRIu64 " ld_slow %" PRIu64 " st_slow %" PRIu64 " ram_oob %" PRIu64
        " unknown_ops %" PRIu64 " h1_tainted %" PRIu64 " device_writes %" PRIu64
        " device_bytes %" PRIu64 " ptr_tainted %" PRIu64 " branch_events %" PRIu64
        " pc_events %" PRIu64 " lab_calls %" PRIu64 " lab_many %" PRIu64 " lab_sets %u"
        " mmio_tainted %" PRIu64 " helper_mem_bytes %" PRIu64 " h2b_calls %" PRIu64
        " to_track %" PRIu64 " to_check %" PRIu64 " chk_slow %" PRIu64 " chk_st_slow %" PRIu64
        "\n",
        stats.tbs, stats.ops, stats.untracked_ld, stats.untracked_st, stats.ptr_calls,
        stats.ld_slow, stats.st_slow, stats.ram_oob, stats.unknown_ops, stats.h1_tainted,
        stats.device_writes, stats.device_bytes, stats.ptr_tainted, stats.branch_events,
        stats.pc_events, stats.lab_calls, stats.lab_many, lsets ? lsets->len - 2 : 0,
        stats.mmio_tainted, stats.helper_mem_bytes, stats.h2b_calls, stats.to_track,
        stats.to_check, stats.chk_slow, stats.chk_st_slow);
    if (h1_hits) {
        g_hash_table_iter_init(&it, h1_hits);
        while (g_hash_table_iter_next(&it, &k, &v)) {
            g_string_append_printf(out, "h1 %s %zu\n", (char *)k, GPOINTER_TO_SIZE(v));
        }
    }
    g_strlcpy(buf, out->str, len);
    g_string_free(out, true);
}
