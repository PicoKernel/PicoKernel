/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file memory.h
 * @author datenbar
 * @date 29-05-2026
 * @brief Provides memory allocations from a static pool upon request.
 * @ingroup kernel
 *
 * @details
 * A simple memory allocator owns the static pool of memory when system boots
 * and provides blocks of memory as requested. It works on a static pool of
 * memory that it owns and hands out chunks from it. When the memory is freed,
 * the memory is coalesced into a single free block ready to be carved out
 * again.
 *
 * Constraints:
 * - Static pool of memory.
 * - Given memory is ALIGNMENT to ALIGNMENT macro defined in @file memory.c.
 * - Returns a void ptr type.
 *
 * Security:
 * - Implements a canary to check for metadata corruption, sends kernel panic
 *   request on corruption detection.
 * - Does not check for double-free (yet)
 */

#ifndef PICOKERNEL_KERNEL_MEMORY_H
#define PICOKERNEL_KERNEL_MEMORY_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Initialise the memory allocator subsystem.
 *
 * @details
 * Sets up the heap by placing a single free block spanning the entire
 * static heap buffer. Must be called before any other allocator function.
 *
 * @note Takes a fixed-size static pool, no dynamic sizing.
 *
 * @warning Calling more than once resets the heap and leaks all prior allocations.
 *
 * @warning Calling any other allocator function before this is undefined behaviour.
 *
 * @return Nothing.
 */
void k_alloc_init(void);

/**
 * @brief Allocate a block of memory from the static heap pool.
 *
 * @details
 * Searches the free list using a first-fit strategy. Splits the block if the
 * remainder is large enough to form a new free block. Writes a canary at the
 * end of the payload to detect overflow at free time. Does not zero the block.
 *
 * @param[in] size Number of bytes to allocate, will be rounded up to the nearest ALIGNMENT boundary.
 *
 * @return Pointer to the allocated payload on success.
 *
 * @return NULL if size is 0, exceeds HEAP_SIZE, or no suitable block is available.
 */
void *k_alloc(size_t size);

/**
 * @brief Mark a previously allocated block as free.
 *
 * @details
 * Validates the canary written at allocation time and calls k_panic() on
 * corruption. Coalesces the freed block with the immediately following free block.
 * Backward coalescing is not yet implemented.
 *
 * @param ptr Pointer to the payload returned by k_alloc(). NULL is safe and has no effect.
 *
 * @warning Passing a pointer not returned by k_alloc() is undefined behaviour.
 *
 * @warning Double-free is not detected and will corrupt the heap.
 *
 * @return Nothing.
 */
void k_free(void *);

#ifdef KERNEL_DEBUG
/**
 * @brief Walks the heap and prints each block's address, size, and canary
 * status.
 *
 * @warning ONLY FOR DEBUG BUILDS.
 *
 * @return Nothing.
 */
void k_dump(void);
#endif
#endif
