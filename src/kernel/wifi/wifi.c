/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file wifi.c
 * @author rootmnt
 * @date 24-07-2026
 * @brief Implements the kernel WiFi subsystem.
 * @ingroup kernel
 *
 * @details
 * Implements the API declared in wifi.h by forwarding requests to the
 * underlying WiFi driver while presenting a stable, kernel-owned
 * interface. This subsystem provides a single entry point for station
 * (STA), access point (AP), and monitor-mode operation without exposing
 * driver-specific implementation details.
 *
 * Design notes:
 * - Acts as a thin abstraction layer over the underlying WiFi driver.
 * - Refer to src/drivers/cyw43_wifi/cyw43_wifi.h for WiFi-related
 *   structures, enums, and callback types used by this subsystem.
 * - Owns the public WiFi API exposed to kernel components and modules.
 * - AP configuration is staged by the underlying driver and applied when
 *   k_wifi_ap_enable() is called.
 * - Raw IEEE 802.11 frames are forwarded directly to registered
 *   callbacks without parsing or interpretation.
 *
 * Known limitations:
 * - Monitor mode and STA/AP operation are mutually exclusive. Requests
 *   to enable one while another is active are rejected.
 * - Raw frame delivery is best effort. Frame loss may occur under sustained
 *   receive load.
 */

#include "wifi.h"

/**
 * @brief Initialises the WiFi subsystem.
 */
bool k_wifi_init(void)
{
    return d_wifi_init();
}

/**
 * @brief Deinitialises the WiFi subsystem.
 */
void k_wifi_deinit(void)
{
    d_wifi_deinit();
}

/**
 * @brief Returns whether the WiFi subsystem is initialised.
 */
bool k_wifi_is_initialized(void)
{
    return d_wifi_is_initialized();
}

/**
 * @brief Starts an asynchronous WiFi scan.
 */
int k_wifi_scan_start(d_wifi_scan_state_t *state, const d_wifi_scan_opts_t *opts)
{
    return d_wifi_scan_start(state, opts);
}

/**
 * @brief Returns whether a WiFi scan is currently in progress.
 */
bool k_wifi_scan_active(void)
{
    return d_wifi_scan_active();
}

/**
 * @brief Joins a WiFi network in STA mode.
 */
int k_wifi_join(const d_wifi_connect_params_t *params)
{
    return d_wifi_join(params);
}

/**
 * @brief Leaves the currently connected WiFi network.
 */
int k_wifi_leave(void)
{
    return d_wifi_leave();
}

/**
 * @brief Disables STA mode.
 */
void k_wifi_sta_disable(void)
{
    d_wifi_sta_disable();
}

/**
 * @brief Returns the current STA link status.
 */
d_wifi_link_status_t k_wifi_link_status(void)
{
    return d_wifi_link_status();
}

/**
 * @brief Retrieves the current RSSI.
 */
int k_wifi_get_rssi(int32_t *rssi_out)
{
    return d_wifi_get_rssi(rssi_out);
}

/**
 * @brief Retrieves the STA interface MAC address.
 */
int k_wifi_get_mac(uint8_t mac_out[6])
{
    return d_wifi_get_mac(mac_out);
}

/**
 * @brief Configures and enables the AP interface.
 */
void k_wifi_ap_enable(const d_wifi_ap_config_t *config)
{
    d_wifi_ap_enable(config);
}

/**
 * @brief Retrieves the configured AP SSID.
 */
const uint8_t *k_wifi_ap_get_ssid(size_t *len)
{
    return d_wifi_ap_get_ssid(len);
}

/**
 * @brief Returns the configured AP authentication mode.
 */
d_wifi_auth_t k_wifi_ap_get_auth(void)
{
    return d_wifi_ap_get_auth();
}

/**
 * @brief Configures the AP operating channel.
 */
void k_wifi_ap_set_channel(uint8_t channel)
{
    d_wifi_ap_set_channel(channel);
}

/**
 * @brief Configures the AP SSID.
 */
void k_wifi_ap_set_ssid(size_t len, const uint8_t *ssid)
{
    d_wifi_ap_set_ssid(len, ssid);
}

/**
 * @brief Configures the AP password.
 */
void k_wifi_ap_set_password(size_t len, const uint8_t *password)
{
    d_wifi_ap_set_password(len, password);
}

/**
 * @brief Configures the AP authentication mode.
 */
void k_wifi_ap_set_auth(d_wifi_auth_t auth)
{
    d_wifi_ap_set_auth(auth);
}

/**
 * @brief Returns the maximum supported number of connected stations.
 */
int k_wifi_ap_get_max_stas(void)
{
    return d_wifi_ap_get_max_stas();
}

/**
 * @brief Retrieves the MAC addresses of connected stations.
 */
int k_wifi_ap_get_stas(uint8_t *macs, int macs_capacity)
{
    return d_wifi_ap_get_stas(macs, macs_capacity);
}

/**
 * @brief Retrieves the AP interface MAC address.
 */
int k_wifi_ap_get_mac(uint8_t mac_out[6])
{
    return d_wifi_ap_get_mac(mac_out);
}

/**
 * @brief Disables the AP interface.
 */
void k_wifi_ap_disable(void)
{
    d_wifi_ap_disable();
}

/**
 * @brief Enables or disables monitor mode.
 */
int k_wifi_set_monitor(bool enable)
{
    return d_wifi_set_monitor(enable);
}

/**
 * @brief Returns whether monitor mode is active.
 */
bool k_wifi_monitor_is_active(void)
{
    return d_wifi_monitor_is_active();
}

/**
 * @brief Sets the monitor mode channel.
 */
int k_wifi_monitor_set_channel(uint8_t channel)
{
    return d_wifi_monitor_set_channel(channel);
}

/**
 * @brief Starts automatic monitor mode channel hopping.
 */
int k_wifi_monitor_hop_start(uint16_t dwell_ms)
{
    return d_wifi_monitor_hop_start(dwell_ms);
}

/**
 * @brief Stops automatic monitor mode channel hopping.
 */
void k_wifi_monitor_hop_stop(void)
{
    d_wifi_monitor_hop_stop();
}

/**
 * @brief Registers a monitor mode receive callback.
 */
int k_wifi_monitor_register_rx_cb(d_wifi_raw_rx_cb_t cb, void *ctx)
{
    return d_wifi_monitor_register_rx_cb(cb, ctx);
}

/**
 * @brief Unregisters the monitor mode receive callback.
 */
void k_wifi_monitor_unregister_rx_cb(void)
{
    d_wifi_monitor_unregister_rx_cb();
}
