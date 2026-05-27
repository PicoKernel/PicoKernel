/* SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 PicoKernel Project
 */

/**
 * @file state.h
 * @author rootmnt
 * @date 28-04-2026
 * @brief Provides kernel runtime state management.
 * @ingroup kernel
 * @version 0.1.0
 *
 * @details
 * Defines the structure and APIs used by the kernel to track system-level
 * runtime information. The state is owned exclusively by the kernel and exposed
 * as read-only to other subsystems through controlled access functions.
 *
 * Constraints:
 * - State must contain only kernel-owned runtime data.
 * - External mutation is not allowed.
 *
 * Security:
 * - Direct modification is prevented by design.
 */

#ifndef STATE_H
#define STATE_H
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Kernel runtime state.
 *
 * @details
 * Stores minimal system-wide runtime information maintained by the kernel.The
 * structure is updated internally and exposed as read-only to external
 * components.
 *
 */
typedef struct state {
  uint32_t uptime_ms;         /**< System uptime in milliseconds.*/
  uint32_t commands_executed; /**< Total number of commands processed by the
                                 kernel.*/
} kernel_state_t;

/**
 * @brief Returns the current kernel state.
 *
 * @return Read-only pointer to the kernel state.
 *
 * @note The pointer remains valid for the lifetime of the system.
 */
const kernel_state_t *kernel_get_state(void);

/**
 * @brief Updates kernel uptime.
 *
 * @return Nothing.
 *
 * @note Must be called after kernel initialization.
 */
void kernel_state_uptime(void);

/**
 * @brief Records a command execution event.
 *
 * @return Nothing.
 */
void kernel_state_record_command(void);

#endif
