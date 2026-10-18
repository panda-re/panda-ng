/*
 * Penguin OSI: the guest kernel's process state, read by the host
 *
 * See include/system/penguin-osi.h.  Every call keeps the kernel pages it
 * touched (their host addresses from the vCPU's TLB, or a copy when the
 * TLB does not have them) until it returns: one page lookup per page, not
 * per field.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/bswap.h"
#include "qemu/cutils.h"
#include "qemu/atomic.h"
#include "qemu/rcu.h"
#include "qemu/timer.h"
#include "hw/core/cpu.h"
#include "exec/target_page.h"
#include "system/address-spaces.h"
#include "system/memory.h"
#include "system/syscall-trap.h"
#include "system/penguin-osi.h"

#define OSI_PAGE    4096
#define OSI_PAGES   32          /* pages one call keeps */
#define OSI_COPIES  8           /* of them copied, not in the TLB */
#define OSI_DEPTH   256         /* path components walked */
#define OSI_LIST    65536       /* list nodes walked */
#define OSI_NAME    256         /* NAME_MAX + 1 */
#define OSI_PATH    4096        /* PATH_MAX */
#define OSI_MAPLE   16          /* maple tree height */
#define OSI_PIVOTS  31

#define VM_READ     0x1
#define VM_WRITE    0x2
#define VM_EXEC     0x4
#define VM_MAYSHARE 0x80
#define VM_GROWSDOWN 0x100
#define MM_CONTEXT_HAS_VSYSCALL 1

static struct penguin_osi_layout ol;
static bool ol_set;
static int okver;               /* the kernel's KV(); 0: unread */
/* a release a.b.c[.d] (2.6 has four parts; later stables' c passes 255) */
#define KV(a, b, c, d) ((a) << 26 | (b) << 18 | MIN(c, 1023) << 8 | MIN(d, 255))

typedef struct OPage {
    uint64_t page;
    const uint8_t *p;
} OPage;

typedef struct OCtx {
    CPUState *cs;
    const struct penguin_osi_layout *l;
    bool bad;                   /* a read failed: the answer is not good */
    unsigned w;                 /* the kernel's word: 8, or 4 (32-bit ARM) */
    unsigned n, copies;
    OPage last;                 /* the page read last */
    OPage pg[OSI_PAGES];
} OCtx;

static __thread uint8_t osi_copy[OSI_COPIES][OSI_PAGE];
/* penguin_osi_counters()'s */
static uint64_t osi_count[6];       /* every thread's: exact with one vCPU */
static bool osi_counting;       /* set by the first penguin_osi_counters() */
#define O_COUNT(i) do { if (osi_counting) { osi_count[i]++; } } while (0)

/*
 * The kernel's own translations, for its direct (linear) map and its
 * image: arithmetic, no page walk.  Set once the kernel has set the
 * variables they come from.
 */
typedef struct OMap {
    uint64_t lin_lo, lin_hi, lin_pa;    /* pa = va - lin_lo + lin_pa */
    uint64_t img_lo, img_hi, img_off;   /* pa = va - img_off */
    int levels;                         /* of a task's page tables */
} OMap;

static OMap omap;
static bool omap_ready;
static unsigned omap_wait;              /* calls before the next try */

/*
 * Guest RAM's host addresses, by 2 MiB chunk: fixed for the VM's life
 * (Penguin's machines do not unplug RAM).  0: not looked up; 1: not RAM.
 */
#define ORAM_SHIFT  21
#define ORAM_SLOTS  8192                /* 16 GiB of guest physical space */
static uintptr_t oram[ORAM_SLOTS];

static const uint8_t *o_ram(uint64_t pa)
{
    uint64_t chunk = pa >> ORAM_SHIFT;
    uintptr_t h;

    if (chunk >= ORAM_SLOTS) {
        return NULL;
    }
    h = qatomic_read(&oram[chunk]);
    if (!h) {
        MemoryRegionSection sec;
        uint64_t base = chunk << ORAM_SHIFT, size = 1ULL << ORAM_SHIFT;

        h = 1;
        WITH_RCU_READ_LOCK_GUARD() {
            sec = memory_region_find(get_system_memory(), base, size);
            if (sec.mr) {
                if (memory_region_is_ram(sec.mr) &&
                    !memory_region_is_ram_device(sec.mr) &&
                    sec.offset_within_address_space == base &&
                    int128_get64(sec.size) == size) {
                    h = (uintptr_t)memory_region_get_ram_ptr(sec.mr) +
                        sec.offset_within_region;
                }
                memory_region_unref(sec.mr);
            }
        }
        qatomic_set(&oram[chunk], h);
    }
    return h == 1 ? NULL : (const uint8_t *)h + (pa & ((1ULL << ORAM_SHIFT) - 1));
}

/* a kernel page's host address by the kernel's own mapping, or NULL */
static const uint8_t *o_fast(uint64_t va)
{
    if (!qatomic_read(&omap_ready)) {
        return NULL;
    }
    if (va - omap.lin_lo < omap.lin_hi - omap.lin_lo) {
        return o_ram(va - omap.lin_lo + omap.lin_pa);
    }
    if (va - omap.img_lo < omap.img_hi - omap.img_lo) {
        return o_ram(va - omap.img_off);
    }
    return NULL;
}

static void o_map(OCtx *o);
static uint32_t o_cstr(OCtx *o, uint64_t va, char *buf, uint32_t max);
static uint64_t o_opt(OCtx *o, uint64_t va, int32_t off, unsigned size);

/* the filesystems whose d_dname a name can be told from (include/uapi/linux/magic.h) */
#define OSI_SOCKFS_MAGIC        0x534F434BULL
#define OSI_PIPEFS_MAGIC        0x50495045ULL
#define OSI_ANON_INODE_FS_MAGIC 0x09041934ULL
#define OSI_TMPFS_MAGIC         0x01021994ULL
#define OSI_DMA_BUF_MAGIC       0x444d4142ULL

/*
 * A d_dname dentry's name by its filesystem's magic, where that
 * filesystem's d_dname symbol is unknown; -1 if it cannot be told
 */
static int o_dname_by_magic(OCtx *o, uint64_t dentry, const char *name,
                            uint64_t ino, char *out, int outlen)
{
    const struct penguin_osi_layout *l = o->l;
    uint64_t sb = o_opt(o, dentry, l->dentry_sb, o->w);
    uint64_t magic = sb && l->sb_magic >= 0 ? o_opt(o, sb, l->sb_magic, o->w) : 0;

    if (o->bad || !magic) {
        o->bad = false;
        return -1;
    }
    if (magic == OSI_SOCKFS_MAGIC && !l->fn_sockfs_dname) {
        return snprintf(out, outlen, "socket:[%" PRIu64 "]", ino);
    } else if (magic == OSI_PIPEFS_MAGIC && !l->fn_pipefs_dname) {
        return snprintf(out, outlen, "pipe:[%" PRIu64 "]", ino);
    } else if (magic == OSI_ANON_INODE_FS_MAGIC && !l->fn_anon_inodefs_dname) {
        return snprintf(out, outlen, "anon_inode:%s", name);
    } else if (magic == OSI_TMPFS_MAGIC && !l->fn_simple_dname) {
        return snprintf(out, outlen, "/%s (deleted)", name);    /* shmem: simple_dname */
    } else if (magic == OSI_DMA_BUF_MAGIC && !l->fn_dmabuffs_dname) {
        return snprintf(out, outlen, "/dmabuf:%s", name);
    }
    return -1;
}

/*
 * A special mapping's name ([vdso], [sigpage], ...) with neither vm_ops'
 * name nor special_mapping_name known: a file-less vma's vm_private_data
 * as a struct vm_special_mapping, if its name reads as "[...]"
 */
static int o_special_name(OCtx *o, uint64_t vma, char *name, uint32_t nlen)
{
    const struct penguin_osi_layout *l = o->l;
    uint64_t sm = o_opt(o, vma, l->vma_private, o->w);
    uint64_t nm = sm && l->special_mapping_name >= 0 ?
        o_opt(o, sm, l->special_mapping_name, o->w) : 0;
    uint32_t n = nm ? o_cstr(o, nm, name, nlen) : 0;

    if (o->bad || n < 2 || name[0] != '[' || name[n - 1] != ']') {
        o->bad = false;
        name[0] = 0;
        return -1;
    }
    return n;
}

static bool o_init(OCtx *o, CPUState *cs)
{
    o->cs = cs;
    o->l = &ol;
    o->w = ol.ptr_size == 4 ? 4 : 8;
    o->bad = false;
    o->n = o->copies = 0;
    o->last.page = 1;           /* no page */
    if (!qatomic_read(&ol_set) || !cs) {
        return false;
    }
    if (!qatomic_read(&okver) && ol.uts_release) {
        char rel[16];
        unsigned maj = 0, min = 0, sub = 0, ext = 0;

        o_cstr(o, ol.uts_release, rel, sizeof(rel));
        if (!o->bad && sscanf(rel, "%u.%u.%u.%u", &maj, &min, &sub, &ext) >= 2) {
            qatomic_set(&okver, (int)KV(maj, min, sub, ext));
        }
        o->bad = false;
    }
    if (!qatomic_read(&omap_ready)) {
        if (omap_wait) {
            omap_wait--;
        } else {
            o_map(o);
            o->bad = false;
        }
    }
    return true;
}

static const uint8_t *o_page_slow(OCtx *o, uint64_t page)
{
    const uint8_t *p = o_fast(page);

    if (p) {
        O_COUNT(0);
        return p;
    }
    for (unsigned i = 0; i < o->n; i++) {
        if (o->pg[i].page == page) {
            O_COUNT(1);
            return o->pg[i].p;
        }
    }
    p = syscall_trap_host_page(o->cs, page);
    O_COUNT(p ? 2 : 3);
    if (o->n == OSI_PAGES || (!p && o->copies == OSI_COPIES)) {
        o->n = o->copies = 0;   /* start over: only a cost */
    }
    if (!p) {
        if (cpu_memory_rw_debug(o->cs, page, osi_copy[o->copies], OSI_PAGE,
                                false) != 0) {
            return NULL;
        }
        p = osi_copy[o->copies++];
    }
    o->pg[o->n++] = (OPage){ page, p };
    return p;
}

/* the fields of a structure are mostly on one page: that one first */
static inline const uint8_t *o_page(OCtx *o, uint64_t page)
{
    const uint8_t *p;

    if (likely(o->last.page == page)) {
        O_COUNT(1);
        return o->last.p;
    }
    p = o_page_slow(o, page);
    if (p) {
        o->last = (OPage){ page, p };
    }
    return p;
}

static bool o_read(OCtx *o, uint64_t va, void *buf, size_t n)
{
    uint8_t *b = buf;

    while (n) {
        uint64_t page = va & ~(uint64_t)(OSI_PAGE - 1);
        size_t off = va - page, k = MIN(n, OSI_PAGE - off);
        const uint8_t *p = o_page(o, page);

        if (!p) {
            o->bad = true;
            return false;
        }
        memcpy(b, p + off, k);
        b += k;
        va += k;
        n -= k;
    }
    return true;
}

/* a field: @size bytes at @va + @off; 0, and bad, if it cannot be read */
static uint64_t o_u(OCtx *o, uint64_t va, int32_t off, unsigned size)
{
    uint64_t a = va + off, in = a & (OSI_PAGE - 1);
    const uint8_t *b;
    uint8_t buf[8];

    if (!va || off < 0) {
        o->bad = true;
        return 0;
    }
    if (likely(in + size <= OSI_PAGE)) {
        b = o_page(o, a - in);          /* one page: no copy */
        if (!b) {
            o->bad = true;
            return 0;
        }
        b += in;
    } else if (o_read(o, a, buf, size)) {
        b = buf;
    } else {
        o->bad = true;
        return 0;
    }
    switch (size) {
    case 1:
        return *b;
    case 2:
        return lduw_le_p(b);
    case 4:
        return (uint32_t)ldl_le_p(b);   /* a 32-bit kernel's word: not signed */
    default:
        return ldq_le_p(b);
    }
}

/* a field this kernel may not have: 0 if it has not */
static uint64_t o_opt(OCtx *o, uint64_t va, int32_t off, unsigned size)
{
    return off < 0 ? 0 : o_u(o, va, off, size);
}

#define P(o, va, f)   o_u(o, va, (o)->l->f, (o)->w)
#define U32(o, va, f) o_u(o, va, (o)->l->f, 4)

/* a kernel string, into @buf (NUL-terminated); its length */
static uint32_t o_cstr(OCtx *o, uint64_t va, char *buf, uint32_t max)
{
    uint32_t n = 0;

    while (va && n + 1 < max) {
        uint32_t k = MIN(max - 1 - n, OSI_PAGE - (va & (OSI_PAGE - 1)));
        char *z;

        if (!o_read(o, va, buf + n, k)) {
            break;
        }
        z = memchr(buf + n, 0, k);
        if (z) {
            return z - buf;
        }
        n += k;
        va += k;
    }
    buf[n] = 0;
    return n;
}

/* the vCPU's translation of @va is @pa? */
static bool o_agrees(OCtx *o, uint64_t va, uint64_t pa)
{
    TranslateForDebugResult r;

    return va && cpu_translate_for_debug(o->cs, va, &r) && r.physaddr == pa;
}

/*
 * A candidate for the direct map: right if the vCPU translates two of the
 * kernel's own direct-map addresses (its first tasks' task_structs, slab
 * memory) as it does
 */
static bool o_try_lin(OCtx *o, OMap *m, const uint64_t *va, int nva,
                      uint64_t lo, uint64_t hi, uint64_t pa0)
{
    for (int i = 0; i < nva; i++) {
        if (va[i] - lo >= hi - lo || !o_agrees(o, va[i], va[i] - lo + pa0)) {
            return false;
        }
    }
    m->lin_lo = lo;
    m->lin_hi = hi;
    m->lin_pa = pa0;
    return true;
}

/*
 * The kernel's direct map and image, once it has set them up.  The ISF
 * gives the variables the kernel keeps them in; where a kernel keeps no
 * variable (a constant of its configuration), each layout Linux has used
 * is a candidate, and the vCPU's own translation of a few kernel addresses
 * picks one.  None: no fast path (reads take the TLB), and a retry later.
 */
static void o_map(OCtx *o)
{
    const struct penguin_osi_layout *l = o->l;
    OMap m = { 0 };
    uint64_t va[2], t;
    bool ok = false;

    if (o->w == 4) {
        goto out;       /* 32-bit: no fast path; reads take the CPU's tables */
    }
    /* the first tasks after init_task: in the direct map */
    t = o_u(o, l->init_task + l->task_tasks, 0, 8);
    va[0] = t ? t - l->task_tasks : 0;
    t = o_u(o, l->init_task + l->task_tasks, 8, 8);
    va[1] = t ? t - l->task_tasks : 0;
    if (o->bad || !va[0] || va[0] == l->init_task || va[1] == l->init_task) {
        goto out;
    }
    if (l->pt_x86) {
        static const uint64_t bases[][2] = {
            { 0xffff888000000000ULL, 46 },      /* 4-level, 4.20+ */
            { 0xffff880000000000ULL, 46 },      /* 4-level, before */
            { 0xff11000000000000ULL, 52 },      /* 5-level */
        };
        uint64_t pb = l->pt_phys_base ? o_u(o, l->pt_phys_base, 0, 8) : 0;

        if (l->pt_page_offset_base) {
            uint64_t po = o_u(o, l->pt_page_offset_base, 0, 8);
            uint64_t l5 = l->pt_l5_enabled ? o_u(o, l->pt_l5_enabled, 0, 4) : 0;
            ok = !o->bad &&
                 o_try_lin(o, &m, va, 2, po, po + (1ULL << (l5 ? 52 : 46)), 0);
            m.levels = l5 ? 5 : 4;
        }
        for (int i = 0; !ok && i < ARRAY_SIZE(bases); i++) {
            ok = o_try_lin(o, &m, va, 2, bases[i][0],
                           bases[i][0] + (1ULL << bases[i][1]), 0);
            m.levels = bases[i][1] == 52 ? 5 : 4;
        }
        m.img_off = 0xffffffff80000000ULL - pb;     /* __START_KERNEL_map */
    } else {
        /*
         * AArch64: the linear map is the bottom half of the kernel's VA
         * space from 5.4, the top half before; VA_BITS is the kernel's
         * vabits_actual where it keeps one, or one of the configurations
         */
        static const int vabits[] = { 48, 39, 42, 47, 52, 36 };
        uint64_t ms = l->pt_memstart_addr ? o_u(o, l->pt_memstart_addr, 0, 8) : 0;
        uint64_t kv = l->pt_kimage_voffset ?
            o_u(o, l->pt_kimage_voffset, 0, 8) : 0;
        uint64_t va_actual = l->pt_vabits_actual ?
            o_u(o, l->pt_vabits_actual, 0, 8) : 0;

        if (o->bad || !kv) {
            goto out;
        }
        for (int i = -1; !ok && i < (int)ARRAY_SIZE(vabits); i++) {
            int b = i < 0 ? (int)va_actual : vabits[i];
            uint64_t bot = -(1ULL << b), mid = -(1ULL << (b - 1));

            if (b < 36 || b > 52) {
                continue;
            }
            ok = o_try_lin(o, &m, va, 2, bot, mid, ms) ||     /* 5.4+ */
                 o_try_lin(o, &m, va, 2, mid, 0, ms) ||       /* before */
                 /* built for 52 bits on smaller hardware: memstart_addr
                    is moved to keep __va() = pa - it + PAGE_OFFSET(52) */
                 (b < 52 && o_try_lin(o, &m, va, 2, bot, mid,
                                      ms + bot + (1ULL << 52)));
            m.levels = b > 39 ? 4 : 3;
        }
        m.img_off = kv;
    }
    if (ok && l->kimg_start && l->kimg_end > l->kimg_start &&
        o_agrees(o, l->init_task, l->init_task - m.img_off)) {
        m.img_lo = l->kimg_start;
        m.img_hi = l->kimg_end;
    }
out:
    if (!ok) {
        omap_wait = 1024;
        return;
    }
    omap = m;
    qatomic_set(&omap_ready, true);
}

bool penguin_osi_set_layout(const struct penguin_osi_layout *layout)
{
    if (qemu_target_page_size() > OSI_PAGE ||
        layout->mr64_pivots > OSI_PIVOTS || layout->ma64_pivots > OSI_PIVOTS) {
        return false;
    }
    ol = *layout;
    qatomic_set(&omap_ready, false);
    qatomic_set(&okver, 0);
    omap_wait = 0;
    qatomic_set(&ol_set, true);
    return true;
}

/* tasks */

/*
 * The containers on the list at @head, linked at @off: @fn(@opaque, task)
 * for each until it returns true.  How many it saw, or -1.
 */
static int o_list(OCtx *o, uint64_t head, int32_t off,
                  bool (*fn)(OCtx *, uint64_t, void *), void *opaque)
{
    uint64_t node = o_u(o, head, 0, o->w);
    int n = 0;

    while (node && node != head && !o->bad) {
        if (n++ >= OSI_LIST) {
            return -1;
        }
        if (fn(o, node - off, opaque)) {
            break;
        }
        node = o_u(o, node, 0, o->w);
    }
    return o->bad ? -1 : n;
}

typedef struct OFind {
    int64_t pid;
    uint64_t found;
} OFind;

static bool o_is_pid(OCtx *o, uint64_t task, void *opaque)
{
    OFind *f = opaque;

    if ((int32_t)U32(o, task, task_pid) == f->pid) {
        f->found = task;
        return true;
    }
    return false;
}

static bool o_find_thread(OCtx *o, uint64_t leader, void *opaque)
{
    const struct penguin_osi_layout *l = o->l;

    if (l->task_thread_node >= 0 && l->signal_thread_head >= 0) {
        uint64_t sig = P(o, leader, task_signal);
        o_list(o, sig + l->signal_thread_head, l->task_thread_node,
               o_is_pid, opaque);
    } else if (!o_is_pid(o, leader, opaque)) {
        o_list(o, leader + l->task_thread_group, l->task_thread_group,
               o_is_pid, opaque);
    }
    return ((OFind *)opaque)->found != 0 || o->bad;
}

uint64_t penguin_osi_task(CPUState *cs, int64_t pid)
{
    OCtx o;
    OFind f = { .pid = pid };

    if (!o_init(&o, cs) || !ol.init_task) {
        return 0;
    }
    o_list(&o, ol.init_task + ol.task_tasks, ol.task_tasks, o_find_thread, &f);
    return f.found;
}

bool penguin_osi_proc(CPUState *cs, uint64_t task, struct penguin_osi_proc *p)
{
    OCtx o;
    const struct penguin_osi_layout *l = &ol;
    uint64_t parent, cred, mm;

    memset(p, 0, sizeof(*p));
    if (!o_init(&o, cs) || !task) {
        return false;
    }
    p->task = task;
    p->pid = (int32_t)U32(&o, task, task_pid);
    p->tgid = (int32_t)U32(&o, task, task_tgid);
    parent = P(&o, task, task_real_parent);
    p->ppid = parent ? (int32_t)U32(&o, parent, task_tgid) : 0;
    p->create_time = o_opt(&o, task, l->task_start_time, 8);
    cred = P(&o, task, task_cred);
    if (cred) {
        p->uid = U32(&o, cred, cred_uid);
        p->gid = U32(&o, cred, cred_gid);
        p->euid = U32(&o, cred, cred_euid);
        p->egid = U32(&o, cred, cred_egid);
    }
    o_read(&o, task + l->task_comm, p->comm, sizeof(p->comm));
    p->comm[sizeof(p->comm) - 1] = 0;
    mm = p->mm = P(&o, task, task_mm);
    if (mm) {
        /* only reads of the task's memory need it (penguin_osi_user_read) */
        p->pgd = o_opt(&o, mm, l->mm_pgd, o.w);
        p->map_count = (int32_t)o_opt(&o, mm, l->mm_map_count, 4);
        p->start_brk = P(&o, mm, mm_start_brk);
        p->brk = P(&o, mm, mm_brk);
        p->start_stack = P(&o, mm, mm_start_stack);
        p->start_code = P(&o, mm, mm_start_code);
        p->end_code = P(&o, mm, mm_end_code);
        p->start_data = P(&o, mm, mm_start_data);
        p->end_data = P(&o, mm, mm_end_data);
        p->arg_start = P(&o, mm, mm_arg_start);
        p->arg_end = P(&o, mm, mm_arg_end);
        p->env_start = P(&o, mm, mm_env_start);
        p->env_end = P(&o, mm, mm_env_end);
        p->mmap_base = o_opt(&o, mm, l->mm_mmap_base, o.w);
        p->task_size = o_opt(&o, mm, l->mm_task_size, o.w);
    }
    return !o.bad;
}

typedef struct OArr {
    uint64_t *v;
    int n, max;
} OArr;

static bool o_collect(OCtx *o, uint64_t task, void *opaque)
{
    OArr *a = opaque;

    if (a->n < a->max) {
        a->v[a->n] = task;
    }
    a->n++;
    return false;
}

int penguin_osi_procs(CPUState *cs, uint64_t *tasks, int max)
{
    OCtx o;
    OArr a = { .v = tasks, .max = max };

    if (!o_init(&o, cs) || !ol.init_task) {
        return -1;
    }
    return o_list(&o, ol.init_task + ol.task_tasks, ol.task_tasks,
                  o_collect, &a) < 0 ? -1 : a.n;
}

/* paths: fs/d_path.c */

static bool o_prepend(char *buf, uint32_t *pos, const char *s, uint32_t n)
{
    if (n > *pos) {
        return false;
    }
    *pos -= n;
    memcpy(buf + *pos, s, n);
    return true;
}

static uint32_t o_dname(OCtx *o, uint64_t dentry, char *buf)
{
    uint32_t n = U32(o, dentry, dentry_name_len);
    uint64_t name = P(o, dentry, dentry_name_name);

    n = MIN(n, OSI_NAME - 1);
    if (!o_read(o, name, buf, n)) {
        n = 0;
    }
    buf[n] = 0;
    return n;
}

typedef struct OMatch {
    int how;
    const char *pat;
    uint32_t plen;
    int verdict;                /* -1: not decided yet */
} OMatch;

static int o_match(int how, const char *s, uint32_t n, const char *pat,
                   uint32_t plen)
{
    switch (how) {
    case PENGUIN_OSI_MATCH_EXACT:
        return n == plen && !memcmp(s, pat, plen);
    case PENGUIN_OSI_MATCH_STARTSWITH:
        return n >= plen && !memcmp(s, pat, plen);
    case PENGUIN_OSI_MATCH_ENDSWITH:
        return n >= plen && !memcmp(s + n - plen, pat, plen);
    default:
        return memmem(s, n, pat, plen) != NULL;
    }
}

/* the tail built so far (@have bytes ending at @end) already rules @m out? */
static bool o_ruled_out(OMatch *m, const char *end, uint32_t have)
{
    uint32_t k;

    if (!m || (m->how != PENGUIN_OSI_MATCH_EXACT &&
               m->how != PENGUIN_OSI_MATCH_ENDSWITH)) {
        return false;
    }
    if (m->how == PENGUIN_OSI_MATCH_EXACT && have > m->plen) {
        m->verdict = 0;
        return true;
    }
    k = MIN(have, m->plen);
    if (memcmp(end - k, m->pat + m->plen - k, k)) {
        m->verdict = 0;
        return true;
    }
    return false;
}

static bool o_root(OCtx *o, uint64_t task, uint64_t *rmnt, uint64_t *rdentry);

/*
 * Before the root is known: the path is "/<leaf><tail>" (the leaf is not
 * the root), or "/<tail>" (it is).  True if @m matches neither.
 */
static bool o_leaf_rules_out(OCtx *o, OMatch *m, uint64_t cur, uint64_t mnt,
                             char *tmp, uint32_t pos)
{
    char name[OSI_NAME], *end = tmp + OSI_PATH;
    uint32_t p = pos, k;

    if (m->how != PENGUIN_OSI_MATCH_EXACT &&
        m->how != PENGUIN_OSI_MATCH_ENDSWITH) {
        return false;
    }
    if (P(o, cur, dentry_parent) == cur || cur == P(o, mnt, vfsmount_root)) {
        return false;           /* a mount's root: the walk decides */
    }
    k = o_dname(o, cur, name);
    if (o->bad || !o_prepend(tmp, &p, name, k) || !o_prepend(tmp, &p, "/", 1) ||
        !o_ruled_out(m, end, OSI_PATH - p)) {
        m->verdict = -1;
        return false;
    }
    /* "/<tail>": the tail is still at tmp + pos */
    p = pos;
    if (o_prepend(tmp, &p, "/", 1) &&
        o_match(m->how, tmp + p, OSI_PATH - p, m->pat, m->plen)) {
        m->verdict = -1;
        return false;
    }
    m->verdict = 0;
    return true;
}

/*
 * d_path() of (@mnt, @dentry) below the root (@rmnt, @rdentry), into @out;
 * or, with @m, only whether it matches (then @out may be NULL).  With
 * @task and no @rmnt, the root is @task's, read only when the walk needs
 * it: a match the last component rules out does not.
 */
static int o_d_path_m(OCtx *o, uint64_t rmnt, uint64_t rdentry, uint64_t mnt,
                      uint64_t dentry, char *out, uint32_t outlen, OMatch *m,
                      uint64_t task)
{
    const struct penguin_osi_layout *l = o->l;
    char name[OSI_NAME], tmp[OSI_PATH];
    uint32_t pos = sizeof(tmp);
    uint64_t mount, cur, op;
    bool is_root, any = false;
    int n;

    if (!mnt || !dentry || (!m && !outlen)) {
        return -1;
    }
    is_root = P(o, dentry, dentry_parent) == dentry;
    /* a dentry with its own name: sockets, pipes, anon inodes, ... */
    op = o_opt(o, dentry, l->dentry_op, o->w);
    if (op && (!is_root || dentry != P(o, mnt, vfsmount_root))) {
        uint64_t fn = o_opt(o, op, l->dops_dname, o->w);
        if (fn) {
            uint64_t inode = P(o, dentry, dentry_inode);
            uint64_t ino = inode ? o_u(o, inode, l->inode_ino, o->w) : 0;

            if (m) {
                out = tmp;
                outlen = sizeof(tmp);
            }
            o_dname(o, dentry, name);
            if (fn == l->fn_sockfs_dname) {
                n = snprintf(out, outlen, "socket:[%" PRIu64 "]", ino);
            } else if (fn == l->fn_pipefs_dname) {
                n = snprintf(out, outlen, "pipe:[%" PRIu64 "]", ino);
            } else if (fn == l->fn_anon_inodefs_dname) {
                n = snprintf(out, outlen, "anon_inode:%s", name);
            } else if (fn == l->fn_simple_dname) {
                n = snprintf(out, outlen, "/%s (deleted)", name);
            } else if (fn == l->fn_dmabuffs_dname) {
                n = snprintf(out, outlen, "/dmabuf:%s", name);
            } else if ((n = o_dname_by_magic(o, dentry, name, ino, out, outlen)) < 0) {
                n = snprintf(out, outlen, "[d_dname 0x%" PRIx64 "]", fn);
            }
            if (o->bad) {
                return -1;
            }
            n = MIN(n, (int)outlen - 1);
            if (m) {
                m->verdict = o_match(m->how, out, n, m->pat, m->plen);
            }
            return n;
        }
    }
    /* d_unlinked(): unhashed, and not a root */
    if (!is_root && !P(o, dentry, dentry_hash_pprev)) {
        o_prepend(tmp, &pos, " (deleted)", 10);
        if (o_ruled_out(m, tmp + sizeof(tmp), sizeof(tmp) - pos)) {
            return 0;
        }
    }
    /* prepend_path(): up the dentries, and the mounts, to the root */
    mount = mnt - l->mount_mnt;
    cur = dentry;
    for (int i = 0; i < OSI_DEPTH && !o->bad; i++) {
        uint64_t parent, mroot;
        uint32_t k;

        if (!rmnt) {
            if (m && i == 0 && o_leaf_rules_out(o, m, cur, mnt, tmp, pos)) {
                return o->bad ? -1 : 0;
            }
            if (!task || !o_root(o, task, &rmnt, &rdentry)) {
                return -1;
            }
        }
        if (cur == rdentry && mnt == rmnt) {
            break;
        }
        parent = P(o, cur, dentry_parent);
        mroot = P(o, mnt, vfsmount_root);
        if (cur == mroot || parent == cur) {
            uint64_t mparent = P(o, mount, mount_parent);
            if (cur != mroot || mparent == mount) {
                break;          /* escaped its mount, or the global root */
            }
            cur = P(o, mount, mount_mountpoint);
            mount = mparent;
            mnt = mount + l->mount_mnt;
            continue;
        }
        k = o_dname(o, cur, name);
        if (!o_prepend(tmp, &pos, name, k) || !o_prepend(tmp, &pos, "/", 1)) {
            o->bad = true;      /* longer than PATH_MAX */
            break;
        }
        any = true;
        if (o_ruled_out(m, tmp + sizeof(tmp), sizeof(tmp) - pos)) {
            return o->bad ? -1 : 0;
        }
        cur = parent;
    }
    if (!any) {
        o_prepend(tmp, &pos, "/", 1);
    }
    if (o->bad) {
        return -1;
    }
    if (m) {
        m->verdict = o_match(m->how, tmp + pos, sizeof(tmp) - pos, m->pat,
                             m->plen);
        return sizeof(tmp) - pos;
    }
    n = MIN(sizeof(tmp) - pos, outlen - 1);
    memcpy(out, tmp + pos, n);
    out[n] = 0;
    return n;
}

static int o_d_path(OCtx *o, uint64_t rmnt, uint64_t rdentry, uint64_t mnt,
                    uint64_t dentry, char *out, uint32_t outlen)
{
    return o_d_path_m(o, rmnt, rdentry, mnt, dentry, out, outlen, NULL, 0);
}

static bool o_root(OCtx *o, uint64_t task, uint64_t *rmnt, uint64_t *rdentry)
{
    uint64_t fs = P(o, task, task_fs);

    *rmnt = fs ? P(o, fs, fs_root_mnt) : 0;
    *rdentry = fs ? P(o, fs, fs_root_dentry) : 0;
    return fs && !o->bad;
}

static int o_file_path(OCtx *o, uint64_t rmnt, uint64_t rdentry,
                       uint64_t file, char *out, uint32_t len)
{
    return o_d_path(o, rmnt, rdentry, P(o, file, file_mnt),
                    P(o, file, file_dentry), out, len);
}

/* fds */

static uint64_t o_fdt(OCtx *o, uint64_t task, uint32_t *max, uint64_t *arr)
{
    uint64_t files = P(o, task, task_files);
    uint64_t fdt = files ? P(o, files, files_fdt) : 0;

    *max = fdt ? MIN(U32(o, fdt, fdt_max_fds), OSI_LIST) : 0;
    *arr = fdt ? P(o, fdt, fdt_fd) : 0;
    return fdt;
}

int penguin_osi_fds(CPUState *cs, uint64_t task, int32_t start,
                    struct penguin_osi_fd *out, int max,
                    char *strs, uint32_t slen)
{
    OCtx o;
    uint64_t rmnt, rdentry, arr;
    uint32_t nfds, used = 0;
    int n = 0, k;

    if (!o_init(&o, cs) || !task) {
        return -1;
    }
    if (!o_root(&o, task, &rmnt, &rdentry)) {
        /* no root (fs_struct's not known): names up to the namespace's */
        rmnt = rdentry = 0;
        o.bad = false;
    }
    if (start == PENGUIN_OSI_AT_FDCWD) {
        uint64_t fs = P(&o, task, task_fs);
        if (max < 1 || !slen) {
            return 0;
        }
        k = o_d_path(&o, rmnt, rdentry, P(&o, fs, fs_pwd_mnt),
                     P(&o, fs, fs_pwd_dentry), strs, slen);
        if (k < 0) {
            return -1;
        }
        out[0] = (struct penguin_osi_fd){ .fd = start, .name = 0 };
        return 1;
    }
    if (start < 0 || !o_fdt(&o, task, &nfds, &arr) || o.bad) {
        return -1;
    }
    for (uint32_t fd = start; fd < nfds && n < max; fd++) {
        uint64_t file = o_u(&o, arr, fd * o.w, o.w);

        if (o.bad) {
            return -1;
        }
        if (!file) {
            continue;
        }
        if (used >= slen) {
            break;
        }
        k = o_file_path(&o, rmnt, rdentry, file, strs + used, slen - used);
        if (k < 0) {
            o.bad = false;      /* this one only */
            strs[used] = 0;
            k = 0;
        }
        out[n++] = (struct penguin_osi_fd){ .fd = fd, .name = used,
                                            .file = file };
        used += k + 1;
    }
    return n;
}

int penguin_osi_fd_path(CPUState *cs, uint64_t task, int32_t fd,
                        char *buf, uint32_t len)
{
    OCtx o;
    uint64_t rmnt, rdentry, arr, file;
    uint32_t nfds;

    if (!o_init(&o, cs) || !task || !o_root(&o, task, &rmnt, &rdentry)) {
        return -1;
    }
    if (fd == PENGUIN_OSI_AT_FDCWD) {
        uint64_t fs = P(&o, task, task_fs);
        return o_d_path(&o, rmnt, rdentry, P(&o, fs, fs_pwd_mnt),
                        P(&o, fs, fs_pwd_dentry), buf, len);
    }
    if (fd < 0 || !o_fdt(&o, task, &nfds, &arr) || (uint32_t)fd >= nfds) {
        return -1;
    }
    file = o_u(&o, arr, fd * o.w, o.w);
    return file && !o.bad ? o_file_path(&o, rmnt, rdentry, file, buf, len)
                          : -1;
}

int penguin_osi_exe_path(CPUState *cs, uint64_t task, char *buf, uint32_t len)
{
    OCtx o;
    uint64_t rmnt, rdentry, mm, exe;

    if (!o_init(&o, cs) || !task || !o_root(&o, task, &rmnt, &rdentry)) {
        return -1;
    }
    mm = P(&o, task, task_mm);
    exe = mm ? o_opt(&o, mm, ol.mm_exe_file, o.w) : 0;
    return exe && !o.bad ? o_file_path(&o, rmnt, rdentry, exe, buf, len) : -1;
}

static int o_file_match(OCtx *o, uint64_t task, uint64_t file, int how,
                        const char *pat, uint32_t plen)
{
    OMatch m = { .how = how, .pat = pat, .plen = plen, .verdict = -1 };

    if (!file || o->bad ||
        o_d_path_m(o, 0, 0, P(o, file, file_mnt), P(o, file, file_dentry),
                   NULL, 0, &m, task) < 0) {
        return -1;
    }
    return m.verdict;
}

/* the matchers' calls and host ticks, while counting */
#define O_TIMED(call) ({                                        \
    int64_t t0_ = osi_counting ? cpu_get_host_ticks() : 0;      \
    int r_ = (call);                                            \
    if (osi_counting) {                                         \
        osi_count[4]++;                                         \
        osi_count[5] += cpu_get_host_ticks() - t0_;             \
    }                                                           \
    r_; })

static int o_fd_path_match(CPUState *cs, uint64_t task, int32_t fd,
                           int how, const char *pat, uint32_t plen);
static int o_exe_match(CPUState *cs, uint64_t task, int how,
                       const char *pat, uint32_t plen);

int penguin_osi_fd_path_match(CPUState *cs, uint64_t task, int32_t fd,
                              int how, const char *pat, uint32_t plen)
{
    return O_TIMED(o_fd_path_match(cs, task, fd, how, pat, plen));
}

int penguin_osi_exe_match(CPUState *cs, uint64_t task, int how,
                          const char *pat, uint32_t plen)
{
    return O_TIMED(o_exe_match(cs, task, how, pat, plen));
}

static int o_fd_path_match(CPUState *cs, uint64_t task, int32_t fd,
                           int how, const char *pat, uint32_t plen)
{
    OCtx o;
    uint64_t arr;
    uint32_t nfds;

    if (!o_init(&o, cs) || !task || !pat || fd < 0 ||
        !o_fdt(&o, task, &nfds, &arr) || (uint32_t)fd >= nfds) {
        return -1;
    }
    return o_file_match(&o, task, o_u(&o, arr, fd * o.w, o.w), how, pat, plen);
}

static int o_exe_match(CPUState *cs, uint64_t task, int how,
                       const char *pat, uint32_t plen)
{
    OCtx o;
    uint64_t mm;

    if (!o_init(&o, cs) || !task || !pat) {
        return -1;
    }
    mm = P(&o, task, task_mm);
    return mm ? o_file_match(&o, task, o_opt(&o, mm, ol.mm_exe_file, o.w), how,
                             pat, plen)
              : -1;
}

/* mappings */

typedef struct OVmas {
    uint64_t *v;
    int n, cap;
} OVmas;

static void o_push(OVmas *vs, uint64_t vma)
{
    if (vs->n == vs->cap) {
        vs->cap = vs->cap ? vs->cap * 2 : 64;
        vs->v = g_renew(uint64_t, vs->v, vs->cap);
    }
    vs->v[vs->n++] = vma;
}

/* the entries of a maple tree node: lib/maple_tree.c's ma_data_end() */
static void o_maple(OCtx *o, uint64_t enc, uint64_t lo, uint64_t hi,
                    OVmas *vs, int depth)
{
    const struct penguin_osi_layout *l = o->l;
    uint8_t pb[OSI_PIVOTS * 8], sb[(OSI_PIVOTS + 1) * 8];
    uint64_t node = enc & ~(uint64_t)0xff, smin = lo;
    unsigned typ = (enc >> 3) & 0xf, np, end;
    int32_t poff, soff, moff;

    if (depth > OSI_MAPLE) {
        o->bad = true;
        return;
    }
    if (typ == l->maple_arange_64) {
        poff = l->ma64_pivot, soff = l->ma64_slot, moff = l->ma64_meta_end;
        np = l->ma64_pivots;
    } else if (typ == l->maple_leaf_64 || typ == l->maple_range_64) {
        poff = l->mr64_pivot, soff = l->mr64_slot, moff = l->mr64_meta_end;
        np = l->mr64_pivots;
    } else {
        o->bad = true;
        return;
    }
    if (!np || !o_read(o, node + poff, pb, np * 8) ||
        !o_read(o, node + soff, sb, (np + 1) * 8)) {
        o->bad = true;
        return;
    }
    if (typ == l->maple_arange_64 || !ldq_le_p(pb + (np - 1) * 8)) {
        end = o_u(o, node, moff, 1);
    } else if (ldq_le_p(pb + (np - 1) * 8) == hi) {
        end = np - 1;
    } else {
        end = np;
    }
    if (end > np) {
        o->bad = true;
        return;
    }
    for (unsigned i = 0; i <= end && !o->bad; i++) {
        uint64_t piv = i < np ? ldq_le_p(pb + i * 8) : 0;
        uint64_t smax = piv ? piv : hi;
        uint64_t s = ldq_le_p(sb + i * 8);

        if (s) {
            if (typ == l->maple_leaf_64) {
                o_push(vs, s);
            } else {
                o_maple(o, s, smin, smax, vs, depth + 1);
            }
        }
        smin = smax + 1;
    }
}

static void o_vmas(OCtx *o, uint64_t mm, OVmas *vs)
{
    const struct penguin_osi_layout *l = o->l;

    if (l->mm_mt_root >= 0) {
        uint64_t root = o_u(o, mm, l->mm_mt_root, 8);
        if ((root & 3) == 2 && root > 4096) {           /* xa_is_node() */
            o_maple(o, root, 0, UINT64_MAX, vs, 0);
        } else if (root) {
            o_push(vs, root);
        }
        return;
    }
    for (uint64_t v = P(o, mm, mm_mmap); v && !o->bad; v = P(o, v, vma_next)) {
        if (vs->n >= OSI_LIST) {
            o->bad = true;
            break;
        }
        o_push(vs, v);
    }
}

/* fs/proc/task_mmu.c's get_vma_name() */
/*
 * @v's /proc/<pid>/maps line; @prev_end, @prev_flags: the mapping before
 * it's (0: none)
 */
/*
 * Does /proc hide a stack's guard page (show_map's start + PAGE_SIZE)?
 * From the guard page's 2.6.36 (and its stable backports: 2.6.27.52,
 * 2.6.32.19, 2.6.34.4, 2.6.35.2) until stack_guard_gap (4.12, and the
 * stable kernels that took that, which have the symbol).  An unknown
 * release: as if it did.
 */
static bool o_hides_guard_page(const struct penguin_osi_layout *l)
{
    int v = qatomic_read(&okver);

    if (l->stack_guard_gap) {
        return false;
    }
    if (!v) {
        return true;
    }
    return (v >= KV(2, 6, 36, 0) && v < KV(4, 12, 0, 0)) ||
           (v >= KV(2, 6, 35, 2) && v < KV(2, 6, 36, 0)) ||
           (v >= KV(2, 6, 34, 4) && v < KV(2, 6, 35, 0)) ||
           (v >= KV(2, 6, 32, 19) && v < KV(2, 6, 33, 0)) ||
           (v >= KV(2, 6, 27, 52) && v < KV(2, 6, 28, 0));
}

static int o_vma(OCtx *o, uint64_t mm, uint64_t rmnt, uint64_t rdentry,
                 uint64_t v, uint64_t prev_end, uint64_t prev_flags,
                 struct penguin_osi_vma *r, char *name, uint32_t nlen)
{
    const struct penguin_osi_layout *l = o->l;
    uint64_t flags, file, vmm, ops, lo, hi;
    int n = 0;

    r->vma = v;
    r->start = P(o, v, vma_start);
    r->end = P(o, v, vma_end);
    flags = r->flags = P(o, v, vma_flags);
    /* before the stack guard gap: stack_guard_page_start() */
    if (o_hides_guard_page(l) && (flags & VM_GROWSDOWN) &&
        !(prev_end == r->start && (prev_flags & VM_GROWSDOWN))) {
        r->start += OSI_PAGE;
    }
    file = r->file = P(o, v, vma_file);
    vmm = P(o, v, vma_mm);
    r->offset = file ? P(o, v, vma_pgoff) << 12 : 0;
    r->perms[0] = flags & VM_READ ? 'r' : '-';
    r->perms[1] = flags & VM_WRITE ? 'w' : '-';
    r->perms[2] = flags & VM_EXEC ? 'x' : '-';
    r->perms[3] = flags & VM_MAYSHARE ? 's' : 'p';
    r->perms[4] = 0;
    name[0] = 0;
    if (file) {
        n = o_file_path(o, rmnt, rdentry, file, name, nlen);
        if (n < 0) {
            o->bad = false;
            n = snprintf(name, nlen, "[unknown file]");
        }
        return n;
    }
    if (l->gate_vma && v == l->gate_vma) {
        /* x86_64's gate_vma, or 32-bit ARM's vectors page */
        return snprintf(name, nlen, o->w == 4 ? "[vectors]" : "[vsyscall]");
    }
    ops = o_opt(o, v, l->vma_ops, o->w);
    if (ops) {
        uint64_t fn = o_opt(o, ops, l->vmops_name, o->w);
        if (fn && fn == l->fn_special_mapping_name) {
            uint64_t sm = P(o, v, vma_private);
            return o_cstr(o, P(o, sm, special_mapping_name), name, nlen);
        } else if (fn && fn == l->fn_gate_vma_name) {
            return snprintf(name, nlen, "[vsyscall]");
        } else if (fn) {
            return snprintf(name, nlen, "[vm_ops 0x%" PRIx64 "]", fn);
        } else if (!l->fn_special_mapping_name &&
                   (n = o_special_name(o, v, name, nlen)) > 0) {
            return n;
        }
    }
    if (!vmm) {
        return snprintf(name, nlen, "[vdso]");
    }
    /* vma_is_initial_heap(), 6.6; inclusive before */
    lo = P(o, mm, mm_start_brk);
    hi = P(o, mm, mm_brk);
    if (qatomic_read(&okver) >= KV(6, 6, 0, 0) || !qatomic_read(&okver) ?
        P(o, v, vma_start) < hi && r->end > lo :
        P(o, v, vma_start) <= hi && r->end >= lo) {
        return snprintf(name, nlen, "[heap]");
    }
    {
        uint64_t ss = P(o, mm, mm_start_stack);
        if (r->start <= ss && r->end >= ss) {
            return snprintf(name, nlen, "[stack]");
        }
    }
    return 0;
}

int penguin_osi_mappings(CPUState *cs, uint64_t task,
                         struct penguin_osi_vma *out, int max,
                         char *strs, uint32_t slen)
{
    OCtx o;
    OVmas vs = { 0 };
    uint64_t rmnt, rdentry, mm;
    uint32_t used = 0;
    int total;

    if (!o_init(&o, cs) || !task) {
        return -1;
    }
    if (!o_root(&o, task, &rmnt, &rdentry)) {
        /* no root (fs_struct's not known): names up to the namespace's */
        rmnt = rdentry = 0;
        o.bad = false;
    }
    mm = P(&o, task, task_mm);
    if (!mm) {
        return 0;
    }
    o_vmas(&o, mm, &vs);
    if (ol.gate_vma && ol.pt_x86 && ol.mm_context_flags >= 0 &&
        (o_u(&o, mm, ol.mm_context_flags, 8) & (1 << MM_CONTEXT_HAS_VSYSCALL))) {
        o_push(&vs, ol.gate_vma);
    } else if (ol.gate_vma && o.w == 4) {
        o_push(&vs, ol.gate_vma);       /* 32-bit ARM's vectors (KUSER_HELPERS) */
    }
    if (o.bad) {
        g_free(vs.v);
        return -1;
    }
    total = vs.n;
    for (int i = 0; i < vs.n && i < max; i++) {
        char name[OSI_PATH];
        uint64_t pe = i ? P(&o, vs.v[i - 1], vma_end) : 0;
        uint64_t pf = i ? P(&o, vs.v[i - 1], vma_flags) : 0;
        int k = o_vma(&o, mm, rmnt, rdentry, vs.v[i], pe, pf, &out[i], name,
                      sizeof(name));

        k = MAX(MIN(k, (int)sizeof(name) - 1), 0);
        if (used + k + 1 > slen) {
            out[i].name = slen ? slen - 1 : 0;  /* no room: "" */
            if (slen) {
                strs[slen - 1] = 0;
            }
            continue;
        }
        memcpy(strs + used, name, k);
        strs[used + k] = 0;
        out[i].name = used;
        used += k + 1;
    }
    g_free(vs.v);
    return o.bad ? -1 : total;
}

/* user memory, through the task's page tables and the direct map */

typedef struct OPt {
    uint64_t lin_lo, lin_pa;
    int levels;
} OPt;

/* the direct map, which user page tables are read through */
static bool o_pt(OCtx *o, OPt *pt)
{
    if (!qatomic_read(&omap_ready)) {
        return false;
    }
    pt->lin_lo = omap.lin_lo;
    pt->lin_pa = omap.lin_pa;
    pt->levels = omap.levels;
    return true;
}

static uint64_t o_phys_va(const OPt *pt, uint64_t pa)
{
    return pa - pt->lin_pa + pt->lin_lo;
}

static bool o_user_pa(OCtx *o, const OPt *pt, uint64_t pgd, uint64_t va,
                      uint64_t *pa)
{
    const struct penguin_osi_layout *l = o->l;
    uint64_t table = pgd - pt->lin_lo + pt->lin_pa;

    for (int lvl = pt->levels - 1; lvl >= 0; lvl--) {
        unsigned shift = 12 + 9 * lvl;
        uint64_t e = o_u(o, o_phys_va(pt, table),
                         ((va >> shift) & 511) * 8, 8);
        uint64_t frame, span = (uint64_t)1 << shift;

        if (o->bad || !(e & 1)) {
            return false;
        }
        if (l->pt_x86) {
            frame = e & 0x000ffffffffff000ULL;
            if ((lvl == 1 || lvl == 2) && (e & 0x80)) {     /* 2M, 1G */
                *pa = (frame & ~(span - 1)) + (va & (span - 1));
                return true;
            }
        } else {
            frame = e & 0x0000fffffffff000ULL;
            if (lvl && !(e & 2)) {                          /* a block */
                *pa = (frame & ~(span - 1)) + (va & (span - 1));
                return true;
            }
        }
        table = frame;
    }
    *pa = table + (va & (OSI_PAGE - 1));
    return true;
}

/*
 * A 32-bit ARM task's page, by its short-descriptor tables: the L1 entry
 * through the pgd's kernel address, then the hardware L2 table and the
 * page in guest RAM (whichever half of its page the kernel keeps them in,
 * the L1 entry points at the hardware table).  Its host address, or NULL.
 */
static const uint8_t *o_user_page32(OCtx *o, uint64_t pgd, uint32_t va)
{
    uint32_t e = o_u(o, pgd, (va >> 20) * 4, 4), pa;
    const uint8_t *t;

    if (o->bad) {
        return NULL;
    }
    switch (e & 3) {
    case 1:                                     /* a page table */
        t = o_ram(e & 0xfffffc00);
        if (!t) {
            return NULL;
        }
        e = ldl_le_p(t + ((va >> 12) & 0xff) * 4);
        if (e & 2) {                            /* a 4K small page */
            pa = (e & 0xfffff000) | (va & 0xfff);
        } else if (e & 1) {                     /* a 64K large page */
            pa = (e & 0xffff0000) | (va & 0xffff);
        } else {
            return NULL;                        /* not mapped (or not young) */
        }
        break;
    case 2:
        if (e & (1 << 18)) {                    /* a 16M supersection */
            pa = (e & 0xff000000) | (va & 0xffffff);
        } else {                                /* a 1M section */
            pa = (e & 0xfff00000) | (va & 0xfffff);
        }
        break;
    default:
        return NULL;
    }
    t = o_ram(pa & ~(uint32_t)(OSI_PAGE - 1));
    return t ? t + (pa & (OSI_PAGE - 1)) : NULL;
}

int penguin_osi_user_read(CPUState *cs, uint64_t task, uint64_t va,
                          void *buf, uint32_t len)
{
    OCtx o;
    uint64_t mm, pgd;
    uint32_t done = 0;

    OPt pt;

    if (!o_init(&o, cs) || !task) {
        return -1;
    }
    if (o.w == 4) {
        mm = P(&o, task, task_mm);
        pgd = mm ? P(&o, mm, mm_pgd) : 0;
        while (pgd && !o.bad && done < len) {
            uint32_t a = va + done;
            uint32_t k = MIN(len - done, OSI_PAGE - (a & (OSI_PAGE - 1)));
            const uint8_t *p = o_user_page32(&o, pgd, a);

            if (!p) {
                break;
            }
            memcpy((uint8_t *)buf + done, p, k);
            done += k;
        }
        return done ? (int)done : -1;
    }
    if (!o_pt(&o, &pt)) {
        return -1;
    }
    mm = P(&o, task, task_mm);
    pgd = mm ? P(&o, mm, mm_pgd) : 0;
    if (!pgd || o.bad) {
        return -1;
    }
    while (done < len) {
        uint32_t k = MIN(len - done, OSI_PAGE - ((va + done) & (OSI_PAGE - 1)));
        uint64_t pa;

        if (!o_user_pa(&o, &pt, pgd, va + done, &pa) ||
            !o_read(&o, o_phys_va(&pt, pa), (uint8_t *)buf + done, k)) {
            break;
        }
        done += k;
    }
    return done ? (int)done : -1;
}

bool penguin_osi_kread(CPUState *cs, uint64_t va, void *buf, uint32_t len)
{
    uint8_t *b = buf;

    if (!qatomic_read(&omap_ready)) {
        OCtx o;

        if (!o_init(&o, cs) || !qatomic_read(&omap_ready)) {
            return false;
        }
    }
    while (len) {
        uint32_t off = va & (OSI_PAGE - 1), k = MIN(len, OSI_PAGE - off);
        const uint8_t *p = o_fast(va - off);

        if (!p) {
            return false;
        }
        memcpy(b, p + off, k);
        b += k;
        va += k;
        len -= k;
    }
    return true;
}

void penguin_osi_counters(uint64_t *out)
{
    qatomic_set(&osi_counting, true);
    memcpy(out, osi_count, sizeof(osi_count));
    out[6] = cpu_get_host_ticks();
}
