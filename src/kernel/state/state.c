/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file state.c
 * @author rootmnt
 * @date 28-04-2026
 * @brief Implementation of kernel state subsystem.
 * @ingroup kernel
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
#include "time/time.h"

/**
 * Internal kernel state.
 *
 * Statically allocated and confined to this file to prevent external
 * modification.
 */
static kernel_state_t k__state = {
    .uptime_ms = 0,
    .commands_executed = 0,
};

/**
 * Returns the current kernel state.
 *
 * Exposes internal state as read-only.
 */
const kernel_state_t *k_state_get(void)
{
    return &k__state;
}

/**
 * Updates kernel uptime.
 *
 * Value is obtained from the kernel time subsystem.
 */
void k_state_uptime(void)
{
    k__state.uptime_ms = k_uptime_ms();
}

/**
 * Records a command execution event.
 */
void k_state_record_command(void)
{
    k__state.commands_executed++;
}
