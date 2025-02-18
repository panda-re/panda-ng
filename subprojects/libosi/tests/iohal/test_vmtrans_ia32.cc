#include "gtest/gtest.h"

#include "sparse_pmem.h"

#include <iohal/memory/virtual_memory.h>

#define TWO_GB (2 * 1024 * 1024 * 1024LL)

template <typename T> void set_uint(SparsePhysicalMemory* spm, T addr, T value)
{
    uint8_t* val = (uint8_t*)&value;
    spm->set_range(addr, val, sizeof(T));
}

bool init_physical_memory_emptymem(void* opaque)
{
    SparsePhysicalMemory* spm = (SparsePhysicalMemory*)opaque;
    // Subset of the physical memory in emptymem.rr2 (f40c4025400e1fbef2966a1eb9d0ad02)
    // at recording index 0

    // PID 428 ASID 0x2312a000 EPROC 0x85666818 NAME services.exe
    //  VA: 0x75338000 -> PA: 0x39da000
    //      468 -> 824
    uint32_t pde_addr = 0x2312a000 + 4 * 468;
    uint32_t pde = 0x395ec867;
    uint32_t pte_addr = (pde & 0xFFFFF000) + 4 * 824;
    uint32_t pte = 0x39da005;
    set_uint(spm, pde_addr, pde);
    set_uint(spm, pte_addr, pte);

    //  VA: 0x75339000 -> PA: paged out
    //      468 -> 825
    pde_addr = 0x2312a000 + 4 * 468;
    pde = 0x395ec867;
    pte_addr = (pde & 0xFFFFF000) + 4 * 825;
    pte = 0x201ba554;
    set_uint(spm, pde_addr, pde);
    set_uint(spm, pte_addr, pte);

    return true;
}

// Sanity check that the object can be included, allocated, and freed

TEST(VmTranlatorIa32Test, VTTranslateNormal)
{
    struct PhysicalMemory* pmem = createSparsePhysicalMemory(TWO_GB);
    VirtualMemoryTranslator ia32_trans(pmem, 32, 0x2312a000, false, "unknown");

    ASSERT_TRUE(init_physical_memory_emptymem(pmem->opaque));
    pm_addr_t pm_addr = 0;

    // PID 428 ASID 0x2312a000 EPROC 0x85666818 NAME services.exe
    //  VA: 0x75338000 -> PA: 0x39da000
    ia32_trans.set_asid(0x2312a000);
    ASSERT_TRUE(TRANSLATE_SUCCEEDED(ia32_trans.translate(0x75338000, &pm_addr, 0)))
        << "Could not translate address";

    pm_addr_t target_addr = 0x39da000;
    for (auto ix = 0; ix < 1024; ++ix) {
        ASSERT_TRUE(
            TRANSLATE_SUCCEEDED(ia32_trans.translate(0x75338000 + ix, &pm_addr, 0)))
            << "Could not translate address";
        ASSERT_EQ(pm_addr, target_addr + ix);
    }

    //  VA: 0x75339000 -> PA: paged out
    //      468 -> 825
    auto status = ia32_trans.translate(0x75339000, &pm_addr, 0);
    ASSERT_EQ(TSTAT_PAGED_OUT, status) << "Did not detect address as paged out";

    if (pmem) {
        pmem->free(pmem);
    }
}

TEST(VmTranlatorIa32Test, VTTranslateOverrideAsid)
{
    struct PhysicalMemory* pmem = createSparsePhysicalMemory(TWO_GB);

    VirtualMemoryTranslator ia32_trans(pmem, 32, 0xdeadbeef, false, "unknown");

    ASSERT_TRUE(init_physical_memory_emptymem(pmem->opaque));
    pm_addr_t pm_addr = 0;
    // PID 428 ASID 0x2312a000 EPROC 0x85666818 NAME services.exe
    //  VA: 0x75338000 -> PA: 0x39da000
    uint64_t asid = 0x2312a000;

    ASSERT_TRUE(TRANSLATE_SUCCEEDED(ia32_trans.translate(0x75338000, &pm_addr, asid)))
        << "Could not translate address";

    pm_addr_t target_addr = 0x39da000;
    for (auto ix = 0; ix < 1024; ++ix) {
        ASSERT_TRUE(
            TRANSLATE_SUCCEEDED(ia32_trans.translate(0x75338000 + ix, &pm_addr, asid)))
            << "Could not translate address";
        ASSERT_EQ(pm_addr, target_addr + ix);
    }

    //  VA: 0x75339000 -> PA: paged out
    //      468 -> 825
    auto status = ia32_trans.translate(0x75339000, &pm_addr, asid);
    ASSERT_EQ(TSTAT_PAGED_OUT, status) << "Did not detect address as paged out";

    if (pmem) {
        pmem->free(pmem);
    }
}
