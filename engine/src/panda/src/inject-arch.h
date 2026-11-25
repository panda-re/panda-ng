/*
 * Per-target system-call injectors (include/panda/inject.h), included by inject.c only.
 *
 * Each target defines, for the task on the vCPU at a CPU-loop boundary:
 *   inj_user(cs, &sp, &abi)       1: user mode and we can inject (sp, abi set); 0: not here or
 *                                 not now (kernel mode, a branch delay slot); -1: a user task of
 *                                 an ABI we cannot serve
 *   inj_can(cs, abi, ret_pc)      the call may return to ret_pc (what the kernel or the CPU
 *                                 reads before it: an OABI immediate, BadInstr across a page)
 *   inj_len(cs, abi)              the system-call instruction's length (restart = ret_pc - len)
 *   inj_save / inj_restore        the registers a system call or our injection may change
 *                                 (not the PC: inject.c puts the PC back itself)
 *   inj_retval(cs, abi)           the call's value, -errno on failure
 *   inj_marker_get / _set         the token register; inj_marker_ok(abi, nargs): it is free
 *   inj_set_pc(cs, pc)
 *   inj_set_call(cs, abi, c)      c's number and arguments in their registers (nothing else)
 *   inj_enter(cs, abi, ret_pc, c) enter the kernel as if a system-call instruction just before
 *                                 ret_pc had run, with c's number and arguments; returns to the
 *                                 CPU loop (does not return)
 *
 * The kernel sees an ordinary system call of the task, made from user mode with the task's own
 * stack and return address; nothing but the registers named here changes.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PANDA_INJECT_ARCH_H
#define PANDA_INJECT_ARCH_H

#define INJ_NREGS 32
#define INJ_NX 12

typedef struct InjRegs {
    uint64_t r[INJ_NREGS];
    uint64_t x[INJ_NX];
} InjRegs;

static inline uint64_t inj_get_pc(CPUState *cs)
{
    return cs->cc->get_pc(cs);
}

#if defined(TARGET_I386)
/*
 * x86_64 tasks: what SYSCALL does in long mode (helper_syscall's effect), returning to ret_pc.
 * 32-bit tasks (i386, and compat tasks under an x86_64 kernel): INT 0x80 as a software
 * interrupt whose next instruction is ret_pc, through the IDT as the guest set it up.
 */
static int inj_user(CPUState *cs, uint64_t *sp, int *abi)
{
    CPUX86State *env = cpu_env(cs);
    if ((env->hflags & HF_CPL_MASK) != 3) {
        return 0;
    }
    if (!(env->cr[0] & CR0_PE_MASK) || (env->eflags & VM_MASK)) {
        return -1;
    }
    *sp = env->regs[R_ESP];
#ifdef TARGET_X86_64
    if (env->hflags & HF_CS64_MASK) {
        *abi = PANDA_INJECT_ABI_X86_64;
        return 1;
    }
#endif
    if (!(env->hflags & HF_CS32_MASK)) {
        return -1;                      /* 16-bit code */
    }
    *sp = (uint32_t)*sp;
    *abi = PANDA_INJECT_ABI_I386;
    return 1;
}
static bool inj_can(CPUState *cs, int abi, uint64_t ret_pc)
{
    return true;
}
static unsigned inj_len(CPUState *cs, int abi)
{
    return 2;                           /* SYSCALL, INT 0x80 */
}
static void inj_save(CPUState *cs, InjRegs *h)
{
    CPUX86State *env = cpu_env(cs);
    for (int i = 0; i < CPU_NB_REGS; i++) {
        h->r[i] = env->regs[i];
    }
    h->x[0] = cpu_compute_eflags(env);
}
static void inj_restore(CPUState *cs, const InjRegs *h)
{
    CPUX86State *env = cpu_env(cs);
    for (int i = 0; i < CPU_NB_REGS; i++) {
        env->regs[i] = h->r[i];
    }
    cpu_load_eflags(env, h->x[0], TF_MASK | AC_MASK | ID_MASK | NT_MASK | RF_MASK);
}
static int64_t inj_retval(CPUState *cs, int abi)
{
    CPUX86State *env = cpu_env(cs);
    return abi == PANDA_INJECT_ABI_I386 ? (int64_t)(int32_t)env->regs[R_EAX]
                                        : (int64_t)env->regs[R_EAX];
}
/* x86_64: r12, preserved by system calls and saved in a signal frame. i386: ebp, unless the call
 * takes six arguments (it is the sixth) */
static bool inj_marker_ok(int abi, int nargs)
{
    return abi != PANDA_INJECT_ABI_I386 || (nargs >= 0 && nargs < 6);
}
static uint64_t inj_marker_get(CPUState *cs, int abi)
{
    CPUX86State *env = cpu_env(cs);
    return abi == PANDA_INJECT_ABI_I386 ? (uint32_t)env->regs[R_EBP] : env->regs[12];
}
static void inj_marker_set(CPUState *cs, int abi, uint64_t v)
{
    CPUX86State *env = cpu_env(cs);
    if (abi == PANDA_INJECT_ABI_I386) {
        env->regs[R_EBP] = (uint32_t)v;
    } else {
        env->regs[12] = v;
    }
}
static void inj_set_pc(CPUState *cs, uint64_t pc)
{
    CPUX86State *env = cpu_env(cs);
    env->eip = pc - env->segs[R_CS].base;
}
static void inj_set_call(CPUState *cs, int abi, const PandaInjectCall *c)
{
    CPUX86State *env = cpu_env(cs);
    static const int argreg32[6] = { R_EBX, R_ECX, R_EDX, R_ESI, R_EDI, R_EBP };
    static const int argreg64[6] = { R_EDI, R_ESI, R_EDX, 10, 8, 9 };
    bool i386 = abi == PANDA_INJECT_ABI_I386;

    env->regs[R_EAX] = i386 ? (uint32_t)c->nr : c->nr;
    for (int i = 0; i < 6; i++) {
        if (c->nargs < 0 || i < c->nargs) {
            env->regs[i386 ? argreg32[i] : argreg64[i]] = i386 ? (uint32_t)c->args[i] : c->args[i];
        }
    }
}
static G_NORETURN void inj_enter(CPUState *cs, int abi, uint64_t ret_pc, const PandaInjectCall *c)
{
    CPUX86State *env = cpu_env(cs);

    inj_set_call(cs, abi, c);
    if (abi == PANDA_INJECT_ABI_I386) {
        inj_set_pc(cs, ret_pc - 2);
        env->exception_is_int = 1;
        env->exception_next_eip = ret_pc - env->segs[R_CS].base;
        env->error_code = 0;
        panda_inject_raising = true;    /* not a call of the program's: no entry reported */
        cs->exception_index = 0x80;
        cpu_loop_exit(cs);
    }
#ifdef TARGET_X86_64
    {
        int selector = (env->star >> 32) & 0xffff;

        env->regs[R_ECX] = ret_pc;
        env->regs[11] = cpu_compute_eflags(env) & ~RF_MASK;
        env->eflags &= ~(env->fmask | RF_MASK);
        cpu_load_eflags(env, env->eflags, 0);
        cpu_x86_load_seg_cache(env, R_CS, selector & 0xfffc, 0, 0xffffffff,
                               DESC_G_MASK | DESC_P_MASK | DESC_S_MASK | DESC_CS_MASK |
                               DESC_R_MASK | DESC_A_MASK | DESC_L_MASK);
        cpu_x86_load_seg_cache(env, R_SS, (selector + 8) & 0xfffc, 0, 0xffffffff,
                               DESC_G_MASK | DESC_B_MASK | DESC_P_MASK | DESC_S_MASK |
                               DESC_W_MASK | DESC_A_MASK);
        env->eip = env->lstar;
        cpu_loop_exit_noexc(cs);
    }
#else
    g_assert_not_reached();
#endif
}

#elif defined(TARGET_ARM)
/*
 * AArch64 tasks (aarch64 target): SVC with x8. AArch32 tasks (arm target, or a compat task
 * under an AArch64 kernel): EABI SVC 0 with r7. The exception's return address is ret_pc.
 */
static bool inj_a32_kernel(CPUARMState *env)
{
    return !arm_el_is_aa64(env, 1);
}
static int inj_user(CPUState *cs, uint64_t *sp, int *abi)
{
    CPUARMState *env = cpu_env(cs);
    if (arm_current_el(env) != 0) {
        return 0;
    }
    if (is_a64(env)) {
        *sp = env->xregs[31];
        *abi = PANDA_INJECT_ABI_AARCH64;
    } else {
        *sp = env->regs[13];
        *abi = PANDA_INJECT_ABI_ARM_EABI;
    }
    return 1;
}
/*
 * An AArch32 kernel with OABI compatibility reads the SVC instruction before the return address
 * and takes a non-zero immediate as an old-ABI call number: return only where that word is an
 * SVC #0 (right after one of the thread's own system calls, or at a held SVC). Thumb entries and
 * AArch64 kernels take the number from r7 alone.
 */
static bool inj_can(CPUState *cs, int abi, uint64_t ret_pc)
{
    CPUARMState *env = cpu_env(cs);
    GByteArray *b;
    bool svc0;

    if (abi != PANDA_INJECT_ABI_ARM_EABI || env->thumb || !inj_a32_kernel(env)) {
        return true;
    }
    b = g_byte_array_new();
    svc0 = qemu_plugin_read_memory_vaddr_fault(ret_pc - 4, b, 4, QEMU_PLUGIN_VADDR_USER, false) ==
           QEMU_PLUGIN_VADDR_OK && (ldl_le_p(b->data) & 0x0fffffff) == 0x0f000000;
    g_byte_array_unref(b);
    return svc0;
}
static unsigned inj_len(CPUState *cs, int abi)
{
    CPUARMState *env = cpu_env(cs);
    return abi == PANDA_INJECT_ABI_ARM_EABI && env->thumb ? 2 : 4;
}
static void inj_save(CPUState *cs, InjRegs *h)
{
    CPUARMState *env = cpu_env(cs);
    if (is_a64(env)) {
        for (int i = 0; i < 31; i++) {
            h->r[i] = env->xregs[i];
        }
    } else {
        for (int i = 0; i < 15; i++) {
            h->r[i] = env->regs[i];
        }
    }
}
static void inj_restore(CPUState *cs, const InjRegs *h)
{
    CPUARMState *env = cpu_env(cs);
    if (is_a64(env)) {
        for (int i = 0; i < 31; i++) {
            env->xregs[i] = h->r[i];
        }
    } else {
        for (int i = 0; i < 15; i++) {
            env->regs[i] = h->r[i];
        }
    }
}
static int64_t inj_retval(CPUState *cs, int abi)
{
    CPUARMState *env = cpu_env(cs);
    return is_a64(env) ? (int64_t)env->xregs[0] : (int64_t)(int32_t)env->regs[0];
}
/* x28 (AArch64) or r8 (AArch32): preserved by system calls and saved in a signal frame */
static bool inj_marker_ok(int abi, int nargs)
{
    return true;
}
static uint64_t inj_marker_get(CPUState *cs, int abi)
{
    CPUARMState *env = cpu_env(cs);
    return is_a64(env) ? env->xregs[28] : env->regs[8];
}
static void inj_marker_set(CPUState *cs, int abi, uint64_t v)
{
    CPUARMState *env = cpu_env(cs);
    if (is_a64(env)) {
        env->xregs[28] = v;
    } else {
        env->regs[8] = (uint32_t)v;
    }
}
static void inj_set_pc(CPUState *cs, uint64_t pc)
{
    CPUARMState *env = cpu_env(cs);
    if (is_a64(env)) {
        env->pc = pc;
    } else {
        env->regs[15] = (uint32_t)pc;
    }
}
static void inj_set_call(CPUState *cs, int abi, const PandaInjectCall *c)
{
    CPUARMState *env = cpu_env(cs);

    if (is_a64(env)) {
        env->xregs[8] = c->nr;
        for (int i = 0; i < 6; i++) {
            if (c->nargs < 0 || i < c->nargs) {
                env->xregs[i] = c->args[i];
            }
        }
    } else {
        env->regs[7] = (uint32_t)c->nr;
        for (int i = 0; i < 6; i++) {
            if (c->nargs < 0 || i < c->nargs) {
                env->regs[i] = (uint32_t)c->args[i];
            }
        }
    }
}
static G_NORETURN void inj_enter(CPUState *cs, int abi, uint64_t ret_pc, const PandaInjectCall *c)
{
    CPUARMState *env = cpu_env(cs);

    inj_set_call(cs, abi, c);
    env->exception.syndrome = is_a64(env) ? syn_aa64_svc(0) : syn_aa32_svc(0, env->thumb);
    inj_set_pc(cs, ret_pc);
    env->exception.target_el = 1;
    panda_inject_raising = true;        /* not the program's call: no entry reported */
    cs->exception_index = EXCP_SWI;
    cpu_loop_exit(cs);
}

#elif defined(TARGET_RISCV64)
/* ECALL 4 bytes before ret_pc: the kernel steps sepc past it */
static int inj_user(CPUState *cs, uint64_t *sp, int *abi)
{
    CPURISCVState *env = cpu_env(cs);
    if (env->priv != PRV_U || env->virt_enabled) {
        return 0;
    }
    if (env->xl != MXL_RV64) {
        return -1;
    }
    *sp = env->gpr[2];
    *abi = PANDA_INJECT_ABI_RISCV64;
    return 1;
}
static bool inj_can(CPUState *cs, int abi, uint64_t ret_pc)
{
    return true;
}
static unsigned inj_len(CPUState *cs, int abi)
{
    return 4;
}
static void inj_save(CPUState *cs, InjRegs *h)
{
    CPURISCVState *env = cpu_env(cs);
    for (int i = 0; i < 32; i++) {
        h->r[i] = env->gpr[i];
    }
}
static void inj_restore(CPUState *cs, const InjRegs *h)
{
    CPURISCVState *env = cpu_env(cs);
    for (int i = 1; i < 32; i++) {
        env->gpr[i] = h->r[i];
    }
}
static int64_t inj_retval(CPUState *cs, int abi)
{
    return (int64_t)cpu_env(cs)->gpr[10];
}
/* s11 */
static bool inj_marker_ok(int abi, int nargs)
{
    return true;
}
static uint64_t inj_marker_get(CPUState *cs, int abi) { return cpu_env(cs)->gpr[27]; }
static void inj_marker_set(CPUState *cs, int abi, uint64_t v) { cpu_env(cs)->gpr[27] = v; }
static void inj_set_pc(CPUState *cs, uint64_t pc)
{
    cpu_env(cs)->pc = pc;
}
static void inj_set_call(CPUState *cs, int abi, const PandaInjectCall *c)
{
    CPURISCVState *env = cpu_env(cs);

    env->gpr[17] = c->nr;
    for (int i = 0; i < 6; i++) {
        if (c->nargs < 0 || i < c->nargs) {
            env->gpr[10 + i] = c->args[i];
        }
    }
}
static G_NORETURN void inj_enter(CPUState *cs, int abi, uint64_t ret_pc, const PandaInjectCall *c)
{
    CPURISCVState *env = cpu_env(cs);

    inj_set_call(cs, abi, c);
    env->pc = ret_pc - 4;
    cs->exception_index = RISCV_EXCP_U_ECALL;
    cpu_loop_exit(cs);
}

#elif defined(TARGET_MIPS)
/*
 * SYSCALL 4 bytes before ret_pc (the kernel steps EPC past it). o32 on 32-bit targets (arguments
 * 5 and 6 on the user stack, 16 and 20 bytes above sp: inject.c saves and restores those words);
 * n64 on 64-bit ones, where the kernel takes the ABI from the number's range, so o32 and n32 tasks
 * are served too. Not in a delay slot, not MIPS16 or microMIPS, and not returning to the start of
 * a page (the CPU may read the instruction at EPC into BadInstr).
 */
static int inj_user(CPUState *cs, uint64_t *sp, int *abi)
{
    CPUMIPSState *env = cpu_env(cs);
    if ((env->hflags & MIPS_HFLAG_MODE) != MIPS_HFLAG_UM) {
        return 0;
    }
    if (env->hflags & MIPS_HFLAG_M16) {
        return -1;
    }
    if (env->hflags & MIPS_HFLAG_BMASK) {
        return 0;
    }
    *sp = env->active_tc.gpr[29];
#if defined(TARGET_MIPS64)
    *abi = PANDA_INJECT_ABI_MIPS_N64;
#else
    *abi = PANDA_INJECT_ABI_MIPS_O32;
#endif
    return 1;
}
static bool inj_can(CPUState *cs, int abi, uint64_t ret_pc)
{
    return (ret_pc & (qemu_target_page_size() - 1)) >= 4;
}
static unsigned inj_len(CPUState *cs, int abi)
{
    return 4;
}
static void inj_save(CPUState *cs, InjRegs *h)
{
    CPUMIPSState *env = cpu_env(cs);
    for (int i = 0; i < 32; i++) {
        h->r[i] = env->active_tc.gpr[i];
    }
    h->x[0] = env->active_tc.HI[0];
    h->x[1] = env->active_tc.LO[0];
}
static void inj_restore(CPUState *cs, const InjRegs *h)
{
    CPUMIPSState *env = cpu_env(cs);
    for (int i = 1; i < 32; i++) {
        env->active_tc.gpr[i] = h->r[i];
    }
    env->active_tc.HI[0] = h->x[0];
    env->active_tc.LO[0] = h->x[1];
}
/* v0 is the errno when a3 is set */
static int64_t inj_retval(CPUState *cs, int abi)
{
    CPUMIPSState *env = cpu_env(cs);
    int64_t v0 = (target_long)env->active_tc.gpr[2];
    return env->active_tc.gpr[7] ? -v0 : v0;
}
/* s7 */
static bool inj_marker_ok(int abi, int nargs)
{
    return true;
}
static uint64_t inj_marker_get(CPUState *cs, int abi)
{
    return cpu_env(cs)->active_tc.gpr[23];
}
static void inj_marker_set(CPUState *cs, int abi, uint64_t v)
{
    cpu_env(cs)->active_tc.gpr[23] = v;     /* tokens are below 2^31: no sign question */
}
static void inj_set_pc(CPUState *cs, uint64_t pc)
{
    cpu_env(cs)->active_tc.PC = pc;
}
static void inj_set_call(CPUState *cs, int abi, const PandaInjectCall *c)
{
    CPUMIPSState *env = cpu_env(cs);
    int n = abi == PANDA_INJECT_ABI_MIPS_O32 ? 4 : 6;  /* the rest: on the stack (inject.c) */

    env->active_tc.gpr[2] = c->nr;
    for (int i = 0; i < n; i++) {
        if (c->nargs < 0 || i < c->nargs) {
            env->active_tc.gpr[4 + i] = (target_ulong)c->args[i];
        }
    }
}
static G_NORETURN void inj_enter(CPUState *cs, int abi, uint64_t ret_pc, const PandaInjectCall *c)
{
    CPUMIPSState *env = cpu_env(cs);

    inj_set_call(cs, abi, c);
    env->active_tc.PC = ret_pc - 4;
    cs->exception_index = EXCP_SYSCALL;
    env->error_code = 0;
    cpu_loop_exit(cs);
}

#elif defined(TARGET_PPC)
/* sc with r0, 4 bytes before ret_pc. On ppc64, 64-bit tasks only (MSR.SF). */
static int inj_user(CPUState *cs, uint64_t *sp, int *abi)
{
    CPUPPCState *env = cpu_env(cs);
    if (!FIELD_EX64(env->msr, MSR, PR)) {
        return 0;
    }
#if defined(TARGET_PPC64)
    if (!FIELD_EX64(env->msr, MSR, SF)) {
        return -1;
    }
    *abi = PANDA_INJECT_ABI_PPC64;
#else
    *abi = PANDA_INJECT_ABI_PPC;
#endif
    *sp = env->gpr[1];
    return 1;
}
static bool inj_can(CPUState *cs, int abi, uint64_t ret_pc)
{
    return true;
}
static unsigned inj_len(CPUState *cs, int abi)
{
    return 4;
}
static void inj_save(CPUState *cs, InjRegs *h)
{
    CPUPPCState *env = cpu_env(cs);
    for (int i = 0; i < 32; i++) {
        h->r[i] = env->gpr[i];
    }
    for (int i = 0; i < 8; i++) {
        h->x[i] = env->crf[i];
    }
    /* sc may change the volatile special registers too (Linux zeroes some on the way out) */
    h->x[8] = env->lr;
    h->x[9] = env->ctr;
    h->x[10] = cpu_read_xer(env);
}
static void inj_restore(CPUState *cs, const InjRegs *h)
{
    CPUPPCState *env = cpu_env(cs);
    for (int i = 0; i < 32; i++) {
        env->gpr[i] = h->r[i];
    }
    for (int i = 0; i < 8; i++) {
        env->crf[i] = h->x[i];
    }
    env->lr = h->x[8];
    env->ctr = h->x[9];
    cpu_write_xer(env, h->x[10]);
}
/* r3 is the errno when CR0.SO is set */
static int64_t inj_retval(CPUState *cs, int abi)
{
    CPUPPCState *env = cpu_env(cs);
    int64_t r3 = (target_long)env->gpr[3];
    return (env->crf[0] & 1) ? -r3 : r3;
}
/* r30: non-volatile across sc */
static bool inj_marker_ok(int abi, int nargs)
{
    return true;
}
static uint64_t inj_marker_get(CPUState *cs, int abi) { return cpu_env(cs)->gpr[30]; }
static void inj_marker_set(CPUState *cs, int abi, uint64_t v)
{
    cpu_env(cs)->gpr[30] = (target_ulong)v;
}
static void inj_set_pc(CPUState *cs, uint64_t pc)
{
    cpu_env(cs)->nip = pc;
}
static void inj_set_call(CPUState *cs, int abi, const PandaInjectCall *c)
{
    CPUPPCState *env = cpu_env(cs);

    env->gpr[0] = c->nr;
    for (int i = 0; i < 6; i++) {
        if (c->nargs < 0 || i < c->nargs) {
            env->gpr[3 + i] = (target_ulong)c->args[i];
        }
    }
}
static G_NORETURN void inj_enter(CPUState *cs, int abi, uint64_t ret_pc, const PandaInjectCall *c)
{
    CPUPPCState *env = cpu_env(cs);

    inj_set_call(cs, abi, c);
    env->nip = ret_pc - 4;
    cs->exception_index = POWERPC_EXCP_SYSCALL;
    env->error_code = 0;
    cpu_loop_exit(cs);
}

#elif defined(TARGET_LOONGARCH64)
/* SYSCALL with a7, 4 bytes before ret_pc (the kernel steps ERA past it) */
static int inj_user(CPUState *cs, uint64_t *sp, int *abi)
{
    CPULoongArchState *env = cpu_env(cs);
    if (FIELD_EX64(env_sys(env)->CSR_CRMD, CSR_CRMD, PLV) != 3) {
        return 0;
    }
    *sp = env->gpr[3];
    *abi = PANDA_INJECT_ABI_LOONGARCH64;
    return 1;
}
/* the exception entry reads the instruction at ERA into BADI: not across a page start */
static bool inj_can(CPUState *cs, int abi, uint64_t ret_pc)
{
    return (ret_pc & (qemu_target_page_size() - 1)) >= 4;
}
static unsigned inj_len(CPUState *cs, int abi)
{
    return 4;
}
static void inj_save(CPUState *cs, InjRegs *h)
{
    CPULoongArchState *env = cpu_env(cs);
    for (int i = 0; i < 32; i++) {
        h->r[i] = env->gpr[i];
    }
}
static void inj_restore(CPUState *cs, const InjRegs *h)
{
    CPULoongArchState *env = cpu_env(cs);
    for (int i = 1; i < 32; i++) {
        env->gpr[i] = h->r[i];
    }
}
static int64_t inj_retval(CPUState *cs, int abi)
{
    return (int64_t)cpu_env(cs)->gpr[4];
}
/* s8 */
static bool inj_marker_ok(int abi, int nargs)
{
    return true;
}
static uint64_t inj_marker_get(CPUState *cs, int abi) { return cpu_env(cs)->gpr[31]; }
static void inj_marker_set(CPUState *cs, int abi, uint64_t v) { cpu_env(cs)->gpr[31] = v; }
static void inj_set_pc(CPUState *cs, uint64_t pc)
{
    cpu_env(cs)->pc = pc;
}
static void inj_set_call(CPUState *cs, int abi, const PandaInjectCall *c)
{
    CPULoongArchState *env = cpu_env(cs);

    env->gpr[11] = c->nr;
    for (int i = 0; i < 6; i++) {
        if (c->nargs < 0 || i < c->nargs) {
            env->gpr[4 + i] = c->args[i];
        }
    }
}
static G_NORETURN void inj_enter(CPUState *cs, int abi, uint64_t ret_pc, const PandaInjectCall *c)
{
    CPULoongArchState *env = cpu_env(cs);

    inj_set_call(cs, abi, c);
    env->pc = ret_pc - 4;
    cs->exception_index = EXCCODE_SYS;
    cpu_loop_exit(cs);
}

#else
#define INJ_NO_INJECTOR 1
#endif

#endif
