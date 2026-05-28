/* SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 PicoKernel Project
 */

/**
 * @file hw_timer.h
 * @author datenbar
 * @date 27-05-2026
 * @brief Provides abstraction layer over the pico-sdk hardware timer API.
 * @ingroup drivers
 * @version 0.1.0
 *
 * @details
 * Provides abstraction layer over the pico-sdk API functions for the timer
 * present on the chip. This driver depends on the pico-sdk and provides easy
 * API functions to other subsystems.
 *
 * Constraints:
 * - Non-blocking API, Only queries
 * - Relies on the hardware timer on RP2350. So resolution and accuracy are
 * hardware dependent.
 *
 * Security:
 * - Must not be used for security-sensitive applications.
 * - Time values must not be used to make critical decisions without validation.
 *
 */

#ifndef HW_TIMER_H
#define HW_TIMER_H

#include <stdint.h>

/**
 * @brief Initializes the subsystem. Must be called before any other
 *        function in this header.
 *
 * @warning Calling any other function before this may result in
 *          undefined behavior.
 *
 * @return Nothing.
 */
void timer_driver_init(void);

/**
 * @brief Gives the time elapsed in ms since boot from hardware timer.
 *
 * @return uint32_t time elapsed from timer in miliseconds.
 */
uint32_t timer_driver_now_ms(void);

/**
 * @brief Gives the time elapsed in us since boot from hardware timer.
 *
 * @return uint64_t time elapsed from timer in microseconds.
 */
uint64_t timer_driver_now_us(void);

#endif
