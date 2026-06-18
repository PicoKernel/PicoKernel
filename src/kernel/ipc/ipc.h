/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file ipc.h
 * @author rootmnt
 * @date 14-06-2026
 * @brief Provide Inter-Process Communication (IPC) primitives for tasks.
 * @ingroup kernel
 *
 * @details
 * Provides a fixed-size ring-buffer message queue for data exchange between
 * tasks running under the cooperative scheduler.
 *
 * All operations are non-blocking and return immediately with a status code. A task that needs to wait on an
 * IPC primitive is responsible for checking the returned status, returning TASK_YIELD, and retrying on the next scheduler cycle.
 * The IPC subsystem itself never blocks, sleeps, or yields.
 *
 * Constraints:
 * - Caller-owned, statically allocated storage. No heap usage.
 * - Each queue has a fixed capacity and fixed element size, set once at init time and immutable afterwards.
 * - Non-blocking, polling-style API only.
 *
 * Security:
 * - Caller must initialize a queue before use.
 * - Caller must ensure that any buffer passed to k_queue_init() remains valid for the lifetime of the queue.
 * - Caller is responsible for ensuring the buffer is at least elem_size * capacity bytes.
 *   No runtime verification of buffer size is performed.
 */

#ifndef PICOKERNEL_KERNEL_IPC_H
#define PICOKERNEL_KERNEL_IPC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Status codes returned by IPC operations.
 * @ingroup kernel
 */
typedef enum {
    IPC_OK,         /**< Operation completed successfully.*/
    IPC_WOULDBLOCK, /**< Operation could not complete immediately.*/
    IPC_ERROR       /**< Invalid argument or uninitialized queue.*/
} ipc_status_t;

/**
 * @brief Fixed-size ring-buffer message queue.
 * @ingroup kernel
 *
 * Caller-owned. Must be initialized with k_queue_init() before use.
 * Fields are managed exclusively by the IPC module after initialization and must not be modified directly.
 *
 * A queue is considered uninitialized if buffer is NULL, elem_size is zero, or capacity is zero.
 */
typedef struct {
    uint8_t *buffer;  /**< Caller-provided backing storage, elem_size * capacity bytes.*/
    size_t elem_size; /**< Fixed size of each element in bytes.*/
    size_t capacity;  /**< Maximum number of elements the queue can hold.*/
    size_t count;     /**< Current number of elements stored.*/
    size_t head;      /**< Index of the next element to dequeue.*/
    size_t tail;      /**< Index of the next free slot to enqueue into.*/
} k_queue_t;

/**
 * @brief Initialize a fixed-size message queue.
 *
 * @details
 * Binds the queue to caller-provided backing storage and initializes it to an empty state.
 * The queue is a ring buffer: elements are written at the `tail` index and read from the `head` index.
 * Head and tail wrap back to the beginning when they reach the end of the buffer.
 *
 * @param[out] queue       Queue to initialize.
 * @param[in]  buffer      Caller-provided storage, at least `elem_size * capacity` bytes.
 * @param[in]  elem_size   Size of each element in bytes. Must be non-zero.
 * @param[in]  capacity    Maximum number of elements the queue can hold. Must be non-zero.
 *
 * @return IPC_OK on success.
 * @return IPC_ERROR if `queue` or `buffer` is NULL, or if `elem_size` or `capacity` is zero.
 * @return IPC_ERROR if `elem_size * capacity` would overflow `size_t`.
 *
 * @warning `buffer` must remain valid for the lifetime of the queue.
 * @warning Caller is responsible for ensuring the buffer is at least `elem_size * capacity` bytes.
 *          No runtime verification is performed.
 */
ipc_status_t k_queue_init(k_queue_t *queue, void *buffer, size_t elem_size, size_t capacity);

/**
 * @brief Enqueue a message.
 *
 * @details
 * Copies `elem_size` bytes from `msg` into the next free slot. Returns immediately with IPC_WOULDBLOCK
 * if the queue is full. No data is copied and the queue is left unchanged until the next successful call.
 *
 * @param[in,out] queue Initialized queue.
 * @param[in]     msg   Message to copy in, at least `elem_size` bytes.
 *
 * @return IPC_OK on success.
 * @return IPC_WOULDBLOCK if the queue is full.
 * @return IPC_ERROR if `queue` or `msg` is NULL, or if `queue` is
 *         uninitialized (NULL buffer, zero elem_size, or zero capacity).
 *
 * @note Not ISR-safe.
 * @note On IPC_ERROR, the caller should return TASK_ERROR to the scheduler.
 *       The IPC subsystem does not know about tasks and will not do this itself.
 */
ipc_status_t k_queue_send(k_queue_t *queue, const void *msg);

/**
 * @brief Dequeue a message.
 *
 * @details
 * Copies `elem_size` bytes from the oldest queued element into `msg`. Returns immediately with IPC_WOULDBLOCK
 * if the queue is empty. No data is copied and the queue is left unchanged until the next successful call.
 *
 * @param[in,out] queue Initialized queue.
 * @param[out]    msg   Buffer to receive the message, at least `elem_size` bytes.
 *
 * @return IPC_OK on success.
 * @return IPC_WOULDBLOCK if the queue is empty.
 * @return IPC_ERROR if `queue` or `msg` is NULL, or if `queue` is
 *         uninitialized (NULL buffer, zero elem_size, or zero capacity).
 *
 * @note Not ISR-safe.
 * @note On IPC_ERROR, the caller should return TASK_ERROR to the scheduler.
 *       The IPC subsystem does not know about tasks and will not do this itself.
 */
ipc_status_t k_queue_receive(k_queue_t *queue, void *msg);

/**
 * @brief Return the number of messages currently queued.
 *
 * @param[in] queue Initialized queue.
 *
 * @return Number of messages currently stored.
 * @return 0 if `queue` is NULL or uninitialized. An error is printed to stdout.
 */
size_t k_queue_count(const k_queue_t *queue);

/**
 * @brief Check whether a queue is full.
 *
 * @param[in] queue Initialized queue.
 *
 * @return true if the queue has no free slots.
 * @return false otherwise, or if `queue` is NULL or uninitialized. An error is printed to stdout.
 */
bool k_queue_is_full(const k_queue_t *queue);

/**
 * @brief Check whether a queue is empty.
 *
 * @param[in] queue Initialized queue.
 *
 * @return true if the queue holds no messages.
 * @return false otherwise, or if `queue` is NULL or uninitialized. An error is printed to stdout.
 */
bool k_queue_is_empty(const k_queue_t *queue);

#endif
