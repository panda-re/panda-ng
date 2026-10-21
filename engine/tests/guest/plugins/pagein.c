/*
 * pagein: prototype of consistent guest-virtual reads from a hook.
 *
 * At a hook we call qemu_plugin_read_memory_vaddr_fault().  If the page has
 * no valid translation, QEMU raises the architectural fault a load at the
 * hooked instruction would take; the guest kernel pages the address in and
 * resumes at the same instruction, so this callback runs again and the read
 * now succeeds -- same PC, same task, same register state.
 *
 * Hooks (all optional):
 *   marker=on        user-mode marker instruction (see guest/pagetest.c):
 *                    address in arg0, length in arg1
 *   kwrite=ADDR      kernel ksys_write(fd, buf, count) entry, fd == kfd
 *   kfd=77
 *
 * Operation:
 *   op=read|write    read [addr, addr+len), or write a fixed pattern there
 *   op=multi         several dependent accesses in one hook: addr points
 *                    to { u64 ptr, len, w1, w2 }; read it, read
 *                    [ptr, ptr+len), write the pattern at w1 and w2
 *   batch=on|off     op=multi: write w1 and w2 as one batch (all or
 *                    nothing; default) or one after the other
 *   wapi=fault|debug write with qemu_plugin_write_memory_vaddr_fault()
 *                    (default) or the existing qemu_plugin_write_memory_vaddr()
 *                    (physical write through a debug translation; shown to
 *                    corrupt shared pages)
 *
 * Read/write policy:
 *   fault=on|off     page in on a miss (off = plain non-faulting probe)
 *   mode=current|user  translation privilege for the read
 *   tries=N          give up on an access after N faults per page it spans
 *   irqguard=on      never fault with interrupts masked
 *
 * Guest-policy delegation ("virtual extable"), needs kernel symbols:
 *   vext=on
 *   search=ADDR      search_exception_tables(): claim an entry for the hook
 *                    PC, so the kernel treats the hook like a uaccess site
 *                    (sleeps on a contended mmap lock instead of oopsing)
 *   fixup=ADDR       fixup_exception(): for the hook PC, report "fixed up"
 *                    without moving the PC: the kernel returns to the hook,
 *                    which then gives up -- no oops
 *   extable=ADDR     any valid exception-table entry (returned by search)
 *   sig=ADDR         force_sig_fault(): drop SIGSEGV/SIGBUS caused by our
 *                    own fault from a user-mode hook
 *   siginfo=ADDR     force_sig_info(sig, info, task), for kernels < 4.20
 *                    that have no force_sig_fault()
 *   tramp=ADDR       the fixup target of the extable= entry.  Where the
 *                    kernel applies the claimed entry itself instead of
 *                    calling an out-of-line fixup_exception() (powerpc
 *                    bad_page_fault(), which compilers may inline), it
 *                    resumes the faulting context at ADDR; we send it
 *                    straight back to the hook, which then gives up.  The
 *                    context is ours if its stack pointer is the hook's.
 *   (on powerpc, fixup= is bad_page_fault(regs, sig): it has no separate
 *   fixup_exception())
 *
 * Target description (defaults from the QEMU target):
 *   endian=big|little  guest byte order (ppc64 serves both)
 *   regspc=OFF       offset of the PC in struct pt_regs
 *
 * Accelerator:
 *   kvm=on           no instruction callbacks (KVM): hook kwrite= and the
 *                    vext= symbols with hardware breakpoints (x86: four in
 *                    all), and take the user marker as an int3 placed
 *                    right before the marker nop (guest/pagetest.c built
 *                    with -DPAGEIN_KVM)
 *
 * Observation only:
 *   quiet=on         don't log successful accesses (benchmarks); a count
 *                    is logged at exit
 *   oops=ADDR[:ADDR] log when these (oops path) functions are entered
 */
#include <glib.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

enum arch {
    ARCH_X86_64, ARCH_AARCH64, ARCH_ARM, ARCH_MIPS, ARCH_PPC64, ARCH_RISCV64,
    ARCH_LOONGARCH64,
};
static enum arch arch;
static bool big_endian, wide = true;    /* byte order, 64-bit registers */
static uint64_t regs_pc_off;

static bool opt_marker, opt_fault = true, opt_vext, opt_irqguard;
static enum qemu_plugin_vaddr_mode opt_mode = QEMU_PLUGIN_VADDR_CURRENT;
static uint64_t kwrite, kfd = 77, sym_search, sym_fixup, sym_extable, sym_sig;
static uint64_t sym_siginfo, sym_tramp;
static bool opt_write, opt_wdebug, opt_kvm, opt_quiet, opt_multi;
static bool opt_batch = true;
static uint64_t n_access, n_bp;
static int max_pending;          /* most invocations with a fault outstanding */

/* What op=write writes */
static const char pattern[] = "<PAGEIN-WROTE-ME>";
static GArray *oops_syms;
static int max_tries = 2;

/* registers, by role */
enum { R_ARG0, R_ARG1, R_ARG2, R_RET, R_SP, R_LR, R_FLAGS, R_PC, R_ASID,
       R_NUM };
static struct qemu_plugin_register *regs[R_NUM];
static bool have_reg[R_NUM];    /* a NULL handle is register 0 */

/*
 * Pending accesses: one entry per hook invocation that has raised a fault
 * and not finished yet.  After each fault the guest re-executes the hooked
 * instruction and the hook runs again from the start, replaying its
 * accesses in order.  An invocation is recognised by (pc, stack pointer,
 * address space for user hooks) -- the same task re-executing the same
 * instruction, whichever vCPU it now runs on -- and by replaying the same
 * address ranges.  Retries are bounded per access (tries= faults per page
 * it spans, without the access completing in between) and per invocation
 * (MAX_TOTAL_FAULTS).  An access that completed may fault again when the
 * hook replays it after a later access faulted: on a software-refilled TLB
 * (MIPS) its translation is often gone by then.  That is progress, not a
 * loop.
 */
#define MAXACC 8                /* accesses per hook invocation */
#define MAXRANGE 4              /* ranges per (batched) access */
#define NPEND 128
#define MAX_TOTAL_FAULTS 64
#define MAXLEN (1 << 20)
#define MAXVCPU 64

struct range {
    uint64_t addr, len;
};

struct acc {
    int nr;
    struct range r[MAXRANGE];
    int faults;                 /* all faults raised for it (logged) */
    int streak;                 /* faults since it last completed (bounded) */
    bool failed;                /* the guest refused our fault */
};

static struct pend {
    bool used, user;
    uint64_t pc, sp, asid, stamp;
    int inflight;               /* access with a fault outstanding, or -1 */
    int nacc, total;
    struct acc acc[MAXACC];
} ptab[NPEND];
static uint64_t pclock;
static GMutex lock;

/* One run of a hook callback */
typedef struct Hook {
    struct pend *p;
    int k;                      /* index of the next access */
    unsigned int vcpu;
    uint64_t pc, sp, asid;
    bool user;
} Hook;

/*
 * Buffers per vCPU: a raised fault leaves the callback with siglongjmp(),
 * so nothing allocated in it is freed.
 */
static GByteArray *vbufs[MAXVCPU][MAXRANGE];

/* Guest-order bytes <-> integer */
static uint64_t load_n(const uint8_t *p, int n)
{
    uint64_t v = 0;
    for (int i = 0; i < n; i++) {
        v |= (uint64_t)p[big_endian ? n - 1 - i : i] << (8 * i);
    }
    return v;
}

static void store_n(uint8_t *p, int n, uint64_t v)
{
    for (int i = 0; i < n; i++) {
        p[big_endian ? n - 1 - i : i] = v >> (8 * i);
    }
}

static uint64_t rreg(int r)
{
    g_autoptr(GByteArray) b = g_byte_array_new();
    if (!have_reg[r]) {
        return 0;
    }
    qemu_plugin_read_register(regs[r], b);
    return load_n(b->data, MIN(b->len, 8));
}

static void wreg(int r, uint64_t v)
{
    g_autoptr(GByteArray) b = g_byte_array_new();
    qemu_plugin_read_register(regs[r], b);
    store_n(b->data, MIN(b->len, 8), v);
    qemu_plugin_write_register(regs[r], b);
}

/* One guest word (pointer size) */
static uint64_t rmemw(uint64_t a)
{
    g_autoptr(GByteArray) b = g_byte_array_new();
    int n = wide ? 8 : 4;
    if (opt_kvm) {
        /* qemu_plugin_read_memory_vaddr() would sync the whole vCPU */
        if (qemu_plugin_read_memory_vaddr_fault(a, b, n,
                                                QEMU_PLUGIN_VADDR_CURRENT,
                                                false)) {
            return 0;
        }
    } else if (!qemu_plugin_read_memory_vaddr(a, b, n)) {
        return 0;
    }
    return load_n(b->data, n);
}

static void logf_(const char *fmt, ...) G_GNUC_PRINTF(1, 2);
static void logf_(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    g_autofree char *s = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    qemu_plugin_outs(s);
}

/* Emulate "return v" at the first instruction of a function. */
static void func_return(uint64_t v)
{
    uint64_t ra;
    if (arch == ARCH_X86_64) {
        uint64_t sp = rreg(R_SP);
        ra = rmemw(sp);
        wreg(R_SP, sp + 8);
    } else {
        ra = rreg(R_LR);
    }
    wreg(R_RET, v);
    qemu_plugin_set_pc(ra);
}

static bool irqs_masked(void)
{
    uint64_t f = rreg(R_FLAGS);
    switch (arch) {
    case ARCH_X86_64:
        return !(f & 0x200);            /* EFLAGS.IF */
    case ARCH_AARCH64:
    case ARCH_ARM:
        return f & 0x80;                /* PSTATE/CPSR.I */
    case ARCH_MIPS:
        return !(f & 1);                /* Status.IE */
    case ARCH_PPC64:
        return !(f & 0x8000);           /* MSR.EE */
    case ARCH_RISCV64:
        return have_reg[R_FLAGS] && !(f & 2);   /* sstatus.SIE */
    case ARCH_LOONGARCH64:
        return false;                   /* CRMD is not exposed */
    }
    return false;
}

/*
 * The current address space: the page-table root without ASID/PCID and
 * flag bits, or 0 where the target does not expose it (then two processes
 * at the same user pc and sp cannot be told apart).
 */
static uint64_t cur_asid(void)
{
    uint64_t v = rreg(R_ASID);
    switch (arch) {
    case ARCH_X86_64:
        return v & 0x000fffffffffe000ull;   /* drop PCID, PTI user bit 12 */
    case ARCH_AARCH64:
        return v & 0x0000fffffffffffeull;   /* drop ASID, CnP */
    case ARCH_RISCV64:
        return v & 0x00000fffffffffffull;   /* PPN */
    default:
        return 0;
    }
}

static const char *res_name(int r)
{
    switch (r) {
    case QEMU_PLUGIN_VADDR_OK: return "OK";
    case QEMU_PLUGIN_VADDR_UNMAPPED: return "UNAVAILABLE";
    case QEMU_PLUGIN_VADDR_IO: return "IO";
    default: return "BADMODE";
    }
}

static bool key_match(const struct pend *e, const Hook *h)
{
    return e->used && e->pc == h->pc && e->sp == h->sp &&
           e->user == h->user && e->asid == h->asid;
}

static void vcpu_init(unsigned int vcpu);

static void hook_begin(Hook *h, unsigned int vcpu, uint64_t pc, bool user)
{
    if (vcpu >= MAXVCPU) {
        fprintf(stderr, "pagein: vCPU %u >= %d\n", vcpu, MAXVCPU);
        abort();
    }
    for (int i = 0; i < MAXRANGE; i++) {
        if (!vbufs[vcpu][i]) {
            vbufs[vcpu][i] = g_byte_array_new();
        }
    }
    *h = (Hook) { .vcpu = vcpu, .pc = pc, .user = user, .sp = rreg(R_SP),
                  .asid = user ? cur_asid() : 0 };
    g_mutex_lock(&lock);
    for (int i = 0; i < NPEND; i++) {
        if (key_match(&ptab[i], h)) {
            h->p = &ptab[i];
            break;
        }
    }
    if (h->p && h->p->inflight >= 0) {
        /* back after the fault we raised for that access */
        h->p->acc[h->p->inflight].faults++;
        h->p->acc[h->p->inflight].streak++;
        h->p->total++;
        h->p->inflight = -1;
    }
    g_mutex_unlock(&lock);
}

/* The invocation finished: nothing is pending for it any more. */
static void hook_end(Hook *h)
{
    g_mutex_lock(&lock);
    if (h->p && key_match(h->p, h)) {
        h->p->used = false;
    }
    g_mutex_unlock(&lock);
}

static int pages_of(const struct range *r, int nr)
{
    int n = 0;
    for (int i = 0; i < nr; i++) {
        /* 4 KiB, the smallest page: a generous bound for larger pages */
        n += ((r[i].addr + r[i].len - 1) >> 12) - (r[i].addr >> 12) + 1;
    }
    return n;
}

static bool same_ranges(const struct acc *a, const struct range *r, int nr)
{
    if (a->nr != nr) {
        return false;
    }
    for (int i = 0; i < nr; i++) {
        if (a->r[i].addr != r[i].addr || a->r[i].len != r[i].len) {
            return false;
        }
    }
    return true;
}

static struct pend *pend_alloc(const Hook *h)
{
    struct pend *e = &ptab[0];
    for (int i = 0; i < NPEND; i++) {
        if (!ptab[i].used) {
            e = &ptab[i];
            break;
        }
        if (ptab[i].stamp < e->stamp) {
            e = &ptab[i];       /* evict the least recently active */
        }
    }
    memset(e, 0, sizeof(*e));
    e->used = true, e->user = h->user, e->pc = h->pc, e->sp = h->sp;
    e->asid = h->asid, e->inflight = -1;
    return e;
}

/*
 * Before access h->k: account for a fault it raised last time, and decide
 * whether it may raise another.  Returns the reason it may not, or NULL.
 */
static const char *acc_begin(Hook *h, const struct range *r, int nr,
                             int *faults, bool *allow)
{
    const char *why = NULL;
    struct acc *a = NULL;
    struct pend *e;

    g_mutex_lock(&lock);
    e = h->p && key_match(h->p, h) ? h->p : NULL;
    if (e && h->k < e->nacc) {
        if (same_ranges(&e->acc[h->k], r, nr)) {
            a = &e->acc[h->k];
        } else {
            /* this run took another path: forget the rest */
            e->nacc = h->k;
        }
    }
    *faults = a ? a->faults : 0;
    if (a && a->failed) {
        why = "guest-refused";
    } else if (a && a->streak >= max_tries * pages_of(r, nr)) {
        why = "tries-exhausted";
    } else if (e && e->total >= MAX_TOTAL_FAULTS) {
        why = "tries-exhausted";
    } else if (h->k >= MAXACC) {
        why = "untracked";
    } else if (!h->user && opt_irqguard && irqs_masked()) {
        why = "irqs-masked";
    }
    *allow = opt_fault && !why;
    if (*allow) {
        if (!e) {
            e = h->p = pend_alloc(h);
        }
        if (h->k >= e->nacc) {
            a = &e->acc[h->k];
            memset(a, 0, sizeof(*a));
            a->nr = nr;
            memcpy(a->r, r, nr * sizeof(*r));
            e->nacc = h->k + 1;
        }
        e->inflight = h->k;
        e->stamp = ++pclock;
        int n = 0;
        for (int i = 0; i < NPEND; i++) {
            n += ptab[i].used && ptab[i].inflight >= 0;
        }
        max_pending = MAX(max_pending, n);
    }
    g_mutex_unlock(&lock);
    return why;
}

static void acc_end(Hook *h)
{
    g_mutex_lock(&lock);
    if (h->p && key_match(h->p, h)) {
        if (h->p->inflight == h->k) {
            h->p->inflight = -1;
        }
        if (h->k < h->p->nacc) {
            h->p->acc[h->k].streak = 0;     /* it completed */
        }
    }
    g_mutex_unlock(&lock);
    h->k++;
}

static uint64_t fnv1a(const uint8_t *p, size_t n)
{
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < n; i++) {
        h = (h ^ p[i]) * 0x100000001b3ull;
    }
    return h;
}

static void fill_pattern(GByteArray *b, size_t len)
{
    g_byte_array_set_size(b, len);
    for (size_t i = 0; i < len; i++) {
        b->data[i] = pattern[i % 17];
    }
}

static void log_access(const Hook *h, const char *op, uint64_t addr,
                       uint64_t len, int r, int faults, const char *why,
                       int batch, const GByteArray *data)
{
    if (opt_quiet && r == QEMU_PLUGIN_VADDR_OK) {
        return;
    }
    GString *s = g_string_new(NULL);
    g_string_printf(s, "PAGEIN hook=%s op=%s pc=0x%" PRIx64 " addr=0x%" PRIx64
                    " len=%" PRIu64 " result=%s faults=%d",
                    h->user ? "user" : "kern", op, h->pc, addr, len,
                    res_name(r), faults);
    if (why) {
        g_string_append_printf(s, " why=%s", why);
    }
    if (batch > 1) {
        g_string_append_printf(s, " batch=%d", batch);
    }
    g_string_append_printf(s, " vcpu=%u", h->vcpu);
    if (r == QEMU_PLUGIN_VADDR_OK && data) {
        g_string_append(s, " data=");
        for (guint i = 0; i < MIN(data->len, 256); i++) {
            g_string_append_printf(s, "%02x", data->data[i]);
        }
        if (data->len > 256) {
            g_string_append_printf(s, " fnv=%016" PRIx64,
                                   fnv1a(data->data, data->len));
        }
    }
    g_string_append_c(s, '\n');
    qemu_plugin_outs(s->str);
    g_string_free(s, true);
}

/*
 * One access at the hook: a read or a write of one range, or a batch of
 * writes (all made writable before any is written).  Does not return if it
 * raises a fault: the hook runs again after the guest handles it.
 */
static int hook_access(Hook *h, const struct range *r, int nr, bool write,
                       GByteArray **bufs, int *faults, const char **why)
{
    bool allow;
    int ret;

    *why = acc_begin(h, r, nr, faults, &allow);
    if (write && opt_wdebug) {
        ret = QEMU_PLUGIN_VADDR_OK;
        for (int i = 0; i < nr; i++) {
            if (!qemu_plugin_write_memory_vaddr(r[i].addr, bufs[i])) {
                ret = QEMU_PLUGIN_VADDR_UNMAPPED;
            }
        }
    } else if (nr == 1 && !write) {
        ret = qemu_plugin_read_memory_vaddr_fault(r[0].addr, bufs[0],
                                                  r[0].len, opt_mode, allow);
    } else if (nr == 1) {
        ret = qemu_plugin_write_memory_vaddr_fault(r[0].addr, bufs[0],
                                                   opt_mode, allow);
    } else {
        struct qemu_plugin_vaddr_op ops[MAXRANGE];
        for (int i = 0; i < nr; i++) {
            ops[i] = (struct qemu_plugin_vaddr_op) {
                r[i].addr, bufs[i], r[i].len, write
            };
        }
        ret = qemu_plugin_access_memory_vaddr_fault(ops, nr, opt_mode, allow,
                                                    NULL);
    }
    acc_end(h);
    return ret;
}

/* op=read|write: one range */
static void body_single(Hook *h, uint64_t addr, uint64_t len)
{
    GByteArray **b = vbufs[h->vcpu];
    struct range r = { addr, MIN(len, MAXLEN) };
    const char *why;
    int faults, ret;

    if (opt_write) {
        fill_pattern(b[0], r.len);
    }
    ret = hook_access(h, &r, 1, opt_write, b, &faults, &why);
    log_access(h, !opt_write ? "read" : opt_wdebug ? "debugwrite" : "write",
               addr, r.len, ret, faults, why, 1, b[0]);
}

/*
 * op=multi: several dependent accesses in one hook.  addr points to
 *   struct { uint64_t ptr, len, w1, w2; }      (guest byte order)
 * Read it, read [ptr, ptr+len), then write the pattern (64 bytes) at w1
 * and w2 -- as one batch (batch=on, default) or one after the other.
 */
static void body_multi(Hook *h, uint64_t addr)
{
    GByteArray **b = vbufs[h->vcpu];
    struct range r[2] = { { addr, 32 } };
    uint64_t ptr, len, w[2];
    const char *why;
    int faults, ret;

    ret = hook_access(h, r, 1, false, b, &faults, &why);
    log_access(h, "read", addr, 32, ret, faults, why, 1, b[0]);
    if (ret) {
        return;
    }
    ptr = load_n(b[0]->data, 8), len = load_n(b[0]->data + 8, 8);
    w[0] = load_n(b[0]->data + 16, 8), w[1] = load_n(b[0]->data + 24, 8);

    r[0] = (struct range) { ptr, MIN(len, MAXLEN) };
    ret = hook_access(h, r, 1, false, b, &faults, &why);
    log_access(h, "read", ptr, r[0].len, ret, faults, why, 1, b[0]);
    /* a refused read does not stop the writes */

    for (int i = 0; i < 2; i++) {
        r[i] = (struct range) { w[i], 64 };
        fill_pattern(b[i], 64);
    }
    if (opt_batch) {
        ret = hook_access(h, r, 2, true, b, &faults, &why);
        for (int i = 0; i < 2; i++) {
            log_access(h, "write", w[i], 64, ret, faults, why, 2, b[i]);
        }
    } else {
        for (int i = 0; i < 2; i++) {
            ret = hook_access(h, &r[i], 1, true, &b[i], &faults, &why);
            log_access(h, "write", w[i], 64, ret, faults, why, 1, b[i]);
        }
    }
}

static void hook_run(unsigned int vcpu, uint64_t pc, uint64_t addr,
                     uint64_t len, bool user)
{
    Hook h;

    if (getenv("PAGEIN_DEBUG")) {
        logf_("DEBUG hook pc=0x%" PRIx64 " addr=0x%" PRIx64 " len=%" PRIu64
              " vcpu=%u sp=0x%" PRIx64 "\n", pc, addr, len, vcpu, rreg(R_SP));
    }
    if (user && addr == 0 && len == 0) {
        /* guest/pagetest.c marks the start of each test */
        logf_("PAGEIN-TEST-BOUNDARY vcpu=%u\n", vcpu);
        return;
    }
    hook_begin(&h, vcpu, pc, user);
    if (opt_multi) {
        body_multi(&h, addr);
    } else {
        body_single(&h, addr, len);
    }
    hook_end(&h);
    __atomic_fetch_add(&n_access, 1, __ATOMIC_RELAXED);
}

static void cb_marker(unsigned int vcpu, void *ud)
{
    hook_run(vcpu, (uintptr_t)ud, rreg(R_ARG0), rreg(R_ARG1), true);
}

static void cb_kwrite(unsigned int vcpu, void *ud)
{
    if (rreg(R_ARG0) != kfd) {
        return;
    }
    hook_run(vcpu, kwrite, rreg(R_ARG1), rreg(R_ARG2), false);
}

static uint64_t pt_regs_pc(uint64_t regs_ptr)
{
    return rmemw(regs_ptr + regs_pc_off);
}

/* Some kernel-hook invocation has a fault outstanding at @pc */
static bool pending_kernel_pc(uint64_t pc)
{
    bool hit = false;
    g_mutex_lock(&lock);
    for (int i = 0; i < NPEND && !hit; i++) {
        struct pend *e = &ptab[i];
        hit = e->used && !e->user && e->inflight >= 0 && e->pc == pc;
    }
    g_mutex_unlock(&lock);
    return hit;
}

/*
 * The kernel-hook invocation at @pc whose fault the current task is
 * handling: the exception runs on the task's own stack, below the hook's
 * stack pointer, so take the closest invocation above @sp.  Lock held.
 */
static struct pend *kernel_pend_for(uint64_t pc, uint64_t sp)
{
    struct pend *best = NULL;
    for (int i = 0; i < NPEND; i++) {
        struct pend *e = &ptab[i];
        if (e->used && !e->user && e->inflight >= 0 && e->pc == pc &&
            e->sp >= sp && (!best || e->sp < best->sp)) {
            best = e;
        }
    }
    return best;
}

static void cb_search(unsigned int vcpu, void *ud)
{
    if (pending_kernel_pc(rreg(R_ARG0))) {
        logf_("VEXT search_exception_tables(0x%" PRIx64 ") -> claimed\n",
              rreg(R_ARG0));
        func_return(sym_extable);
    }
}

static void cb_fixup(unsigned int vcpu, void *ud)
{
    uint64_t pc = pt_regs_pc(rreg(R_ARG0));
    struct pend *e;

    g_mutex_lock(&lock);
    e = kernel_pend_for(pc, rreg(R_SP));
    if (e) {
        e->acc[e->inflight].failed = true;
    }
    g_mutex_unlock(&lock);
    if (e) {
        logf_("VEXT fixup_exception(pc=0x%" PRIx64 ") -> fixed up in place\n",
              pc);
        func_return(1);
    }
}

static void cb_tramp(unsigned int vcpu, void *ud)
{
    uint64_t pc = 0, sp = rreg(R_SP);
    bool ours = false;

    g_mutex_lock(&lock);
    for (int i = 0; i < NPEND && !ours; i++) {
        struct pend *e = &ptab[i];
        if (e->used && !e->user && e->inflight >= 0 && e->sp == sp) {
            e->acc[e->inflight].failed = true;
            pc = e->pc;
            ours = true;
        }
    }
    g_mutex_unlock(&lock);
    if (ours) {
        logf_("VEXT fixup target reached -> back to hook pc=0x%" PRIx64 "\n",
              pc);
        qemu_plugin_set_pc(pc);
    }
}

/*
 * A user-hook invocation in this address space with a fault outstanding
 * on @a: the signal is the guest refusing our fault.
 */
static bool user_fault_ours(uint64_t a)
{
    uint64_t asid = cur_asid();
    bool ours = false;

    g_mutex_lock(&lock);
    for (int i = 0; i < NPEND && !ours; i++) {
        struct pend *e = &ptab[i];
        if (!e->used || !e->user || e->inflight < 0 || e->asid != asid) {
            continue;
        }
        struct acc *c = &e->acc[e->inflight];
        for (int j = 0; j < c->nr && !ours; j++) {
            ours = a >= c->r[j].addr && a < c->r[j].addr + c->r[j].len;
        }
        if (ours && opt_vext) {
            c->failed = true;
        }
    }
    g_mutex_unlock(&lock);
    return ours;
}

static void cb_sig(unsigned int vcpu, void *ud)
{
    uint64_t sig = rreg(R_ARG0), code = rreg(R_ARG1), a = rreg(R_ARG2);

    if (!user_fault_ours(a)) {
        return;
    }
    logf_("SIGNAL force_sig_fault(sig=%" PRIu64 ", code=%" PRIu64
          ", addr=0x%" PRIx64 ")%s\n", sig, code, a,
          opt_vext ? " -> suppressed" : "");
    if (opt_vext) {
        func_return(0);
    }
}

/*
 * force_sig_info(sig, struct siginfo *info, task): kernels before 4.20.
 * si_addr follows three ints, aligned to the pointer size.
 */
static void cb_siginfo(unsigned int vcpu, void *ud)
{
    uint64_t sig = rreg(R_ARG0), info = rreg(R_ARG1);
    uint64_t a = rmemw(info + (wide ? 16 : 12));

    if (!user_fault_ours(a)) {
        return;
    }
    logf_("SIGNAL force_sig_info(sig=%" PRIu64 ", addr=0x%" PRIx64 ")%s\n",
          sig, a, opt_vext ? " -> suppressed" : "");
    if (opt_vext) {
        func_return(0);
    }
}

static void cb_oops(unsigned int vcpu, void *ud)
{
    int n = 0;
    for (int i = 0; i < NPEND; i++) {
        n += ptab[i].used && ptab[i].inflight >= 0;
    }
    logf_("OOPS-PATH entered 0x%" PRIx64 " (faults outstanding=%d)\n",
          (uint64_t)(uintptr_t)ud, n);
}

static const uint8_t x86_marker[] = { 0x0f, 0x1f, 0x80, 0x41, 0x50, 0x47,
                                      0x45 };

/*
 * KVM: a breakpoint stands in for the instruction callbacks tb_trans()
 * would register.  The same callbacks run, with the same effects.
 */
static void vcpu_init(unsigned int vcpu);

static bool cb_bp(unsigned int vcpu, uint64_t pc, bool sw, void *ud)
{
    static bool regs_ready;

    n_bp++;
    if (!regs_ready) {
        /* QEMU runs vcpu_init callbacks only under TCG */
        vcpu_init(vcpu);
        regs_ready = true;
    }
    if (sw) {
        g_autoptr(GByteArray) b = g_byte_array_new();

        /* only an int3 right before the marker nop is ours */
        if (!opt_marker ||
            qemu_plugin_read_memory_vaddr_fault(pc + 1, b, sizeof(x86_marker),
                                                QEMU_PLUGIN_VADDR_CURRENT,
                                                false) ||
            memcmp(b->data, x86_marker, sizeof(x86_marker))) {
            return false;
        }
        cb_marker(vcpu, (void *)(uintptr_t)pc);
        return true;
    }
    if (getenv("PAGEIN_DEBUG")) {
        logf_("DEBUG bp pc=0x%" PRIx64 " arg0=0x%" PRIx64 "\n", pc,
              rreg(R_ARG0));
    }
    if (pc == kwrite) {
        cb_kwrite(vcpu, NULL);
    } else if (opt_vext && pc == sym_search) {
        cb_search(vcpu, NULL);
    } else if (opt_vext && pc == sym_fixup) {
        cb_fixup(vcpu, NULL);
    } else if (pc == sym_sig) {
        cb_sig(vcpu, NULL);
    } else if (pc == sym_siginfo) {
        cb_siginfo(vcpu, NULL);
    } else if (opt_vext && pc == sym_tramp) {
        cb_tramp(vcpu, NULL);
    }
    return true;
}

static bool is_marker(struct qemu_plugin_insn *insn)
{
    uint8_t b[8];
    size_t n = qemu_plugin_insn_data(insn, b, sizeof(b));
    uint32_t w = n == 4 ? load_n(b, 4) : 0;

    switch (arch) {
    case ARCH_X86_64:
        return n == sizeof(x86_marker) && !memcmp(b, x86_marker, n);
    case ARCH_AARCH64:
        return w == 0xd503255f;                     /* hint #42 */
    case ARCH_ARM:
        return w == 0xe18cc00c;                     /* orr r12, r12, r12 */
    case ARCH_MIPS:
        return w == 0x34005041;                     /* ori $0, $0, 0x5041 */
    case ARCH_PPC64:
        return w == 0x60005041;                     /* ori r0, r0, 0x5041 */
    case ARCH_RISCV64:
        return w == 0x54100013;                     /* addi x0, x0, 0x541 */
    case ARCH_LOONGARCH64:
        return w == 0x03550400;                     /* andi $zero, $zero, 0x541 */
    }
    return false;
}

static void hook(struct qemu_plugin_insn *insn, qemu_plugin_vcpu_udata_cb_t f,
                 void *ud)
{
    qemu_plugin_register_vcpu_insn_exec_cb(insn, f, QEMU_PLUGIN_CB_RW_REGS_PC,
                                           ud);
}

static void tb_trans(struct qemu_plugin_tb *tb)
{
    size_t n = qemu_plugin_tb_n_insns(tb);
    for (size_t i = 0; i < n; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        uint64_t pc = qemu_plugin_insn_vaddr(insn);

        if (opt_marker && is_marker(insn)) {
            hook(insn, cb_marker, (void *)(uintptr_t)pc);
        }
        if (kwrite && pc == kwrite) {
            hook(insn, cb_kwrite, NULL);
        }
        if (opt_vext && sym_search && pc == sym_search) {
            hook(insn, cb_search, NULL);
        }
        if (opt_vext && sym_fixup && pc == sym_fixup) {
            hook(insn, cb_fixup, NULL);
        }
        if (sym_sig && pc == sym_sig) {
            hook(insn, cb_sig, NULL);
        }
        if (opt_vext && sym_tramp && pc == sym_tramp) {
            hook(insn, cb_tramp, NULL);
        }
        if (sym_siginfo && pc == sym_siginfo) {
            hook(insn, cb_siginfo, NULL);
        }
        for (guint j = 0; oops_syms && j < oops_syms->len; j++) {
            if (pc == g_array_index(oops_syms, uint64_t, j)) {
                qemu_plugin_register_vcpu_insn_exec_cb(
                    insn, cb_oops, QEMU_PLUGIN_CB_R_REGS,
                    (void *)(uintptr_t)pc);
            }
        }
    }
}

static void vcpu_init(unsigned int vcpu)
{
    static const char *names[][R_NUM] = {
        [ARCH_X86_64] = { "rdi", "rsi", "rdx", "rax", "rsp", NULL,
                          "eflags", "rip", "cr3" },
        [ARCH_AARCH64] = { "x0", "x1", "x2", "x0", "sp", "x30", "cpsr",
                           "pc", "TTBR0_EL1" },
        [ARCH_ARM] = { "r0", "r1", "r2", "r0", "sp", "lr", "cpsr", "pc" },
        [ARCH_MIPS] = { "a0", "a1", "a2", "v0", "sp", "ra", "status",
                        "pc" },
        [ARCH_PPC64] = { "r3", "r4", "r5", "r3", "r1", "lr", "msr", "pc" },
        [ARCH_RISCV64] = { "a0", "a1", "a2", "a0", "sp", "ra", "sstatus",
                           "pc", "satp" },
        [ARCH_LOONGARCH64] = { "r4", "r5", "r6", "r4", "r3", "r1", NULL,
                               "pc" },
    };
    g_autoptr(GArray) rl = qemu_plugin_get_registers();

    if (arch == ARCH_MIPS && rl->len == 0) {
        /*
         * QEMU's MIPS target has no gdb XML, so the plugin register list is
         * empty.  Handles encode the gdb register number << 1 (see
         * plugins/api.c), and MIPS gdb numbering is fixed: r0-r31, status,
         * lo, hi, badvaddr, cause, pc.
         */
        static const int num[R_NUM] = { 4, 5, 6, 2, 29, 31, 32, 37 };
        for (int r = 0; r <= R_PC; r++) {
            regs[r] = GINT_TO_POINTER(num[r] << 1);
            have_reg[r] = true;
        }
        return;
    }

    for (int r = 0; r < R_NUM; r++) {
        const char *want = names[arch][r];
        for (guint i = 0; want && i < rl->len; i++) {
            qemu_plugin_reg_descriptor *d =
                &g_array_index(rl, qemu_plugin_reg_descriptor, i);
            if (getenv("PAGEIN_DEBUG") && vcpu == 0 && r == 0) {
                fprintf(stderr, "reg %u %s %p\n", i, d->name, d->handle);
            }
            /* first match: ppc also lists an SPR-table "lr" */
            if (!have_reg[r] && !strcmp(d->name, want)) {
                regs[r] = d->handle;
                have_reg[r] = true;
            }
        }
    }
}

#if QEMU_PLUGIN_VERSION >= 7
static void at_exit(void *ud)
#else
static void at_exit(qemu_plugin_id_t id, void *ud)
#endif
{
    logf_("PAGEIN-STATS accesses=%" PRIu64 " breakpoint_events=%" PRIu64
          " max_concurrent_faults=%d\n", n_access, n_bp, max_pending);
}

/* Plugin API v7 (upstream 11.1) passes userdata instead of the plugin id. */
#if QEMU_PLUGIN_VERSION >= 7
static void vcpu_init_cb(unsigned int vcpu, void *ud) { vcpu_init(vcpu); }
static void tb_trans_cb(struct qemu_plugin_tb *tb, void *ud) { tb_trans(tb); }
#define REGISTER(id) \
    (qemu_plugin_register_vcpu_init_cb(id, vcpu_init_cb, NULL), \
     qemu_plugin_register_vcpu_tb_trans_cb(id, tb_trans_cb, NULL))
#else
static void vcpu_init_cb(qemu_plugin_id_t id, unsigned int vcpu)
{
    vcpu_init(vcpu);
}
static void tb_trans_cb(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    tb_trans(tb);
}
#define REGISTER(id) \
    (qemu_plugin_register_vcpu_init_cb(id, vcpu_init_cb), \
     qemu_plugin_register_vcpu_tb_trans_cb(id, tb_trans_cb))
#endif

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv)
{
    const char *t = info->target_name;
    if (!strcmp(t, "x86_64")) {
        arch = ARCH_X86_64, regs_pc_off = 16 * 8;       /* ip */
    } else if (!strcmp(t, "aarch64")) {
        arch = ARCH_AARCH64, regs_pc_off = 32 * 8;      /* pc */
    } else if (!strcmp(t, "arm")) {
        arch = ARCH_ARM, wide = false, regs_pc_off = 15 * 4;    /* uregs[15] */
    } else if (!strcmp(t, "mips") || !strcmp(t, "mipsel")) {
        /* pad0[8], regs[32], status, hi, lo, badvaddr, cause, epc */
        arch = ARCH_MIPS, wide = false, regs_pc_off = 45 * 4;
        big_endian = !strcmp(t, "mips");
    } else if (!strcmp(t, "mips64") || !strcmp(t, "mips64el")) {
        arch = ARCH_MIPS, regs_pc_off = 37 * 8;         /* no pad0 */
        big_endian = !strcmp(t, "mips64");
    } else if (!strcmp(t, "ppc64")) {
        arch = ARCH_PPC64, regs_pc_off = 32 * 8, big_endian = true; /* nip */
    } else if (!strcmp(t, "riscv64")) {
        arch = ARCH_RISCV64, regs_pc_off = 0;           /* epc */
    } else if (!strcmp(t, "loongarch64")) {
        arch = ARCH_LOONGARCH64, regs_pc_off = 33 * 8;  /* csr_era */
    } else {
        fprintf(stderr, "pagein: unsupported target %s\n", t);
        return -1;
    }

    for (int i = 0; i < argc; i++) {
        g_auto(GStrv) kv = g_strsplit(argv[i], "=", 2);
        const char *k = kv[0], *v = kv[1] ? kv[1] : "";
        uint64_t n = g_ascii_strtoull(v, NULL, 0);

        if (!strcmp(k, "marker")) {
            qemu_plugin_bool_parse(k, v, &opt_marker);
        } else if (!strcmp(k, "fault")) {
            qemu_plugin_bool_parse(k, v, &opt_fault);
        } else if (!strcmp(k, "vext")) {
            qemu_plugin_bool_parse(k, v, &opt_vext);
        } else if (!strcmp(k, "irqguard")) {
            qemu_plugin_bool_parse(k, v, &opt_irqguard);
        } else if (!strcmp(k, "mode")) {
            opt_mode = !strcmp(v, "user") ? QEMU_PLUGIN_VADDR_USER
                                          : QEMU_PLUGIN_VADDR_CURRENT;
        } else if (!strcmp(k, "tries")) {
            max_tries = n;
        } else if (!strcmp(k, "kwrite")) {
            kwrite = n;
        } else if (!strcmp(k, "kfd")) {
            kfd = n;
        } else if (!strcmp(k, "search")) {
            sym_search = n;
        } else if (!strcmp(k, "fixup")) {
            sym_fixup = n;
        } else if (!strcmp(k, "extable")) {
            sym_extable = n;
        } else if (!strcmp(k, "sig")) {
            sym_sig = n;
        } else if (!strcmp(k, "tramp")) {
            sym_tramp = n;
        } else if (!strcmp(k, "siginfo")) {
            sym_siginfo = n;
        } else if (!strcmp(k, "op")) {
            opt_write = !strcmp(v, "write");
            opt_multi = !strcmp(v, "multi");
        } else if (!strcmp(k, "batch")) {
            qemu_plugin_bool_parse(k, v, &opt_batch);
        } else if (!strcmp(k, "wapi")) {
            opt_wdebug = !strcmp(v, "debug");
        } else if (!strcmp(k, "endian")) {
            big_endian = !strcmp(v, "big");
        } else if (!strcmp(k, "regspc")) {
            regs_pc_off = n;
        } else if (!strcmp(k, "quiet")) {
            qemu_plugin_bool_parse(k, v, &opt_quiet);
        } else if (!strcmp(k, "kvm")) {
            qemu_plugin_bool_parse(k, v, &opt_kvm);
        } else if (!strcmp(k, "oops")) {
            g_auto(GStrv) l = g_strsplit(v, ":", -1);
            oops_syms = g_array_new(false, false, sizeof(uint64_t));
            for (int j = 0; l[j]; j++) {
                uint64_t a = g_ascii_strtoull(l[j], NULL, 0);
                g_array_append_val(oops_syms, a);
            }
        } else {
            fprintf(stderr, "pagein: unknown option %s\n", k);
            return -1;
        }
    }

    if (opt_kvm) {
        uint64_t bps[] = { kwrite, opt_vext ? sym_search : 0,
                           opt_vext ? sym_fixup : 0, sym_sig, sym_siginfo,
                           opt_vext ? sym_tramp : 0 };

        if (arch != ARCH_X86_64) {
            fprintf(stderr, "pagein: kvm=on is x86_64 only\n");
            return -1;
        }
        for (size_t i = 0; i < G_N_ELEMENTS(bps); i++) {
            if (bps[i] && qemu_plugin_hw_breakpoint_insert(bps[i])) {
                fprintf(stderr, "pagein: no breakpoint for 0x%" PRIx64 "\n",
                        bps[i]);
                return -1;
            }
        }
        qemu_plugin_register_vcpu_bp_cb(id, cb_bp, NULL);
    }
    qemu_plugin_register_atexit_cb(id, at_exit, NULL);
    REGISTER(id);
    return 0;
}

