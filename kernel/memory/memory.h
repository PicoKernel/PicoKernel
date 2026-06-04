/* SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 PicoKernel Project
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

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Initializes the memory allocator. Must be called before any other
 *        function in this header.
 *
 * @note Takes a fixed size static pool of memory.
 *
 * @warning Calling any other function before this results in
 *          undefined behavior.
 *
 * @return Nothing.
 */
void allocator_init(void);

/**
 * @brief Allocates a memory block of requested size from the static pool.
 *
 * @param[in] size Number of bytes to allocate. Will be rounded up to the
 * nearest ALIGNMENT boundary.
 *
 * @note Does not zero out the block.
 *
 * @return Pointer to allocated payload on success.
 * @return NULL if size is 0, exceeds HEAP_SIZE, or heap is full.
 *
 */
void *kalloc(size_t size);

/**
 * @brief Marks the given memory block as free.
 *
 * @param[in] ptr Pointer to the payload of a block previously returned by
 * kalloc(). Passing NULL is safe and has no effect.
 *
 * @warning Triggers kernel_panic on heap corruption detection.
 * @warning Double-free is not detected (yet) and will corrupt the heap.
 *
 * @note Does not check for double-free (yet)
 *
 * @return Nothing.
 */
void kfree(void *);

#ifdef KERNEL_DEBUG
/**
 * @brief Walks the heap and prints each block's address, size, and canary
 * status.
 *
 * @warning ONLY FOR DEBUG BUILDS.
 *
 * @return Nothing.
 */
void kdump(void);
#endif
#endif

