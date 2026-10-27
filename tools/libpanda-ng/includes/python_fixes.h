typedef void* FILE;
#define __struct_FILE_defined 1
/* The host's va_list, so a struct holding one keeps gcc's layout. */
#if defined(__x86_64__)
typedef struct { unsigned int gp_offset, fp_offset; void *overflow_arg_area, *reg_save_area; } __builtin_va_list[1];
#elif defined(__aarch64__)
typedef struct { void *__stack, *__gr_top, *__vr_top; int __gr_offs, __vr_offs; } __builtin_va_list;
#else
#error "python_fixes.h: no va_list layout for this host"
#endif
/* __attribute__ stays: build.py's cffi_layout turns aligned and packed into declarations cffi
 * lays out like gcc, and drops the rest. */
#define __restrict
#define __inline
#define __USE_GNU
#define _BITS_STDIO2_H
#define __USE_FORTIFY_LEVEL 0
#define __STRINGS_FORTIFIED
#define _BITS_STRING_FORTIFIED_H
#define __G_UTILS_H__
/* cffi has no 128-bit integer. A union with gcc's __int128 size (16) and alignment (16 on the
 * x86_64 and aarch64 hosts, as long double) keeps every field after one where gcc puts it;
 * u64[0] is the low half on those (little-endian) hosts. */
typedef union { unsigned long long u64[2]; long double __cffi_align16[0]; } __cffi_int128;
#define __int128_t __cffi_int128
#define __int128 __cffi_int128
#define __uint128_t __cffi_int128
#define __extension__
#define __asm__(...)
#define _Static_assert(...)
#define __thread
#define asm(...)

/* The typedefs below use these; cffi needs them defined first. */
#include <stdint.h>
typedef uint64_t hwaddr;
typedef uint32_t MemTxResult;
typedef uint64_t MemTxAttrs;
typedef struct MemoryRegion MemoryRegion;
typedef struct CPUState CPUState;
typedef struct TranslationBlock TranslationBlock;