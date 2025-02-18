#ifndef __MEMORY_VIRTUAL_MEMORY_TRANSLATOR_H
#define __MEMORY_VIRTUAL_MEMORY_TRANSLATOR_H

#include "iohal/memory/common.h"
#include "iohal/memory/physical_memory.h"
#include <memory>
#define IA32_TRANSLATOR "ia32"
#define I386_TRANSLATOR "ia32"
#define AMD64_TRANSLATOR "amd64"
#define X86_64_TRANSLATOR "amd64"

enum TranslateStatus {
    TSTAT_SUCCESS = 0,
    TSTAT_GENERIC_FAILURE = 1,
    TSTAT_PAGED_OUT = 2,
    TSTAT_INVALID_ADDRESS = 3,
    TSTAT_UNSUPPORTED_OPERATION = 4
};

enum TranslateProfile {
    TPROF_UNKNOWN = 0,
    TPROF_GENERIC_WINDOWS = 1,
    TPROF_GENERIC_LINUX = 2
};

#define TRANSLATE_SUCCEEDED(status) ((status) == TSTAT_SUCCESS)

class VirtualMemoryTranslator
{
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

public:
    VirtualMemoryTranslator(struct PhysicalMemory* pmem, uint8_t bits, pm_addr_t asid,
                            bool pae, const char* profile);
    ~VirtualMemoryTranslator();

    VirtualMemoryTranslator(const VirtualMemoryTranslator& rhs);
    VirtualMemoryTranslator& operator=(const VirtualMemoryTranslator& rhs);

    VirtualMemoryTranslator(VirtualMemoryTranslator&&) noexcept;
    VirtualMemoryTranslator& operator=(VirtualMemoryTranslator&&) noexcept;

    /**
     * \brief Translate a virtual memory address
     *
     * \param vm_addr the virtual memory address to translate
     * \param pm_addr storage for translated address
     * \param asid the asid to use for this translation (or zero to use the
     * default)
     * \return outcome of the translation
     */
    TranslateStatus translate(vm_addr_t vm_addr, pm_addr_t* pm_addr, pm_addr_t asid = 0,
                              bool pae = false);

    /**
     * \brief set the default asid for translations
     *
     * \param asid the default asid to use for translations
     * \return the prior asid value
     */
    pm_addr_t set_asid(pm_addr_t asid);

    /**
     * \brief get the default asid for translations
     *
     * \return the asid value
     */
    pm_addr_t get_asid();

    /**
     * \brief invalidate any caches
     *
     */
    void invalidate();
};

#endif
