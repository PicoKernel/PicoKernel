/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file scheduler.c
 * @author rootmnt
 * @date 30-05-2026
 * @brief Cooperative round-robin scheduler implementation.
 * @ingroup kernel
 *
 * @details
 * Implements PicoKernel's cooperative task scheduler.
 *
 * Tasks are executed sequentially in registration order.
 * The scheduler relies on task return codes to determine
 * whether a task should continue executing, be removed,
 * or be marked as failed.
 *
 * Design notes:
 * - Cooperative scheduling chosen for simplicity.
 * - Tasks execute on the caller's stack.
 * - Dead tasks are removed using an array compaction pass.
 * - Compaction is performed after task execution to avoid
 *   modifying the task table during iteration.
 * - Task IDs remain unique for the lifetime of the system.
 *
 * Known limitations:
 * - No preemption.
 * - No task priorities.
 * - No blocking or wake-up primitives.
 * - Task table capacity is fixed after initialization.
 * - TASK_YIELD currently behaves the same as TASK_OK.
 *
 * @todo [Kernel] [Enhancement] Implement blocking and wake-up mechanisms.
 * @todo [Kernel] [Enhancement] Replace the flat task table with a dynamically allocated linked-list TCBs when introducing preemptive scheduling.
 * @todo [Kernel] [Enhancement] Introduce a scheduler result/status API for task registration.
 */
#include "scheduler.h"
#include "memory/memory.h"
#include "panic/panic.h"
#include <stdio.h>

/**
 * Scheduler task table.
 *
 * Allocated during scheduler initialization and owned
 * exclusively by this module.
 */
static task_t *k__tasks = NULL;

/**
 * Number of active tasks currently managed by the scheduler.
 */
static size_t k__task_count = 0;

/**
 * Maximum number of task slots available in the task table.
 */
static size_t k__task_limit = 0;

/**
 * Next task identifier to be assigned.
 *
 * Incremented whenever a new task is registered.
 */
static int k__task_id_ctr = 1;

/**
 * Indicates whether task table compaction is required.
 *
 * Set when tasks enter COMPLETED or FAILED states.
 */
static int k__needs_compact = 0;

/** @brief Allocate the task table and reset all scheduler state. */
void k_scheduler_init(size_t size)
{
    k__tasks = k_alloc(sizeof(task_t) * size);
    if (k__tasks == NULL) {
        k_panic("scheduler: Failed to allocate task array");
    }
    k__task_limit = size;
    k__task_count = 0;
    k__task_id_ctr = 1;
    k__needs_compact = 0;
}

/** @brief Assign the next free task slot to the provided entry function. */
void k_scheduler_register(const char *name, task_fn_t task)
{
    if (k__tasks == NULL) {
        k_panic("scheduler: k_scheduler_register() called before k_scheduler_init()");
    }
    if (name == NULL) {
        printf("scheduler: Cannot register task with NULL name\n");
        return;
    }
    if (task == NULL) {
        printf("scheduler: Cannot register NULL task\n");
        return;
    }
    if (k__task_count >= k__task_limit) {
        printf("scheduler: Task limit reached\n");
        return;
    }
    task_t *new_task = &k__tasks[k__task_count];
    new_task->id = k__task_id_ctr++;
    new_task->name = name;
    new_task->state = TASK_READY;
    new_task->entry = task;

    k__task_count++;
}

/** @brief Run all ready tasks once and compact dead tasks from the table. */
void k_scheduler_run_once(void)
{
    if (k__tasks == NULL || k__task_count == 0) {
        return;
    }

    const size_t cycle_k__task_count = k__task_count;

    for (size_t i = 0; i < cycle_k__task_count; i++) {
        task_t *task = &k__tasks[i];

        if (task->state != TASK_READY) {
            continue;
        }
        if (task->entry == NULL) {
            printf("scheduler: task [%d] '%s' has NULL entry, Marking FAILED\n",
                   task->id, task->name);
            task->state = TASK_FAILED;
            k__needs_compact = 1;
            continue;
        }

        task->state = TASK_RUNNING;
        task_status_t result = task->entry();
        switch (result) {

        case TASK_OK:
        case TASK_YIELD:
            task->state = TASK_READY;
            break;

        case TASK_DONE:
            task->state = TASK_COMPLETED;
            printf("scheduler: task [%d] '%s' completed\n", task->id, task->name);
            k__needs_compact = 1;
            break;

        case TASK_ERROR:
            task->state = TASK_FAILED;
            printf("scheduler: task [%d] '%s' failed\n", task->id, task->name);
            k__needs_compact = 1;
            break;

        default:
            task->state = TASK_FAILED;
            printf("scheduler: task [%d] '%s' returned unknown status\n", task->id,
                   task->name);
            k__needs_compact = 1;
            break;
        }
    }

    if (!k__needs_compact) {
        return;
    }

    size_t free_slot = 0;
    for (size_t i = 0; i < k__task_count; i++) {
        if (k__tasks[i].state == TASK_COMPLETED || k__tasks[i].state == TASK_FAILED) {
            continue;
        }
        k__tasks[free_slot] = k__tasks[i];
        free_slot++;
    }
    k__task_count = free_slot;
    k__needs_compact = 0;
}
