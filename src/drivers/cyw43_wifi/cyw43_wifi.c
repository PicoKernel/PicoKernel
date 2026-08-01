/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file cyw43_wifi.c
 * @author datenbar
 * @date 12-07-2026
 * @brief Implements the CYW43 WiFi driver.
 * @ingroup drivers
 *
 * @details
 * Implements the API declared in cyw43_wifi.h, plus cyw43_cb_process_ethernet(),
 * the raw-frame-receive callback mandated by the vendored CYW43 driver
 * (declared, not defined, in cyw43_ll.h and cyw43_ll.c calls it directly and
 * the link fails without a definition somewhere). Since this driver is built
 * with CYW43_LWIP=0, that callback exists here purely to route raw frames. No lwIP/TCP-IP handling occurs.
 *
 * Design notes:
 * - All state is tracked in a single bitmask, d__wifi_state.
 * - Auth mode translation (d__wifi_auth_to_cyw43) is centralised so no
 *   CYW43_AUTH_* value is ever referenced outside this file.
 * - Channel hopping is a persistent no-op scheduler task gated by HOP_ACTIVE.
 *   It is registered once on first d_wifi_monitor_hop_start() call and never
 *   deregistered.
 *
 * Known limitations:
 * - Monitor mode and STA/AP association are mutually exclusive on this radio.
 *   Enabling one while the other is active is rejected, not auto-torn-down.
 * - Behaviour of the raw SET_MONITOR ioctl on stock (non-Nexmon) firmware is
 *   unverified. Capture completeness under load is not guaranteed.
 *
 * @todo [Driver][Enhancement] Add PM (power management) get/set once scoped.
 * @todo [Driver][Enhancement] Expose configurable country code instead of
 *       hardcoded CYW43_COUNTRY_WORLDWIDE.
 * @todo [Driver][Enhancement] Hop task relies on scheduler having no unregister
 *       primitive. Revisit if/when scheduler gains a proper deregister API.
 * @todo [Driver][Enhancement] Parse RSN IE AKM suite to distinguish WPA2 vs WPA3 in scan results, or re-visit when SDK implements it.
 */
#include "cyw43_wifi.h"
#include "scheduler/scheduler.h"
#include "time/time.h"
#include <cyw43.h>
#include <cyw43_country.h>
#include <cyw43_ll.h>
#include <pico/cyw43_arch.h>
#include <pico/types.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define WIFI_INITTED  (1u << 0) /**< @brief Bitmask: driver is initialised.*/
#define STA_MODE      (1u << 1) /**< @brief Bitmask: STA interface is enabled.*/
#define STA_CONNECTED (1u << 2) /**< @brief Bitmask: STA is joined to a network.*/
#define AP_MODE       (1u << 3) /**< @brief Bitmask: AP interface is up.*/
#define MONITOR_MODE  (1u << 4) /**< @brief Bitmask: monitor mode is active.*/

#define HOP_ACTIVE     (1u << 5) /**< @brief Bitmask: channel hopping is currently running.*/
#define HOP_REGISTERED (1u << 6) /**< @brief Bitmask: the hop task has been registered with the scheduler at least once.*/

#define D_WLC_SET_CHANNEL (30) /**< @brief WLC_SET_CHANNEL ioctl command. Not part of the public CYW43 API. Sourced from the private definition in the vendored cyw43_ll.c. Re-verify this value on any CYW43 SDK version bump. */

static uint8_t d__wifi_state = 0;       /**< @brief Combined driver state bitmask. See WIFI_INITTED, STA_MODE, STA_CONNECTED, AP_MODE, MONITOR_MODE, HOP_ACTIVE, HOP_REGISTERED. */
static uint8_t d__hop_channel = 1;      /**< @brief Current channel-hop cursor, wraps 1-13.*/
static uint16_t d__hop_dwell_ms = 0;    /**< @brief Current configured dwell time per channel, in ms.*/
static uint32_t d__hop_last_switch = 0; /**< @brief k_uptime_ms() timestamp of the last channel switch.*/

static d_wifi_raw_rx_cb_t d__rx_cb = NULL; /**< @brief Registered raw-frame callback, or NULL if none registered.*/
static void *d__rx_cb_ctx = NULL;          /**< @brief Context pointer passed to d__rx_cb on every invocation.*/

/** @brief Initialise the CYW43 driver and enable STA mode. */
bool d_wifi_init(void)
{
    int ret = cyw43_arch_init();
    if (ret != 0) {
        return false;
    }
    cyw43_arch_enable_sta_mode();
    d__wifi_state |= STA_MODE;
    d__wifi_state |= WIFI_INITTED;
    return true;
}

/** @brief Tear down STA/AP interfaces and deinitialise the CYW43 driver. */
void d_wifi_deinit(void)
{
    if (!d_wifi_is_initialized()) {
        return;
    }
    if (d__wifi_state & STA_MODE) {
        cyw43_arch_disable_sta_mode();
        d__wifi_state &= ~STA_MODE;
    }
    if (d__wifi_state & AP_MODE) {
        cyw43_arch_disable_ap_mode();
        d__wifi_state &= ~AP_MODE;
    }
    if (d__wifi_state & MONITOR_MODE) {
        d_wifi_set_monitor(false);
    }
    cyw43_arch_deinit();
    d__wifi_state &= ~WIFI_INITTED;
}

/** @brief Check whether the driver has been initialised. */
bool d_wifi_is_initialized(void)
{
    if (d__wifi_state & WIFI_INITTED) {
        return cyw43_is_initialized(&cyw43_state);
    }
    return false;
}

/**
 * @brief Decode a raw CYW43 scan-result security bitmask into a d_wifi_auth_t.
 *
 * @details
 * CYW43 reports scan security as an OR'd bitmask (WEP=1, WPA=2, WPA2/WPA3=4)
 * set by cyw43_ll_wifi_parse_scan_result(), not a d_wifi_auth_t ordinal.
 * The vendored parser only checks for RSN IE presence, not its AKM suite
 * contents. WPA2-PSK and WPA3-SAE both set the same bit and are reported
 * identically as D_WIFI_AUTH_WPA2_AES here. Distinguishing them would
 * require parsing the RSN IE's AKM suite selector, not currently implemented in the SDK.
 *
 * @param[in] raw Raw CYW43 auth_mode bitmask from a scan result.
 * @return d_wifi_auth_t translation.
 */
static d_wifi_auth_t d__wifi_scan_auth_from_bitmask(uint8_t raw)
{
    bool wpa2_or_wpa3 = (bool)(raw & 4); // RSN IE present. WPA2 and WPA3 are indistinguishable here
    bool wpa = (bool)(raw & 2);
    bool wep = (bool)(raw & 1);

    if (wpa2_or_wpa3 && wpa) {
        return D_WIFI_AUTH_WPA2_MIXED;
    }
    if (wpa2_or_wpa3) {
        return D_WIFI_AUTH_WPA2_AES; // may actually be WPA3-SAE. See @details
    }
    if (wpa) {
        return D_WIFI_AUTH_WPA_TKIP;
    }
    if (wep) {
        return D_WIFI_AUTH_WEP;
    }
    return D_WIFI_AUTH_OPEN;
}

/**
 * @brief cyw43_wifi_scan() result callback; copies one result into the
 *        caller's scan state.
 *
 * @param[in] env    Caller-supplied context, cast back to d_wifi_scan_state_t*.
 * @param[in] result Single scan result from the CYW43 driver.
 * @return 0 to continue scanning, 1 once the result buffer is full.
 */
static int d__wifi_scan_result_handler(void *env, const cyw43_ev_scan_result_t *result)
{
    d_wifi_scan_state_t *state = (d_wifi_scan_state_t *)env;
    if (state->count < WIFI_MAX_SCAN_RESULTS) {
        d_wifi_scan_result_t *out = &state->results[state->count];
        memcpy(out->bssid, result->bssid, 6);
        out->ssid_len = result->ssid_len;
        memcpy(out->ssid, result->ssid, result->ssid_len);
        out->channel = result->channel;
        out->auth_mode = d__wifi_scan_auth_from_bitmask(result->auth_mode);
        state->count++;
        return 0;
    }
    return 1;
}

/** @brief Start an asynchronous wifi scan. */
int d_wifi_scan_start(d_wifi_scan_state_t *state, const d_wifi_scan_opts_t *opts)
{
    if (!d_wifi_is_initialized() || !(d__wifi_state & STA_MODE)) {
        return -1;
    }
    cyw43_wifi_scan_options_t raw_opts = {0};
    memcpy(raw_opts.ssid, opts->ssid_filter, opts->ssid_filter_len);
    raw_opts.scan_type = opts->scan_type;
    raw_opts.ssid_len = opts->ssid_filter_len;
    return cyw43_wifi_scan(&cyw43_state, &raw_opts, state, d__wifi_scan_result_handler);
}

/** @brief Check whether a wifi scan is currently in progress. */
bool d_wifi_scan_active(void)
{
    return cyw43_wifi_scan_active(&cyw43_state);
}

/**
 * @brief Translate a d_wifi_auth_t into the corresponding CYW43_AUTH_* value.
 *
 * @details
 * Centralises the only place in this driver that references CYW43_AUTH_*
 * constants directly, keeping SDK encodings out of every caller.
 *
 * @param[in] auth Driver-level auth mode.
 * @return Equivalent CYW43_AUTH_* value; CYW43_AUTH_OPEN if auth is unrecognised.
 */
static uint32_t d__wifi_auth_to_cyw43(d_wifi_auth_t auth)
{
    switch (auth) {
    case D_WIFI_AUTH_OPEN:
        return CYW43_AUTH_OPEN;
    case D_WIFI_AUTH_WPA2_AES:
        return CYW43_AUTH_WPA2_AES_PSK;
    case D_WIFI_AUTH_WPA2_MIXED:
        return CYW43_AUTH_WPA2_MIXED_PSK;
    case D_WIFI_AUTH_WPA3_SAE:
        return CYW43_AUTH_WPA3_SAE_AES_PSK;
    case D_WIFI_AUTH_WPA3_WPA2_MIXED:
        return CYW43_AUTH_WPA3_WPA2_AES_PSK;
    case D_WIFI_AUTH_WPA_TKIP:
        return CYW43_AUTH_WPA_TKIP_PSK;
    default:
        return CYW43_AUTH_OPEN;
    }
}

/** @brief Join a wifi network in STA (client) mode. */
int d_wifi_join(const d_wifi_connect_params_t *params)
{
    if (params->auth == D_WIFI_AUTH_WEP) {
        return -1;
    }
    int err = cyw43_wifi_join(&cyw43_state, params->ssid_len, params->ssid, params->key_len, params->key, d__wifi_auth_to_cyw43(params->auth), params->bssid, params->channel);
    if (err == 0) {
        d__wifi_state |= STA_CONNECTED;
    }
    return err;
}

/** @brief Disassociate from the currently joined wifi network. */
int d_wifi_leave(void)
{
    int err = cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    if (err == 0) {
        d__wifi_state &= ~STA_CONNECTED;
    }
    return err;
}

/** @brief Disable STA mode entirely. */
void d_wifi_sta_disable(void)
{
    if (d__wifi_state & STA_MODE) {
        cyw43_arch_disable_sta_mode();
        d__wifi_state &= ~STA_MODE;
    }
}

/** @brief Get the current STA link status. */
d_wifi_link_status_t d_wifi_link_status(void)
{
    return (d_wifi_link_status_t)cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
}

/** @brief Get the signal strength of the currently joined network. */
int d_wifi_get_rssi(int32_t *rssi_out)
{
    return cyw43_wifi_get_rssi(&cyw43_state, rssi_out);
}

/** @brief Get the STA interface's MAC address. */
int d_wifi_get_mac(uint8_t mac_out[6])
{
    return cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, mac_out);
}

/** @brief Configure and bring up the access point interface. */
void d_wifi_ap_enable(const d_wifi_ap_config_t *config)
{
    if (config->auth == D_WIFI_AUTH_WEP) {
        return;
    }
    cyw43_wifi_ap_set_ssid(&cyw43_state, config->ssid_len, config->ssid);
    if (config->channel > 0 && config->channel <= 13) {
        cyw43_wifi_ap_set_channel(&cyw43_state, (uint32_t)config->channel);
    }
    if (config->auth == D_WIFI_AUTH_OPEN) {
        cyw43_wifi_ap_set_auth(&cyw43_state, CYW43_AUTH_OPEN);
    } else {
        cyw43_wifi_ap_set_password(&cyw43_state, config->key_len, config->key);
        cyw43_wifi_ap_set_auth(&cyw43_state, d__wifi_auth_to_cyw43(config->auth));
    }
    d__wifi_state |= AP_MODE;
    cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_AP, true, CYW43_COUNTRY_WORLDWIDE);
}

/** @brief Get the currently configured access point SSID. */
const uint8_t *d_wifi_ap_get_ssid(size_t *len)
{
    const uint8_t *ssid_ptr;
    cyw43_wifi_ap_get_ssid(&cyw43_state, len, &ssid_ptr);
    return ssid_ptr;
}

/** @brief Get the currently configured access point authentication mode. */
d_wifi_auth_t d_wifi_ap_get_auth(void)
{
    return (d_wifi_auth_t)cyw43_wifi_ap_get_auth(&cyw43_state);
}

/** @brief Set the access point's channel. */
void d_wifi_ap_set_channel(uint32_t channel)
{
    cyw43_wifi_ap_set_channel(&cyw43_state, channel);
}

/** @brief Stage a new access point SSID. */
void d_wifi_ap_set_ssid(size_t len, const uint8_t *ssid)
{
    cyw43_wifi_ap_set_ssid(&cyw43_state, len, ssid);
}

/** @brief Stage a new access point password. */
void d_wifi_ap_set_password(size_t len, const uint8_t *password)
{
    cyw43_wifi_ap_set_password(&cyw43_state, len, password);
}

/** @brief Stage a new access point authentication mode. */
void d_wifi_ap_set_auth(d_wifi_auth_t auth)
{
    cyw43_wifi_ap_set_auth(&cyw43_state, d__wifi_auth_to_cyw43(auth));
}

/** @brief Get the maximum number of STAs the access point can support. */
int d_wifi_ap_get_max_stas(void)
{
    int max_stas = 0;
    cyw43_wifi_ap_get_max_stas(&cyw43_state, &max_stas);
    return max_stas;
}

/** @brief Get the MAC addresses of STAs currently connected to the access point. */
int d_wifi_ap_get_stas(uint8_t *macs, int macs_capacity)
{
    int max_stas = 0;
    cyw43_wifi_ap_get_max_stas(&cyw43_state, &max_stas);
    int num_stas = (macs_capacity < max_stas) ? macs_capacity : max_stas;
    cyw43_wifi_ap_get_stas(&cyw43_state, &num_stas, macs);
    return num_stas;
}

/** @brief Get the access point interface's MAC address. */
int d_wifi_ap_get_mac(uint8_t mac_out[6])
{
    return cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_AP, mac_out);
}

/** @brief Tear down the access point interface. */
void d_wifi_ap_disable(void)
{
    cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_AP, false, CYW43_COUNTRY_WORLDWIDE);
    d__wifi_state &= ~AP_MODE;
}

/** @brief Enable or disable monitor (promiscuous) mode. */
int d_wifi_set_monitor(bool enable)
{
    if (d__wifi_state & STA_CONNECTED || d__wifi_state & AP_MODE) {
        return -1;
    }
    uint8_t buf[4];
    uint32_t val = (int)enable ? 1 : 0;

    buf[0] = (uint8_t)(val >> 0);
    buf[1] = (uint8_t)(val >> 8);
    buf[2] = (uint8_t)(val >> 16);
    buf[3] = (uint8_t)(val >> 24);

    int err = cyw43_ioctl(&cyw43_state, CYW43_IOCTL_SET_MONITOR, sizeof(buf), buf, CYW43_ITF_STA);
    if (!err && enable == (bool)true) {
        d__wifi_state |= MONITOR_MODE;
    }
    if (!err && enable == (bool)false) {
        d__wifi_state &= ~MONITOR_MODE;
    }
    return err;
}

/** @brief Check whether monitor mode is currently active. */
bool d_wifi_monitor_is_active(void)
{
    return (bool)(d__wifi_state & MONITOR_MODE);
}

/** @brief Set the radio's current channel while in monitor mode. */
int d_wifi_monitor_set_channel(uint8_t channel)
{
    if (!(d__wifi_state & MONITOR_MODE)) {
        return -1;
    }
    if (channel > 13 || channel < 1) {
        return -1;
    }
    uint8_t buf[4];
    uint32_t val = channel;
    buf[0] = (uint8_t)(val >> 0);
    buf[1] = (uint8_t)(val >> 8);
    buf[2] = (uint8_t)(val >> 16);
    buf[3] = (uint8_t)(val >> 24);

    int err = cyw43_ioctl(&cyw43_state, D_WLC_SET_CHANNEL, sizeof(buf), buf, CYW43_ITF_STA);
    return err;
}

/**
 * @brief Scheduler task entry for automatic channel hopping.
 *
 * @details
 * Self-gates on every invocation: no-ops if hopping is inactive or the
 * dwell period hasn't elapsed yet. Runs unconditionally once registered,
 * per the scheduler's lack of an unregister primitive.
 *
 * @return Always TASK_OK. Hopping cannot fail in a way that warrants removal.
 */
static task_status_t d__wifi_hop_task_entry(void)
{
    if (!(d__wifi_state & HOP_ACTIVE)) {
        return TASK_OK;
    }
    if (!(k_time_elapsed(d__hop_last_switch, d__hop_dwell_ms))) {
        return TASK_OK;
    }
    d_wifi_monitor_set_channel(d__hop_channel);
    d__hop_channel = (d__hop_channel % 13) + 1;
    d__hop_last_switch = k_uptime_ms();
    return TASK_OK;
}

/** @brief Start automatic channel hopping across 1-13 while in monitor mode. */
int d_wifi_monitor_hop_start(uint16_t dwell_ms)
{
    if (!d_wifi_monitor_is_active()) {
        return -1;
    }
    if (dwell_ms < MIN_HOP_DWELL_MS) {
        return -2;
    }
    d__hop_dwell_ms = dwell_ms;
    d__hop_last_switch = k_uptime_ms();
    d__wifi_state |= HOP_ACTIVE;
    if (!(d__wifi_state & HOP_REGISTERED)) {
        k_scheduler_register("wifi_monitor_hop", d__wifi_hop_task_entry);
        d__wifi_state |= HOP_REGISTERED;
    }
    return 0;
}

/** @brief Stop automatic channel hopping. */
void d_wifi_monitor_hop_stop(void)
{
    if (!d_wifi_monitor_is_active()) {
        return;
    }
    if (!(d__wifi_state & HOP_ACTIVE)) {
        return;
    }
    d__wifi_state &= ~HOP_ACTIVE;
    d__hop_last_switch = k_uptime_ms();
}

/** @brief Register a callback to receive raw captured frames. */
int d_wifi_monitor_register_rx_cb(d_wifi_raw_rx_cb_t cb, void *ctx)
{
    if (d__rx_cb != NULL) {
        return -1;
    }
    d__rx_cb = cb;
    d__rx_cb_ctx = ctx;
    return 0;
}

/** @brief Clear a previously registered raw capture callback. */
void d_wifi_monitor_unregister_rx_cb(void)
{
    d__rx_cb = NULL;
    d__rx_cb_ctx = NULL;
}

/**
 * @brief CYW43 raw-frame-receive callback, required by cyw43_ll.c.
 *
 * @details
 * Declared (not defined) in cyw43_ll.h and called directly from
 * cyw43_ll_process_packets()/cyw43_do_ioctl() in the vendored driver
 * whenever a data packet arrives, must keep this exact name and signature
 * or the link fails with an undefined-reference error. Non-static by
 * necessity. Internal linkage would hide the symbol from cyw43_ll.c's
 * translation unit. Since CYW43_LWIP=0, no TCP/IP handling occurs here.
 * This only routes frames to a registered monitor-mode consumer.
 *
 * @param[in] cb_data Unused. Would be the driver's cb_data pointer under lwIP builds.
 * @param[in] itf     Unused. Interface the frame arrived on.
 * @param[in] len     Number of valid bytes in buf.
 * @param[in] buf     Raw frame bytes.
 */

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void cyw43_cb_process_ethernet(void *cb_data, int itf, size_t len, const uint8_t *buf)
{
    (void)cb_data;
    (void)itf;
    if (d_wifi_monitor_is_active() && d__rx_cb != NULL) {
        d__rx_cb(buf, len, d__rx_cb_ctx);
        return;
    }
}
