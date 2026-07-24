/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file wifi.c
 * @author rootmnt
 * @date 24-07-2026
 * @brief Implements the kernel WiFi subsystem interface.
 * @ingroup kernel
 *
 * @details
 * Provides a stable kernel-facing abstraction over the underlying WiFi
 * driver, allowing higher-level kernel components and modules to remain
 * independent of driver-specific implementation details.
 *
 * Design notes:
 * - This implementation is intentionally a thin forwarding layer. It
 *   introduces no additional buffering, scheduling, or policy beyond that
 *   provided by the underlying driver.
 * - Driver-specific types and implementation details are confined to this
 *   translation unit. Callers interact solely with the kernel WiFi API.
 *
 * Known limitations:
 * - The current implementation forwards all requests directly to the CYW43
 *   WiFi driver and therefore inherits its capabilities and constraints.
 */

#include "wifi.h"
#include "cyw43_wifi/cyw43_wifi.h"

/** @brief Initialise the kernel WiFi subsystem. */
bool k_wifi_init(void)
{
    return d_wifi_init();
}

/** @brief Deinitialise the kernel WiFi subsystem. */
void k_wifi_deinit(void)
{
    d_wifi_deinit();
}

/** @brief Check whether the WiFi subsystem has been initialised. */
bool k_wifi_is_initialized(void)
{
    return d_wifi_is_initialized();
}

/** @brief Enable or disable monitor mode. */
int k_wifi_set_monitor(bool enable)
{
    return d_wifi_set_monitor(enable);
}

/** @brief Check whether monitor mode is currently active. */
bool k_wifi_monitor_is_active(void)
{
    return d_wifi_monitor_is_active();
}

/** @brief Set the monitor-mode capture channel. */
int k_wifi_monitor_set_channel(uint8_t channel)
{
    return d_wifi_monitor_set_channel(channel);
}

/** @brief Start automatic monitor-mode channel hopping. */
int k_wifi_monitor_hop_start(uint16_t dwell_ms)
{
    return d_wifi_monitor_hop_start(dwell_ms);
}

/** @brief Stop automatic monitor-mode channel hopping. */
void k_wifi_monitor_hop_stop(void)
{
    d_wifi_monitor_hop_stop();
}

/** @brief Register a raw-frame receive callback. */
int k_wifi_monitor_register_rx_cb(k_wifi_raw_rx_cb_t cb, void *ctx)
{
    return d_wifi_monitor_register_rx_cb(cb, ctx);
}

/** @brief Unregister the current raw-frame receive callback. */
void k_wifi_monitor_unregister_rx_cb(void)
{
    d_wifi_monitor_unregister_rx_cb();
}
