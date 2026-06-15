/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file scheduler.h
 * @author rootmnt
 * @date 30-05-2026
 * @brief Cooperative round-robin task scheduler.
 * @ingroup kernel
 *
 * @details
 * Provides a cooperative task scheduling service for PicoKernel.
 * Tasks are registered with the scheduler and executed sequentially in
 * round-robin order. The scheduler does not perform preemption, context
 * switching, or time slicing. Tasks are expected to return control voluntarily
 * by returning a task status code. Completed and failed tasks are automatically
 * removed during the scheduler cleanup phase.
 *
 * Constraints:
 * - Must be initialized before use.
 * - No preemption.
 * - No task priorities.
 * - Tasks execute on the caller's stack.
 * - Single-core operation only.
 * - Not safe for ISR context.
 *
 * Security:
 * - Caller must ensure task functions are valid.
 * - Scheduler validates NULL task entries.
 * - Input names are not sanitized by this module.
 */
#ifndef PICOKERNEL_KERNEL_SCHEDULER_H
#define PICOKERNEL_KERNEL_SCHEDULER_H

#include <stddef.h>

/**
 * @brief Status code returned by a task.
 *
 * Returned by task entry functions to inform the scheduler how the task should
 * be handled after execution.
 */
typedef enum {
    TASK_OK,    /**< Task executed successfully.*/
    TASK_YIELD, /**< Task voluntarily yielded execution.*/
    TASK_DONE,  /**< Task completed permanently.*/
    TASK_ERROR  /**< Task encountered an unrecoverable error.*/
} task_status_t;

/**
 * @brief Task function executed by the scheduler.
 *
 * @return Task status code.
 */
typedef task_status_t (*task_fn_t)(void);

/**
 * @brief Internal lifecycle state of a task.
 *
 * Managed exclusively by the scheduler.
 */
typedef enum {
    TASK_READY,     /**< Eligible for execution.*/
    TASK_RUNNING,   /**< Currently executing.*/
    TASK_BLOCKED,   /**< Reserved for future use.*/
    TASK_COMPLETED, /**< Finished execution permanently.*/
    TASK_FAILED     /**< Terminated due to an error.*/
} task_state_t;

/**
 * @brief Task Control Block (TCB).
 *
 * Stores metadata required by the scheduler to manage a task throughout its
 * lifetime.
 *
 * Ownership:
 * - Allocated and managed by the scheduler.
 * - Must not be modified by modules directly.
 */
typedef struct {
    int id;             /**< Unique task identifier.*/
    const char *name;   /**< Human-readable task name.*/
    task_state_t state; /**< Current scheduler state.*/
    task_fn_t entry;    /**< Task entry function.*/
} task_t;

/**
 * @brief Initialise the scheduler subsystem.
 *
 * @details
 * Allocates storage for the task table from the heap via k_alloc().
 * Sets task count, limit, and internal counters to their initial state.
 * Must be called once during kernel initialisation before any other
 * scheduler function.
 *
 * @param[in] size Maximum number of task slots to allocate.
 *
 * @warning Calls k_panic() if heap allocation fails.
 *
 * @warning Calling any other scheduler function before this is undefined behaviour.
 *
 * @return Nothing.
 */
void k_scheduler_init(size_t size);

/**
 * @brief Register a task with the scheduler.
 *
 * @details
 * Assigns the next available slot in the task table to the provided entry
 * function. The task is placed in TASK_READY state and assigned a unique
 * ID that is never reused, even after compaction.
 *
 * Returns without registering if name or entry is NULL, or if the task
 * table is at capacity. Calls k_panic() if called before k_scheduler_init().
 *
 * @param name Human-readable task name. Must remain valid for the lifetime of the task.
 *
 * @param task Task entry function pointer.
 *
 * @warning Calls k_panic() if the scheduler is uninitialised.
 *
 * @warning Passing a stack-allocated name that goes out of scope is undefined behaviour.
 *
 * @return Nothing.
 */
void k_scheduler_register(const char *name, task_fn_t task);

/**
 * @brief Execute one full scheduler cycle.
 *
 * @details
 * Runs all TASK_READY tasks in registration order. Task count is snapshotted
 * at the start of the cycle, tasks registered mid-cycle are deferred to the
 * next call. After execution, TASK_COMPLETED and TASK_FAILED tasks are removed
 * via array compaction. TASK_YIELD currently behaves identically to TASK_OK.
 *
 * @note Returns immediately if the task table is uninitialised or empty.
 *
 * @warning Not safe for ISR context.
 *
 * @return Nothing.
 */
void k_scheduler_run_once(void);

#endif
