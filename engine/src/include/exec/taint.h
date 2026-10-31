/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Taint: a bit-precise shadow of guest registers, CPU state and RAM, propagated by a TCG pass.
 *
 * Provisioned by the "taint" startup capability (ANALYSIS_CAP_TAINT); usable only while
 * replaying. One shadow bit per guest bit (Memcheck's V-bits): a set bit means "this bit
 * depends on a labelled source". RAM shadow is keyed by ram_addr_t.
 */
#ifndef EXEC_TAINT_H
#define EXEC_TAINT_H

#include "exec/vaddr.h"
#include "exec/hwaddr.h"
#include "exec/mmu-access-type.h"

/* Set while taint is enabled: cputlb and physmem test it before calling in. */
extern bool taint_active;

/* Engine call sites (cap-gated by their callers' flags). */
void taint_tcg_init(CPUState *cpu);
void taint_vcpu_init(CPUState *cpu);
void taint_gen_tb(CPUState *cpu, TranslationBlock *tb);
void taint_tlb_flush_mmuidx(CPUState *cpu, int mmu_idx);
void taint_tlb_flush_page(CPUState *cpu, int mmu_idx, vaddr page);
void taint_device_write(uint64_t ram_addr, uint64_t len);
/* cputlb.c: the ram_addr behind a TLB-mapped address, without filling or faulting. */
bool tlb_peek_ram_addr(CPUState *cpu, vaddr addr, int mmu_idx,
                       MMUAccessType type, uint64_t *ram_addr);

/* API (pandare2 and plugins). Valid only in replay; enable flushes translated code. */
bool taint_enable(void);
void taint_disable(void);
bool taint_enabled(void);
/* Set the shadow of guest RAM bytes [ram_addr, ram_addr+len) to @mask (0 clears). */
void taint_label_ram(uint64_t ram_addr, uint64_t len, uint8_t mask);
/* The shadow byte of one guest RAM byte (0 = clean). */
uint8_t taint_query_ram(uint64_t ram_addr);
/* Number of tainted bytes (nonzero shadow) in the whole RAM shadow. */
uint64_t taint_count_ram(void);
/* A guest virtual address to its ram_addr through @cpu's current TLB/page tables, or -1. */
uint64_t taint_va_to_ram(CPUState *cpu, uint64_t va);
/* Called after the default clear on a device/DMA write into RAM; may label the range. */
typedef void (*TaintDeviceWriteFn)(void *opaque, uint64_t ram_addr, uint64_t len);
void taint_set_device_write_cb(TaintDeviceWriteFn fn, void *opaque);
/*
 * Guest registers by gdbstub number (general-purpose registers: see tcg_register_location).
 * Call from a callback between blocks. Return the register's size in bytes, or -1 if it is not
 * held in a single field of the CPU state.
 */
int taint_label_reg(CPUState *cpu, int regno, uint8_t mask);
/* Copies up to @len shadow bytes of the register into @out, lowest-addressed byte first. */
int taint_query_reg(CPUState *cpu, int regno, uint8_t *out, int len);
/*
 * Labels (tier B), opt-in: each tainted byte also carries a set of u32 labels (sources), up to
 * 256 per byte; beyond that a query returns -1. Turn on before taint_enable (or code is
 * re-translated). Labels follow data flow by byte; the mask still decides what is tainted.
 */
bool taint_enable_labels(bool on);
/* Like taint_label_ram, and give the bytes the single label @label (0: none). */
void taint_label_ram_l(uint64_t ram_addr, uint64_t len, uint8_t mask, uint32_t label);
int taint_label_reg_l(CPUState *cpu, int regno, uint8_t mask, uint32_t label);
/* Labels of a byte, up to @max into @out; returns how many it has (0 if clean, -1 if too many). */
int taint_query_ram_labels(uint64_t ram_addr, uint32_t *out, int max);
int taint_query_reg_labels(CPUState *cpu, int regno, int byte, uint32_t *out, int max);
/*
 * Events, opt-in: setting a function re-translates code so the checks exist only while one is
 * set. @pc is the guest instruction's address (for PC-relative code, page-crossing blocks may
 * report the start page); @mask is the shadow of the condition inputs or of the target.
 *   branch: a conditional branch in translated code whose condition is tainted;
 *   pc:     an indirect jump (to an address not known at translation) whose target is tainted.
 */
typedef void (*TaintEventFn)(void *opaque, CPUState *cpu, uint64_t pc, uint64_t mask);
void taint_set_branch_cb(TaintEventFn fn, void *opaque);
void taint_set_pc_cb(TaintEventFn fn, void *opaque);
/*
 * MMIO as a source: called for each guest load from an address that is not RAM (a device
 * register) while taint is on, with the load's guest virtual address and size. Return a nonzero
 * label to taint the loaded bytes (with labels on, they carry that label), or 0 to leave them
 * clean (the default). Called from translated code, where registers may not be up to date:
 * decide from the address; don't change guest state.
 */
typedef uint32_t (*TaintMmioReadFn)(void *opaque, CPUState *cpu, uint64_t vaddr, unsigned size);
void taint_set_mmio_read_cb(TaintMmioReadFn fn, void *opaque);
/* Counters: helper calls with tainted inputs that have no model (H1 fallback), etc. */
void taint_stats(char *buf, size_t len);

#endif
