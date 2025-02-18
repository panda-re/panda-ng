#ifndef __MEMORY_I386_TRANSLATOR_H
#define __MEMORY_I386_TRANSLATOR_H

#include "iohal/memory/physical_memory.h"
#include "iohal/memory/virtual_memory.h"
#include <cstdlib>

namespace i386_translator
{

TranslateStatus translate_address(struct PhysicalMemory* pm, vm_addr_t vm_addr,
                                  pm_addr_t* pm_addr, pm_addr_t asid,
                                  TranslateProfile profile);
};

#endif
