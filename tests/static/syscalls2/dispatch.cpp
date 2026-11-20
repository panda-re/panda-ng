// Runs syscalls2's real dispatch (sc2_dispatch.inc), the generated x64 and i386 tables and the
// generated PPP functions against a stub engine, as an x86_64 target with its 64-bit ABI and
// int 0x80: the engine's wants after each subscription change, the arguments each callback type
// gets, the "every call" and context callbacks, the keep decision, unknown numbers.
#include "panda.h"
#include "panda/syscall-pair.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <mutex>
#include <vector>
#include "syscalls2.h"
#include "syscalls2_info.h"
#include "sc2_compat.h"
extern "C" {
#include "syscalls_ext_typedefs.h"
#include "generated/syscall_ppp_boilerplate_enter.cpp"
#include "generated/syscall_ppp_boilerplate_return.cpp"
}

// --- what syscalls2.cpp gives sc2_dispatch.inc ---------------------------------------------
Profile profiles[PROFILE_LAST];
bool load_info = false;
static int info_loads;
void sysinfo_load(int) { ++info_loads; }
target_ulong syscalls_get_id(CPUState *) { return 0x123000; }
syscalls2_vcpu_t syscalls2_vcpus[SYSCALLS2_VCPUS];
static bool engine_mode;
#define SC2_NB 1024
#define SC2_NABI 2
static int engine_abi[SC2_NABI] = { 10, 11 };
static ProfileType engine_profile[SC2_NABI] = { PROFILE_LINUX_X64, PROFILE_LINUX_X86_INT80 };
static uint8_t engine_want[SC2_NABI][SC2_NB + 1];
static uint64_t sc2_slow_word(CPUState *, unsigned) { return 0; }

// --- the stub engine ------------------------------------------------------------------------
static std::map<std::pair<int, int64_t>, uint32_t> want;        // (handle, nr): want
static unsigned want_calls, keeps, keep_last;
void panda_syscall_want(int abi, int64_t nr, uint32_t w) { want[{abi, nr}] = w; ++want_calls; }
void panda_syscall_keep(CPUState *, PandaSyscallCall *, bool keep) { ++keeps; keep_last = keep; }
struct Route { PandaSyscallFn fn[2]; void *ctx; };
static std::map<std::pair<int, int64_t>, Route> routes;         // (handle, index): route
void panda_syscall_route(int abi, int64_t nr, PandaSyscallFn e, PandaSyscallFn r, void *ctx) {
    routes[{abi, nr}] = Route{ { e, r }, ctx };
}


#include "sc2_dispatch.inc"

// --- calls ----------------------------------------------------------------------------------
static CPUState cpu;
static uint64_t next_id = 100;
struct Call {
    alignas(8) unsigned char mem[sizeof(PandaSyscallCall) + sizeof(sc2_data)];
    PandaSyscallCall *c = (PandaSyscallCall *)mem;
    Call(int abi, int64_t nr, std::initializer_list<uint64_t> args, bool kept = false) {
        memset(mem, 0xa5, sizeof(mem));             // stale data from an earlier call
        c->id = next_id++;
        c->nr = nr;
        unsigned i = 0;
        for (uint64_t a : args) c->args[i++] = a;
        for (; i < 6; i++) c->args[i] = 0xdead0000 + i;
        c->pc = 0x401000;
        c->len = 2;
        c->abi = abi;
        c->keytype = kept ? PANDA_TASK_KEY_REG : PANDA_TASK_KEY_NONE;
        c->vcpu = 0;
    }
    // as the engine delivers: to the number's route if there is one
    void deliver(uint32_t flags) {
        int64_t i = (uint64_t)c->nr < SC2_NB ? c->nr : SC2_NB;
        auto r = routes.find({engine_abi[c->abi], i});
        if (r != routes.end()) r->second.fn[flags & PANDA_SYSCALL_RETURN](r->second.ctx, &cpu, c, 0, flags);
        else engine_call(nullptr, &cpu, c, 0, flags);
    }
    void enter() { deliver(0); }
    void ret(uint32_t flags = PANDA_SYSCALL_RETURN) { deliver(flags); }
    const syscall_ctx_t *ctx() { return &((sc2_data *)c->data)->ctx; }
};

static uint32_t w(int abi, int64_t nr) { return want.count({engine_abi[abi], nr}) ? want[{engine_abi[abi], nr}] : 0; }
static int fails;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); ++fails; } } while (0)

// --- callbacks ------------------------------------------------------------------------------
static int n_read, n_read_ret, n_all, n_all_ret, n_unknown, n_e2, n_r2, n_openat, n_pread, n_close_ret;
static uint32_t r_fd, r_count; static uint64_t r_buf;
static int32_t o_dfd; static uint64_t p_pos;
static uint64_t all_no, unk_no, cur_id; static bool cur_ok;
static syscall_ctx_t e2_ctx, r2_ctx;
static void read_enter(CPUState *, target_ulong pc, uint32_t fd, uint64_t buf, uint32_t count) {
    ++n_read; r_fd = fd; r_buf = buf; r_count = count;
    uint32_t fl; cur_ok = syscalls2_current_call(&cpu, &cur_id, &fl);
}
static void read_ret(CPUState *, target_ulong, uint32_t fd, uint64_t buf, uint32_t count) {
    ++n_read_ret; r_fd = fd; r_buf = buf; r_count = count;
}
static void read_enter_ctx(void *x, CPUState *c, target_ulong pc, uint32_t fd, uint64_t buf, uint32_t n) {
    CHECK(x == &cpu); read_enter(c, pc, fd, buf, n);
}
static void openat_enter(CPUState *, target_ulong, int32_t dfd, uint64_t, int32_t, uint32_t) { ++n_openat; o_dfd = dfd; }
static int32_t f_dfd;
static void fanotify_enter(CPUState *, target_ulong, int32_t, uint32_t, uint64_t mask, int32_t dfd, uint64_t) {
    ++n_pread; p_pos = mask; f_dfd = dfd;
}
static void close_range_ret(CPUState *, target_ulong, uint32_t, uint32_t, uint32_t) { ++n_close_ret; }
static void all_enter(CPUState *, target_ulong, target_ulong no) { ++n_all; all_no = no; }
static void all_ret(CPUState *, target_ulong, target_ulong no) { ++n_all_ret; all_no = no; }
static void unknown_enter(CPUState *, target_ulong, target_ulong no) { ++n_unknown; unk_no = no; }
static void enter2(CPUState *, target_ulong, const syscall_info_t *, const syscall_ctx_t *x) { ++n_e2; e2_ctx = *x; }
static void return2(CPUState *, target_ulong, const syscall_info_t *, const syscall_ctx_t *x) { ++n_r2; r2_ctx = *x; }
static void write_ret(CPUState *, target_ulong, uint32_t, uint64_t, uint32_t) {}
static void write_enter_late(CPUState *, target_ulong, uint32_t, uint64_t, uint32_t) {
    ppp_add_cb_on_sys_write_return(write_ret);      // its own return, subscribed from its entry
}
static void read_enter_unsub(CPUState *, target_ulong, uint32_t, uint64_t, uint32_t) {
    ppp_remove_cb_on_sys_read_return(read_ret);
}

static void reset_counts() {
    n_read = n_read_ret = n_all = n_all_ret = n_unknown = n_e2 = n_r2 = n_openat = n_pread = n_close_ret = 0;
    keeps = 0;
}

static void tests() {
    profiles[PROFILE_LINUX_X64].table = &sc2_table_linux_x64;
    profiles[PROFILE_LINUX_X86].table = &sc2_table_linux_x86;
    profiles[PROFILE_LINUX_X86_INT80].table = &sc2_table_linux_x86;
    sc2_engine_start();
    CHECK(want_calls == 0);                                         // nothing wanted

    // A call's own callback: its numbers in both ABIs, O(numbers) want updates
    ppp_add_cb_on_sys_read_enter(read_enter);
    CHECK(want_calls == 2 && w(0, 0) == PANDA_SYSCALL_WANT_ENTRY && w(1, 3) == PANDA_SYSCALL_WANT_ENTRY);
    Call a(0, 0, { 0xffffffff00000005ull, 0x7fff12345678ull, 0x100000040ull });
    a.enter();
    CHECK(n_read == 1 && r_fd == 5 && r_buf == 0x7fff12345678ull && r_count == 0x40);
    CHECK(cur_ok && cur_id == a.c->id);
    uint64_t id; uint32_t fl;
    CHECK(!syscalls2_current_call(&cpu, &id, &fl));                // outside a callback
    Call b(1, 3, { 7, 0xf7f00000, 12 });                            // i386: the x64 type's widths
    b.enter();
    CHECK(n_read == 2 && r_fd == 7 && r_buf == 0xf7f00000 && r_count == 12);
    Call notread(0, 1, { 1, 2, 3 });
    notread.enter();
    CHECK(n_read == 2);

    // argument kinds: S32 (sign-extended), PAIR (two words of a 32-bit ABI)
    ppp_add_cb_on_sys_openat_enter(openat_enter);
    Call o(0, 257, { 0xabcdffffff9cull, 0x1000, 0, 0 });
    o.enter();
    CHECK(n_openat == 1 && o_dfd == -100);
    ppp_add_cb_on_sys_fanotify_mark_enter(fanotify_enter);         // i386: (int, u32, u64, int, ptr)
    Call p(1, 339, { 3, 1, 0x89abcdef, 0x01234567, 0xffffff9c, 0x8000 });
    p.enter();
    CHECK(n_pread == 1 && p_pos == 0x0123456789abcdefull && f_dfd == -100);

    // a return: wanted, the entry not; its arguments from the engine's
    ppp_add_cb_on_sys_read_return(read_ret);
    CHECK(w(0, 0) == (PANDA_SYSCALL_WANT_ENTRY | PANDA_SYSCALL_WANT_RETURN));
    ppp_remove_cb_on_sys_read_enter(read_enter);
    CHECK(w(0, 0) == PANDA_SYSCALL_WANT_RETURN && w(1, 3) == PANDA_SYSCALL_WANT_RETURN);
    Call r(0, 0, { 9, 0x5000, 1 }, true);
    r.ret();
    CHECK(n_read_ret == 1 && r_fd == 9 && r_buf == 0x5000 && r_count == 1);
    r.ret(PANDA_SYSCALL_RETURN | PANDA_SYSCALL_RESTART);           // restarted: no return
    CHECK(n_read_ret == 1);
    ppp_remove_cb_on_sys_read_return(read_ret);
    CHECK(w(0, 0) == 0 && w(1, 3) == 0);

    // every call: O(numbers) once; the number as the switches gave it (an int, sign-extended)
    unsigned before = want_calls;
    ppp_add_cb_on_all_sys_enter(all_enter);
    CHECK(want_calls - before >= 2 * SC2_NB - 8 && w(0, 400) == PANDA_SYSCALL_WANT_ENTRY);
    Call neg(0, -1, {});
    neg.enter();
    CHECK(n_all == 1 && all_no == (target_ulong)-1);
    ppp_add_cb_on_all_sys_return(all_ret);
    CHECK(w(0, 0) == (PANDA_SYSCALL_WANT_ENTRY | PANDA_SYSCALL_WANT_RETURN));
    CHECK(!(w(0, 60) & PANDA_SYSCALL_WANT_RETURN));                // exit never returns
    Call k(0, 39, {}, true);
    k.ret();
    CHECK(n_all_ret == 1 && all_no == 39);
    ppp_remove_cb_on_all_sys_enter(all_enter);
    ppp_remove_cb_on_all_sys_return(all_ret);
    CHECK(w(0, 400) == 0 && w(0, SC2_NB) == 0);

    // a return subscribed in an entry callback: this call is kept (late)
    reset_counts();
    ppp_add_cb_on_sys_write_enter(write_enter_late);
    Call late(0, 1, { 1, 2, 3 });
    late.enter();
    CHECK(keeps == 1 && keep_last == true && w(0, 1) & PANDA_SYSCALL_WANT_RETURN);
    ppp_remove_cb_on_sys_write_enter(write_enter_late);
    ppp_remove_cb_on_sys_write_return(write_ret);
    CHECK(w(0, 1) == 0);
    // and unsubscribed in one: a kept call is dropped
    reset_counts();
    ppp_add_cb_on_sys_read_return(read_ret);
    ppp_add_cb_on_sys_read_enter(read_enter_unsub);
    Call drop(0, 0, { 1, 2, 3 }, true);
    drop.enter();
    CHECK(keeps == 1 && keep_last == false && w(0, 0) == PANDA_SYSCALL_WANT_ENTRY);
    ppp_remove_cb_on_sys_read_enter(read_enter_unsub);

    // context callbacks: the context built in the call's data, kept for its return
    reset_counts();
    ppp_add_cb_on_all_sys_enter2(enter2);
    Call x(0, 435, { 0x7000, 88 });                                 // clone3: double return
    x.enter();
    CHECK(n_e2 == 1 && e2_ctx.no == 435 && e2_ctx.profile == PROFILE_LINUX_X64 &&
          e2_ctx.retaddr == 0x401002 && e2_ctx.asid == 0x123000 && e2_ctx.double_return);
    uint64_t v; memcpy(&v, e2_ctx.args[0], 8); CHECK(v == 0x7000);
    uint32_t v32; memcpy(&v32, e2_ctx.args[1], 4); CHECK(v32 == 88);
    ppp_remove_cb_on_all_sys_enter2(enter2);
    ppp_add_cb_on_all_sys_return2(return2);
    CHECK(w(0, 3) == (PANDA_SYSCALL_WANT_ENTRY | PANDA_SYSCALL_WANT_RETURN));
    Call y(1, 6, { 4 }, true);                                       // i386 close, kept
    y.enter();
    CHECK(((sc2_data *)y.c->data)->mark == SC2_MARK(y.c));          // built at the entry
    y.ret();
    CHECK(n_r2 == 1 && r2_ctx.no == 6 && r2_ctx.profile == PROFILE_LINUX_X86_INT80);
    Call z(0, 3, { 4 }, true);                                       // kept, entry not seen
    z.ret();
    CHECK(n_r2 == 2 && r2_ctx.no == 3);                              // built at the return
    ppp_remove_cb_on_all_sys_return2(return2);

    // unknown numbers only; numbers from SC2_NB on looked up
    reset_counts();
    ppp_add_cb_on_unknown_sys_enter(unknown_enter);
    CHECK(w(0, 0) == 0 && w(0, 500) == PANDA_SYSCALL_WANT_ENTRY && w(0, SC2_NB) == PANDA_SYSCALL_WANT_ENTRY);
    Call u(0, 500, {});
    u.enter();
    Call big(0, 70000, {});
    big.enter();
    CHECK(n_unknown == 2 && unk_no == 70000);
    ppp_remove_cb_on_unknown_sys_enter(unknown_enter);

    // _with_context, _slot and remove as the PPP arrays had them
    reset_counts();
    ppp_add_cb_on_sys_read_enter_with_context(read_enter_ctx, &cpu);
    Call c1(0, 0, { 1, 2, 3 });
    c1.enter();
    CHECK(n_read == 1);
    CHECK(!ppp_remove_cb_on_sys_read_enter_with_context(read_enter_ctx, nullptr));
    CHECK(ppp_remove_cb_on_sys_read_enter_with_context(read_enter_ctx, &cpu));
    ppp_add_cb_on_sys_read_enter_slot(read_enter, 0);               // slot 0 of 0: not counted
    c1.enter();
    CHECK(n_read == 1 && w(0, 0) == 0);
    ppp_add_cb_on_sys_read_enter_slot(read_enter, 1);               // count 1: slot 0 runs
    c1.enter();
    CHECK(n_read == 2);
    CHECK(ppp_remove_cb_on_sys_read_enter(read_enter) && w(0, 0) == 0);

    // own callbacks and one plain on_all_sys_enter: both, on both ABIs
    reset_counts();
    ppp_add_cb_on_sys_read_enter(read_enter);
    ppp_add_cb_on_all_sys_enter(all_enter);
    Call m1(0, 0, { 1, 2, 3 }), m2(1, 3, { 4, 5, 6 }), m3(1, 4, { 4, 5, 6 });
    m1.enter(); m2.enter(); m3.enter();
    CHECK(n_read == 2 && n_all == 3 && r_fd == 4);
    ppp_remove_cb_on_all_sys_enter(all_enter);
    ppp_remove_cb_on_sys_read_enter(read_enter);

    // a return callback with the arguments of a 3-argument call
    ppp_add_cb_on_sys_close_range_return(close_range_ret);
    CHECK(w(0, 436) == PANDA_SYSCALL_WANT_RETURN);
    Call cr(0, 436, { 3, 10, 0 }, true);
    cr.ret();
    CHECK(n_close_ret == 1);
    ppp_remove_cb_on_sys_close_range_return(close_range_ret);
    if (!fails)
        std::puts("PASS: syscalls2 dispatch (tables, wants, argument kinds, keep, contexts, unknown numbers)");
}

static void nop_enter(CPUState *, target_ulong, uint32_t, uint64_t, uint32_t) {}
static void nop_all(CPUState *, target_ulong, target_ulong) {}

static void bench(const char *mode) {
    profiles[PROFILE_LINUX_X64].table = &sc2_table_linux_x64;
    profiles[PROFILE_LINUX_X86].table = &sc2_table_linux_x86;
    profiles[PROFILE_LINUX_X86_INT80].table = &sc2_table_linux_x86;
    sc2_engine_start();
    if (!std::strcmp(mode, "typed")) { ppp_add_cb_on_sys_read_enter(nop_enter); ppp_add_cb_on_sys_read_return(nop_enter); }
    else if (!std::strcmp(mode, "all")) { ppp_add_cb_on_all_sys_enter(nop_all); ppp_add_cb_on_all_sys_return(nop_all); }
    else if (!std::strcmp(mode, "context")) { ppp_add_cb_on_all_sys_enter2(enter2); ppp_add_cb_on_all_sys_return2(return2); }
    else if (std::strcmp(mode, "idle")) std::abort();
    const int iterations = getenv("SC2_ITERS") ? atoi(getenv("SC2_ITERS")) : 10000000;
    Call c(0, 0, { 1, 2, 3 }, true);
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        c.enter();
        c.ret();
    }
    double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
    std::printf("%s %.2f ns per entry and return\n", mode, ns / iterations);
}

int main(int argc, char **argv) {
    if (argc == 3 && !std::strcmp(argv[1], "--bench")) bench(argv[2]);
    else tests();
    return fails ? 1 : 0;
}
