/*
 * The configurable machine's per-target CPU setup. The machine
 * (configurable-machine.c) is target-independent; this file is compiled for
 * every target and gives it what a board of that target would set up around
 * the CPU: MIPS's CP0 timer and interrupt lines, PPC's timebase and
 * decrementer. Without them a guest reading Count or the timebase
 * dereferences NULL in the host. A target with no setup here gets NULL and
 * the machine refuses it, naming the target.
 *
 * Set up here (tested by pandare2's tests/machine.py and tests/machine_raw.sh):
 * arm, aarch64, riscv32 (untested), riscv64 (generic CPU, GPIO interrupt
 * inputs; no ACLINT, so rdtime traps), mips, mipsel, mips64, mips64el (CP0
 * Count/Compare, 8 interrupt lines), ppc, ppc64 (timebase, decrementer;
 * BookE/e500/440, 40x and PPE42 CPUs get their families' timers),
 * i386, x86_64 (no interrupt inputs).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/core/cpu.h"
#include "hw/core/qdev.h"
#include "hw/core/boards.h"
#include "hw/core/configurable-cpu.h"
#include "elf.h"
#include "cpu.h"
#if defined(TARGET_MIPS)
#include "hw/core/clock.h"
#include "hw/core/qdev-clock.h"
#elif defined(TARGET_PPC)
#include "hw/ppc/ppc.h"
#include "exec/cputlb.h"
#endif

static G_GNUC_UNUSED CPUState *generic_create(const char *cpu_type,
                                              Error **errp)
{
    Object *obj = object_new(cpu_type);

    if (!qdev_realize(DEVICE(obj), NULL, errp)) {
        object_unref(obj);
        return NULL;
    }
    return CPU(obj);
}

#if defined(TARGET_ARM)

static CPUState *arm_create(const char *cpu_type, Error **errp)
{
    Object *obj = object_new(cpu_type);

    if (arm_feature(&ARM_CPU(obj)->env, ARM_FEATURE_M)) {
        error_setg(errp, "%s is an M-profile CPU: it needs an NVIC (the "
                   "armv7m container), which the configurable machine does "
                   "not create yet", cpu_type);
        object_unref(obj);
        return NULL;
    }
    if (!qdev_realize(DEVICE(obj), NULL, errp)) {
        object_unref(obj);
        return NULL;
    }
    return CPU(obj);
}

static int arm_elf_machine(CPUState *cpu)
{
#ifdef TARGET_AARCH64
    if (arm_feature(cpu_env(cpu), ARM_FEATURE_AARCH64)) {
        return EM_AARCH64;
    }
#endif
    return EM_ARM;
}

static const ConfigurableCpuOps ops = {
    .create = arm_create,
    .elf_machine = arm_elf_machine,
};

#elif defined(TARGET_RISCV)

static int riscv_elf_machine(CPUState *cpu)
{
    return EM_RISCV;
}

static const ConfigurableCpuOps ops = {
    .create = generic_create,
    .elf_machine = riscv_elf_machine,
};

#elif defined(TARGET_MIPS)

#define CONFIGURABLE_MIPS_CLOCK_HZ (200 * 1000 * 1000)

/* As mips_cpu_create_with_clock(), with errors to @errp. */
static CPUState *mips_create(const char *cpu_type, Error **errp)
{
    Clock *clk = clock_new(OBJECT(qdev_get_machine()), "cpu-refclk");
    DeviceState *dev;
    MIPSCPU *cpu;

    clock_set_hz(clk, CONFIGURABLE_MIPS_CLOCK_HZ);
    dev = qdev_new(cpu_type);
    qdev_connect_clock_in(dev, "clk-in", clk);
    if (!object_property_set_bool(OBJECT(dev), "big-endian",
                                  TARGET_BIG_ENDIAN, errp) ||
        !qdev_realize(dev, NULL, errp)) {
        object_unref(OBJECT(dev));
        return NULL;
    }
    cpu = MIPS_CPU(dev);
    /* What every MIPS board does: env->irq[0..7], and the CP0 timer. */
    cpu_mips_irq_init_cpu(cpu);
    cpu_mips_clock_init(cpu);
    return CPU(cpu);
}

static int mips_irq_count(CPUState *cpu)
{
    return ARRAY_SIZE(MIPS_CPU(cpu)->env.irq);
}

/* Lines 0-1 are the software interrupts, 2-7 the hardware ones (IP2-IP7). */
static qemu_irq mips_irq_in(CPUState *cpu, int n)
{
    return MIPS_CPU(cpu)->env.irq[n];
}

static int mips_elf_machine(CPUState *cpu)
{
    return EM_MIPS;
}

static const ConfigurableCpuOps ops = {
    .create = mips_create,
    .irq_count = mips_irq_count,
    .irq_in = mips_irq_in,
    .elf_machine = mips_elf_machine,
};

#elif defined(TARGET_PPC)

#define CONFIGURABLE_PPC_TB_HZ (100 * 1000 * 1000)

static CPUState *ppc_create(const char *cpu_type, Error **errp)
{
    CPUState *cs = generic_create(cpu_type, errp);

    CPUPPCState *env;

    if (!cs) {
        return NULL;
    }
    /*
     * The timebase and decrementer (env->tb_env), as that CPU family's
     * boards set them up: BookE (e500, 440) and 40x keep their FIT/watchdog
     * (and 40x PIT) state behind tb_env->opaque, which a guest write to
     * TCR or PIT dereferences.
     */
    env = cpu_env(cs);
    switch (env->excp_model) {
    case POWERPC_EXCP_BOOKE:
        ppc_booke_timers_init(POWERPC_CPU(cs), CONFIGURABLE_PPC_TB_HZ,
                              env->mmu_model == POWERPC_MMU_BOOKE206 ?
                              PPC_TIMER_E500 : 0);
        break;
    case POWERPC_EXCP_PPE42:
        ppc_booke_timers_init(POWERPC_CPU(cs), CONFIGURABLE_PPC_TB_HZ,
                              PPC_TIMER_PPE);
        break;
    case POWERPC_EXCP_40x:
        ppc_40x_timers_init(env, CONFIGURABLE_PPC_TB_HZ, PPC_INTERRUPT_PIT);
        break;
    default:
        cpu_ppc_tb_init(env, CONFIGURABLE_PPC_TB_HZ);
        break;
    }
    return cs;
}

static void ppc_reset(CPUState *cs)
{
    CPUPPCState *env = cpu_env(cs);

    cpu_ppc_tb_reset(env);
    if (env->mmu_model == POWERPC_MMU_BOOKE206) {
        /*
         * e500's MMU is always on and QEMU resets its TLBs empty, so no
         * code could run. As e500 boards do: TLB1 entry 0 maps the first
         * 1 GiB one to one (supervisor and user, read/write/execute).
         */
        ppcmas_tlb_t *tlb = booke206_get_tlbm(env, 1, 0, 0);

        tlb->mas1 = MAS1_VALID | (20 << MAS1_TSIZE_SHIFT);  /* 2^20 KiB */
        tlb->mas2 = 0;
        tlb->mas7_3 = MAS3_UR | MAS3_UW | MAS3_UX | MAS3_SR | MAS3_SW |
                      MAS3_SX;
        tlb_flush(cs);
    }
}

static int ppc_elf_machine(CPUState *cpu)
{
#ifdef TARGET_PPC64
    return EM_PPC64;            /* load_elf() takes EM_PPC files too */
#else
    return EM_PPC;
#endif
}

static const ConfigurableCpuOps ops = {
    .create = ppc_create,
    .reset = ppc_reset,
    .elf_machine = ppc_elf_machine,
};

#elif defined(TARGET_I386)

/*
 * No interrupt inputs: an x86 CPU takes interrupts from a PIC or its local
 * APIC (created with the CPU when it has one), and the configurable machine
 * wires nothing to them. The CPU resets to real mode; 'entry' sets EIP only.
 */
static CPUState *x86_create(const char *cpu_type, Error **errp)
{
    Object *obj = object_new(cpu_type);

    /* What x86_cpu_new() does for a one-CPU pc: APIC ID 0. */
    if (!object_property_set_uint(obj, "apic-id", 0, errp) ||
        !qdev_realize(DEVICE(obj), NULL, errp)) {
        object_unref(obj);
        return NULL;
    }
    return CPU(obj);
}

static int x86_elf_machine(CPUState *cpu)
{
#ifdef TARGET_X86_64
    return EM_X86_64;           /* load_elf() takes EM_386 files too */
#else
    return EM_386;
#endif
}

static const ConfigurableCpuOps ops = {
    .create = x86_create,
    .elf_machine = x86_elf_machine,
};

#else
#define CONFIGURABLE_NO_OPS
#endif

const ConfigurableCpuOps *configurable_cpu_ops(void)
{
#ifdef CONFIGURABLE_NO_OPS
    return NULL;
#else
    return &ops;
#endif
}
