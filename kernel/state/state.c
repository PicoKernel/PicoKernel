/* SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 PicoKernel Project
 */

/**
 * @file state.c
 * @author rootmnt
 * @date 28-04-2026
 * @brief Implementation of kernel state subsystem.
 * @ingroup kernel
 * @version 0.1.0
 *
 * @details
 * Maintains internal kernel state and provides controlled updates to runtime
 * metrics.
 *
 * Design notes:
 * - State is statically allocated, no dynamic memory usage.
 * - State is modified only through this file to enforce consistency.
 * - Uptime is derived from the kernel time subsystem.
 *
 * Known limitations:
 * - No concurrency protection (assumes single-core, no preemption).
 */

#include "state.h"
#include "../time/time.h"

/**
 * Internal kernel state.
 *
 * Statically allocated and confined to this file to prevent external
 * modification.
 */
static kernel_state_t state = {
    .uptime_ms = 0,
    .commands_executed = 0,
};
/**
 * Returns the current kernel state.
 *
 * Exposes internal state as read-only.
 */
const kernel_state_t *kernel_get_state(void) { return &state; }

/**
 * Updates kernel uptime.
 *
 * Value is obtained from the kernel time subsystem.
 */
void kernel_state_uptime(void) { state.uptime_ms = kernel_uptime_ms(); }

/**
 * Records a command execution event.
 */
void kernel_state_record_command(void) { state.commands_executed++; }
