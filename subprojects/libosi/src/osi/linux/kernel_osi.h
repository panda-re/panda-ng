#ifndef __LIBINTRO_KERNEL_OSI_H
#define __LIBINTRO_KERNEL_OSI_H

// boundary between kernel and user memory per process
#define PAGE_OFFSET_x86 0xc0000000
#define PAGE_OFFSET_x64 0xffffffff80000000

#define PAGE_SIZE 4096
#define KERNEL_THREAD_MASK 0x00200000

/**
 *  macro for getting the address of the thread_info structure given any kernel stack
 * address based on "Understanding the Linux Kernel" 3rd ed. - Pg. 85-87 ! note that
 * kernel stacks have been 8/16k since kernel v3.?, where this book assumes 4/8k
 *
 *  Method:
 *      (1) Divide pointer width by 2 (gives us the number of pages in kernel stack)
 *      (2) Multiply this by the size of the pages (gives us 8k or 16k)
 *      (3) Subtract 1 and get 1's complement (gives us a good mask for any kernel esp)
 *      (4) Mask kernel stack pointer (0 out bottom 8/16k -> gives us top of kernel stack)
 *      (5) Return this value (beginning of thread_info struct)
 */
#define GET_THREAD_INFO_FROM_ESP0(esp0, ptr_width)                                       \
    (esp0 & (~(((ptr_width / 2) * PAGE_SIZE) - 1)))

#endif
