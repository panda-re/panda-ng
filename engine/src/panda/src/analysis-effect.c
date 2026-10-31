/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include "panda/analysis-effect.h"
#include "system/replay-effect.h"

bool panda_analysis_ram_write(uint64_t address, const uint8_t *data,
                              uint32_t length)
{
    return replay_analysis_ram_write_request(address, data, length);
}
