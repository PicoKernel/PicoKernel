/**
 * @file time.c
 * @brief Implementation of kernel time subsystem.
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
 * @ingroup kernel
 * @author datenbar
 * @version 0.1.0
 * @date 26-04-2026
 * @copyright Copyright (c) 2025 PicoKernel Project.
 *            Licensed under the MIT License.
 *
 * @todo Time Sync from different sources to be implemented .
*/

#include"time.h"
#include"../../drivers/hw_timer/hw_timer.h"
#include<stdbool.h>

/**
 * Returns system uptime in milliseconds.
 *
 * Direct wrapper over timer driver.
*/
uint32_t kernel_uptime_ms(void) {
    return timer_driver_now_ms();
}


/**
 * Returns system uptime in microseconds.
 *
 * Direct wrapper over timer driver.
*/
uint64_t kernel_uptime_us(void) {
    return timer_driver_now_us();
}


/**
 * Checks if a duration has elapsed since a given timestamp.
 *
 * Uses unsigned subtraction to remain correct across
 * uint32_t wraparound.
 *
 * @note
 * - timestamp must originate from kernel_uptime_ms()
*/
bool kernel_time_elapsed(uint32_t timestamp, uint32_t duration_ms) {
    return (kernel_uptime_ms() - timestamp) >= duration_ms;
}