/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file ringbuf.c
 * @author datenbar
 * @date 20-11-2026
 * @brief Implementation of the lock-free single-producer single-consumer (SPSC) byte ring buffer.
 * @ingroup kernel
 *
 * @details
 * Implements an SPSC First-In First-Out (FIFO) queue on top of C11 atomics. No locks and no
 * interrupt masking are used, so the producer may run in an Interrupt Service Routine (ISR)
 * while the consumer runs in the main loop.
 *
 * Design notes:
 * - Uses free-running 32-bit @c head and @c tail counters. The slot index is `counter & mask`.
 *   Occupancy is `head - tail`, which is correct across 2^32 wrap-around because the capacity
 *   is a power of two.
 * - Full is `head - tail == capacity`. Empty is `head - tail == 0`.
 * - Each index has exactly one writer. The producer owns @c head and the consumer owns @c tail,
 *   so no compare-and-swap is needed.
 * - Memory ordering: each side loads its own index relaxed, and the other side's index with
 *   acquire. After touching a slot it publishes its own index with a release store. The release
 *   on @c head makes the pushed byte visible before the consumer can see the new head. The
 *   release on @c tail guarantees the consumer has finished reading a slot before the producer
 *   can reuse it.
 *
 * Known limitations:
 * - Supports exactly one producer and one consumer.
 * - Moves one byte per call, no bulk push or pop (yet).
 * - Does not zero the storage (yet).
 * - Does not report the current fill level or whether the ring is full (yet).
 * - Uses its own error codes instead of the kernel-wide error enum (yet).
 *
 * @todo [Kernel][Enhancement] Implement bulk k_ring_write() and k_ring_read() to cut per-byte
 *       atomic overhead.
 * @todo [Kernel][Enhancement] Implement k_ring_is_full() and k_ring_count().
 * @todo [Kernel][Enhancement] Align k_ring_err_t with the kernel-wide error enum.
 */

#include "ringbuf.h"

#include <stdatomic.h>
#include <stdint.h>

/** @brief Validate the arguments and set up an empty ring over the given storage. */
k_ring_err_t k_ring_init(k_ringbuf_t *ring, uint8_t *storage, uint32_t size)
{
    if (!ring || !storage) {
        return RING_ERR_INVALID;
    }

    if (!size || ((size & (size - 1)) != 0)) {
        return RING_ERR_INVALID;
    }

    atomic_init(&ring->head, 0);
    atomic_init(&ring->tail, 0);
    ring->mask = size - 1;
    ring->storage = storage;
    return RING_ERR_OK;
}

/** @brief Store one byte at head if the ring is not full, then publish it with a release store. */
k_ring_err_t k_ring_push(k_ringbuf_t *ring, uint8_t byte)
{
    if (!ring) {
        return RING_ERR_INVALID;
    }

    uint32_t h = atomic_load_explicit(&ring->head, memory_order_relaxed);
    uint32_t t = atomic_load_explicit(&ring->tail, memory_order_acquire);

    if (h - t == (ring->mask + 1)) {
        return RING_ERR_FULL;
    }

    ring->storage[h & ring->mask] = byte;
    h++;

    atomic_store_explicit(&ring->head, h, memory_order_release);

    return RING_ERR_OK;
}

/** @brief Read one byte from tail if the ring is not empty, then release the slot. */
k_ring_err_t k_ring_pop(k_ringbuf_t *ring, uint8_t *out)
{
    if (!ring || !out) {
        return RING_ERR_INVALID;
    }

    uint32_t h = atomic_load_explicit(&ring->head, memory_order_acquire);
    uint32_t t = atomic_load_explicit(&ring->tail, memory_order_relaxed);

    if (h - t == 0) {
        return RING_ERR_EMPTY;
    }

    *out = ring->storage[t & ring->mask];
    t++;

    atomic_store_explicit(&ring->tail, t, memory_order_release);

    return RING_ERR_OK;
}

/** @brief Report whether head equals tail. A NULL ring is treated as empty. */
bool k_ring_is_empty(const k_ringbuf_t *ring)
{
    if (!ring) {
        return true;
    }

    uint32_t h = atomic_load_explicit(&ring->head, memory_order_acquire);
    uint32_t t = atomic_load_explicit(&ring->tail, memory_order_relaxed);

    return (h - t) == 0;
}

/** @brief Report whether the ring has at least n bytes of free space. A NULL ring returns false. */
bool k_ring_has_space(const k_ringbuf_t *ring, size_t n)
{
    if (!ring) {
        return false;
    }
    uint32_t capacity = ring->mask + 1;

    if (n > capacity) {
        return false;
    }

    uint32_t h = atomic_load_explicit(&ring->head, memory_order_relaxed);
    uint32_t t = atomic_load_explicit(&ring->tail, memory_order_acquire);

    uint32_t used = h - t;

    return (capacity - used) >= n;
}
