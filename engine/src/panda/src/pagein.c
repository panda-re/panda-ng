/*
 * Page-in requests (include/panda/pagein.h): injection sessions (include/panda/inject.h) whose
 * call is madvise(MADV_POPULATE_READ/WRITE).
 *
 * A request is a session for the request's address space. At a user-mode boundary of it, a
 * range that is already accessible is copied at once, with no system call. Otherwise the thread
 * makes madvise() for the range; when it is back, the range is copied without faulting, and the
 * thread's registers go back. The guest kernel decides what it maps: an address it won't map
 * completes with its errno, never with a signal or an oops.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qemu/timer.h"
#include "hw/core/cpu.h"
#include "exec/cpu-common.h"
#include "system/replay-analysis.h"
#include "panda/common.h"
#include "panda/inject.h"
#include "panda/pagein.h"
#include "plugins/qemu-plugin.h"

#define MADV_POPULATE_READ 22
#define MADV_POPULATE_WRITE 23
#define PAGEIN_MAX_ROUNDS 6         /* system calls per request (page size probes, reclaim) */

/*
 * The guest kernel's page size, as a shift, once known: madvise wants a start aligned to it,
 * and it may be larger than the target's (16 or 64 KiB on arm64, ppc64, mips, loongarch). Until
 * a call proves it, a request answered EINVAL is tried again with the start aligned to 16, then
 * 64 KiB; the first alignment that works, from a start not aligned to the next one, is it.
 */
static int guest_page_shift;

typedef struct PageinReq {
    uint64_t id, asid, addr;
    size_t len;
    uint8_t *buf;               /* read: filled at completion; write: the bytes */
    bool write;
    size_t done_len;            /* copied so far, page by page (after the call) */
    bool populated;             /* the last call succeeded: what's missing is a TLB miss away */
    int rounds, touches;
    int shift, first_errno;     /* the start's alignment for the next call; its first answer */
    int status, guest_errno;    /* the answer, once next() decided it */
    uint64_t timeout_ns;
    panda_pagein_cb_t cb;
    void *opaque;
} PageinReq;

static QemuMutex lock;
static bool inited;
static uint64_t next_id = 1;
static unsigned nreqs;
static uint64_t st_submitted, st_injected, st_completed, st_abandoned, st_status[11];

/* madvise's number for each injector ABI */
static const int64_t nr_madvise[PANDA_INJECT_ABI_COUNT] = {
    [PANDA_INJECT_ABI_X86_64] = 28,
    [PANDA_INJECT_ABI_I386] = 219,
    [PANDA_INJECT_ABI_AARCH64] = 233,
    [PANDA_INJECT_ABI_ARM_EABI] = 220,
    [PANDA_INJECT_ABI_RISCV64] = 233,
    [PANDA_INJECT_ABI_MIPS_O32] = 4218,
    [PANDA_INJECT_ABI_MIPS_N64] = 5027,
    [PANDA_INJECT_ABI_PPC] = 205,
    [PANDA_INJECT_ABI_PPC64] = 205,
    [PANDA_INJECT_ABI_LOONGARCH64] = 233,
};

static void pagein_init(void)
{
    if (!qatomic_read(&inited)) {
        static gsize once;
        if (g_once_init_enter(&once)) {
            qemu_mutex_init(&lock);
            qatomic_set(&inited, true);
            g_once_init_leave(&once, 1);
        }
    }
}

bool panda_pagein_can_wait(void)
{
    return current_cpu == NULL && !bql_locked();
}

const char *panda_pagein_strerror(int status)
{
    switch (status) {
    case PANDA_PAGEIN_OK: return "ok";
    case PANDA_PAGEIN_QUEUED: return "queued";
    case PANDA_PAGEIN_EGUEST: return "the guest kernel refused the range";
    case PANDA_PAGEIN_ETIMEDOUT: return "the process did not run in user mode in time";
    case PANDA_PAGEIN_ECANCELED: return "cancelled by a snapshot load or reset";
    case PANDA_PAGEIN_ERECORD: return "refused: record/replay is active";
    case PANDA_PAGEIN_EACCEL: return "refused: not supported under this accelerator";
    case PANDA_PAGEIN_EARCH: return "refused: no system-call injector for this target or task";
    case PANDA_PAGEIN_EINVAL: return "refused: bad range or no completion";
    case PANDA_PAGEIN_EBUSY: return "refused: too many requests outstanding";
    case PANDA_PAGEIN_ERECLAIMED: return "the guest reclaimed the range again before it was read";
    case PANDA_PAGEIN_EIO: return "the range is not RAM";
    }
    return "unknown";
}

void panda_pagein_stats(uint64_t *submitted, uint64_t *injected, uint64_t *completed,
                        uint64_t *abandoned, uint64_t *stale, uint64_t *overframes,
                        uint64_t by_status[11])
{
    PandaInjectStats is;

    /* the token rule's counters are the injection sessions' (page-in's are among them) */
    panda_inject_stats(&is);
    *stale = is.stale;
    *overframes = is.overframes;
    pagein_init();
    qemu_mutex_lock(&lock);
    *abandoned = st_abandoned;
    *submitted = st_submitted;
    *injected = st_injected;
    *completed = st_completed;
    memcpy(by_status, st_status, sizeof(st_status));
    qemu_mutex_unlock(&lock);
}

/* At a user-mode boundary of r's address space: copy without faulting */
static int pagein_copy_range(PageinReq *r, uint64_t off, size_t len)
{
    GByteArray *b = g_byte_array_new();
    int ret;

    if (r->write) {
        g_byte_array_append(b, r->buf + off, len);
        ret = qemu_plugin_write_memory_vaddr_fault(r->addr + off, b, QEMU_PLUGIN_VADDR_USER,
                                                   false);
    } else {
        ret = qemu_plugin_read_memory_vaddr_fault(r->addr + off, b, len, QEMU_PLUGIN_VADDR_USER,
                                                  false);
        if (ret == QEMU_PLUGIN_VADDR_OK) {
            memcpy(r->buf + off, b->data, len);
        }
    }
    g_byte_array_unref(b);
    return ret;
}

/*
 * At a user-mode boundary of r's address space: copy without faulting, all of it or nothing.
 * After the system call, @partial: as much as the CPU can reach now, a page at a time, from where
 * the last one stopped. A software-refilled TLB (MIPS, BookE PowerPC) may hold fewer pages than
 * the range spans, so such a range is copied over several boundaries; a write may then land in
 * part if the process goes away in between.
 */
static int pagein_copy(PageinReq *r, bool partial)
{
    uint64_t page = qemu_target_page_size();
    int ret;

    if (!partial && r->done_len == 0) {
        return pagein_copy_range(r, 0, r->len);
    }
    while (r->done_len < r->len) {
        uint64_t a = r->addr + r->done_len;
        size_t chunk = MIN(r->len - r->done_len, page - (a & (page - 1)));

        ret = pagein_copy_range(r, r->done_len, chunk);
        if (ret != QEMU_PLUGIN_VADDR_OK) {
            return ret;
        }
        r->done_len += chunk;
    }
    return QEMU_PLUGIN_VADDR_OK;
}

/* The first page of r the CPU can't reach without a fault, or -1 */
static uint64_t pagein_first_missing(PageinReq *r)
{
    uint64_t page = qemu_target_page_size();
    GByteArray *b = g_byte_array_new();
    uint64_t a, bad = (uint64_t)-1;

    for (a = (r->addr + r->done_len) & ~(page - 1); a < r->addr + r->len; a += page) {
        uint64_t at = MAX(a, r->addr + r->done_len);
        int ret = r->write ?
            (g_byte_array_set_size(b, 1), memcpy(b->data, r->buf + (at - r->addr), 1),
             qemu_plugin_write_memory_vaddr_fault(at, b, QEMU_PLUGIN_VADDR_USER, false)) :
            qemu_plugin_read_memory_vaddr_fault(at, b, 1, QEMU_PLUGIN_VADDR_USER, false);
        if (ret != QEMU_PLUGIN_VADDR_OK) {
            bad = at;
            break;
        }
    }
    g_byte_array_unref(b);
    return bad;
}

static bool pagein_may_touch(PageinReq *r)
{
    return r->touches < 2 * (r->len / qemu_target_page_size()) + 8;
}

/* next(): copy if possible; else madvise; after it, copy what's reachable */
static int pagein_next(void *opaque, CPUState *cs, PandaInjectStep *step, PandaInjectCall *call)
{
    PageinReq *r = opaque;
    int c;

    if (step->calls > 0) {
        int64_t ret = step->ret;
        int shift = r->shift ? r->shift : TARGET_PAGE_BITS;
        uint64_t start = r->addr & ~((1ull << shift) - 1);

        if (ret == -EINVAL && !qatomic_read(&guest_page_shift) && shift < 16) {
            r->first_errno = r->first_errno ? r->first_errno : EINVAL;
            r->shift = shift + 2;       /* maybe a larger page: try again aligned to it */
        } else if (ret < 0 && ret > -4096) {
            r->status = PANDA_PAGEIN_EGUEST;
            r->guest_errno = r->first_errno ? r->first_errno : (int)-ret;
            return PANDA_INJECT_DONE;
        } else {
            if (!qatomic_read(&guest_page_shift) && (start & ((1ull << (shift + 2)) - 1))) {
                qatomic_set(&guest_page_shift, shift);  /* worked; no larger alignment did */
            }
            r->populated = true;
            c = pagein_copy(r, true);
            if (c == QEMU_PLUGIN_VADDR_OK || c == QEMU_PLUGIN_VADDR_IO) {
                r->status = c == QEMU_PLUGIN_VADDR_OK ? PANDA_PAGEIN_OK : PANDA_PAGEIN_EIO;
                return PANDA_INJECT_DONE;
            }
            if (r->rounds >= PAGEIN_MAX_ROUNDS) {
                r->status = PANDA_PAGEIN_ERECLAIMED;
                return PANDA_INJECT_DONE;
            }
            if (pagein_may_touch(r)) {
                step->touch = pagein_first_missing(r);
                r->touches++;
            }
            return PANDA_INJECT_AGAIN;  /* the registers go back; the rest at a later boundary */
        }
    } else {
        /* already there (or now): no system call */
        c = pagein_copy(r, r->populated);
        if (c == QEMU_PLUGIN_VADDR_OK || c == QEMU_PLUGIN_VADDR_IO) {
            r->status = c == QEMU_PLUGIN_VADDR_OK ? PANDA_PAGEIN_OK : PANDA_PAGEIN_EIO;
            return PANDA_INJECT_DONE;
        }
        if (r->populated && pagein_may_touch(r)) {
            /* populated, but the CPU's TLB doesn't have the next page yet: its miss, here */
            step->touch = pagein_first_missing(r);
            r->touches++;
            return PANDA_INJECT_AGAIN;
        }
        r->populated = false;
    }
    if (r->rounds >= PAGEIN_MAX_ROUNDS) {
        r->status = PANDA_PAGEIN_ERECLAIMED;
        return PANDA_INJECT_DONE;
    }
    if (step->abi < 0 || step->abi >= PANDA_INJECT_ABI_COUNT || !nr_madvise[step->abi]) {
        r->status = PANDA_PAGEIN_EARCH;
        return PANDA_INJECT_DONE;
    }
    {
        int gs = qatomic_read(&guest_page_shift);
        int shift = r->shift ? r->shift : gs ? gs : TARGET_PAGE_BITS;
        uint64_t page = qemu_target_page_size();
        uint64_t start = r->addr & ~((1ull << shift) - 1);
        uint64_t end = ROUND_UP(r->addr + r->len, page);

        r->shift = shift;
        r->rounds++;
        qemu_mutex_lock(&lock);
        st_injected++;
        qemu_mutex_unlock(&lock);
        call->nr = nr_madvise[step->abi];
        call->args[0] = start;
        call->args[1] = end - start;
        call->args[2] = r->write ? MADV_POPULATE_WRITE : MADV_POPULATE_READ;
        call->nargs = 3;
        return PANDA_INJECT_CALL;
    }
}

static int pagein_status_of(int st)
{
    switch (st) {
    case PANDA_INJECT_ETIMEDOUT: return PANDA_PAGEIN_ETIMEDOUT;
    case PANDA_INJECT_ECANCELED: return PANDA_PAGEIN_ECANCELED;
    case PANDA_INJECT_ERECORD: return PANDA_PAGEIN_ERECORD;
    case PANDA_INJECT_EACCEL: return PANDA_PAGEIN_EACCEL;
    case PANDA_INJECT_EARCH: return PANDA_PAGEIN_EARCH;
    case PANDA_INJECT_EBUSY: return PANDA_PAGEIN_EBUSY;
    case PANDA_INJECT_EINVAL: return PANDA_PAGEIN_EINVAL;
    }
    return PANDA_PAGEIN_EINVAL;
}

static int pagein_start(PageinReq *r);

static void pagein_done(void *opaque, uint64_t sid, int status)
{
    PageinReq *r = opaque;
    int st;

    if (status == PANDA_INJECT_EABANDONED) {
        /*
         * The thread came back from the call somewhere else (a signal handler longjmp'd, an rseq
         * abort): it kept its own registers, and the request goes round again
         */
        qemu_mutex_lock(&lock);
        st_abandoned++;
        qemu_mutex_unlock(&lock);
        if (pagein_start(r) == PANDA_INJECT_QUEUED) {
            return;
        }
        status = PANDA_INJECT_ERECORD;
    }
    if (status == PANDA_INJECT_ECANCELED) {
        qatomic_set(&guest_page_shift, 0);      /* a snapshot load: maybe another guest now */
    }
    st = status == PANDA_INJECT_OK ? r->status : pagein_status_of(status);
    qemu_mutex_lock(&lock);
    nreqs--;
    st_completed++;
    if (st <= 0 && st >= -10) {
        st_status[-st]++;
    }
    qemu_mutex_unlock(&lock);
    r->cb(r->id, st, st == PANDA_PAGEIN_EGUEST ? r->guest_errno : 0,
          st == PANDA_PAGEIN_OK ? r->buf : NULL, st == PANDA_PAGEIN_OK ? r->len : 0, r->opaque);
    g_free(r->buf);
    g_free(r);
}

static int pagein_start(PageinReq *r)
{
    return panda_inject_submit(PANDA_INJECT_TARGET_ASID, r->asid, r->timeout_ns, pagein_next,
                               pagein_done, r, NULL);
}

int panda_pagein_submit(uint64_t asid, uint64_t addr, size_t len, const uint8_t *wdata,
                        uint64_t timeout_ns, panda_pagein_cb_t cb, void *opaque, uint64_t *id)
{
    PageinReq *r;
    int ret;

    pagein_init();
    if (!replay_analysis_write_allowed()) {
        return PANDA_PAGEIN_ERECORD;     /* the system call would not be in the recording */
    }
    if (!cb || len == 0 || len > PANDA_PAGEIN_MAX_LEN || addr + len < addr) {
        return PANDA_PAGEIN_EINVAL;
    }
    qemu_mutex_lock(&lock);
    if (nreqs >= PANDA_PAGEIN_MAX_OUTSTANDING) {
        qemu_mutex_unlock(&lock);
        return PANDA_PAGEIN_EBUSY;
    }
    nreqs++;
    r = g_new0(PageinReq, 1);
    r->id = next_id++;
    qemu_mutex_unlock(&lock);
    r->asid = asid;
    r->addr = addr;
    r->len = len;
    r->write = wdata != NULL;
    r->buf = wdata ? g_memdup2(wdata, len) : g_malloc0(len);
    r->cb = cb;
    r->opaque = opaque;
    r->timeout_ns = timeout_ns;
    r->status = PANDA_PAGEIN_EINVAL;
    ret = pagein_start(r);
    if (ret != PANDA_INJECT_QUEUED) {
        qemu_mutex_lock(&lock);
        nreqs--;
        qemu_mutex_unlock(&lock);
        g_free(r->buf);
        g_free(r);
        return pagein_status_of(ret);
    }
    qemu_mutex_lock(&lock);
    st_submitted++;
    qemu_mutex_unlock(&lock);
    if (id) {
        *id = r->id;
    }
    return PANDA_PAGEIN_QUEUED;
}
