#include "gtest/gtest.h"

#include "sparse_pmem.h"
#include <iohal/memory/virtual_memory.h>

#define TWO_GB (2 * 1024 * 1024 * 1024LL)

template <typename T> void set_uint(SparsePhysicalMemory* spm, T addr, T value)
{
    uint8_t* val = (uint8_t*)&value;
    spm->set_range(addr, val, sizeof(T));
}

bool init_physical_memory_amd64(void* opaque)
{
    SparsePhysicalMemory* spm = (SparsePhysicalMemory*)opaque;
    // Subset of the physical memory in emptymem.rr2 (2b4f0f13c07e7f10f3a907db33eb0bcb)
    // at recording index 0

    // PID 420 ASID 0x1c55a000 EPROC 0xfffffa80023d2740 NAME services.exe
    //  VA: 0x001e0123 -> 0x1c29f123
    //      000 -> 000 -> 000 -> 480
    uint64_t pml4e = 0x2d000001c54a867L;
    uint64_t pdpte = 0x13000001c44b867L;
    uint64_t pdge = 0x14000001c70c867L;
    uint64_t pte = 0xa60000001c29f867L;

    uint64_t pml4e_addr = 0x1c55a000 + 8 * 0;
    uint64_t pdpte_addr = (pml4e & 0xFFFFFFF000) + 8 * 0;
    uint64_t pdge_addr = (pdpte & 0xFFFFFFF000) + 8 * 0;
    uint64_t pte_addr = (pdge & 0xFFFFFFF000) + 8 * 480;

    set_uint(spm, pml4e_addr, pml4e);
    set_uint(spm, pdpte_addr, pdpte);
    set_uint(spm, pdge_addr, pdge);
    set_uint(spm, pte_addr, pte);

    // VA 0x7fefc600000 -> PA 0xec12345L
    // 015 pml4e 8000001c726867
    //   507 pdpte 71000001bfe3867
    //     483 pde f1000000edb0886 100010000110 USER LARGE
    pml4e = 0x8000001c726867;
    pdpte = 0x71000001bfe3867;
    pdge = 0xf1000000edb0886;
    pte = 0x0;
    pml4e_addr = 0x1c55a000 + 8 * 15;
    pdpte_addr = (pml4e & 0xFFFFFFF000) + 8 * 507;
    pdge_addr = (pdpte & 0xFFFFFFF000) + 8 * 483;
    set_uint(spm, pml4e_addr, pml4e);
    set_uint(spm, pdpte_addr, pdpte);
    set_uint(spm, pdge_addr, pdge);

    // VA 0x7feff5e9000 PA 0x23f6a123L
    // 015 pml4e 8000001c726867
    //  507 pdpte 71000001bfe3867
    //    506 pde f1000000edb0886 100010000110 USER LARGE
    //      489  pte 2630000023f6a025       100101 VALID USER  ADDR: 7feff5e9000
    pml4e = 0x8000001c726867;
    pdpte = 0x71000001bfe3867;
    pdge = 0xf1000000edb0886;
    pte = 0x2630000023f6a025;
    pml4e_addr = 0x1c55a000 + 8 * 15;
    pdpte_addr = (pml4e & 0xFFFFFFF000) + 8 * 507;
    pdge_addr = (pdpte & 0xFFFFFFF000) + 8 * 506;
    pte_addr = (pdge & 0xFFFFFFF000) + 8 * 489;
    set_uint(spm, pml4e_addr, pml4e);
    set_uint(spm, pdpte_addr, pdpte);
    set_uint(spm, pdge_addr, pdge);
    set_uint(spm, pte_addr, pte);

    // VA 0xfffffa8000e00000 PA
    // 501  0x3c00863
    //  000  0x3c01863
    //   007  0x3fc009e3
    pml4e = 0x3c00863;
    pdpte = 0x3c01863;
    pdge = 0x3fc009e3;
    pml4e_addr = 0x1c55a000 + 8 * 501;
    pdpte_addr = (pml4e & 0xFFFFFFF000) + 8 * 0;
    pdge_addr = (pdpte & 0xFFFFFFF000) + 8 * 7;
    set_uint(spm, pml4e_addr, pml4e);
    set_uint(spm, pdpte_addr, pdpte);
    set_uint(spm, pdge_addr, pdge);

    return true;
}

TEST(VmTranlatorAmd64Test, VTTranslateNormal)
{
    struct PhysicalMemory* pmem = createSparsePhysicalMemory(TWO_GB);
    VirtualMemoryTranslator amd64_trans(pmem, 64, 0x1c55a000, false, "unknown");

    ASSERT_TRUE(init_physical_memory_amd64(pmem->opaque));
    pm_addr_t pm_addr = 0;
    // PID 420 ASID 0x1c55a000 EPROC 0xfffffa80023d2740 NAME services.exe
    //  VA: 0x001e0123 -> 0x1c29f123
    amd64_trans.set_asid(0x1c55a000);
    ASSERT_TRUE(TRANSLATE_SUCCEEDED(amd64_trans.translate(0x001e0123, &pm_addr, 0)))
        << "Could not translate address";

    pm_addr_t target_addr = 0x1c29f000;
    for (auto ix = 0; ix < 1024; ++ix) {
        ASSERT_TRUE(
            TRANSLATE_SUCCEEDED(amd64_trans.translate(0x001e0000 + ix, &pm_addr, 0)))
            << "Could not translate address";
        ASSERT_EQ(pm_addr, target_addr + ix);
    }

    if (pmem) {
        pmem->free(pmem);
    }
}

// Test disabled until we figure out what the actual correct interpretation
// of this is. The in-transition pages seem too important to treat as
// paged out (i.e. common in our windows recordings)
// TEST(VmTranlatorAmd64Test, VTTranslateLargePagedOut)
//{
//    struct PhysicalMemory* pmem = createSparsePhysicalMemory(TWO_GB);
//    VirtualMemoryTranslator amd64_trans(pmem, 64, 0x1c55a000, false);
//
//    ASSERT_TRUE(init_physical_memory_amd64(pmem->opaque));
//    pm_addr_t pm_addr = 0;
//    // PID 420 ASID 0x1c55a000 EPROC 0xfffffa80023d2740 NAME services.exe
//    // VA 0x7fefc600000 -> PA 0xec12345L (paged out)
//    // Volatility disagrees, but VOL-3ABCD Figure 4-11 clearly indicates this should be
//    trated as paged out
//    amd64_trans.set_asid(0x1c55a000);
//    ASSERT_FALSE(TRANSLATE_SUCCEEDED(
//        amd64_trans.translate(0x7fefc600000, &pm_addr, 0)))
//        << "Could not translate address";
//
//    if (pmem) {
//        pmem->free(pmem);
//    }
//}

TEST(VmTranlatorAmd64Test, VTTranslateLarge)
{
    struct PhysicalMemory* pmem = createSparsePhysicalMemory(TWO_GB);
    VirtualMemoryTranslator amd64_trans(pmem, 64, 0x1c55a000, false, "unknown");

    ASSERT_TRUE(init_physical_memory_amd64(pmem->opaque));
    pm_addr_t pm_addr = 0;
    // PID 420 ASID 0x1c55a000 EPROC 0xfffffa80023d2740 NAME services.exe
    // VA 0xfffffa8000e00000
    // 501 000 007
    // Volatility disagrees, but VOL-3ABCD Figure 4-11 clearly indicates this should be
    // trated as paged out
    amd64_trans.set_asid(0x1c55a000);
    ASSERT_TRUE(
        TRANSLATE_SUCCEEDED(amd64_trans.translate(0xfffffa8000e00000, &pm_addr, 0)))
        << "Could not translate address";

    if (pmem) {
        pmem->free(pmem);
    }
}
