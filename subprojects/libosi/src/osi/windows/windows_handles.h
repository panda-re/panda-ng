#ifndef __LIBINTRO_WINDOWS_HANDLE_TABLE_H
#define __LIBINTRO_WINDOWS_HANDLE_TABLE_H

#include "osi/windows/wintrospection.h"
#include <offset/i_t.h>

osi::i_t resolve_handle_table_entry(struct WindowsProcessOSI* posi, uint64_t handle,
                                    bool x64);
osi::i_t resolve_handle_table_entry_win2000(struct WindowsProcessOSI* posi,
                                            uint32_t handle);
#endif
