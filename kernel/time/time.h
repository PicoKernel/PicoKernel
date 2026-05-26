/**
 * @file time.h
 * @brief Provides kernel interface for various timer functions.
 *
 * @details
 * It depends on timer driver and offers timer functions to other parts of the
 * OS through the kernel. This keeps the time control centralized and in the
 * hands of the kernel. Every other module must inquire about time through the
 * kernel.
 *
 * Constraints:
 * - Non-blocking API, Only queries
 * - Relies on the hardware timer on RP2350. So resolution and accuracy are
 * hardware dependent.
 * - Time values may wrap around for uint32_t.
 *
 * Security:
 * - Must not be used for security-sensitive applications.
 * - Time values must not be used to make critical decisions without validation.
 *
 * @ingroup kernel
 * @author datenbar
 * @version 0.1.0
 * @date 25-04-2026
 * @copyright Copyright (c) 2026 PicoKernel Project.
 *            Licensed under the MIT License.
 */

#ifndef KERNEL_TIME_H
#define KERNEL_TIME_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes the subsystem. Must be called before any other
 *        function in this header.
 *
 * @note Non-blocking. Does not sleep or wait for hardware.
 *
 * @warning Calling any other function before this results in
 *          undefined behavior.
 *
 * @return Nothing.
 */
void kernel_time_init(void);

/**
 * @brief Gives the uptime of kernel in milliseconds.
 *
 * @return uint32_t time elapsed since last boot
 *         Wraps around in ~49 days.
 *
 * @note uint32_t wraps around in ~49 days.
 */
uint32_t kernel_uptime_ms(void);

/**
 * @brief Gives the uptime of kernel in microseconds.
 *
 * @return uint64_t time elapsed since last boot.
 *
 */
uint64_t kernel_uptime_us(void);

/**
 * @brief Check if a duration has elapsed since a given timestamp
 *
 * @param[in] timestamp Start time (from kernel_uptime_ms())
 * @param[in] duration_ms Duration to check in milliseconds
 *
 * @return true if the duration has elapsed, false otherwise.
 *
 * @note
 * - Uses unsigned subtraction to handle timer overflow safely
 * - timestamp must originate from kernel_uptime_ms()
 * - Caller must ensure timestamp is not user controlled.
 */
bool kernel_time_elapsed(uint32_t timestamp, uint32_t duration_ms);

#endif
