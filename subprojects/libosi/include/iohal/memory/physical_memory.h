#ifndef __MEMORY_PHYSICAL_MEMORY_H
#define __MEMORY_PHYSICAL_MEMORY_H

#include "iohal/memory/common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct PhysicalMemory;

/**
 * \brief Return the maximum physical memory address
 *
 * \param this_pm the physical memory to query
 * \return largest valid memory address
 */
typedef pm_addr_t (*UpperBoundFunc)(struct PhysicalMemory* this_pm);

/**
 * \brief Read \p size bytes from \p addr into \p buffer.
 *
 * \param this_pm the physical memory to query
 * \param addr the physical address to start reading from
 * \param buffer the buffer to read into
 * \param size the number of bytes to read
 * \return true if the read completes successful
 */
typedef bool (*ReadPhysicalMemoryFunc)(struct PhysicalMemory* this_pm, pm_addr_t addr,
                                       uint8_t* buffer, uint64_t size);
/**
 * \brief free the PhysicalMemory object
 *
 * This function frees both \ref PhysicalMemory.opaque and \p this_pm itself
 *
 * \param this_pm the \ref PhysicalMemory to free
 */
typedef void (*FreePhysicalMemoryFunc)(struct PhysicalMemory* this_pm);

/**
 * \brief An interface to a physical memory source
 *
 * This struct is intended to wrap a class to make it easier to
 * expose as a C API. \ref PhysicalMemory.opaque is a pointer to
 * the backing implementation and \ref PhysicalMemory.upper_bound,
 * \ref PhysicalMemory.read, and \ref PhysicalMemory.free wrap member
 * functions.
 *
 */
struct PhysicalMemory {
    /** \brief flag for asserting the type of \ref PhysicalMemory.opaque */
    uint32_t tagvalue;

    /** \brief pointer to implementation
     *
     * Must only be freed with \ref PhysicalMemory.free
     */
    void* opaque;

    /** \brief \ref UpperBoundFunc implementation */
    UpperBoundFunc upper_bound;

    /** \brief \ref ReadPhysicalMemoryFunc implementation */
    ReadPhysicalMemoryFunc read;

    /** \brief \ref FreePhysicalMemoryFunc implementation */
    FreePhysicalMemoryFunc free;
};

/**
 * \brief Load a physical memory snapshot from a file
 *
 * This method is primarily used for testing
 * \param filepath the snapshot file to load
 * \return struct PhysicalMemory*
 */
struct PhysicalMemory* load_physical_memory_snapshot(const char* fpath);

#ifdef __cplusplus
}
#endif

#endif
