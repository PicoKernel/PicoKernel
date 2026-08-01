/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file cyw43_wifi.h
 * @author datenbar
 * @date 12-07-2026
 * @brief driver API for the CYW43 WiFi driver (STA, AP, and monitor-mode capture).
 * @ingroup drivers
 *
 * @details
 * Wraps the vendored CYW43 driver (cyw43.h / cyw43_ll.h) for PicoKernel. Provides
 * STA (client) and AP (access point) lifecycle management, and a monitor-mode
 * raw 802.11 capture path. All CYW43 SDK types and value
 * encodings (auth modes, link status, security bitmasks) are translated at
 * this boundary; no CYW43 SDK type is exposed to callers of this header.
 *
 * Constraints:
 * - d_wifi_init() must be called before any other function in this API.
 * - Monitor mode and STA/AP mode are mutually exclusive on the CYW43439 radio and
 *   d_wifi_set_monitor(true) fails if either is active.
 * - Channel hopping (d_wifi_monitor_hop_*) requires monitor mode to already
 *   be active.
 * - Only channels 1-13 (2.4GHz) are valid, this chip has no 5/6GHz radio.
 *
 * Security:
 * - Raw capture callback data (d_wifi_raw_rx_cb_t) is handed to the caller
 *   unparsed. This driver does not inspect 802.11 frame contents.
 * - AP password material can be set (d_wifi_ap_set_password) but is never
 *   exposed via a getter.
 */

#ifndef PICOKERNEL_DRIVERS_CYW43_WIFI_H
#define PICOKERNEL_DRIVERS_CYW43_WIFI_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define WIFI_MAX_SCAN_RESULTS (20)  /**< @brief Maximum number of results a single scan can collect. */
#define MIN_HOP_DWELL_MS      (100) /**< @brief Minimum permitted dwell time per channel, in ms. */

/**
 * @brief Wifi link status, mirroring CYW43_LINK_* values.
 *
 * @details
 * Values are pinned explicitly to match the underlying CYW43_LINK_*
 * macros exactly (including the negative failure codes) rather than relying
 * on default sequential enum assignment, since cyw43_wifi_link_status() can
 * return negative values directly.
 */
typedef enum {
    D_WIFI_LINK_FAIL = -1,    /**< Connection attempt failed.*/
    D_WIFI_LINK_NONET = -2,   /**< No matching SSID found (out of range or down).*/
    D_WIFI_LINK_BADAUTH = -3, /**< Authentication failure.*/
    D_WIFI_LINK_DOWN = 0,     /**< Link is down.*/
    D_WIFI_LINK_JOIN = 1,     /**< Connected to wifi.*/
    D_WIFI_LINK_NOIP = 2,     /**< Connected to wifi, no IP address yet.*/
    D_WIFI_LINK_UP = 3        /**< Connected to wifi with an IP address.*/
} d_wifi_link_status_t;

/**
 * @brief Wifi authentication / security mode.
 *
 * @details
 * Translated to/from the underlying CYW43_AUTH_* macros internally by this
 * driver. Values here do not numerically match cyw43.h and must never be
 * passed to a CYW43 SDK call without going through the driver's internal
 * conversion helper.
 */
typedef enum {
    D_WIFI_AUTH_OPEN,           /**< No authentication (open network).*/
    D_WIFI_AUTH_WEP,            /**< WEP. Scan-detection only and cannot be used to join or host a network. See d__wifi_auth_to_cyw43.*/
    D_WIFI_AUTH_WPA_TKIP,       /**< WPA with TKIP.*/
    D_WIFI_AUTH_WPA2_AES,       /**< WPA2 with AES (preferred).*/
    D_WIFI_AUTH_WPA2_MIXED,     /**< WPA2/WPA mixed mode.*/
    D_WIFI_AUTH_WPA3_SAE,       /**< WPA3 with SAE.*/
    D_WIFI_AUTH_WPA3_WPA2_MIXED /**< WPA2/WPA3 mixed mode.*/
} d_wifi_auth_t;

/**
 * @brief A single access point discovered during a scan.
 */
typedef struct {
    uint8_t ssid[32];        /**< Network name, not null-terminated, see ssid_len.*/
    uint16_t channel;        /**< Wifi channel the AP was found on.*/
    int16_t rssi;            /**< Signal strength in dBm.*/
    d_wifi_auth_t auth_mode; /**< Security mode advertised by the AP.*/
    uint8_t bssid[6];        /**< Access point MAC address.*/
    uint8_t ssid_len;        /**< Number of valid bytes in ssid.*/
} d_wifi_scan_result_t;

/**
 * @brief Caller-owned buffer collecting results across a scan.
 *
 * @details
 * Passed to d_wifi_scan_start() and populated incrementally as results
 * arrive, must remain valid for the duration of the scan.
 */
typedef struct {
    d_wifi_scan_result_t results[WIFI_MAX_SCAN_RESULTS]; /**< Collected scan results.*/
    uint8_t count;                                       /**< Number of valid entries in results.*/
} d_wifi_scan_state_t;

/**
 * @brief Options controlling a wifi scan.
 */
typedef struct {
    uint8_t ssid_filter[32]; /**< SSID to filter for, ignored if ssid_filter_len is 0.*/
    int8_t scan_type;        /**< 0 = active scan, 1 = passive scan.*/
    uint8_t ssid_filter_len; /**< Number of valid bytes in ssid_filter, 0 scans all SSIDs.*/
} d_wifi_scan_opts_t;

/**
 * @brief Parameters for joining a wifi network in STA mode.
 */
typedef struct {
    uint8_t ssid[32];   /**< Target network name, not null-terminated, see ssid_len.*/
    uint8_t key[64];    /**< Network password, unused if auth is D_WIFI_AUTH_OPEN.*/
    d_wifi_auth_t auth; /**< Authentication mode to use.*/
    uint8_t bssid[6];   /**< Specific AP MAC to join, or all-zero to join by SSID only.*/
    uint8_t channel;    /**< Channel hint- only used when bssid is non-zero.*/
    uint8_t ssid_len;   /**< Number of valid bytes in ssid.*/
    uint8_t key_len;    /**< Number of valid bytes in key.*/
} d_wifi_connect_params_t;

/**
 * @brief Configuration for bringing up the access point interface.
 */
typedef struct {
    uint8_t ssid[32];   /**< Access point network name, not null-terminated, see ssid_len.*/
    uint8_t key[64];    /**< Access point password, unused if auth is D_WIFI_AUTH_OPEN.*/
    d_wifi_auth_t auth; /**< Authentication mode for the access point.*/
    uint8_t channel;    /**< Wifi channel to broadcast on (1-13).*/
    uint8_t ssid_len;   /**< Number of valid bytes in ssid.*/
    uint8_t key_len;    /**< Number of valid bytes in key.*/
} d_wifi_ap_config_t;

/**
 * @brief Callback signature for delivering raw captured 802.11 frames.
 *
 * @details
 * Invoked from within cyw43_cb_process_ethernet() while monitor mode is
 * active and a callback is registered. buf/len are handed through unparsed.
 * This driver performs no 802.11 header inspection.
 *
 * @param[in] buf Raw frame bytes, valid only for the duration of the call.
 * @param[in] len Number of valid bytes in buf.
 * @param[in] ctx Caller-supplied context pointer, passed through unmodified.
 */
typedef void (*d_wifi_raw_rx_cb_t)(const uint8_t *buf, size_t len, void *ctx);

/**
 * @brief Initialise the CYW43 driver and enable STA mode.
 *
 * @details
 * Calls cyw43_arch_init() and enables STA mode by default. Must be called
 * once before any other function in this API.
 *
 * @return true on success, false if CYW43 initialisation failed.
 */
bool d_wifi_init(void);

/**
 * @brief Tear down STA/AP/Monitor interfaces and deinitialise the CYW43 driver.
 *
 * @details
 * Safe to call even if d_wifi_init() was never called or already deinitialised.
 */
void d_wifi_deinit(void);

/**
 * @brief Check whether the driver has been initialised.
 *
 * @return true if d_wifi_init() has succeeded and not since been deinitialised.
 */
bool d_wifi_is_initialized(void);

/**
 * @brief Start an asynchronous wifi scan.
 *
 * @details
 * Requires STA mode to be enabled. Results are written into state as they
 * arrive, poll d_wifi_scan_active() to determine completion.
 *
 * @param[out] state Caller-owned scan result buffer, must remain valid until
 *                    the scan completes.
 * @param[in]  opts   Scan filter/type options.
 * @return 0 on success, negative error code if not initialised, STA mode is
 *         off, or the underlying scan call fails.
 */
int d_wifi_scan_start(d_wifi_scan_state_t *state, const d_wifi_scan_opts_t *opts);

/**
 * @brief Check whether a wifi scan is currently in progress.
 *
 * @return true if a scan started by d_wifi_scan_start() is still running.
 */
bool d_wifi_scan_active(void);

/**
 * @brief Join a wifi network in STA (client) mode.
 *
 * @param[in] params Connection parameters (SSID, key, auth mode, optional BSSID).
 * @return 0 on success, negative error code on failure.
 *
 * @note Connecting to WEP networks is not supported and returns a negative error code if passed.
 *
 * @note This call is asynchronous. Poll d_wifi_link_status() afterward.
 */
int d_wifi_join(const d_wifi_connect_params_t *params);

/**
 * @brief Disassociate from the currently joined wifi network.
 *
 * @return 0 on success, negative error code on failure.
 */
int d_wifi_leave(void);

/**
 * @brief Disable STA mode entirely.
 *
 * @details
 * Disconnects any active STA connection and tears down the STA interface.
 * No-op if STA mode is not currently enabled.
 */
void d_wifi_sta_disable(void);

/**
 * @brief Get the current STA link status.
 *
 * @return Current link status, see d_wifi_link_status_t.
 */
d_wifi_link_status_t d_wifi_link_status(void);

/**
 * @brief Get the signal strength of the currently joined network.
 *
 * @param[out] rssi_out Populated with the RSSI value in dBm.
 * @return 0 on success, negative error code on failure.
 */
int d_wifi_get_rssi(int32_t *rssi_out);

/**
 * @brief Get the STA interface's MAC address.
 *
 * @param[out] mac_out Populated with the 6-byte MAC address.
 * @return 0 on success, negative error code on failure.
 */
int d_wifi_get_mac(uint8_t mac_out[6]);

/**
 * @brief Configure and bring up the access point interface.
 *
 * @details
 * Stages SSID, channel, and (if not open) password/auth into CYW43's
 * internal AP state, then commits the interface up.
 *
 * @note AP Interface doesn't support WEP mode and returns a negative error code if passed.
 *
 * @param[in] config Access point configuration to apply.
 */
void d_wifi_ap_enable(const d_wifi_ap_config_t *config);

/**
 * @brief Get the currently configured access point SSID.
 *
 * @param[out] len Populated with the number of valid bytes at the returned pointer.
 * @return Pointer to the internal SSID buffer, valid for the driver's lifetime,
 *         must be treated as read-only.
 */
const uint8_t *d_wifi_ap_get_ssid(size_t *len);

/**
 * @brief Get the currently configured access point authentication mode.
 *
 * @return Current AP auth mode.
 */
d_wifi_auth_t d_wifi_ap_get_auth(void);

/**
 * @brief Stage a new access point channel.
 *
 * @details
 * Only takes effect on the next d_wifi_ap_enable() call. Does not change
 * the channel of an already-running access point. There is no corresponding
 * getter, the underlying CYW43 driver exposes no channel readback.
 *
 * @param[in] channel Wifi channel to stage, 1-13.
 */
void d_wifi_ap_set_channel(uint32_t channel);

/**
 * @brief Stage a new access point SSID.
 *
 * @details
 * Only takes effect on the next d_wifi_ap_enable() call.
 *
 * @param[in] len  Number of bytes in ssid.
 * @param[in] ssid SSID bytes to stage.
 */
void d_wifi_ap_set_ssid(size_t len, const uint8_t *ssid);

/**
 * @brief Stage a new access point password.
 *
 * @details
 * Only takes effect on the next d_wifi_ap_enable() call. There is no getter
 * for this value once set.
 *
 * @param[in] len      Number of bytes in password.
 * @param[in] password Password bytes to stage.
 */
void d_wifi_ap_set_password(size_t len, const uint8_t *password);

/**
 * @brief Stage a new access point authentication mode.
 *
 * @details
 * Only takes effect on the next d_wifi_ap_enable() call.
 *
 * @param[in] auth Authentication mode to stage.
 */
void d_wifi_ap_set_auth(d_wifi_auth_t auth);

/**
 * @brief Get the maximum number of STAs the access point can support.
 *
 * @return Maximum supported STA count, or 0 on error.
 */
int d_wifi_ap_get_max_stas(void);

/**
 * @brief Get the MAC addresses of STAs currently connected to the access point.
 *
 * @param[out] macs          Caller-owned buffer, must hold at least
 *                            macs_capacity * 6 bytes.
 * @param[in]  macs_capacity Number of MAC slots macs can hold.
 * @return Number of STAs actually written into macs.
 */
int d_wifi_ap_get_stas(uint8_t *macs, int macs_capacity);

/**
 * @brief Get the access point interface's MAC address.
 *
 * @param[out] mac_out Populated with the 6-byte MAC address.
 * @return 0 on success, negative error code on failure.
 */
int d_wifi_ap_get_mac(uint8_t mac_out[6]);

/**
 * @brief Tear down the access point interface.
 */
void d_wifi_ap_disable(void);

/**
 * @brief Enable or disable monitor (promiscuous) mode.
 *
 * @details
 * Rejects enabling monitor mode while STA is connected or AP is up, since
 * monitor mode forces the radio out of any active association. The caller
 * must explicitly tear down STA/AP first.
 *
 * @param[in] enable true to enable monitor mode, false to disable.
 * @return 0 on success, negative error code if STA/AP is active or the
 *         underlying ioctl fails.
 *
 * @note Behaviour on stock (non-Nexmon) CYW43439 firmware is unverified;
 *       capture may be limited or incomplete.
 */
int d_wifi_set_monitor(bool enable);

/**
 * @brief Check whether monitor mode is currently active.
 *
 * @return true if monitor mode is enabled.
 */
bool d_wifi_monitor_is_active(void);

/**
 * @brief Set the radio's current channel while in monitor mode.
 *
 * @param[in] channel Channel number, 1-13.
 * @return 0 on success, negative error code if monitor mode is inactive,
 *         channel is out of range, or the underlying ioctl fails.
 */
int d_wifi_monitor_set_channel(uint8_t channel);

/**
 * @brief Start automatic channel hopping across 1-13 while in monitor mode.
 *
 * @details
 * Registers a persistent scheduler task on first call, subsequent calls
 * only update the dwell time and re-arm hopping.
 *
 * @param[in] dwell_ms Time to remain on each channel before hopping, in
 *                      milliseconds. Rejected if below the minimum floor defined in MIN_HOP_DWELL_MS macro above.
 * @return 0 on success, negative error code if monitor mode is inactive or
 *         dwell_ms is below the minimum.
 */
int d_wifi_monitor_hop_start(uint16_t dwell_ms);

/**
 * @brief Stop automatic channel hopping.
 *
 * @details
 * Leaves the radio parked on whatever channel was active when stopped.
 * The underlying scheduler task remains registered, idling.
 */
void d_wifi_monitor_hop_stop(void);

/**
 * @brief Register a callback to receive raw captured frames.
 *
 * @param[in] cb  Function to invoke for each received frame while monitor
 *                mode is active.
 * @param[in] ctx Opaque context pointer passed through to every cb call.
 * @return 0 on success, negative error code if a callback is already registered.
 */
int d_wifi_monitor_register_rx_cb(d_wifi_raw_rx_cb_t cb, void *ctx);

/**
 * @brief Clear a previously registered raw capture callback.
 */
void d_wifi_monitor_unregister_rx_cb(void);

#endif
