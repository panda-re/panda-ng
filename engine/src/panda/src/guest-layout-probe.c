/*
 * Compiled with -g for each softmmu target, only so that
 * panda/scripts/guest_layout.py can read CPUArchState's layout, and that of
 * every struct it points to, from this build's debug info: the layout map
 * panda/src/guest-write.c includes is generated from it (meson.build), so its
 * offsets are the ones this compiler gives on this host.
 *
 * A pointee whose definition isn't included here is an incomplete type in the
 * debug info, and the map won't follow it: include a header to make a pointer
 * followable.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "cpu.h"
#include "qemu/timer.h"
#include "qemu/rcu.h"
#include "qom/object.h"
#include "system/memory.h"
#include "system/ram-discard-manager.h"
#include "hw/core/cpu.h"
#include "hw/core/qdev.h"
#include "hw/core/clock.h"

/* Referenced so the compiler emits the type even without -fno-eliminate-unused-debug-types */
CPUArchState *guest_layout_probe;
