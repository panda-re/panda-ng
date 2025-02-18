#include <cstring>

#include "iohal/memory/physical_memory.h"
#include "iohal/memory/virtual_memory_translator.h"
#include "vm_amd64.h"
#include "vm_i386.h"
#include "vm_i386_pae.h"

// We are using PIMPL here because we expect there to be a lot of
// updates / re-implementation of this class to be more efficient
// and we don't want to have to recompile all the client code when
// testing
class VirtualMemoryTranslator::Impl
{
private:
    struct PhysicalMemory* m_pmem;
    uint8_t m_bits;
    pm_addr_t m_asid;
    bool __attribute__((unused)) m_pae; /* Is there any way someone could be depending on this that precludes removal? */
    TranslateProfile m_profile;

public:
    Impl(struct PhysicalMemory* pmem, uint8_t bits, pm_addr_t asid, bool pae,
         const char* profile)
        : m_pmem(pmem), m_bits(bits), m_asid(asid), m_pae(pae)
    {
        if (strncmp(profile, "linux", 5) == 0) {
            m_profile = TPROF_GENERIC_LINUX;
        } else if (strncmp(profile, "windows", 7) == 0) {
            m_profile = TPROF_GENERIC_WINDOWS;
        } else {
            m_profile = TPROF_UNKNOWN;
        }
    }

    TranslateStatus translate(vm_addr_t vm_addr, pm_addr_t* pm_addr, pm_addr_t asid,
                              bool pae);

    pm_addr_t set_asid(pm_addr_t asid)
    {
        auto old_asid = m_asid;
        m_asid = asid;
        return old_asid;
    }

    pm_addr_t get_asid() { return m_asid; }

    void invalidate() {}
};

VirtualMemoryTranslator::VirtualMemoryTranslator(struct PhysicalMemory* pmem,
                                                 uint8_t bits, pm_addr_t asid, bool pae,
                                                 const char* profile)
{
    m_impl =
        std::make_unique<VirtualMemoryTranslator::Impl>(pmem, bits, asid, pae, profile);
}

TranslateStatus VirtualMemoryTranslator::translate(vm_addr_t vm_addr, pm_addr_t* pm_addr,
                                                   pm_addr_t asid, bool pae)
{
    return m_impl->translate(vm_addr, pm_addr, asid, pae);
}

pm_addr_t VirtualMemoryTranslator::set_asid(pm_addr_t asid)
{
    return m_impl->set_asid(asid);
}

pm_addr_t VirtualMemoryTranslator::get_asid() { return m_impl->get_asid(); }

void VirtualMemoryTranslator::invalidate() { m_impl->invalidate(); }

VirtualMemoryTranslator::~VirtualMemoryTranslator() = default;
VirtualMemoryTranslator::VirtualMemoryTranslator(VirtualMemoryTranslator&&) noexcept =
    default;
VirtualMemoryTranslator&
VirtualMemoryTranslator::operator=(VirtualMemoryTranslator&&) noexcept = default;

VirtualMemoryTranslator::VirtualMemoryTranslator(const VirtualMemoryTranslator& rhs)
    : m_impl(nullptr)
{
    if (rhs.m_impl) {
        m_impl = std::make_unique<Impl>(*rhs.m_impl);
    }
}

VirtualMemoryTranslator&
VirtualMemoryTranslator::operator=(const VirtualMemoryTranslator& rhs)
{
    if (!rhs.m_impl) {
        m_impl.reset();
    } else if (!m_impl) {
        m_impl = std::make_unique<Impl>(*rhs.m_impl);
    } else {
        *m_impl = *rhs.m_impl; // copy constrctor
    }
    return *this;
}

TranslateStatus VirtualMemoryTranslator::Impl::translate(vm_addr_t vm_addr,
                                                         pm_addr_t* pm_addr,
                                                         pm_addr_t asid, bool pae)
{
    if (asid == 0) {
        asid = m_asid;
    }

    // TODO cleaner disptach. this is a holdover from the original
    // implemenation of this library
    if (m_bits == 32 && pae == false) {
        return i386_translator::translate_address(m_pmem, vm_addr, pm_addr, asid,
                                                  m_profile);
    } else if (m_bits == 32 && pae == true) {
        return i386_pae_translator::translate_address(m_pmem, vm_addr, pm_addr, asid,
                                                      m_profile);
    } else if (m_bits == 64) {
        return amd64_translator::translate_address(m_pmem, vm_addr, pm_addr, asid,
                                                   m_profile);
    } else {
        return TSTAT_UNSUPPORTED_OPERATION;
    }
}
