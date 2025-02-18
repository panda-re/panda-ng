#ifndef __LIBINTRO_WINDOWS_MANAGER_H
#define __LIBINTRO_WINDOWS_MANAGER_H

#include <memory>
#include <string>

#include <iohal/memory/common.h>
#include <offset/i_t.h>
#include <offset/offset.h>

#include "wintrospection.h"

class WindowsKernelManager
{
protected:
    std::string m_profile;
    std::shared_ptr<WindowsKernelOSI> m_kosi;

    bool m_initialized;

public:
    WindowsKernelManager(std::string profile) : m_profile(profile)
    {
        m_kosi = std::make_shared<WindowsKernelOSI>();
        m_initialized = false;
    }

    ~WindowsKernelManager()
    {
        m_kosi->system_vmem.reset();
        m_kosi.reset();
    }

    bool initialize(struct PhysicalMemory* interface, uint8_t pointer_width,
                    uint64_t system_asid, vm_addr_t kpcr, bool pae_enabled = false);

    struct WindowsKernelOSI* get_kernel_object() { return m_kosi.get(); }

    osi::i_t get_type(vm_addr_t address, std::string type);

    uint64_t get_swapcontext_offset() { return m_kosi->details.swapcontext_offset; }
};

class WindowsProcessManager
{
protected:
    std::shared_ptr<WindowsProcessOSI> m_posi;

    bool m_initialized;

public:
    WindowsProcessManager()
    {
        m_posi = std::make_shared<WindowsProcessOSI>();
        m_initialized = false;
    }

    ~WindowsProcessManager()
    {
        m_posi->vmem.reset();
        m_posi.reset();
    }

    bool initialize(struct WindowsKernelOSI* kosi, uint64_t eprocess = 0,
                    uint64_t pid = 0);

    struct WindowsProcessOSI* get_process_object() { return m_posi.get(); }

    osi::i_t get_process();

    osi::i_t get_type(vm_addr_t address, std::string type);
};

#endif
