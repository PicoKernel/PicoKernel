/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file time.c
 * @author rootmnt
 * @date 26-04-2026
 * @brief Implementation of kernel time subsystem.
 * @ingroup kernel
 *
 * @details
 * Provides a thin abstraction over the hardware timer driver.
 * All time queries are routed through this layer to ensure
 * a consistent time source across the kernel.
 *
 * Design notes:
 * - Delegates directly to timer driver (no additional state)
 * - Uses unsigned arithmetic for overflow-safe time comparisons
 *
 * Known limitations:
 * - Depends entirely on timer driver correctness
 * - No time synchronization (pure monotonic source)
 * - No protection against misuse of timestamps from other sources
 *
 * @todo [Kernel] [Enhancement] Time Sync from different sources to be implemented.
 */

#include "time.h"
#include "hw_timer/hw_timer.h"
#include <stdbool.h>

/**
 * @brief Initializes the kernel time drivers.
 */
void k_time_init(void)
{
    d_timer_init();
}

/**
 * @brief Returns system uptime in milliseconds. Direct wrapper over timer driver.
 */
uint32_t k_uptime_ms(void)
{
    return d_timer_now_ms();
}

/**
 * @brief Returns system uptime in microseconds. Direct wrapper over timer driver.
 */
uint64_t k_uptime_us(void)
{
    return d_timer_now_us();
}

/**
 * @brief Checks if a duration has elapsed since a given timestamp. Uses unsigned subtraction to remain correct across uint32_t wraparound.
 *
 * @note timestamp must originate from k_uptime_ms()
 */
bool k_time_elapsed(uint32_t timestamp, uint32_t duration_ms)
{
    return (k_uptime_ms() - timestamp) >= duration_ms;
}
