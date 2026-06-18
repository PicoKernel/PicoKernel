/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file ipc.c
 * @author rootmnt
 * @date 15-06-2026
 * @brief Implementation of IPC message queue primitives.
 * @ingroup kernel
 *
 * @details
 * Implements a fixed-size ring-buffer message queue for inter-task
 * communication under the cooperative scheduler.
 *
 * Design notes:
 * - Ring buffer state is tracked using three fields: head, tail, and count.
 *   Head is the index of the next element to dequeue, tail is the index
 *   of the next free slot to enqueue into, and count is the number of
 *   elements currently stored.
 * - Count is tracked explicitly rather than derived from head and tail.
 *   A ring buffer that relies solely on head and tail must reserve one
 *   unused slot to distinguish full from empty, reducing usable capacity
 *   by one element. Explicit count avoids this limitation and allows all
 *   configured slots to be used.
 * - Head and tail wrap back to the beginning of the buffer when they
 *   reach the end.
 * - Any state where head or tail falls outside the buffer bounds, or
 *   count exceeds capacity, indicates corruption of the queue structure
 *   and triggers k_panic().
 * - Element copies use memcpy bounded by elem_size, which is fixed at
 *   init time and immutable afterwards.
 * - No heap usage. All storage is caller-owned and caller-provided.
 *
 * Known limitations:
 * - Not ISR-safe (yet).
 * - No multicore protection (yet). Safe under cooperative single-core scheduling only.
 *
 * @todo [Kernel][Enhancement] Add queue synchronization when preemption
 *       or multicore execution is introduced.
 * @todo [Kernel][Enhancement] Add k_sem_t binary semaphore primitive
 *       when preemption or multicore is introduced. Back with SIO
 *       spinlocks for multicore safety.
 * @todo [Kernel][Enhancement] Implement k_queue_peek() to read the head
 *       element without dequeuing it.
 * @todo [Kernel][Enhancement] Implement k_queue_flush() to reset a queue
 *       to empty without reinitializing.
 */

#include "ipc.h"
#include "panic/panic.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Check whether a queue's internal state is consistent.
 *
 * Validates that head, tail, and count are all within bounds for the
 * given queue. Triggers k_panic if any invariant is violated, as this
 * indicates external corruption of the queue struct.
 *
 * @param[in] queue Queue to validate.
 */
static void k__queue_check_invariants(const k_queue_t *queue)
{
    if (queue->count > queue->capacity) {
        k_panic("ipc: queue count exceeds capacity, struct corrupted");
    }
    if (queue->head >= queue->capacity) {
        k_panic("ipc: queue head out of bounds, struct corrupted");
    }
    if (queue->tail >= queue->capacity) {
        k_panic("ipc: queue tail out of bounds, struct corrupted");
    }
}

/**
 * @brief Check whether a queue is uninitialized.
 *
 * A queue is considered uninitialized if buffer is NULL, elem_size is
 * zero, or capacity is zero.
 *
 * @param[in] queue Queue to check.
 *
 * @return true if the queue is uninitialized, false otherwise.
 */
static inline bool k__queue_is_uninitialized(const k_queue_t *queue)
{
    return (bool)(queue->buffer == NULL || queue->elem_size == 0 || queue->capacity == 0);
}

/**
 * @brief Initialize a fixed-size message queue.
 */
ipc_status_t k_queue_init(k_queue_t *queue, void *buffer, size_t elem_size, size_t capacity)
{
    if (queue == NULL || buffer == NULL || elem_size == 0 || capacity == 0) {
        return IPC_ERROR;
    }
    if (capacity > SIZE_MAX / elem_size) {
        return IPC_ERROR;
    }
    queue->buffer = (uint8_t *)buffer;
    queue->elem_size = elem_size;
    queue->capacity = capacity;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    return IPC_OK;
}

/**
 * @brief Enqueue a message.
 */
ipc_status_t k_queue_send(k_queue_t *queue, const void *msg)
{
    if (queue == NULL || msg == NULL) {
        return IPC_ERROR;
    }
    if (k__queue_is_uninitialized(queue)) {
        return IPC_ERROR;
    }
    k__queue_check_invariants(queue);
    if (queue->count == queue->capacity) {
        return IPC_WOULDBLOCK;
    }
    memcpy(queue->buffer + (queue->tail * queue->elem_size), msg, queue->elem_size);
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    return IPC_OK;
}

/**
 * @brief Dequeue a message.
 */
ipc_status_t k_queue_receive(k_queue_t *queue, void *msg)
{
    if (queue == NULL || msg == NULL) {
        return IPC_ERROR;
    }
    if (k__queue_is_uninitialized(queue)) {
        return IPC_ERROR;
    }
    k__queue_check_invariants(queue);
    if (queue->count == 0) {
        return IPC_WOULDBLOCK;
    }
    memcpy(msg, queue->buffer + (queue->head * queue->elem_size), queue->elem_size);
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    return IPC_OK;
}

/**
 * @brief Return the number of messages currently queued.
 */
size_t k_queue_count(const k_queue_t *queue)
{
    if (queue == NULL || k__queue_is_uninitialized(queue)) {
        printf("ipc: k_queue_count called on NULL or uninitialized queue\n");
        return 0;
    }
    return queue->count;
}

/**
 * @brief Check whether a queue is full.
 */
bool k_queue_is_full(const k_queue_t *queue)
{
    if (queue == NULL || k__queue_is_uninitialized(queue)) {
        printf("ipc: k_queue_is_full called on NULL or uninitialized queue\n");
        return false;
    }
    return queue->count == queue->capacity;
}

/**
 * @brief Check whether a queue is empty.
 */
bool k_queue_is_empty(const k_queue_t *queue)
{
    if (queue == NULL || k__queue_is_uninitialized(queue)) {
        printf("ipc: k_queue_is_empty called on NULL or uninitialized queue\n");
        return false;
    }
    return queue->count == 0;
}
