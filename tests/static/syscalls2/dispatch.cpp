// Runs the actual generated x64 enter/return code with the real PPP macros. Only the engine
// boundary is stubbed: argument reads and hook installs are counted, without needing a guest.
#include "panda.h"
#include "syscalls2.h"
#include "hooks/hooks_int_fns.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "syscalls_ext_typedefs.h"
#include "generated/syscall_ppp_boilerplate_enter.cpp"
#include "generated/syscall_ppp_boilerplate_return.cpp"
Profile profiles[PROFILE_LAST];
}

bool load_info = false;
context_map_t running_syscalls;
static uint64_t reads, hooks, enters, returns;
static uint64_t args[6] = {100, 200, 3, 400, 500, 600};
#include <climits>
#include <vector>
static std::vector<void (*)(CPUState*, target_ulong)> preExecCallbacks;
#include "native_observer.h"
static uint64_t seen[3];
static CPUState cpu;
static constexpr uint64_t pc = 0x400100;
uint32_t get_32(CPUState *, syscall_ctx *, uint32_t n) { ++reads; return native_frame ? native_frame->event->args[n] : args[n]; }
uint64_t get_64(CPUState *, syscall_ctx *, uint32_t n) { ++reads; return native_frame ? native_frame->event->args[n] : args[n]; }
int32_t get_s32(CPUState *c, syscall_ctx *x, uint32_t n) { return get_32(c, x, n); }
int64_t get_s64(CPUState *c, syscall_ctx *x, uint32_t n) { return get_64(c, x, n); }
target_ulong calc_retaddr(CPUState *, syscall_ctx *, target_ulong p) { return p + 2; }
target_ulong syscalls_get_id(CPUState *) { return 0x123000; }
void sysinfo_load_profile(int, syscall_info_t **, syscall_meta_t **) { std::abort(); }
void hook_syscall_return(CPUState *, TranslationBlock *, hook *) {}
static void count_hook(hook *) { ++hooks; }
void (*hooks_add_hook)(hook *) = count_hook;

static void enter(CPUState *, target_ulong, uint32_t a, uint32_t b, uint32_t c) {
    ++enters; seen[0] = a; seen[1] = b; seen[2] = c;
}
static void ret(CPUState *, target_ulong, uint32_t a, uint32_t b, uint32_t c) {
    ++returns; seen[0] = a; seen[1] = b; seen[2] = c;
}
static void enter_context(void *ctx, CPUState *c, target_ulong p, uint32_t a, uint32_t b, uint32_t d) {
    assert(ctx == &cpu); enter(c, p, a, b, d);
}
static void return_context(void *ctx, CPUState *c, target_ulong p, uint32_t a, uint32_t b, uint32_t d) {
    assert(ctx == &cpu); ret(c, p, a, b, d);
}
static void all_enter(CPUState *, target_ulong, target_ulong) { ++enters; }
static void all_return(CPUState *, target_ulong, target_ulong) { ++returns; }
static void all_enter2(CPUState *, target_ulong, const syscall_info_t *, const syscall_ctx_t *ctx) {
    ++enters;
    for (int n = 0; n < 3; ++n) std::memcpy(&seen[n], ctx->args[n], sizeof(uint32_t));
}
static void all_return2(CPUState *, target_ulong, const syscall_info_t *, const syscall_ctx_t *ctx) {
    ++returns;
    for (int n = 0; n < 3; ++n) std::memcpy(&seen[n], ctx->args[n], sizeof(uint32_t));
}
static void getpid_return(CPUState *, target_ulong) { ++returns; }
static void unknown_enter(CPUState *, target_ulong, target_ulong) { ++enters; }
static void unknown_return(CPUState *, target_ulong, target_ulong) { ++returns; }
static void late_specific(CPUState *c, target_ulong p, uint32_t a, uint32_t b, uint32_t d) {
    enter(c, p, a, b, d);
    ppp_add_cb_on_sys_close_range_return(ret);
}
static void late_generic(CPUState *, target_ulong, target_ulong) {
    ++enters;
    ppp_add_cb_on_sys_close_range_return(ret);
}
static void remove_return(CPUState *c, target_ulong p, uint32_t a, uint32_t b, uint32_t d) {
    enter(c, p, a, b, d);
    assert(ppp_remove_cb_on_sys_close_range_return(ret));
}

static void reset() {
    reads = hooks = enters = returns = 0;
    std::memset(seen, 0, sizeof(seen));
    running_syscalls.clear();
#define CLEAR(cb) ppp_##cb##_num_cb = ppp_##cb##_num_cb_with_context = 0
    CLEAR(on_sys_close_range_enter); CLEAR(on_sys_close_range_return);
    CLEAR(on_sys_getpid_return); CLEAR(on_all_sys_enter); CLEAR(on_all_sys_return);
    CLEAR(on_all_sys_enter2); CLEAR(on_all_sys_return2);
    CLEAR(on_unknown_sys_enter); CLEAR(on_unknown_sys_return);
#undef CLEAR
}
static void call(int nr = 436) {
    syscall_enter_switch_linux_x64(&cpu, PROFILE_LINUX_X64, pc, nr);
}
static void finish() {
    assert(running_syscalls.size() == 1);
    syscall_ctx_t ctx = running_syscalls.begin()->second;
    syscall_return_switch_linux_x64(&cpu, ctx.retaddr, &ctx);
    running_syscalls.clear();
}
static void check_args() {
    assert(seen[0] == args[0] && seen[1] == args[1] && seen[2] == args[2]);
}

static int preexec_count;
static void preexec(CPUState *, target_ulong p) {
    assert(p == pc && enters == 0); ++preexec_count;
}
static void native_tests() {
    PandaSyscallObservation event = {};
    event.abi = PANDA_SYSCALL_OBSERVER_X86_64;
    event.num = 436;
    event.pc = pc + 2;
    for (int i = 0; i < 6; ++i) event.args[i] = args[i];
    reset(); preExecCallbacks.push_back(preexec);
    ppp_add_cb_on_all_sys_enter(all_enter);
    assert(native_enter(&cpu, &event, nullptr) == nullptr);
    assert(preexec_count == 1 && enters == 1);
    preExecCallbacks.clear();
    assert(reads == 0 && hooks == 0 && running_syscalls.empty());
    reset(); ppp_add_cb_on_sys_close_range_return(ret);
    void *cookie = native_enter(&cpu, &event, nullptr);
    assert(cookie && reads == 3 && hooks == 0 && running_syscalls.empty());
    auto *context = static_cast<syscall_ctx_t *>(cookie);
    assert(context->retaddr == pc + 2);
    native_return(&cpu, &event, cookie, nullptr);
    assert(returns == 1); check_args(); native_drop(cookie, nullptr);
    reset(); ppp_add_cb_on_all_sys_enter(late_generic);
    cookie = native_enter(&cpu, &event, nullptr);
    assert(cookie && hooks == 0);
    event.restart = true;
    native_return(&cpu, &event, cookie, nullptr);
    assert(returns == 0); native_drop(cookie, nullptr);
    event.restart = false;
    reset(); ppp_add_cb_on_all_sys_return(all_return);
    event.num = 60; assert(!native_enter(&cpu, &event, nullptr)); // no-return
    event.num = 9999; cookie = native_enter(&cpu, &event, nullptr);
    assert(cookie); native_return(&cpu, &event, cookie, nullptr); native_drop(cookie, nullptr);
    assert(returns == 1);
    reset(); ppp_add_cb_on_all_sys_return(all_return);
    event.num = 435; assert(!native_enter(&cpu, &event, nullptr));
    assert(hooks == 1 && running_syscalls.begin()->second.double_return);
    reset(); event.abi = 1;
    assert(!native_enter(&cpu, &event, nullptr) && reads == 0 && hooks == 0);
    event.abi = 0; event.num = -1;
    assert(!native_enter(&cpu, &event, nullptr));
    reset();
    std::puts("PASS: native context capture, late subscription, restart suppression, ABI rejection and double-return fallback");
}

static void tests() {
    reset(); call();
    assert(reads == 0 && hooks == 0 && running_syscalls.empty());
    reset(); ppp_add_cb_on_sys_close_range_enter(enter); call();
    assert(reads == 3 && enters == 1 && hooks == 0); check_args();
    reset(); ppp_add_cb_on_sys_getpid_return(getpid_return); call();
    assert(reads == 0 && hooks == 0 && running_syscalls.empty());
    reset(); ppp_add_cb_on_sys_close_range_return(ret); call(); finish();
    assert(reads == 3 && hooks == 1 && returns == 1); check_args();
    reset(); ppp_add_cb_on_all_sys_enter(all_enter); call();
    assert(reads == 0 && enters == 1 && hooks == 0);
    reset(); ppp_add_cb_on_all_sys_enter2(all_enter2); call();
    assert(reads == 3 && enters == 1 && hooks == 0); check_args();
    reset(); ppp_add_cb_on_all_sys_return(all_return); call(); finish();
    assert(reads == 0 && hooks == 1 && returns == 1);
    reset(); ppp_add_cb_on_all_sys_return2(all_return2); call(); finish();
    assert(reads == 3 && hooks == 1 && returns == 1); check_args();
    reset(); ppp_add_cb_on_unknown_sys_enter(unknown_enter); call(9999);
    assert(enters == 1 && hooks == 0);
    ppp_add_cb_on_unknown_sys_return(unknown_return); call(9999); finish();
    assert(enters == 2 && hooks == 1 && returns == 1);
    reset(); ppp_add_cb_on_sys_close_range_enter(late_specific); call(); finish();
    assert(enters == 1 && hooks == 1 && returns == 1);
    reset(); ppp_add_cb_on_all_sys_enter(late_generic); call(); finish();
    assert(enters == 1 && hooks == 1 && returns == 1);
    reset(); ppp_add_cb_on_sys_close_range_enter_with_context(enter_context, &cpu); call();
    assert(enters == 1 && hooks == 0); check_args();
    reset(); ppp_add_cb_on_sys_close_range_return_with_context(return_context, &cpu); call(); finish();
    assert(hooks == 1 && returns == 1); check_args();
    reset(); ppp_add_cb_on_all_sys_return(all_return); call(60); // exit never returns
    assert(hooks == 0 && running_syscalls.empty());
    reset(); ppp_add_cb_on_sys_close_range_return(ret);
    ppp_add_cb_on_sys_close_range_enter(remove_return); call();
    assert(enters == 1 && hooks == 0 && running_syscalls.empty());
    reset(); ppp_add_cb_on_all_sys_return(all_return); call(435); // clone3 retains two returns
    assert(hooks == 1 && running_syscalls.begin()->second.double_return);
    reset(); ppp_add_cb_on_sys_close_range_return(ret); call();
    args[2] = 17; call(); finish();  // a repeated (return PC, ASID) replaces the old context
    assert(hooks == 2 && returns == 1); check_args(); args[2] = 3;
    std::puts("PASS: generated syscall dispatch (18 subscription, argument and return scenarios)");
}

static void bench(const char *mode) {
    reset();
    if (!std::strcmp(mode, "enter")) ppp_add_cb_on_sys_close_range_enter(enter);
    else if (!std::strcmp(mode, "return")) ppp_add_cb_on_sys_close_range_return(ret);
    else if (!std::strcmp(mode, "all-return")) ppp_add_cb_on_all_sys_return(all_return);
    else if (std::strcmp(mode, "idle")) std::abort();
    constexpr int iterations = 1000000;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        call();
        if (!running_syscalls.empty()) finish();
    }
    double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
    std::printf("%s %.3f ns/call reads=%llu hooks=%llu enters=%llu returns=%llu\n", mode,
                ns / iterations, (unsigned long long)reads, (unsigned long long)hooks,
                (unsigned long long)enters, (unsigned long long)returns);
}

int main(int argc, char **argv) {
    if (argc == 3 && !std::strcmp(argv[1], "--bench")) bench(argv[2]);
    else { tests(); native_tests(); }
}
