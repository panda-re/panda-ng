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
 * Offsets and addresses come from the ISF (dwarf2json or btf2json over the kernel, or kernel-lift
 * for a stripped one), symbols also from a System.map or kallsyms file. What the plugin can
 * answer is decided at load (caps.h): each capability's strategy for this kernel version and
 * arch, with every input it reads resolved, shape-checked and, for a kernel-lift ISF, trusted.
 * A capability without a usable strategy answers "not available" (NULL, or -1 for a pid) and
 * the others keep working.
 *
 * Before answering anything the plugin pairs the ISF with the running kernel: linux_banner at its
 * ISF address (moved by the KASLR slide, found from the kernel's pc) must be the ISF's banner, and
 * init_task's task list must be consistent. A kernel the ISF doesn't describe gets no answers.
 */
#include <cinttypes>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "caps.h"
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
#include "osi_isf_int_fns.h"
}

namespace {

#if defined(TARGET_X86_64)
constexpr osicaps::Arch ARCH = osicaps::Arch::x86_64;
#elif defined(TARGET_I386)
constexpr osicaps::Arch ARCH = osicaps::Arch::i386;
#elif defined(TARGET_AARCH64)
constexpr osicaps::Arch ARCH = osicaps::Arch::aarch64;
#elif defined(TARGET_ARM)
constexpr osicaps::Arch ARCH = osicaps::Arch::arm;
#elif defined(TARGET_MIPS) || defined(TARGET_MIPS64)
constexpr osicaps::Arch ARCH = osicaps::Arch::mips;
#elif defined(TARGET_RISCV) || defined(TARGET_RISCV64) || defined(TARGET_RISCV32)
constexpr osicaps::Arch ARCH = osicaps::Arch::riscv;
#elif defined(TARGET_PPC) || defined(TARGET_PPC64)
constexpr osicaps::Arch ARCH = osicaps::Arch::ppc;
#elif defined(TARGET_LOONGARCH64)
constexpr osicaps::Arch ARCH = osicaps::Arch::loongarch;
#else
constexpr osicaps::Arch ARCH = osicaps::Arch::unknown;
#endif

isf::Profile prof;
osicaps::Symbols syms;
osicaps::Plan plan;
std::vector<std::string> notes;

constexpr uint64_t PAGE_SIZE_GUEST = 4096;
constexpr int MAX_TASKS = 1 << 16;
constexpr int MAX_VMAS = 1 << 16;
constexpr int MAX_PATH_DEPTH = 64;

// Offsets from the plan: -1 where no chosen strategy declared the member.
struct {
    int64_t task_tasks, task_pid, task_tgid, task_comm, task_comm_size, task_mm, task_real_parent,
        task_group_leader, task_start_time, ts_sec, ts_nsec, ts_width;
    int64_t mm_pgd, mm_mt, mm_mmap, mm_start_brk, mm_brk, mm_start_stack;
    int64_t mt_root;
    int64_t r64_pivot, r64_slot, r64_npivot, r64_nslot;
    int64_t a64_pivot, a64_slot, a64_npivot, a64_nslot;
    int64_t vma_start, vma_end, vma_next, vma_mm, vma_flags, vma_file, vma_pgoff;
    int64_t file_dentry, dentry_parent, dentry_name;
    int64_t file_mnt, mount_mnt, mnt_parent, mnt_mountpoint, mnt_root;
    int64_t mod_list, mod_name, mod_name_size, mod_mem, mod_mem_size, mm_base, mm_size, mod_text,
        mod_core_base, mod_core_size;
    int64_t ti_task, thread_size;
} off;

// Capabilities, as decided by the plan (all false until the ISF is paired with the kernel).
struct {
    bool tasks, pid, name, ppid, thread_group, asid, start_time, current, mappings, mapping_files,
        mount_paths, heap_stack, modules, task_change;
} cap;

enum class Current { None, PerCpu, Global, StackArm, StackArm64, StackMips, RegRiscv } current_kind;
enum class StartTime { None, Ns, Timespec } start_kind;
enum class Mappings { None, Maple, List } map_kind;
enum class Modules { None, Mem, CoreLayout, ModuleCore } mod_kind;
bool mount_struct;  // mount_paths through struct mount (3.3+), else vfsmount

// Symbol addresses with the KASLR slide applied; 0 where absent.
uint64_t sym_init_task, sym_per_cpu_offset, sym_current, sym_modules, sym_banner, sym_kernelsp, sym_fts;

/* ******************************************************************
 Guest memory
****************************************************************** */

bool read_mem(CPUState *cpu, uint64_t addr, void *buf, size_t len) {
    return panda_virtual_memory_read(cpu, (target_ulong)addr, (uint8_t *)buf, len) == 0;
}

// An unsigned integer of the given size in the guest's byte order.
bool read_uint(CPUState *cpu, uint64_t addr, size_t size, uint64_t *out) {
    uint8_t b[8] = {0};
    if (size == 0 || size > 8 || !read_mem(cpu, addr, b, size)) return false;
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

// The task whose thread_info is at the THREAD_SIZE-aligned base of the stack sp points into.
uint64_t task_from_stack(CPUState *cpu, uint64_t sp) {
    if (!is_kernel_ptr(sp) || off.thread_size <= 0 || (off.thread_size & (off.thread_size - 1))) return 0;
    uint64_t ti = sp & ~(uint64_t)(off.thread_size - 1);
    return field_ptr(cpu, ti, off.ti_task);
}

uint64_t current_task(CPUState *cpu) {
    uint64_t ts = 0;
    switch (current_kind) {
    case Current::PerCpu:
        ts = ptr_at(cpu, per_cpu_base(cpu) + sym_current);
        break;
    case Current::Global:
        ts = ptr_at(cpu, sym_current);
        break;
    case Current::StackMips:
        // kernelsp[cpu]: the current task's stack, saved at every switch.
        ts = task_from_stack(cpu, ptr_at(cpu, sym_kernelsp + (uint64_t)cpu->cpu_index * prof.pointer_size));
        break;
#if defined(TARGET_ARM) && !defined(TARGET_AARCH64)
    case Current::StackArm: {
        // The SVC-mode sp: live in SVC mode, banked (bank 1) in every other mode.
        CPUArchState *env = (CPUArchState *)panda_cpu_env(cpu);
        uint32_t sp = (env->uncached_cpsr & 0x1f) == 0x13 ? env->regs[13] : env->banked_r13[1];
        ts = task_from_stack(cpu, sp);
        break;
    }
#endif
#if defined(TARGET_AARCH64)
    case Current::StackArm64: {
        // SP_EL1: live while at EL1 on SP_ELx, saved in sp_el[1] otherwise.
        CPUArchState *env = (CPUArchState *)panda_cpu_env(cpu);
        bool live = env->aarch64 && ((env->pstate >> 2) & 3) == 1 && (env->pstate & 1);
        ts = task_from_stack(cpu, live ? env->xregs[31] : env->sp_el[1]);
        break;
    }
#endif
#if defined(TARGET_RISCV) || defined(TARGET_RISCV64) || defined(TARGET_RISCV32)
    case Current::RegRiscv: {
        // riscv keeps current in tp while in the kernel and in sscratch while in user mode (the
        // trap entry swaps them); a user tp, or a kernel tp before the swap, isn't a kernel pointer.
        CPUArchState *env = (CPUArchState *)panda_cpu_env(cpu);
        uint64_t tp = env->gpr[4];
        ts = (env->priv != 0 /* PRV_U */ && is_kernel_ptr(tp)) ? tp : env->sscratch;
        break;
    }
#endif
    default:
        return 0;
    }
    return is_kernel_ptr(ts) ? ts : 0;
}

uint64_t task_next(CPUState *cpu, uint64_t ts) {
    uint64_t n = field_ptr(cpu, ts, off.task_tasks);
    return n ? n - off.task_tasks : 0;
}

uint64_t group_leader(CPUState *cpu, uint64_t ts) {
    uint64_t gl = cap.thread_group ? field_ptr(cpu, ts, off.task_group_leader) : 0;
    return gl ? gl : ts;
}

std::vector<uint64_t> all_tasks(CPUState *cpu) {
    std::vector<uint64_t> out;
    if (!cap.tasks) return out;
    uint64_t ts = sym_init_task;
    do {
        out.push_back(ts);
        ts = task_next(cpu, ts);
    } while (ts && ts != sym_init_task && is_kernel_ptr(ts) && out.size() < MAX_TASKS);
    return out;
}

uint64_t start_time(CPUState *cpu, uint64_t ts) {
    switch (start_kind) {
    case StartTime::Ns:
        return field_uint(cpu, ts, off.task_start_time, 8);
    case StartTime::Timespec: {
        uint64_t base = ts + off.task_start_time;
        uint64_t sec = field_uint(cpu, base, off.ts_sec, off.ts_width);
        uint64_t nsec = field_uint(cpu, base, off.ts_nsec, off.ts_width);
        return sec * 1000000000ull + nsec;
    }
    default:
        return 0;
    }
}

uint64_t task_asid(CPUState *cpu, uint64_t ts) {
    if (!cap.asid) return 0;
    uint64_t pgd = field_ptr(cpu, field_ptr(cpu, ts, off.task_mm), off.mm_pgd);
    return pgd ? panda_virt_to_phys(cpu, pgd) : 0;
}

void fill_osiproc(CPUState *cpu, OsiProc *p, uint64_t ts) {
    memset(p, 0, sizeof(*p));
    p->taskd = group_leader(cpu, ts);
    if (cap.asid) {
        p->pgd = field_ptr(cpu, field_ptr(cpu, ts, off.task_mm), off.mm_pgd);
        p->asid = p->pgd ? panda_virt_to_phys(cpu, p->pgd) : 0;
    }
    p->pid = cap.pid ? field_int(cpu, ts, off.task_tgid) : -1;
    p->ppid = cap.ppid ? field_int(cpu, field_ptr(cpu, ts, off.task_real_parent), off.task_tgid) : -1;
    p->name = cap.name ? g_strdup(read_cstr(cpu, ts + off.task_comm, off.task_comm_size).c_str()) : NULL;
    p->pages = NULL;
    p->create_time = start_time(cpu, ts);
}

void fill_osiprochandle(CPUState *cpu, OsiProcHandle *h, uint64_t ts) {
    h->asid = task_asid(cpu, ts);
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
    if (!cap.mappings) return out;
    uint64_t mm = field_ptr(cpu, ts, off.task_mm);
    if (mm == 0) return out;
    if (map_kind == Mappings::Maple) {
        uint64_t root = field_ptr(cpu, mm + off.mm_mt, off.mt_root);
        uint64_t max = prof.pointer_size == 8 ? UINT64_MAX : UINT32_MAX;
        if (mt_is_node(root)) {
            maple_walk(cpu, root, 0, max, 0, raw);
        } else if (root && (root & 3) != 2) {
            raw.push_back(root);
        }
    } else {
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

std::string dentry_name(CPUState *cpu, uint64_t dentry) {
    return read_cstr(cpu, field_ptr(cpu, dentry, off.dentry_name), 256);
}

// The file's path: from the root of the mount tree with mount_paths, else from the root of the
// file's own filesystem.
std::string file_path(CPUState *cpu, uint64_t file) {
    uint64_t dentry = field_ptr(cpu, file, off.file_dentry);
    uint64_t mnt = 0;  // struct mount, or struct vfsmount before 3.3
    if (cap.mount_paths) {
        uint64_t vfsmnt = field_ptr(cpu, file, off.file_mnt);
        mnt = vfsmnt ? vfsmnt - off.mount_mnt : 0;
    }
    std::vector<std::string> parts;
    for (int i = 0; dentry && i < MAX_PATH_DEPTH; i++) {
        uint64_t parent = field_ptr(cpu, dentry, off.dentry_parent);
        bool fs_root = parent == dentry || parent == 0;
        if (mnt && (fs_root || dentry == field_ptr(cpu, mnt + off.mount_mnt, off.mnt_root))) {
            uint64_t up = field_ptr(cpu, mnt, off.mnt_parent);
            if (up == 0 || up == mnt) break;  // the root mount
            dentry = field_ptr(cpu, mnt, off.mnt_mountpoint);
            mnt = up;
            continue;
        }
        if (fs_root) break;
        parts.push_back(dentry_name(cpu, dentry));
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
    if (!cap.heap_stack) return {0, 0, 0};
    uint64_t mm = field_ptr(cpu, ts, off.task_mm);
    return {field_ptr(cpu, mm, off.mm_start_brk), field_ptr(cpu, mm, off.mm_brk),
            field_ptr(cpu, mm, off.mm_start_stack)};
}

uint64_t vma_file(CPUState *cpu, uint64_t vma) {
    return cap.mapping_files ? field_ptr(cpu, vma, off.vma_file) : 0;
}

// Fills m from the VMA when it is one of the types asked for; the same classes as osi_linux.
bool fill_osimodule(CPUState *cpu, OsiModule *m, uint64_t vma, const MmInfo &mi, uint32_t types) {
    memset(m, 0, sizeof(*m));
    uint64_t start = field_ptr(cpu, vma, off.vma_start), end = field_ptr(cpu, vma, off.vma_end);
    uint64_t file = vma_file(cpu, vma);
    if (file) {
        if (!(types & MAPPING_FILE)) return false;
        std::string path = file_path(cpu, file);
        m->file = g_strdup(path.c_str());
        m->name = g_strdup(path.substr(path.rfind('/') + 1).c_str());
        m->offset = field_ptr(cpu, vma, off.vma_pgoff) * PAGE_SIZE_GUEST;
    } else if (cap.heap_stack && mi.brk && start <= mi.start_brk && end >= mi.brk) {
        if (!(types & MAPPING_HEAP)) return false;
        m->name = g_strdup("[heap]");
    } else if (cap.heap_stack && start <= mi.start_stack && end >= mi.start_stack) {
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

/* ******************************************************************
 Pairing the ISF with the running kernel
****************************************************************** */

enum class Pair { Pending, Ok, Mismatch } pair_state = Pair::Pending;
uint64_t slide;
int pair_tries, kernel_searches;
bool banner_seen;           // linux_banner was readable at its ISF address
std::string pair_method, pair_detail;
std::string isf_banner;     // the ISF's banner, first line

void choose();              // below

bool banner_ok(CPUState *cpu, uint64_t addr, std::string *got) {
    static const char lv[] = "Linux version ";
    char head[sizeof(lv) - 1];
    if (!read_mem(cpu, addr, head, sizeof(head))) return false;
    banner_seen = true;
    if (memcmp(head, lv, sizeof(head)) != 0 && !got) return false;
    std::string s = read_cstr(cpu, addr, 512);
    if (got) *got = s;
    std::string line = s.substr(0, s.find('\n'));
    if (!isf_banner.empty()) return line == isf_banner;
    if (line.rfind("Linux version ", 0) != 0) return false;
    return !plan.opt.version.known() || osicaps::KVer::parse(line).key() == plan.opt.version.key();
}

// init_task's tasks ring links back to it, and it is the idle task (swapper).
// Only with members the plan trusts.
bool init_task_ok(CPUState *cpu, uint64_t s) {
    int64_t t = off.task_tasks;
    uint64_t it = syms.get("init_task").value_or(0);
    if (t < 0 || !it) return true;  // nothing to check with
    it += s;
    uint64_t next = ptr_at(cpu, it + t);
    if (!is_kernel_ptr(next) || ptr_at(cpu, next + prof.pointer_size) != it + t) return false;
    int64_t c = off.task_comm;
    return c < 0 || read_cstr(cpu, it + c, 16).rfind("swapper", 0) == 0;
}

bool try_slide(CPUState *cpu, uint64_t banner, uint64_t s) {
    return (banner == 0 || banner_ok(cpu, banner + s, nullptr)) && init_task_ok(cpu, s);
}

uint64_t sym_or(const char *a, const char *b) {
    if (auto v = syms.get(a)) return *v;
    if (auto v = syms.get(b)) return *v;
    return 0;
}

void pair_done(CPUState *cpu, uint64_t s, const std::string &how) {
    slide = s;
    pair_state = Pair::Ok;
    if (!plan.opt.version.known()) {
        // Now the guest's banner gives the version: choose the strategies again with it.
        uint64_t banner = syms.get("linux_banner").value_or(0);
        osicaps::KVer v = osicaps::KVer::parse(banner ? read_cstr(cpu, banner + s, 256) : "");
        if (v.known()) {
            osicaps::Options o = plan.opt;
            o.version = v;
            o.version_source = "guest banner";
            plan.resolve(prof, syms, o);
            for (const auto &l : plan.lines()) printf(PLUGIN_NAME ": %s\n", l.c_str());
        }
    }
    pair_method = how;
    printf(PLUGIN_NAME ": paired with the running kernel (%s)%s\n", how.c_str(),
           s ? (", KASLR slide " + std::to_string((int64_t)s)).c_str() : "");
    choose();
}

void pair_fail(const std::string &why) {
    pair_state = Pair::Mismatch;
    pair_detail = why;
    fprintf(stderr, PLUGIN_NAME ": the ISF is not this kernel's: %s; OSI answers nothing\n", why.c_str());
}

// Pairs the ISF with the kernel: at the ISF's addresses, else at a KASLR slide found from the
// kernel's pc (in kernel mode only). True once paired.
bool paired(CPUState *cpu) {
    if (pair_state != Pair::Pending) return pair_state == Pair::Ok;
    uint64_t banner = syms.get("linux_banner").value_or(0);
    if (banner == 0 && !syms.get("init_task")) {
        pair_done(cpu, 0, "not verified: no linux_banner or init_task symbol");
        return true;
    }
    std::string got;
    if (banner ? banner_ok(cpu, banner, &got) && init_task_ok(cpu, 0) : init_task_ok(cpu, 0)) {
        pair_done(cpu, 0, banner ? (isf_banner.empty() ? "banner version" : "banner") + std::string(", init_task")
                                 : "init_task");
        return true;
    }
    pair_tries++;
    if (panda_in_kernel_mode(cpu)) {
        // The kernel image moved by s: [_stext, _etext] + s holds the pc.
        uint64_t pc = panda_current_pc(cpu);
        uint64_t stext = sym_or("_stext", "_text"), etext = sym_or("_etext", "__end_rodata");
        uint64_t lo, hi;
        if (stext && etext > stext) {
            lo = pc - etext;
            hi = pc - stext;
        } else {
            // No text bounds: the anchor symbol within 64 MiB of the pc.
            uint64_t anchor = banner ? banner : syms.get("init_task").value_or(pc);
            lo = pc - anchor - (64u << 20);
            hi = lo + (128u << 20);
        }
        uint64_t mask = prof.pointer_size == 8 ? UINT64_MAX : UINT32_MAX;
        for (uint64_t s = (lo & ~(PAGE_SIZE_GUEST - 1)); (int64_t)(hi - s) >= 0; s += PAGE_SIZE_GUEST) {
            if (((s & mask) != 0) && try_slide(cpu, banner, s & mask)) {
                pair_done(cpu, s & mask, "banner, init_task, KASLR slide from the pc");
                return true;
            }
        }
        // A pc outside the kernel image (a module, the vDSO) proves nothing; a few tries.
        if (++kernel_searches >= 20) {
            pair_fail(banner_seen ? "linux_banner differs from the ISF's (\"" + got.substr(0, got.find('\n')) +
                                        "\" against \"" + isf_banner + "\") and no KASLR slide fits"
                                  : "linux_banner is unreadable at its ISF address and no KASLR slide fits");
        }
    } else if (pair_tries >= 2000) {
        pair_fail("linux_banner never became readable");
    }
    return false;
}

/* ******************************************************************
 Choosing what to read
****************************************************************** */

uint64_t slid(uint64_t a) { return a && is_kernel_ptr(a) ? a + slide : a; }

void task_switched(CPUState *cpu, TranslationBlock *tb, struct hook *h) {
    notify_task_change(cpu);
}

void add_task_switch_hook() {
    static bool added = false;
    if (added || !cap.task_change) return;
    void *hooks = panda_get_plugin_by_name("hooks");
    auto add_hook = hooks ? (void (*)(struct hook *))dlsym(hooks, "add_hook") : nullptr;
    if (!add_hook) return;
    struct hook h = {};
    h.addr = sym_fts;
    h.asid = 0;
    h.type = PANDA_CB_START_BLOCK_EXEC;
    h.cb.start_block_exec = task_switched;
    h.km = MODE_KERNEL_ONLY;
    h.enabled = true;
    add_hook(&h);
    added = true;
}

// Fills off, cap and the symbols from the plan (after pairing: with the slide).
void choose() {
    auto o = [](const char *t, const char *p) { return plan.off(t, p); };
    off.task_tasks = o("task_struct", "tasks");
    off.task_pid = o("task_struct", "pid");
    off.task_tgid = o("task_struct", "tgid");
    off.task_comm = o("task_struct", "comm");
    off.task_comm_size = plan.count("task_struct", "comm");
    off.task_mm = o("task_struct", "mm");
    off.task_real_parent = o("task_struct", "real_parent");
    off.task_group_leader = o("task_struct", "group_leader");
    off.task_start_time = o("task_struct", "start_time");
    off.ts_sec = o("timespec", "tv_sec");
    off.ts_nsec = o("timespec", "tv_nsec");
    off.ts_width = plan.width("timespec", "tv_sec");

    off.mm_pgd = o("mm_struct", "pgd");
    off.mm_mt = o("mm_struct", "mm_mt");
    off.mm_mmap = o("mm_struct", "mmap");
    off.mm_start_brk = o("mm_struct", "start_brk");
    off.mm_brk = o("mm_struct", "brk");
    off.mm_start_stack = o("mm_struct", "start_stack");
    off.mt_root = o("maple_tree", "ma_root");
    off.r64_pivot = o("maple_range_64", "pivot");
    off.r64_slot = o("maple_range_64", "slot");
    off.r64_npivot = plan.count("maple_range_64", "pivot");
    off.r64_nslot = plan.count("maple_range_64", "slot");
    off.a64_pivot = o("maple_arange_64", "pivot");
    off.a64_slot = o("maple_arange_64", "slot");
    off.a64_npivot = plan.count("maple_arange_64", "pivot");
    off.a64_nslot = plan.count("maple_arange_64", "slot");

    off.vma_start = o("vm_area_struct", "vm_start");
    off.vma_end = o("vm_area_struct", "vm_end");
    off.vma_next = o("vm_area_struct", "vm_next");
    off.vma_mm = o("vm_area_struct", "vm_mm");
    off.vma_flags = o("vm_area_struct", "vm_flags");
    off.vma_file = o("vm_area_struct", "vm_file");
    off.vma_pgoff = o("vm_area_struct", "vm_pgoff");
    off.file_dentry = plan.off("file", "f_path.dentry") >= 0 ? o("file", "f_path.dentry") : o("file", "f_dentry");
    off.dentry_parent = o("dentry", "d_parent");
    off.dentry_name = o("dentry", "d_name.name");

    mount_struct = plan.strategy("mount_paths") == "struct mount";
    off.file_mnt = o("file", "f_path.mnt");
    off.mount_mnt = mount_struct ? o("mount", "mnt") : 0;
    off.mnt_parent = mount_struct ? o("mount", "mnt_parent") : o("vfsmount", "mnt_parent");
    off.mnt_mountpoint = mount_struct ? o("mount", "mnt_mountpoint") : o("vfsmount", "mnt_mountpoint");
    off.mnt_root = o("vfsmount", "mnt_root");

    off.mod_list = o("module", "list");
    off.mod_name = o("module", "name");
    off.mod_name_size = plan.count("module", "name");
    off.mod_mem = o("module", "mem");
    off.mod_mem_size = plan.size("module_memory");
    off.mm_base = o("module_memory", "base");
    off.mm_size = o("module_memory", "size");
    off.mod_text = plan.enum_value("mod_mem_type", "MOD_TEXT");
    const std::string &ms = plan.strategy("modules");
    off.mod_core_base = ms == "core_layout" ? o("module", "core_layout.base") : o("module", "module_core");
    off.mod_core_size = ms == "core_layout" ? o("module", "core_layout.size") : o("module", "core_size");

    off.ti_task = o("thread_info", "task");
    off.thread_size = plan.size("THREAD_SIZE");

    bool ok = pair_state == Pair::Ok;
    cap.tasks = ok && plan.ok("tasks");
    cap.pid = ok && plan.ok("pid");
    cap.name = ok && plan.ok("name");
    cap.ppid = ok && plan.ok("ppid");
    cap.thread_group = ok && plan.ok("thread_group");
    cap.asid = ok && plan.ok("asid");
    cap.start_time = ok && plan.ok("start_time");
    cap.current = ok && plan.ok("current");
    cap.mappings = ok && plan.ok("mappings");
    cap.mapping_files = ok && plan.ok("mapping_files");
    cap.mount_paths = ok && plan.ok("mount_paths");
    cap.heap_stack = ok && plan.ok("heap_stack");
    cap.modules = ok && plan.ok("modules");
    cap.task_change = ok && plan.ok("task_change");

    const std::string &st = plan.strategy("start_time");
    start_kind = !cap.start_time ? StartTime::None : st == "struct timespec" ? StartTime::Timespec : StartTime::Ns;
    map_kind = !cap.mappings ? Mappings::None : plan.strategy("mappings") == "maple tree" ? Mappings::Maple
                                                                                          : Mappings::List;
    mod_kind = !cap.modules ? Modules::None : ms == "mem[MOD_TEXT]" ? Modules::Mem
                                                : ms == "core_layout" ? Modules::CoreLayout : Modules::ModuleCore;

    sym_init_task = slid(plan.sym("init_task"));
    sym_per_cpu_offset = slid(plan.sym("__per_cpu_offset"));
    sym_modules = slid(plan.sym("modules"));
    sym_kernelsp = slid(plan.sym("kernelsp"));
    sym_fts = slid(plan.sym("finish_task_switch"));
    const std::string &cs = plan.strategy("current");
    current_kind = Current::None;
    sym_current = 0;
    if (cap.current) {
        if (cs == "pcpu_hot.current_task") {
            current_kind = Current::PerCpu;
            sym_current = slid(plan.sym("pcpu_hot")) + o("pcpu_hot", "current_task");
        } else if (cs == "current_task" || cs == "__entry_task") {
            current_kind = Current::PerCpu;
            sym_current = slid(plan.sym(cs));
        } else if (cs == "__current") {
            current_kind = Current::Global;
            sym_current = slid(plan.sym("__current"));
        } else if (cs == "svc sp thread_info") {
            current_kind = Current::StackArm;
        } else if (cs == "sp_el1 thread_info") {
            current_kind = Current::StackArm64;
        } else if (cs == "kernelsp thread_info") {
            current_kind = Current::StackMips;
        } else if (cs == "tp/sscratch") {
            current_kind = Current::RegRiscv;
        }
    }
    if (ok) add_task_switch_hook();
}

// Paired and the capability chosen; logs once per capability the first time it is asked for
// and isn't available.
bool want(CPUState *cpu, bool &c, const char *name) {
    if (!paired(cpu)) return false;
    if (c) return true;
    static std::vector<std::string> told;
    if (std::find(told.begin(), told.end(), name) == told.end()) {
        told.push_back(name);
        fprintf(stderr, PLUGIN_NAME ": %s is not available: %s\n", name, plan.why(name).c_str());
    }
    return false;
}

void get_mappings(CPUState *cpu, OsiProc *p, GArray **out, uint32_t types) {
    *out = NULL;
    if (!want(cpu, cap.mappings, "mappings") || p == NULL || p->taskd == 0) return;
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
    *out = NULL;
    if (!want(cpu, cap.tasks, "tasks")) return;
    std::vector<uint64_t> tasks = all_tasks(cpu);
    if (tasks.empty()) return;
    *out = g_array_sized_new(false, false, sizeof(OsiProc), tasks.size());
    g_array_set_clear_func(*out, (GDestroyNotify)free_osiproc_contents);
    OsiProc p;
    for (uint64_t ts : tasks) {
        fill_osiproc(cpu, &p, ts);
        g_array_append_val(*out, p);
    }
}

void on_get_process_handles(CPUState *cpu, GArray **out) {
    *out = NULL;
    if (!want(cpu, cap.tasks, "tasks")) return;
    std::vector<uint64_t> tasks = all_tasks(cpu);
    if (tasks.empty()) return;
    *out = g_array_sized_new(false, false, sizeof(OsiProcHandle), tasks.size());
    OsiProcHandle h;
    for (uint64_t ts : tasks) {
        fill_osiprochandle(cpu, &h, ts);
        g_array_append_val(*out, h);
    }
}

void on_get_current_process(CPUState *cpu, OsiProc **out) {
    *out = NULL;
    if (!want(cpu, cap.current, "current")) return;
    uint64_t ts = current_task(cpu);
    if (ts) {
        *out = (OsiProc *)g_malloc(sizeof(OsiProc));
        fill_osiproc(cpu, *out, ts);
    }
}

void on_get_current_process_handle(CPUState *cpu, OsiProcHandle **out) {
    *out = NULL;
    if (!want(cpu, cap.current, "current")) return;
    uint64_t ts = current_task(cpu);
    if (ts) {
        *out = (OsiProcHandle *)g_malloc(sizeof(OsiProcHandle));
        fill_osiprochandle(cpu, *out, ts);
    }
}

void on_get_process(CPUState *cpu, const OsiProcHandle *h, OsiProc **out) {
    *out = NULL;
    if (paired(cpu) && h && h->taskd) {
        *out = (OsiProc *)g_malloc(sizeof(OsiProc));
        fill_osiproc(cpu, *out, h->taskd);
    }
}

void on_get_process_pid(CPUState *cpu, const OsiProcHandle *h, target_pid_t *pid) {
    *pid = (want(cpu, cap.pid, "pid") && h && h->taskd) ? field_int(cpu, h->taskd, off.task_tgid) : -1;
}

void on_get_process_ppid(CPUState *cpu, const OsiProcHandle *h, target_pid_t *ppid) {
    *ppid = (want(cpu, cap.ppid, "ppid") && h && h->taskd)
        ? field_int(cpu, field_ptr(cpu, h->taskd, off.task_real_parent), off.task_tgid) : -1;
}

void on_get_current_thread(CPUState *cpu, OsiThread **out) {
    *out = NULL;
    if (!want(cpu, cap.current, "current") || !want(cpu, cap.pid, "pid")) return;
    uint64_t ts = current_task(cpu);
    if (ts) {
        *out = (OsiThread *)g_malloc(sizeof(OsiThread));
        (*out)->tid = field_int(cpu, ts, off.task_pid);
        (*out)->pid = field_int(cpu, ts, off.task_tgid);
    }
}

void on_get_proc_mem(CPUState *cpu, const OsiProc *p, OsiProcMem **out) {
    static OsiProcMem pm;
    *out = NULL;
    if (!want(cpu, cap.heap_stack, "heap_stack") || p == NULL || p->taskd == 0) return;
    MmInfo mi = mm_info(cpu, p->taskd);
    if (mi.start_brk == 0 || mi.brk == 0) return;
    pm.start_brk = mi.start_brk;
    pm.brk = mi.brk;
    *out = &pm;
}

void on_get_mappings(CPUState *cpu, OsiProc *p, GArray **out) { get_mappings(cpu, p, out, MAPPING_ALL); }
void on_get_file_mappings(CPUState *cpu, OsiProc *p, GArray **out) {
    *out = NULL;
    if (want(cpu, cap.mapping_files, "mapping_files")) get_mappings(cpu, p, out, MAPPING_FILE);
}
void on_get_heap_mappings(CPUState *cpu, OsiProc *p, GArray **out) {
    *out = NULL;
    if (want(cpu, cap.heap_stack, "heap_stack")) get_mappings(cpu, p, out, MAPPING_HEAP);
}
void on_get_stack_mappings(CPUState *cpu, OsiProc *p, GArray **out) {
    *out = NULL;
    if (want(cpu, cap.heap_stack, "heap_stack")) get_mappings(cpu, p, out, MAPPING_STACK);
}
void on_get_unknown_mappings(CPUState *cpu, OsiProc *p, GArray **out) { get_mappings(cpu, p, out, MAPPING_UNKNOWN); }

void on_get_mapping_by_addr(CPUState *cpu, OsiProc *p, const target_ptr_t addr, OsiModule **out) {
    *out = NULL;
    if (!want(cpu, cap.mappings, "mappings") || p == NULL || p->taskd == 0) return;
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
    if (!want(cpu, cap.mapping_files, "mapping_files") || p == NULL || p->taskd == 0) return 0;
    uint64_t last_dentry = 0;
    for (uint64_t v : process_vmas(cpu, p->taskd)) {
        uint64_t dentry = field_ptr(cpu, vma_file(cpu, v), off.file_dentry);
        if (!dentry || dentry == last_dentry) continue;
        last_dentry = dentry;
        if (match(dentry_name(cpu, dentry))) return v;
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
    if (!want(cpu, cap.modules, "modules")) return;
    *out = new_module_array();
    uint64_t node = ptr_at(cpu, sym_modules);
    for (int i = 0; node && node != sym_modules && i < 4096; i++) {
        uint64_t mod = node - off.mod_list;
        OsiModule m;
        memset(&m, 0, sizeof(m));
        m.modd = mod;
        m.name = g_strdup(read_cstr(cpu, mod + off.mod_name, off.mod_name_size).c_str());
        m.file = g_strdup(m.name);
        if (mod_kind == Modules::Mem) {
            uint64_t text = mod + off.mod_mem + off.mod_text * off.mod_mem_size;
            m.base = field_ptr(cpu, text, off.mm_base);
            m.size = field_uint(cpu, text, off.mm_size, 4);
        } else {
            m.base = field_ptr(cpu, mod, off.mod_core_base);
            m.size = field_uint(cpu, mod, off.mod_core_size, 4);
        }
        g_array_append_val(*out, m);
        node = ptr_at(cpu, node);
    }
}

std::string report_json() {
    nlohmann::json j = nlohmann::json::parse(plan.json());
    j["kernel"] = prof.kernel;
    j["banner"] = isf_banner;
    j["notes"] = notes;
    nlohmann::json p;
    p["state"] = pair_state == Pair::Ok ? "paired" : pair_state == Pair::Mismatch ? "mismatch" : "pending";
    if (pair_state == Pair::Ok) {
        p["method"] = pair_method;
        p["slide"] = (int64_t)slide;
    }
    if (!pair_detail.empty()) p["detail"] = pair_detail;
    j["pairing"] = p;
    return j.dump(1);
}

}  // namespace

/* ******************************************************************
 The plugin's own API
****************************************************************** */

// The capability report (JSON): each capability's status, strategy and inputs, and the pairing.
// The string stays valid until the next call.
const char *osi_isf_report(void) {
    static std::string s;
    s = report_json();
    return s.c_str();
}

// 2: available, 1: available from untrusted inputs, 0: not available (or the ISF isn't paired
// with the kernel yet, or doesn't match it), -1: no such capability.
int osi_isf_capability(const char *name) {
    const osicaps::CapResult *c = plan.cap(name ? name : "");
    if (!c) return -1;
    if (pair_state != Pair::Ok) return 0;
    return c->status == osicaps::Status::Available ? 2 : c->status == osicaps::Status::Untrusted ? 1 : 0;
}

bool init_plugin(void *self) {
    panda_arg_list *args = panda_get_args(PLUGIN_NAME);
    osicaps::Config cfg;
    cfg.isf = panda_parse_string_req(args, "isf", "Volatility 3 ISF of the guest kernel (.json or .json.xz)");
    cfg.provenance = panda_parse_string_opt(args, "provenance", "",
            "a kernel-lift ISF's provenance.json (default: the one its metadata names, beside the ISF)");
    cfg.symbols = panda_parse_string_opt(args, "symbols", "",
            "a System.map or /proc/kallsyms file: symbols the ISF lacks, or overrides");
    cfg.version = panda_parse_string_opt(args, "version", "",
            "the kernel version, when the ISF has no linux_banner (else the guest's banner gives it)");
    cfg.allow_untrusted = panda_parse_bool_opt(args, "allow_untrusted",
            "use a kernel-lift ISF's reference-carried and unverified members (default: trusted only)");
    cfg.thread_size = panda_parse_uint64_opt(args, "thread_size", 0,
            "THREAD_SIZE, for the stack-based current task when the ISF's is untrusted or absent "
            "(kernel-lift's modabi reads it from the kernel's code)");
    std::string report_path = panda_parse_string_opt(args, "report", "", "write the capability report (JSON) here");
    cfg.arch = osicaps::arch_name(ARCH);
    std::string err;
    bool ok = osicaps::load(cfg, prof, syms, plan.opt, notes, err);
    panda_free_args(args);
    if (!ok) {
        fprintf(stderr, PLUGIN_NAME ": %s\n", err.c_str());
        return false;
    }
    isf_banner = prof.banner.substr(0, prof.banner.find('\n'));
    plan.resolve(prof, syms, plan.opt);
    printf(PLUGIN_NAME ": %s, %zu symbols, %zu types, %s ISF, kernel %s (%s)%s\n", prof.kernel.c_str(),
           prof.symbols.size() + syms.file.size(), prof.user_types.size(), plan.isf_kind.c_str(),
           plan.opt.version.str().c_str(), plan.opt.version_source.empty() ? "unknown" : plan.opt.version_source.c_str(),
           cfg.allow_untrusted ? ", untrusted inputs allowed" : "");
    for (const auto &n : notes) printf(PLUGIN_NAME ": %s\n", n.c_str());
    for (const auto &l : plan.lines()) printf(PLUGIN_NAME ": %s\n", l.c_str());
    if (!report_path.empty()) {
        FILE *f = fopen(report_path.c_str(), "w");
        if (f) {
            fputs(report_json().c_str(), f);
            fclose(f);
        }
    }
    choose();  // nothing is available until the ISF is paired with the kernel

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

    // Pairing is tried at address-space switches (in the kernel, where the KASLR search works)
    // and at OSI calls, until it is decided.
    panda_cb pcb = { .asid_changed = [](CPUState *cpu, uint64_t, uint64_t) -> bool {
        paired(cpu);
        return false;
    } };
    panda_register_callback_modes(self, PANDA_CB_ASID_CHANGED, pcb,
            panda_callback_supported_modes(PANDA_CB_ASID_CHANGED), true);

    // finish_task_switch runs first thing in the task switched to, so current is already the new
    // task; the hook goes in once the ISF is paired (at the slid address).
    if (plan.ok("task_change")) panda_require("hooks");
    // An exec changes the process without a switch.
    panda_require("proc_start_linux");
    PPP_REG_CB("proc_start_linux", on_rec_auxv, [](CPUState *cpu, TranslationBlock *, struct auxv_values *) {
        if (cap.task_change) notify_task_change(cpu);
    });
    return true;
}

void uninit_plugin(void *self) {}
