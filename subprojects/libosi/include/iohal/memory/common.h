#ifndef __MEMORY_COMMON_H
#define __MEMORY_COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t pm_addr_t;
typedef uint64_t vm_addr_t;

#define PRIxVMADDR PRIx64
#define PRIxPMADDR PRIx64

#ifdef __cplusplus
}
#endif

#endif
