/* PANDABEGINCOMMENT
 *
 * This work is licensed under the terms of the GNU GPL, version 2.
 * See the COPYING file in the top-level directory.
 *
PANDAENDCOMMENT */
/*!
 * @file osi_isf.cpp
 * @brief Linux OSI provider driven by a Volatility 3 ISF of the guest kernel.
 *
 * Every offset and address comes from the ISF (dwarf2json over the kernel's debug vmlinux), so
 * there is no per-kernel kernelinfo and no guessing: a field the ISF lacks is a field this plugin
 * does without. The current task is a per-CPU variable read (x86's pcpu_hot.current_task or
 * current_task, arm64's __entry_task), memory mappings come from the mm's maple tree (6.1+) or its
 * mmap list, and task changes are reported from finish_task_switch.
 */
#include <cinttypes>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "isf.h"

#include "panda.h"
#include "osi/osi_types.h"
#include "osi/os_intro.h"
#include "hooks/hooks_int_fns.h"
#include "proc_start_linux/proc_start_linux.h"
#include "proc_start_linux/proc_start_linux_ppp.h"

extern "C" {
bool init_plugin(void *);
void uninit_plugin(void *);
#include "osi/osi_ext.h"
}

namespace {

#if defined(TARGET_RISCV)
constexpr bool HAVE_REG_CURRENT = true;   // current is a register (tp/sscratch), not a variable
#else
constexpr bool HAVE_REG_CURRENT = false;
#endif

isf::Profile prof;

constexpr uint64_t PAGE_SIZE_GUEST = 4096;
constexpr int MAX_TASKS = 1 << 16;
constexpr int MAX_VMAS = 1 << 16;
constexpr int MAX_PATH_DEPTH = 64;

// Offsets resolved from the ISF at load; -1 where the kernel has no such member.
struct {
    int64_t task_tasks, task_pid, task_tgid, task_comm, task_comm_size, task_mm, task_real_parent,
        task_group_leader, task_start_time;
    int64_t mm_pgd, mm_mt, mm_mmap, mm_start_brk, mm_brk, mm_start_stack;
    int64_t mt_root;
    int64_t r64_pivot, r64_slot, r64_npivot, r64_nslot;
    int64_t a64_pivot, a64_slot, a64_npivot, a64_nslot;
    int64_t vma_start, vma_end, vma_next, vma_mm, vma_flags, vma_file, vma_pgoff;
    int64_t file_dentry, dentry_parent, dentry_name;
    int64_t mod_list, mod_name, mod_name_size, mod_mem, mod_mem_size, mm_base, mm_size,
        mod_core_base, mod_core_size, mod_text;
} off;

// Symbol addresses; 0 where absent.
uint64_t sym_init_task, sym_per_cpu_offset, sym_current, sym_modules, sym_banner;
bool current_is_percpu = true;

int64_t ofs(const char *type, const char *path) { return prof.offset(type, path).value_or(-1); }
int64_t cnt(const char *type, const char *path) {
    const isf::Field *f = prof.field(type, path);
    return f ? f->count : 0;
}

/* ******************************************************************
 Guest memory
****************************************************************** */

bool read_mem(CPUState *cpu, uint64_t addr, void *buf, size_t len) {
    return panda_virtual_memory_read(cpu, (target_ulong)addr, (uint8_t *)buf, len) == 0;
}

// An unsigned integer of the given size in the guest's byte order.
bool read_uint(CPUState *cpu, uint64_t addr, size_t size, uint64_t *out) {
    uint8_t b[8] = {0};
    if (size > 8 || !read_mem(cpu, addr, b, size)) return false;
    uint64_t v = 0;
    for (size_t i = 0; i < size; i++) {
        size_t shift = prof.big_endian ? (size - 1 - i) * 8 : i * 8;
        v |= (uint64_t)b[i] << shift;
    }
    *out = v;
    return true;
}

uint64_t ptr_at(CPUState *cpu, uint64_t addr) {
    uint64_t v = 0;
    if (addr == 0 || !read_uint(cpu, addr, prof.pointer_size, &v)) return 0;
    return v;
}

// A field read; 0 on a NULL base, a missing member or unreadable memory.
uint64_t field_ptr(CPUState *cpu, uint64_t base, int64_t o) {
    return (base == 0 || o < 0) ? 0 : ptr_at(cpu, base + o);
}

uint64_t field_uint(CPUState *cpu, uint64_t base, int64_t o, size_t size) {
    uint64_t v = 0;
    if (base == 0 || o < 0 || !read_uint(cpu, base + o, size, &v)) return 0;
    return v;
}

int32_t field_int(CPUState *cpu, uint64_t base, int64_t o) {
    return (int32_t)field_uint(cpu, base, o, 4);
}

std::string read_cstr(CPUState *cpu, uint64_t addr, size_t max) {
    std::string s;
    char buf[64];
    while (addr && s.size() < max) {
        size_t n = std::min(sizeof(buf), max - s.size());
        // Byte by byte at a page end, so a short string before an unmapped page still reads.
        if (!read_mem(cpu, addr, buf, n)) {
            if (n == 1 || !read_mem(cpu, addr, buf, 1)) break;
            n = 1;
        }
        size_t len = strnlen(buf, n);
        s.append(buf, len);
        if (len < n) break;
        addr += n;
    }
    return s;
}

bool is_kernel_ptr(uint64_t p) {
    // Linux maps itself in the top half on every 64-bit target, and above PAGE_OFFSET (at least
    // 0x40000000) on 32-bit ones.
    return prof.pointer_size == 8 ? (p >> 63) != 0 : p >= 0x40000000u;
}

/* ******************************************************************
 Tasks
****************************************************************** */

uint64_t per_cpu_base(CPUState *cpu) {
    if (sym_per_cpu_offset == 0) return 0;  // !SMP: per-CPU variables are plain variables
    return ptr_at(cpu, sym_per_cpu_offset + (uint64_t)cpu->cpu_index * prof.pointer_size);
}

uint64_t current_task(CPUState *cpu) {
#if defined(TARGET_RISCV)
    // riscv keeps current in tp while in the kernel and in sscratch while in user mode (the
    // trap entry swaps them); a user tp, or a kernel tp before the swap, isn't a kernel pointer.
    CPUArchState *env = (CPUArchState *)panda_cpu_env(cpu);
    uint64_t tp = env->gpr[4];
    uint64_t ts = (env->priv != 0 /* PRV_U */ && is_kernel_ptr(tp)) ? tp : env->sscratch;
#else
    if (sym_current == 0) return 0;
    uint64_t addr = current_is_percpu ? per_cpu_base(cpu) + sym_current : sym_current;
    uint64_t ts = ptr_at(cpu, addr);
#endif
    return is_kernel_ptr(ts) ? ts : 0;
}

uint64_t task_next(CPUState *cpu, uint64_t ts) {
    uint64_t n = field_ptr(cpu, ts, off.task_tasks);
    return n ? n - off.task_tasks : 0;
}

uint64_t group_leader(CPUState *cpu, uint64_t ts) {
    uint64_t gl = field_ptr(cpu, ts, off.task_group_leader);
    return gl ? gl : ts;
}

std::vector<uint64_t> all_tasks(CPUState *cpu) {
    std::vector<uint64_t> out;
    if (sym_init_task == 0 || off.task_tasks < 0) return out;
    uint64_t ts = sym_init_task;
    do {
        out.push_back(ts);
        ts = task_next(cpu, ts);
    } while (ts && ts != sym_init_task && is_kernel_ptr(ts) && out.size() < MAX_TASKS);
    return out;
}

void fill_osiproc(CPUState *cpu, OsiProc *p, uint64_t ts) {
    memset(p, 0, sizeof(*p));
    uint64_t leader = group_leader(cpu, ts);
    uint64_t mm = field_ptr(cpu, ts, off.task_mm);
    p->taskd = leader;
    p->pgd = field_ptr(cpu, mm, off.mm_pgd);
    p->asid = p->pgd ? panda_virt_to_phys(cpu, p->pgd) : 0;
    p->pid = field_int(cpu, ts, off.task_tgid);
    p->ppid = field_int(cpu, field_ptr(cpu, ts, off.task_real_parent), off.task_tgid);
    p->name = g_strdup(read_cstr(cpu, ts + off.task_comm, off.task_comm_size).c_str());
    p->pages = NULL;
    p->create_time = field_uint(cpu, ts, off.task_start_time, 8);
}

void fill_osiprochandle(CPUState *cpu, OsiProcHandle *h, uint64_t ts) {
    uint64_t pgd = field_ptr(cpu, field_ptr(cpu, ts, off.task_mm), off.mm_pgd);
    h->asid = pgd ? panda_virt_to_phys(cpu, pgd) : 0;
    h->taskd = group_leader(cpu, ts);
}

/* ******************************************************************
 Memory mappings
****************************************************************** */

// maple_tree.h: an encoded node carries its type in bits 3-6; the root pointer also has 0b10
// (MAPLE_ROOT_NODE) in its low bits.
constexpr uint64_t MAPLE_NODE_MASK = 255;
enum { maple_dense, maple_leaf_64, maple_range_64, maple_arange_64 };

bool mt_is_node(uint64_t entry) { return (entry & 3) == 2 && entry > 4096; }

void maple_walk(CPUState *cpu, uint64_t entry, uint64_t min, uint64_t max, int depth,
                std::vector<uint64_t> &out) {
    if (depth > 16 || out.size() >= MAX_VMAS) return;
    uint64_t node = entry & ~MAPLE_NODE_MASK;
    int type = (entry >> 3) & 15;
    int64_t pivot_off, slot_off, npivot, nslot;
    if (type == maple_leaf_64 || type == maple_range_64) {
        pivot_off = off.r64_pivot; slot_off = off.r64_slot; npivot = off.r64_npivot; nslot = off.r64_nslot;
    } else if (type == maple_arange_64) {
        pivot_off = off.a64_pivot; slot_off = off.a64_slot; npivot = off.a64_npivot; nslot = off.a64_nslot;
    } else {
        return;  // dense nodes don't hold VMAs
    }
    if (pivot_off < 0 || slot_off < 0 || nslot <= 0) return;
    uint64_t lo = min;
    for (int64_t i = 0; i < nslot; i++) {
        uint64_t hi = max;
        if (i < npivot) {
            uint64_t pv = 0;
            read_uint(cpu, node + pivot_off + i * prof.pointer_size, prof.pointer_size, &pv);
            // A zero pivot past the first slot ends the node's data.
            if (pv == 0 && i > 0) break;
            hi = pv;
        }
        uint64_t slot = ptr_at(cpu, node + slot_off + i * prof.pointer_size);
        if (slot) {
            if (type == maple_leaf_64) {
                if ((slot & 3) != 2) out.push_back(slot);
            } else {
                // Children are encoded nodes without the root's internal-entry tag.
                maple_walk(cpu, slot, lo, hi, depth + 1, out);
            }
        }
        if (hi >= max) break;
        lo = hi + 1;
    }
}

// The process's VMAs in address order.
std::vector<uint64_t> process_vmas(CPUState *cpu, uint64_t ts) {
    std::vector<uint64_t> raw, out;
    uint64_t mm = field_ptr(cpu, ts, off.task_mm);
    if (mm == 0) return out;
    if (off.mm_mt >= 0) {
        uint64_t root = field_ptr(cpu, mm + off.mm_mt, off.mt_root);
        uint64_t max = prof.pointer_size == 8 ? UINT64_MAX : UINT32_MAX;
        if (mt_is_node(root)) {
            maple_walk(cpu, root, 0, max, 0, raw);
        } else if (root && (root & 3) != 2) {
            raw.push_back(root);
        }
    } else if (off.mm_mmap >= 0 && off.vma_next >= 0) {
        for (uint64_t v = field_ptr(cpu, mm, off.mm_mmap); v && raw.size() < MAX_VMAS;
             v = field_ptr(cpu, v, off.vma_next))
            raw.push_back(v);
    }
    // A slot can be stale while the tree is being changed; keep only VMAs of this mm.
    for (uint64_t v : raw) {
        if (!is_kernel_ptr(v) || field_ptr(cpu, v, off.vma_mm) != mm) continue;
        if (field_ptr(cpu, v, off.vma_start) >= field_ptr(cpu, v, off.vma_end)) continue;
        out.push_back(v);
    }
    std::sort(out.begin(), out.end(), [cpu](uint64_t a, uint64_t b) {
        return field_ptr(cpu, a, off.vma_start) < field_ptr(cpu, b, off.vma_start);
    });
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// The dentry's path from its filesystem's root (mount points aren't crossed).
std::string dentry_path(CPUState *cpu, uint64_t dentry) {
    std::vector<std::string> parts;
    for (int i = 0; dentry && i < MAX_PATH_DEPTH; i++) {
        std::string name = read_cstr(cpu, field_ptr(cpu, dentry, off.dentry_name), 256);
        uint64_t parent = field_ptr(cpu, dentry, off.dentry_parent);
        if (parent == dentry || parent == 0) break;
        parts.push_back(name);
        dentry = parent;
    }
    std::string path;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) path += "/" + *it;
    return path.empty() ? "/" : path;
}

enum : uint32_t {
    MAPPING_FILE = 1,
    MAPPING_HEAP = 2,
    MAPPING_STACK = 4,
    MAPPING_UNKNOWN = 8,
    MAPPING_ALL = 15,
};

struct MmInfo {
    uint64_t start_brk, brk, start_stack;
};

MmInfo mm_info(CPUState *cpu, uint64_t ts) {
    uint64_t mm = field_ptr(cpu, ts, off.task_mm);
    return {field_ptr(cpu, mm, off.mm_start_brk), field_ptr(cpu, mm, off.mm_brk),
            field_ptr(cpu, mm, off.mm_start_stack)};
}

// Fills m from the VMA when it is one of the types asked for; the same classes as osi_linux.
bool fill_osimodule(CPUState *cpu, OsiModule *m, uint64_t vma, const MmInfo &mi, uint32_t types) {
    memset(m, 0, sizeof(*m));
    uint64_t start = field_ptr(cpu, vma, off.vma_start), end = field_ptr(cpu, vma, off.vma_end);
    uint64_t file = field_ptr(cpu, vma, off.vma_file);
    if (file) {
        if (!(types & MAPPING_FILE)) return false;
        std::string path = dentry_path(cpu, field_ptr(cpu, file, off.file_dentry));
        m->file = g_strdup(path.c_str());
        m->name = g_strdup(path.substr(path.rfind('/') + 1).c_str());
        m->offset = field_ptr(cpu, vma, off.vma_pgoff) * PAGE_SIZE_GUEST;
    } else if (start <= mi.start_brk && end >= mi.brk) {
        if (!(types & MAPPING_HEAP)) return false;
        m->name = g_strdup("[heap]");
    } else if (start <= mi.start_stack && end >= mi.start_stack) {
        if (!(types & MAPPING_STACK)) return false;
        m->name = g_strdup("[stack]");
    } else {
        if (!(types & MAPPING_UNKNOWN)) return false;
        m->name = g_strdup("[???]");
    }
    m->modd = vma;
    m->base = start;
    m->size = end - start;
    m->flags = field_ptr(cpu, vma, off.vma_flags);
    return true;
}

GArray *new_module_array() {
    GArray *a = g_array_sized_new(false, false, sizeof(OsiModule), 128);
    g_array_set_clear_func(a, (GDestroyNotify)free_osimodule_contents);
    return a;
}

void get_mappings(CPUState *cpu, OsiProc *p, GArray **out, uint32_t types) {
    *out = NULL;
    if (p == NULL || p->taskd == 0) return;
    std::vector<uint64_t> vmas = process_vmas(cpu, p->taskd);
    if (vmas.empty()) return;
    MmInfo mi = mm_info(cpu, p->taskd);
    *out = new_module_array();
    OsiModule m;
    for (uint64_t v : vmas) {
        if (fill_osimodule(cpu, &m, v, mi, types)) g_array_append_val(*out, m);
    }
}

/* ******************************************************************
 PPP callbacks
****************************************************************** */

void on_get_processes(CPUState *cpu, GArray **out) {
    std::vector<uint64_t> tasks = all_tasks(cpu);
    if (tasks.empty()) { *out = NULL; return; }
    *out = g_array_sized_new(false, false, sizeof(OsiProc), tasks.size());
    g_array_set_clear_func(*out, (GDestroyNotify)free_osiproc_contents);
    OsiProc p;
    for (uint64_t ts : tasks) {
        fill_osiproc(cpu, &p, ts);
        g_array_append_val(*out, p);
    }
}

void on_get_process_handles(CPUState *cpu, GArray **out) {
    std::vector<uint64_t> tasks = all_tasks(cpu);
    if (tasks.empty()) { *out = NULL; return; }
    *out = g_array_sized_new(false, false, sizeof(OsiProcHandle), tasks.size());
    OsiProcHandle h;
    for (uint64_t ts : tasks) {
        fill_osiprochandle(cpu, &h, ts);
        g_array_append_val(*out, h);
    }
}

void on_get_current_process(CPUState *cpu, OsiProc **out) {
    uint64_t ts = current_task(cpu);
    *out = NULL;
    if (ts) {
        *out = (OsiProc *)g_malloc(sizeof(OsiProc));
        fill_osiproc(cpu, *out, ts);
    }
}

void on_get_current_process_handle(CPUState *cpu, OsiProcHandle **out) {
    uint64_t ts = current_task(cpu);
    *out = NULL;
    if (ts) {
        *out = (OsiProcHandle *)g_malloc(sizeof(OsiProcHandle));
        fill_osiprochandle(cpu, *out, ts);
    }
}

void on_get_process(CPUState *cpu, const OsiProcHandle *h, OsiProc **out) {
    *out = NULL;
    if (h && h->taskd) {
        *out = (OsiProc *)g_malloc(sizeof(OsiProc));
        fill_osiproc(cpu, *out, h->taskd);
    }
}

void on_get_process_pid(CPUState *cpu, const OsiProcHandle *h, target_pid_t *pid) {
    *pid = (h && h->taskd) ? field_int(cpu, h->taskd, off.task_tgid) : -1;
}

void on_get_process_ppid(CPUState *cpu, const OsiProcHandle *h, target_pid_t *ppid) {
    *ppid = (h && h->taskd)
        ? field_int(cpu, field_ptr(cpu, h->taskd, off.task_real_parent), off.task_tgid) : -1;
}

void on_get_current_thread(CPUState *cpu, OsiThread **out) {
    uint64_t ts = current_task(cpu);
    *out = NULL;
    if (ts) {
        *out = (OsiThread *)g_malloc(sizeof(OsiThread));
        (*out)->tid = field_int(cpu, ts, off.task_pid);
        (*out)->pid = field_int(cpu, ts, off.task_tgid);
    }
}

void on_get_proc_mem(CPUState *cpu, const OsiProc *p, OsiProcMem **out) {
    static OsiProcMem pm;
    *out = NULL;
    if (p == NULL || p->taskd == 0) return;
    MmInfo mi = mm_info(cpu, p->taskd);
    if (mi.start_brk == 0 || mi.brk == 0) return;
    pm.start_brk = mi.start_brk;
    pm.brk = mi.brk;
    *out = &pm;
}

void on_get_mappings(CPUState *cpu, OsiProc *p, GArray **out) { get_mappings(cpu, p, out, MAPPING_ALL); }
void on_get_file_mappings(CPUState *cpu, OsiProc *p, GArray **out) { get_mappings(cpu, p, out, MAPPING_FILE); }
void on_get_heap_mappings(CPUState *cpu, OsiProc *p, GArray **out) { get_mappings(cpu, p, out, MAPPING_HEAP); }
void on_get_stack_mappings(CPUState *cpu, OsiProc *p, GArray **out) { get_mappings(cpu, p, out, MAPPING_STACK); }
void on_get_unknown_mappings(CPUState *cpu, OsiProc *p, GArray **out) { get_mappings(cpu, p, out, MAPPING_UNKNOWN); }

void on_get_mapping_by_addr(CPUState *cpu, OsiProc *p, const target_ptr_t addr, OsiModule **out) {
    *out = NULL;
    if (p == NULL || p->taskd == 0) return;
    MmInfo mi = mm_info(cpu, p->taskd);
    for (uint64_t v : process_vmas(cpu, p->taskd)) {
        if (addr >= field_ptr(cpu, v, off.vma_start) && addr < field_ptr(cpu, v, off.vma_end)) {
            *out = (OsiModule *)g_malloc(sizeof(OsiModule));
            fill_osimodule(cpu, *out, v, mi, MAPPING_ALL);
            return;
        }
    }
}

// The first file mapping whose basename passes match, or 0.
template <typename F>
uint64_t find_file_mapping(CPUState *cpu, OsiProc *p, F match) {
    if (p == NULL || p->taskd == 0) return 0;
    uint64_t last_dentry = 0;
    for (uint64_t v : process_vmas(cpu, p->taskd)) {
        uint64_t file = field_ptr(cpu, v, off.vma_file);
        uint64_t dentry = field_ptr(cpu, file, off.file_dentry);
        if (!dentry || dentry == last_dentry) continue;
        last_dentry = dentry;
        std::string name = read_cstr(cpu, field_ptr(cpu, dentry, off.dentry_name), 256);
        if (match(name)) return v;
    }
    return 0;
}

void on_get_mapping_base_address_by_name(CPUState *cpu, OsiProc *p, const char *name, target_ptr_t *base) {
    uint64_t v = find_file_mapping(cpu, p, [name](const std::string &n) { return n == name; });
    *base = v ? field_ptr(cpu, v, off.vma_start) : 0;
}

void on_has_mapping_prefix(CPUState *cpu, OsiProc *p, const char *prefix, bool *found) {
    *found = find_file_mapping(cpu, p, [prefix](const std::string &n) {
        return n.compare(0, strlen(prefix), prefix) == 0;
    }) != 0;
}

// Loaded kernel modules, with their text section as base and size.
void on_get_modules(CPUState *cpu, GArray **out) {
    *out = NULL;
    if (sym_modules == 0 || off.mod_list < 0) return;
    *out = new_module_array();
    uint64_t node = ptr_at(cpu, sym_modules);
    for (int i = 0; node && node != sym_modules && i < 4096; i++) {
        uint64_t mod = node - off.mod_list;
        OsiModule m;
        memset(&m, 0, sizeof(m));
        m.modd = mod;
        m.name = g_strdup(read_cstr(cpu, mod + off.mod_name, off.mod_name_size).c_str());
        m.file = g_strdup(m.name);
        if (off.mod_mem >= 0) {
            uint64_t text = mod + off.mod_mem + off.mod_text * off.mod_mem_size;
            m.base = field_ptr(cpu, text, off.mm_base);
            m.size = field_uint(cpu, text, off.mm_size, 4);
        } else if (off.mod_core_base >= 0) {
            m.base = field_ptr(cpu, mod, off.mod_core_base);
            m.size = field_uint(cpu, mod, off.mod_core_size, 4);
        }
        g_array_append_val(*out, m);
        node = ptr_at(cpu, node);
    }
}

/* ******************************************************************
 Initialization
****************************************************************** */

void resolve() {
    off.task_tasks = ofs("task_struct", "tasks");
    off.task_pid = ofs("task_struct", "pid");
    off.task_tgid = ofs("task_struct", "tgid");
    off.task_comm = ofs("task_struct", "comm");
    off.task_comm_size = cnt("task_struct", "comm");
    if (off.task_comm_size <= 0) off.task_comm_size = 16;
    off.task_mm = ofs("task_struct", "mm");
    off.task_real_parent = ofs("task_struct", "real_parent");
    off.task_group_leader = ofs("task_struct", "group_leader");
    off.task_start_time = ofs("task_struct", "start_time");

    off.mm_pgd = ofs("mm_struct", "pgd");
    off.mm_mt = ofs("mm_struct", "mm_mt");
    off.mm_mmap = ofs("mm_struct", "mmap");
    off.mm_start_brk = ofs("mm_struct", "start_brk");
    off.mm_brk = ofs("mm_struct", "brk");
    off.mm_start_stack = ofs("mm_struct", "start_stack");
    off.mt_root = ofs("maple_tree", "ma_root");
    off.r64_pivot = ofs("maple_range_64", "pivot");
    off.r64_slot = ofs("maple_range_64", "slot");
    off.r64_npivot = cnt("maple_range_64", "pivot");
    off.r64_nslot = cnt("maple_range_64", "slot");
    off.a64_pivot = ofs("maple_arange_64", "pivot");
    off.a64_slot = ofs("maple_arange_64", "slot");
    off.a64_npivot = cnt("maple_arange_64", "pivot");
    off.a64_nslot = cnt("maple_arange_64", "slot");

    off.vma_start = ofs("vm_area_struct", "vm_start");
    off.vma_end = ofs("vm_area_struct", "vm_end");
    off.vma_next = ofs("vm_area_struct", "vm_next");
    off.vma_mm = ofs("vm_area_struct", "vm_mm");
    off.vma_flags = ofs("vm_area_struct", "vm_flags");
    off.vma_file = ofs("vm_area_struct", "vm_file");
    off.vma_pgoff = ofs("vm_area_struct", "vm_pgoff");
    off.file_dentry = ofs("file", "f_path.dentry");
    off.dentry_parent = ofs("dentry", "d_parent");
    off.dentry_name = ofs("dentry", "d_name.name");

    off.mod_list = ofs("module", "list");
    off.mod_name = ofs("module", "name");
    off.mod_name_size = cnt("module", "name");
    if (off.mod_name_size <= 0) off.mod_name_size = 56;
    off.mod_mem = ofs("module", "mem");
    off.mod_mem_size = prof.type_size("module_memory").value_or(0);
    off.mm_base = ofs("module_memory", "base");
    off.mm_size = ofs("module_memory", "size");
    off.mod_text = prof.enum_value("mod_mem_type", "MOD_TEXT").value_or(0);
    off.mod_core_base = ofs("module", "core_layout.base");
    off.mod_core_size = ofs("module", "core_layout.size");

    sym_init_task = prof.symbol("init_task").value_or(0);
    sym_per_cpu_offset = prof.symbol("__per_cpu_offset").value_or(0);
    sym_modules = prof.symbol("modules").value_or(0);
    sym_banner = prof.symbol("linux_banner").value_or(0);
    // The current task: a per-CPU pointer on x86 (pcpu_hot since 6.2, current_task before) and
    // arm64 (__entry_task, written at every context switch).
    if (auto hot = prof.symbol("pcpu_hot"); hot && ofs("pcpu_hot", "current_task") >= 0) {
        sym_current = *hot + ofs("pcpu_hot", "current_task");
    } else if (auto ct = prof.symbol("current_task")) {
        sym_current = *ct;
    } else if (auto et = prof.symbol("__entry_task")) {
        sym_current = *et;
    }
}

// The banner at its ISF address says whether the kernel runs where the ISF says (no KASLR slide).
void check_banner(CPUState *cpu) {
    static bool checked = false;
    static int unreadable = 0;
    if (checked || sym_banner == 0) return;
    std::string b = read_cstr(cpu, sym_banner, 128);
    if (b.empty()) {
        // Not mapped yet early in boot; still unreadable after this many address-space switches,
        // the ISF is not this kernel's (another arch or build) or the kernel moved.
        if (++unreadable == 200) {
            checked = true;
            fprintf(stderr, PLUGIN_NAME ": linux_banner at its ISF address (0x%" PRIx64 ") is unreadable; "
                    "is the ISF for this kernel (and arch)? Boot the guest with nokaslr\n", (uint64_t)sym_banner);
        }
        return;
    }
    checked = true;
    if (b.rfind("Linux version ", 0) != 0) {
        fprintf(stderr, PLUGIN_NAME ": linux_banner isn't at its ISF address; boot the guest with nokaslr\n");
    }
}

void task_switched(CPUState *cpu, TranslationBlock *tb, struct hook *h) {
    notify_task_change(cpu);
}

}  // namespace

bool init_plugin(void *self) {
    panda_arg_list *args = panda_get_args(PLUGIN_NAME);
    const char *path = panda_parse_string_req(args, "isf", "Volatility 3 ISF of the guest kernel (.json or .json.xz)");
    std::string err;
    bool ok = prof.load(path, err);
    panda_free_args(args);
    if (!ok) {
        fprintf(stderr, PLUGIN_NAME ": %s\n", err.c_str());
        return false;
    }
    resolve();
    if (sym_init_task == 0 || off.task_tasks < 0 || off.task_tgid < 0 || off.task_comm < 0) {
        fprintf(stderr, PLUGIN_NAME ": the ISF lacks init_task or task_struct's tasks/tgid/comm\n");
        return false;
    }
    printf(PLUGIN_NAME ": %s, %zu symbols, %zu types%s\n", prof.kernel.c_str(), prof.symbols.size(),
           prof.user_types.size(), (sym_current || HAVE_REG_CURRENT) ? "" : "; no per-CPU current task for this arch");

    panda_require("osi");
    assert(init_osi_api());
    PPP_REG_CB("osi", on_get_processes, on_get_processes);
    PPP_REG_CB("osi", on_get_process_handles, on_get_process_handles);
    PPP_REG_CB("osi", on_get_current_process, on_get_current_process);
    PPP_REG_CB("osi", on_get_current_process_handle, on_get_current_process_handle);
    PPP_REG_CB("osi", on_get_process, on_get_process);
    PPP_REG_CB("osi", on_get_proc_mem, on_get_proc_mem);
    PPP_REG_CB("osi", on_get_modules, on_get_modules);
    PPP_REG_CB("osi", on_get_mappings, on_get_mappings);
    PPP_REG_CB("osi", on_get_file_mappings, on_get_file_mappings);
    PPP_REG_CB("osi", on_get_heap_mappings, on_get_heap_mappings);
    PPP_REG_CB("osi", on_get_stack_mappings, on_get_stack_mappings);
    PPP_REG_CB("osi", on_get_unknown_mappings, on_get_unknown_mappings);
    PPP_REG_CB("osi", on_get_mapping_by_addr, on_get_mapping_by_addr);
    PPP_REG_CB("osi", on_get_mapping_base_address_by_name, on_get_mapping_base_address_by_name);
    PPP_REG_CB("osi", on_has_mapping_prefix, on_has_mapping_prefix);
    PPP_REG_CB("osi", on_get_current_thread, on_get_current_thread);
    PPP_REG_CB("osi", on_get_process_pid, on_get_process_pid);
    PPP_REG_CB("osi", on_get_process_ppid, on_get_process_ppid);

    panda_cb pcb = { .asid_changed = [](CPUState *cpu, uint64_t, uint64_t) -> bool {
        check_banner(cpu);
        return false;
    } };
    panda_register_callback_modes(self, PANDA_CB_ASID_CHANGED, pcb,
            panda_callback_supported_modes(PANDA_CB_ASID_CHANGED), true);

    // finish_task_switch runs first thing in the task switched to, so current is already the new task.
    if (auto fts = prof.symbol_prefix("finish_task_switch")) {
        panda_require("hooks");
        void *hooks = panda_get_plugin_by_name("hooks");
        auto add_hook = hooks ? (void (*)(struct hook *))dlsym(hooks, "add_hook") : nullptr;
        if (add_hook) {
            struct hook h = {};
            h.addr = *fts;
            h.asid = 0;
            h.type = PANDA_CB_START_BLOCK_EXEC;
            h.cb.start_block_exec = task_switched;
            h.km = MODE_KERNEL_ONLY;
            h.enabled = true;
            add_hook(&h);
        }
    }
    // An exec changes the process without a switch.
    panda_require("proc_start_linux");
    PPP_REG_CB("proc_start_linux", on_rec_auxv, [](CPUState *cpu, TranslationBlock *, struct auxv_values *) {
        notify_task_change(cpu);
    });
    return true;
}

void uninit_plugin(void *self) {}
