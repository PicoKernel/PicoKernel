/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file wifi.h
 * @author rootmnt
 * @date 24-07-2026
 * @brief Kernel WiFi subsystem interface.
 * @ingroup kernel
 *
 * @details
 * Provides the kernel-facing abstraction over the underlying WiFi driver.
 * Higher-level kernel components and modules interact with WiFi exclusively
 * through this interface rather than calling the driver directly. The
 * current implementation is a thin forwarding layer over the CYW43 driver;
 * additional policy or hardware abstraction may be introduced here without
 * affecting callers.
 *
 * Constraints:
 * - Intended for use by kernel components and modules only.
 * - Unless otherwise documented, all functions are synchronous wrappers
 *   around the underlying driver API.
 *
 * @warning This interface assumes cooperative, single-core execution.
 *          Synchronisation semantics may change if preemptive scheduling
 *          or multicore execution is introduced.
 */

#ifndef PICOKERNEL_KERNEL_WIFI_H
#define PICOKERNEL_KERNEL_WIFI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Callback invoked for each captured raw 802.11 frame.
 *
 * @param[in] buf Pointer to the captured frame bytes.
 * @param[in] len Number of valid bytes in the pointer buf.
 * @param[in] ctx Caller-supplied context pointer passed unchanged from
 *                k_wifi_monitor_register_rx_cb().
 */
typedef void (*k_wifi_raw_rx_cb_t)(const uint8_t *buf, size_t len, void *ctx);

/**
 * @brief Initialise the kernel WiFi subsystem.
 *
 * @return true if initialisation succeeded.
 * @return false otherwise.
 */
bool k_wifi_init(void);

/**
 * @brief Deinitialise the kernel WiFi subsystem.
 */
void k_wifi_deinit(void);

/**
 * @brief Check whether the WiFi subsystem has been initialised.
 *
 * @return true if initialised.
 * @return false otherwise.
 */
bool k_wifi_is_initialized(void);

/**
 * @brief Enable or disable monitor mode.
 *
 * @param[in] enable true to enable monitor mode, false to disable it.
 *
 * @return 0 on success, otherwise a negative error code.
 */
int k_wifi_set_monitor(bool enable);

/**
 * @brief Check whether monitor mode is currently active.
 *
 * @return true if monitor mode is active.
 * @return false otherwise.
 */
bool k_wifi_monitor_is_active(void);

/**
 * @brief Set the monitor-mode capture channel.
 *
 * @param[in] channel IEEE 802.11 channel (1-13).
 *
 * @return 0 on success, otherwise a negative error code.
 */
int k_wifi_monitor_set_channel(uint8_t channel);

/**
 * @brief Start automatic monitor-mode channel hopping.
 *
 * @param[in] dwell_ms Time to remain on each channel, in milliseconds.
 *
 * @return 0 on success, otherwise a negative error code.
 */
int k_wifi_monitor_hop_start(uint16_t dwell_ms);

/**
 * @brief Stop automatic monitor-mode channel hopping.
 */
void k_wifi_monitor_hop_stop(void);

/**
 * @brief Register a raw-frame receive callback.
 *
 * @param[in] cb  Callback invoked for each captured frame.
 * @param[in] ctx Caller-defined context pointer passed back to @p cb.
 *
 * @return 0 on success, otherwise a negative error code.
 */
int k_wifi_monitor_register_rx_cb(k_wifi_raw_rx_cb_t cb, void *ctx);

/**
 * @brief Unregister the currently registered raw-frame receive callback.
 */
void k_wifi_monitor_unregister_rx_cb(void);

#endif
