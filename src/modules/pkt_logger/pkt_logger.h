/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file pkt_logger.h
 * @author rootmnt
 * @date 18-07-2026
 * @brief Raw IEEE 802.11 packet capture module.
 * @ingroup modules
 *
 * @details
 * Captures raw IEEE 802.11 frames using the kernel WiFi monitor-mode
 * interface and prints captured frames to serial as hexadecimal dumps.
 * Frames are delivered asynchronously through a registered receive callback,
 * while a cooperative scheduler task processes and prints buffered frames.
 * Subsequent calls only reconfigure the capture mode.
 *
 * Constraints:
 * - m_pkt_logger_init() must be called after kernel WiFi and scheduler
 *   initialization, but before the scheduler begins executing tasks.
 * - When channel hopping is disabled, channel must be in the range 1-13.
 * - When channel hopping is enabled, channel must be 0 and dwell_ms must be
 *   greater than or equal to MIN_HOP_DWELL_MS (See @file cyw43_wifi.h).
 * - Frames are buffered for deferred processing by the scheduler task to
 *   minimize work performed in the receive callback. If the receive buffer
 *   is already occupied, newly received frames are discarded.
 *
 * Security:
 * - Captured frame contents are untrusted radio input and are only
 *   hex-dumped, never parsed or interpreted.
 *
 * @warning This module assumes cooperative, single-core execution. Its
 *          callback and scheduler interaction must be revisited if
 *          preemptive scheduling or multicore execution is introduced.
 */

#ifndef PICOKERNEL_MODULES_PKT_LOGGER_H
#define PICOKERNEL_MODULES_PKT_LOGGER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes or reconfigures packet capture.
 *
 * @details
 * Configures the kernel WiFi subsystem for monitor mode using either a
 * fixed capture channel or automatic channel hopping. On the first successful
 * call, registers the packet receive callback and packet logger task with the scheduler.
 * Subsequent calls only reconfigure the capture mode.
 *
 * @param[in] channel  Fixed IEEE 802.11 capture channel (1-13),
 *                     or 0 when channel hopping is enabled.
 * @param[in] dwell_ms Channel hopping dwell time in milliseconds.
 *                     Must be at least MIN_HOP_DWELL_MS (See @file cyw43_wifi.h) when hopping is enabled.
 * @param[in] hop      true to enable channel hopping,
 *                     false to use the fixed capture channel.
 *
 * @return true on successful initialization, false otherwise.
 */
bool m_pkt_logger_init(uint8_t channel, uint16_t dwell_ms, bool hop);

#endif
