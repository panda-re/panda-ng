#pragma once
// Minimal engine boundary for exercising the actual generated dispatch and PPP registry.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#define MAX(a, b) std::max(a, b)
#define MIN(a, b) std::min(a, b)
#include "plugin_plugin.h"
using target_ulong = uint64_t;
using target_ptr_t = uint64_t;
using target_long = int64_t;
struct CPUArchState { uint64_t regs[16]; };
struct CPUState { CPUArchState env; };
struct TranslationBlock {};
enum { R_EAX, R_ESP = 4, PANDA_CB_BEFORE_TCG_CODEGEN, MODE_ANY };
inline void *panda_cpu_env(CPUState *cpu) { return &cpu->env; }
