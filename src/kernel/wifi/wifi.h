/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file wifi.h
 * @author rootmnt
 * @date 24-07-2026
 * @brief Kernel WiFi subsystem API.
 * @ingroup kernel
 *
 * @details
 * Provides the public WiFi subsystem interface for PicoKernel.
 * Supports STA (client), AP (access point), and monitor-mode operation.
 * All implementation-specific details are encapsulated behind this API,
 * allowing kernel components and modules to remain independent of the
 * underlying WiFi implementation.
 *
 * @note Type definitions referenced by this API (such as d_wifi_* enums and structures)
 *       are declared in src/drivers/cyw43_wifi/cyw43_wifi.h.
 *
 * Constraints:
 * - k_wifi_init() must be called before any other function in this API.
 * - Monitor mode and STA/AP mode are mutually exclusive.
 * - Channel hopping requires monitor mode to already be active.
 * - Only 2.4 GHz channels 1-13 are currently supported.
 *
 * Security:
 * - Raw IEEE 802.11 frames are delivered to registered callbacks without
 *   interpretation by this subsystem.
 * - Authentication credentials supplied through this API are never exposed
 *   through read-back interfaces.
 */

#ifndef PICOKERNEL_KERNEL_WIFI_H
#define PICOKERNEL_KERNEL_WIFI_H

#include "cyw43_wifi/cyw43_wifi.h"

/**
 * @brief Initialise the WiFi subsystem.
 *
 * @details
 * Must be called once before any other function in this API.
 *
 * @return true on success, false if initialisation failed.
 */
bool k_wifi_init(void);

/**
 * @brief Tear down the WiFi subsystem.
 *
 * @details
 * Safe to call even if k_wifi_init() was never called or the subsystem has
 * already been deinitialised.
 */
void k_wifi_deinit(void);

/**
 * @brief Check whether the WiFi subsystem has been initialised.
 *
 * @return true if k_wifi_init() has succeeded and not since been deinitialised.
 */
bool k_wifi_is_initialized(void);

/**
 * @brief Start an asynchronous WiFi scan.
 *
 * @details
 * Requires the STA mode to be enabled. Results are written into
 * state as they arrive. Poll k_wifi_scan_active() to determine
 * completion.
 *
 * @param[out] state Caller-owned scan result buffer. Must remain valid
 *                   until the scan completes.
 * @param[in] opts   Scan filter/type options.
 *
 * @return 0 on success, negative error code if not initialised, STA mode
 *         is off, or the underlying scan call fails.
 */
int k_wifi_scan_start(d_wifi_scan_state_t *state, const d_wifi_scan_opts_t *opts);

/**
 * @brief Check whether a WiFi scan is currently in progress.
 *
 * @return true if a scan started by k_wifi_scan_start() is still running.
 */
bool k_wifi_scan_active(void);

/**
 * @brief Join a WiFi network in STA (client) mode.
 *
 * @param[in] params Connection parameters (SSID, key, auth mode, optional BSSID).
 *
 * @return 0 on success, negative error code on failure.
 *
 * @note Connecting to WEP networks is not supported and returns a
 *       negative error code if requested.
 *
 * @note This call is asynchronous. Poll k_wifi_link_status() afterward.
 */
int k_wifi_join(const d_wifi_connect_params_t *params);

/**
 * @brief Disassociate from the currently joined WiFi network.
 *
 * @return 0 on success, negative error code on failure.
 */
int k_wifi_leave(void);

/**
 * @brief Disable STA mode entirely.
 *
 * @details
 * Disconnects any active STA connection and tears down the STA interface.
 * No-op if STA mode is not currently enabled.
 */
void k_wifi_sta_disable(void);

/**
 * @brief Get the current STA link status.
 *
 * @return Current link status.
 */
d_wifi_link_status_t k_wifi_link_status(void);

/**
 * @brief Get the signal strength of the currently joined network.
 *
 * @param[out] rssi_out Populated with the RSSI value in dBm.
 *
 * @return 0 on success, negative error code on failure.
 */
int k_wifi_get_rssi(int32_t *rssi_out);

/**
 * @brief Get the STA interface's MAC address.
 *
 * @param[out] mac_out Populated with the 6-byte MAC address.
 *
 * @return 0 on success, negative error code on failure.
 */
int k_wifi_get_mac(uint8_t mac_out[6]);

/**
 * @brief Configure and bring up the AP interface.
 *
 * @details
 * Enables the access point using the currently configured SSID,
 * password, authentication mode, and channel.
 *
 * @note Hosting WEP-secured networks is not supported.
 *
 * @param[in] config Access point configuration to apply.
 */
void k_wifi_ap_enable(const d_wifi_ap_config_t *config);

/**
 * @brief Get the currently configured access point SSID.
 *
 * @param[out] len Populated with the number of valid bytes at the returned pointer.
 *
 * @return Pointer to the current SSID, valid for the lifetime of the subsystem
 *         and must be treated as read-only.
 */
const uint8_t *k_wifi_ap_get_ssid(size_t *len);

/**
 * @brief Get the currently configured access point authentication mode.
 *
 * @return Current AP auth mode.
 */
d_wifi_auth_t k_wifi_ap_get_auth(void);

/**
 * @brief Configure the access point operating channel.
 *
 * @details
 * The new channel takes effect the next time the access point is enabled.
 *
 * @param[in] channel WiFi channel (1-13).
 */
void k_wifi_ap_set_channel(uint8_t channel);

/**
 * @brief Configure the access point SSID.
 *
 * @details
 * The new SSID takes effect the next time the access point is enabled.
 *
 * @param[in] len Number of bytes in ssid.
 * @param[in] ssid SSID bytes to stage.
 */
void k_wifi_ap_set_ssid(size_t len, const uint8_t *ssid);

/**
 * @brief Configure the access point password.
 *
 * @details
 * Replaces the currently configured password. The new value takes
 * effect the next time the access point is enabled.
 *
 * Passwords supplied through this API are write-only and cannot be
 * retrieved through any public interface.
 *
 * @param[in] len Number of valid bytes in password.
 * @param[in] password Pointer to the password bytes.
 */
void k_wifi_ap_set_password(size_t len, const uint8_t *password);

/**
 * @brief Configure the access point authentication mode.
 *
 * @details
 * The new authentication mode takes effect the next time the access point is enabled.
 *
 * @param[in] auth Authentication mode.
 */
void k_wifi_ap_set_auth(d_wifi_auth_t auth);

/**
 * @brief Get the maximum number of stations the access point can support.
 *
 * @return Maximum supported station count, or 0 on error.
 */
int k_wifi_ap_get_max_stas(void);

/**
 * @brief Get the MAC addresses of stations currently connected to the access point.
 *
 * @param[out] macs Caller-owned buffer. Must hold at least
 *                  macs_capacity * 6 bytes.
 * @param[in] macs_capacity Number of MAC address slots available in
 *                          macs.
 *
 * @return Number of MAC addresses written into macs.
 */
int k_wifi_ap_get_stas(uint8_t *macs, int macs_capacity);

/**
 * @brief Get the access point interface's MAC address.
 *
 * @param[out] mac_out Populated with the 6-byte MAC address.
 *
 * @return 0 on success, negative error code on failure.
 */
int k_wifi_ap_get_mac(uint8_t mac_out[6]);

/**
 * @brief Tear down the access point interface.
 */
void k_wifi_ap_disable(void);

/**
 * @brief Enable or disable monitor (promiscuous) mode.
 *
 * @details
 * Rejects enabling monitor mode while a station connection or access
 * point is active. The caller must explicitly tear down those
 * interfaces first.
 *
 * @param[in] enable true to enable monitor mode, false to disable.
 *
 * @return 0 on success, negative error code if another interface is
 *         active or the requested operation fails.
 */
int k_wifi_set_monitor(bool enable);

/**
 * @brief Check whether monitor mode is currently active.
 *
 * @return true if monitor mode is enabled.
 */
bool k_wifi_monitor_is_active(void);

/**
 * @brief Set the radio's current channel while in monitor mode.
 *
 * @param[in] channel Channel number(1-13).
 *
 * @return 0 on success, negative error code if monitor mode is
 *         inactive, the channel is out of range, or the requested
 *         operation fails.
 */
int k_wifi_monitor_set_channel(uint8_t channel);

/**
 * @brief Start automatic channel hopping across channels 1-13 while in monitor mode.
 *
 * @details
 * Starts automatic channel hopping using the supplied dwell time.
 * Subsequent calls update the dwell time and restart hopping.
 *
 * @param[in] dwell_ms Time to remain on each channel before hopping, in milliseconds.
 *                     Values below MIN_HOP_DWELL_MS are rejected.
 *
 * @return 0 on success, negative error code if monitor mode is
 *         inactive or dwell_ms is below the minimum.
 */
int k_wifi_monitor_hop_start(uint16_t dwell_ms);

/**
 * @brief Stop automatic channel hopping.
 *
 * @details
 * Leaves the radio parked on whatever channel was active when hopping was stopped.
 */
void k_wifi_monitor_hop_stop(void);

/**
 * @brief Register a callback to receive raw captured IEEE 802.11 frames.
 *
 * @param[in] cb Function invoked for each received frame while monitor
 *               mode is active.
 * @param[in] ctx Opaque context pointer passed through to every
 *                callback invocation.
 *
 * @return 0 on success, negative error code if a callback is already registered.
 */
int k_wifi_monitor_register_rx_cb(d_wifi_raw_rx_cb_t cb, void *ctx);

/**
 * @brief Clear a previously registered raw capture callback.
 */
void k_wifi_monitor_unregister_rx_cb(void);

#endif
