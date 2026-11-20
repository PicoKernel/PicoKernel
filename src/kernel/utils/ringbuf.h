/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file ringbuf.h
 * @author datenbar
 * @date 20-11-2026
 * @brief Lock-free single-producer single-consumer byte ring buffer.
 * @ingroup kernel
 *
 * @details
 * A fixed-capacity First-In First-Out (FIFO) queue of bytes backed by caller-provided storage.
 * One context pushes bytes (the producer) and one context pops them (the consumer). Typical use
 * is passing bytes from an Interrupt Service Routine (ISR), such as a Universal Asynchronous
 * Receiver-Transmitter (UART) receive handler, to the main loop or a cooperative task without
 * disabling interrupts. The producer and the consumer may also be the same context.
 *
 * The buffer never allocates. The caller owns the storage and must keep it alive for as long as
 * the ring is in use.
 *
 * Constraints:
 * - Exactly ONE producer and ONE consumer. Multiple producers or multiple consumers are not
 *   supported and will corrupt the buffer.
 * - Storage size must be a non-zero power of two (so indexing is a mask, not a modulo). The full
 *   size is usable, no slot is sacrificed.
 * - Requires lock-free 32-bit atomics, enforced at compile time.
 * - Elements are single bytes.
 * - Non-blocking. A full or empty ring is reported through the return value, never waited on.
 *
 * Security:
 * - No canary or bounds metadata. Indexing is masked, so a correctly initialised ring cannot
 *   write outside its storage.
 * - An uninitialised ring, or one initialised with an invalid size, is undefined behaviour. No
 *   panic is raised.
 * - Does not zero the storage. Stale bytes remain in memory after they are popped.
 */

#ifndef PICOKERNEL_KERNEL_RINGBUF_H
#define PICOKERNEL_KERNEL_RINGBUF_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The counters are uint32_t, but C only reports lock-free status per basic type. Require uint32_t
 * to be the same width as int, so the ATOMIC_INT_LOCK_FREE check covers them. */
_Static_assert((sizeof(int) == sizeof(uint32_t) && ATOMIC_INT_LOCK_FREE == 2) != 0, "ringbuf needs lock-free 32-bit atomics");

/**
 * @brief Ring buffer control block. Managed exclusively through the k_ring_*() functions.
 *
 * @details
 * @c head and @c tail are free-running counters. They are never reduced modulo the capacity and
 * wrap naturally at 2^32. The slot for a counter value is found with `counter & mask`. The
 * number of stored bytes is `head - tail`, which stays correct across wrap-around because the
 * capacity is a power of two.
 *
 * @ingroup kernel
 */
typedef struct {
    _Atomic uint32_t head; /**< Total bytes ever pushed. Written only by the producer. */
    _Atomic uint32_t tail; /**< Total bytes ever popped. Written only by the consumer. */
    uint32_t mask;         /**< Capacity minus one. Set by k_ring_init(), read-only afterwards. */
    uint8_t *storage;      /**< Caller-owned backing array of capacity bytes. Never freed here. */
} k_ringbuf_t;

/**
 * @brief Result codes returned by the ring buffer API.
 *
 * @ingroup kernel
 */
typedef enum {
    RING_ERR_OK = 0,       /**< Operation succeeded. */
    RING_ERR_INVALID = -1, /**< A pointer argument was NULL or the size was invalid. */
    RING_ERR_FULL = -2,    /**< Push rejected, the ring holds capacity bytes. */
    RING_ERR_EMPTY = -3,   /**< Pop rejected, the ring holds no bytes. */
} k_ring_err_t;

/**
 * @brief Initialise a ring buffer over caller-provided storage.
 *
 * @details
 * Resets @c head and @c tail to zero and records the storage and mask. The ring then holds
 * @p size bytes. The storage contents are left untouched. Must complete before the producer or
 * consumer touches the ring. On failure the ring is left unmodified.
 *
 * @param[in,out] ring    Ring control block to initialise. Must not be NULL.
 * @param[in]     storage Backing array of at least @p size bytes. Must not be NULL and must
 *                        outlive the ring.
 * @param[in]     size    Capacity in bytes. Must be a non-zero power of two.
 *
 * @return RING_ERR_OK on success.
 *         RING_ERR_INVALID if @p ring or @p storage is NULL, or @p size is zero or not a power
 *         of two.
 *
 * @note Context: thread, before the producer or consumer starts. Not safe while another context
 *       is using the ring.
 *
 * @warning Calling this on a ring in use discards its contents and races with the producer and
 *          the consumer.
 */
k_ring_err_t k_ring_init(k_ringbuf_t *ring, uint8_t *storage, uint32_t size);

/**
 * @brief Append one byte to the ring.
 *
 * @details
 * Writes @p byte at the head position, then publishes it to the consumer with a release store.
 * The byte is not visible to k_ring_pop() until the store completes. The ring is not modified
 * when it is full.
 *
 * @param[in,out] ring Initialised ring buffer. Must not be NULL.
 * @param[in]     byte Byte to store.
 *
 * @return RING_ERR_OK if the byte was stored.
 *         RING_ERR_FULL if the ring already holds capacity bytes. The byte is dropped.
 *         RING_ERR_INVALID if @p ring is NULL.
 *
 * @note Context: producer only. Safe from an ISR provided the consumer is a different context
 *       and no other producer exists. Requires prior k_ring_init().
 *
 * @warning The caller must handle RING_ERR_FULL. Ignoring it silently loses data.
 */
k_ring_err_t k_ring_push(k_ringbuf_t *ring, uint8_t byte);

/**
 * @brief Remove the oldest byte from the ring.
 *
 * @details
 * Reads the byte at the tail position into @p out, then releases the slot to the producer with a
 * release store. @p out is not modified when the ring is empty.
 *
 * @param[in,out] ring Initialised ring buffer. Must not be NULL.
 * @param[out]    out  Receives the popped byte on success. Must not be NULL.
 *
 * @return RING_ERR_OK if a byte was read into @p out.
 *         RING_ERR_EMPTY if the ring holds no bytes.
 *         RING_ERR_INVALID if @p ring or @p out is NULL.
 *
 * @note Context: consumer only. Requires prior k_ring_init().
 */
k_ring_err_t k_ring_pop(k_ringbuf_t *ring, uint8_t *out);

/**
 * @brief Check whether the ring holds no bytes.
 *
 * @details
 * Takes a snapshot of @c head and @c tail. From the consumer, false is reliable, because only the
 * consumer removes bytes, while true may be stale, because the producer can push right after the
 * check. From the producer, true is reliable and false may be stale.
 *
 * @param[in] ring Initialised ring buffer.
 *
 * @return true if the ring is empty, or if @p ring is NULL.
 *         false if the ring holds at least one byte.
 *
 * @note Context: either side (see the details for which answer to trust). Does not modify the
 *       ring. Requires prior k_ring_init().
 *
 * @warning A NULL @p ring reports true (empty), which is indistinguishable from a valid empty
 *          ring.
 */
bool k_ring_is_empty(const k_ringbuf_t *ring);

/**
 * @brief Check whether the ring can accept @p n more bytes.
 *
 * @details
 * Takes a snapshot of @c head and @c tail and compares the free space against @p n. Intended for
 * the producer. A true result is reliable from the producer, because only the producer adds
 * bytes, so the space cannot shrink before the push. A false result may be stale, because the
 * consumer can pop bytes right after the check. From the consumer the result is only a hint.
 *
 * @param[in] ring Initialised ring buffer.
 * @param[in] n    Number of bytes to check for, not a byte value. May be 0.
 *
 * @return true if at least @p n bytes of free space are available. Always true for @p n == 0 on
 *         a valid ring.
 *         false if fewer than @p n bytes are free, if @p n is larger than the ring capacity, or
 *         if @p ring is NULL.
 *
 * @note Context: producer only. Does not modify the ring. Requires prior k_ring_init().
 *
 * @warning A NULL @p ring reports false (no space), which is indistinguishable from a full ring.
 */
bool k_ring_has_space(const k_ringbuf_t *ring, size_t n);

#endif
