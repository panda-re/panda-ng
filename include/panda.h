#ifndef PANDA_H
#define PANDA_H

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define CONCAT(a, b) a##b
#define MAKE_HEADER(prefix, arch) TOSTRING(CONCAT(prefix, arch).h)

#ifdef __cplusplus
#include "cpp_sys_includes.h"
#define typename typename__
#define _Static_assert(...)
#define PANDA_HEADER MAKE_HEADER(autogen/panda_cpp_, PANDA_TGT)
#undef typename
extern "C" {
#else
#ifdef RUST_BINDGEN
#define _Static_assert(...)
#endif

#include "c_sys_includes.h"
#define PANDA_HEADER MAKE_HEADER(autogen/panda_c_, PANDA_TGT)
#endif

#include PANDA_HEADER
#include "plugin_plugin.h"
#include "debug.h"
#include "defines.h"

#ifdef __cplusplus
}
#endif
#endif