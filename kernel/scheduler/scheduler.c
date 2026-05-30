/* SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 PicoKernel Project
 */

/**
 * @file scheduler.c
 * @author rootmnt
 * @date 30-05-2026
 * @brief Cooperative round-robin scheduler implementation.
 * @ingroup kernel
 * @version 0.1.0
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
 * @todo Implement blocking and wake-up mechanisms.
 *
 * @todo Replace the flat task table with a dynamically allocated linked-list
 *       TCBs when introducing preemptive scheduling.
 *
 * @todo Introduce a scheduler result/status API for task registration.
 */
#include "scheduler.h"
#include "../memory/memory.h"
#include "../panic/panic.h"
#include <stdio.h>

/**
 * Scheduler task table.
 *
 * Allocated during scheduler initialization and owned
 * exclusively by this module.
 */
static task_t *tasks = NULL;

/**
 * Number of active tasks currently managed by the scheduler.
 */
static size_t task_count = 0;

/**
 * Maximum number of task slots available in the task table.
 */
static size_t task_limit = 0;

/**
 * Next task identifier to be assigned.
 *
 * Incremented whenever a new task is registered.
 */
static int task_id_ctr = 1;

/**
 * Indicates whether task table compaction is required.
 *
 * Set when tasks enter COMPLETED or FAILED states.
 */
static int needs_compact = 0;

/**
 * @details
 * Triggers kernel_panic if kalloc returns NULL as the scheduler cannot operate
 * without a valid task table.
 *
 * @note This function is intended to be called once during kernel
 *       initialization.
 */
void scheduler_init(size_t size) {
  tasks = kalloc(sizeof(task_t) * size);
  if (tasks == NULL) {
    kernel_panic("scheduler: Failed to allocate task array\n");
  }
  task_limit = size;
  task_count = 0;
  task_id_ctr = 1;
  needs_compact = 0;
}

/**
 * @details
 * Assigns the next available task table slot to the
 * provided task entry function. New tasks are placed
 * in TASK_READY state and assigned a unique task ID.
 *
 * Returns without registering the task if the scheduler
 * is uninitialized, the task entry is NULL, or the task
 * table is at capacity.
 *
 * @note Task IDs are never reused, even after task
 *       removal through compaction.
 *
 * @warning The caller is responsible for ensuring that the task name remains
 *          valid for the lifetime of the task.
 */
void scheduler_register(const char *name, task_fn_t task) {
  if (tasks == NULL) {
    printf("scheduler: Not initialized\n");
    return;
  }
  if (task == NULL) {
    printf("scheduler: Cannot register NULL task\n");
    return;
  }
  if (task_count >= task_limit) {
    printf("scheduler: Task limit reached\n");
    return;
  }
  task_t *new_task = &tasks[task_count];
  new_task->id = task_id_ctr++;
  new_task->name = name;
  new_task->state = TASK_READY;
  new_task->entry = task;

  task_count++;
}

/**
 * @details
 * Executes all runnable tasks in registration order.
 *
 * Task state is updated according to the status returned
 * by the task entry function.
 *
 * Completed and failed tasks are removed during a cleanup
 * pass after execution.
 *
 * @note Cleanup is delayed until execution completion to avoid modifying the
 *       task table while it is being traversed.
 *
 * @note Yielded tasks currently behave the same as successfully completed
 *       iterations.
 */
void scheduler_run_once(void) {
  if (tasks == NULL || task_count == 0) {
    return;
  }

  for (size_t i = 0; i < task_count; i++) {
    task_t *task = &tasks[i];

    if (task->state != TASK_READY)
      continue;
    if (task->entry == NULL) {
      printf("scheduler: task [%d] '%s' has NULL entry, Marking FAILED\n",
             task->id, task->name);
      task->state = TASK_FAILED;
      needs_compact = 1;
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
      needs_compact = 1;
      break;

    case TASK_ERROR:
      task->state = TASK_FAILED;
      printf("scheduler: task [%d] '%s' failed\n", task->id, task->name);
      needs_compact = 1;
      break;

    default:
      task->state = TASK_FAILED;
      printf("scheduler: task [%d] '%s' returned unknown status\n", task->id,
             task->name);
      needs_compact = 1;
      break;
    }
  }

  if (!needs_compact)
    return;

  size_t free_slot = 0;
  for (size_t i = 0; i < task_count; i++) {
    if (tasks[i].state == TASK_COMPLETED || tasks[i].state == TASK_FAILED)
      continue;
    tasks[free_slot] = tasks[i];
    free_slot++;
  }
  task_count = free_slot;
  needs_compact = 0;
}
