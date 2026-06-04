/* SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 PicoKernel Project
 */

/**
 * @file hw_timer.c
 * @author datenbar
 * @date 27-05-2026
 * @brief Implementation of hardware timer driver.
 * @ingroup drivers
 *
 * @details
 * Provides a thin abstraction over the pico-sdk API. It uses the pico-sdk's
 * provided APIs for the hardware timer on the PCB.
 *
 * Known limitations:
 * - Depends entirely on pico-sdk
 *
 */

#include "hw_timer.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include <stdint.h>

void timer_driver_init(void) {
  /**
   * @todo Implement later
   *
   */
}

/**
 * Returns system uptime in milliseconds.
 *
 * Direct wrapper over pico-sdk's timer API.
 */
uint32_t timer_driver_now_ms(void) {
  return to_ms_since_boot(get_absolute_time());
}

/**
 * Returns system uptime in microseconds.
 *
 * Direct wrapper over pico-sdk's timer API.
 */
uint64_t timer_driver_now_us(void) {
  return to_us_since_boot(get_absolute_time());
}
