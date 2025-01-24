#define MEMTX_OK 0
#define bswap32(x) ((unsigned int)__builtin_bswap32(x))
#define bswap64(x) ((unsigned int)__builtin_bswap64(x))

/**
 * @brief Wrapper macro for quashing warnings for unused variables.
 */
#if defined(UNUSED)
#elif defined(__GNUC__)
#define UNUSED(x) x __attribute__((unused))
#elif defined(__LCLINT__)
#define UNUSED(x) /*@unused@*/ x
#else
#define UNUSED(x) x
#endif

#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x)   __builtin_expect(!!(x), 0)
#endif