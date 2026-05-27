/* SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 PicoKernel Project
 */

/**
 * @file hw_timer.c
 * @author datenbar
 * @date 27-05-2026
 * @brief Implementation of hardware timer driver.
 * @ingroup drivers
 * @version 0.1.0
 *
 * @details
 * Provides a thin abstraction over the pico-sdk API. It uses the pico-sdk's
 * provided APIs for the hardware timer on the PCB.
 *
 * Known limitations:
 * - Depends entirely on pico-sdk
 *
 */

#include "pico/stdlib.h"
#include "pico/time.h"
#include "timer.h"
#include <stdint.h>

void driver_time_init(void) {
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
uint32_t driver_uptime_ms(void) {
  return to_ms_since_boot(get_absolute_time());
}

/**
 * Returns system uptime in microseconds.
 *
 * Direct wrapper over pico-sdk's timer API.
 */
uint64_t driver_uptime_us(void) {
  return to_us_since_boot(get_absolute_time());
}
