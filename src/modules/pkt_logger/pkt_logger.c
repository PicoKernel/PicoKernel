/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file pkt_logger.c
 * @author rootmnt
 * @date 18-07-2026
 * @brief Implements the raw IEEE 802.11 packet capture module.
 * @ingroup modules
 *
 * @details
 * Implements the API declared in pkt_logger.h. Captures raw IEEE 802.11
 * frames via the kernel WiFi monitor-mode interface and prints captured frames
 * to serial as plain hexadecimal dumps. Frame capture and processing are
 * performed by a cooperative scheduler task.
 *
 * Design notes:
 * - Acts as a consumer of the kernel WiFi subsystem.
 * - Refer to kernel/wifi/wifi.h for the WiFi-related structures, enums,
 *   and APIs used by this module.
 * - Capture operates either on a fixed channel (1-13) or by automatically
 *   hopping across the supported channels at a configurable dwell time.
 * - The receive callback and scheduler task are registered only once;
 *   subsequent initialization calls only reconfigure capture settings.
 * - Captured frames are printed as plain hexadecimal dumps with no
 *   protocol parsing or interpretation.
 *
 * Known limitations:
 * - Frame contents are printed as a flat hexadecimal dump only;
 *   no IEEE 802.11 header parsing is performed.
 *
 * @todo [Module][Enhancement] Support pcap/pcapng output format.
 * @todo [Module][Enhancement] Support per-frame filtering.
 * @todo [Module][Enhancement] Parse IEEE 802.11 frame headers and introduce double-buffered packet storage to prevent frame loss.
 * @todo [Module][Enhancement] Review IRQ/preemption safety of shared RX state.
 * @todo [Driver][Enhancement] Add a WiFi channel-hopping state getter to avoid tracking hopping state within the module.
 */

#include "pkt_logger.h"
#include "scheduler/scheduler.h"
#include "wifi/wifi.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define M_PKT_LOGGER_BUF_SIZE       (2560U) /**< @brief Size of the module-owned receive buffer, in bytes. Sized for the largest supported IEEE 802.11 frame (~2346 bytes) with additional headroom. */
#define M_PKT_LOGGER_BYTES_PER_LINE (16U)   /**< @brief Number of bytes printed per output line. */

static uint8_t m__pkt_buf[M_PKT_LOGGER_BUF_SIZE]; /**< @brief Module-owned receive buffer for captured IEEE 802.11 frames.*/

static uint16_t m__pkt_len = 0; /**< @brief Length, in bytes, of the buffered frame awaiting processing. */

static bool m__pkt_ready = false; /**< @brief Indicates whether a captured frame is awaiting processing by the scheduler task. */

static bool m__initialized = false; /**< @brief Indicates whether the packet logger module has already been initialized. Prevents duplicate scheduler task registration and callback setup. */

static bool m__hop = false; /**< @brief Tracks whether automatic channel hopping is currently enabled.*/

/**
 * @brief Receives captured IEEE 802.11 frames.
 *
 * @details
 * Registered with the kernel WiFi monitor-mode interface during
 * m_pkt_logger_init(). Invoked for each captured IEEE 802.11 frame.
 * Copies the received frame into the module-owned buffer and marks it
 * ready for processing by the scheduler task.
 *
 * @note Frame dumping is deferred to the scheduler task to minimize work
 *       performed in the receive callback.
 *
 * @warning Frames larger than M_PKT_LOGGER_BUF_SIZE or received while the module
 *          buffer is occupied are discarded.
 *
 * @param[in] buf Pointer to the received frame.
 * @param[in] len Length of the received frame, in bytes.
 * @param[in] ctx User-supplied callback context. Unused.
 */
static void m__pkt_logger_rx_cb(const uint8_t *buf, size_t len, void *ctx)
{
    (void)ctx;

    if (m__pkt_ready) {
        printf("pkt_logger: dropping frame (buffer busy)\n");
        return;
    }

    if (len > M_PKT_LOGGER_BUF_SIZE) {
        printf("pkt_logger: dropped %zu-byte frame (buffer %u bytes)\n", len, M_PKT_LOGGER_BUF_SIZE);
        return;
    }

    memcpy(m__pkt_buf, buf, len);
    m__pkt_len = (uint16_t)len;
    m__pkt_ready = true;
}

/**
 * @brief Prints captured IEEE 802.11 frames.
 *
 * @details
 * Invoked periodically by the cooperative scheduler. If a captured
 * frame is pending, prints it to the serial console as a hexadecimal
 * dump and marks the receive buffer available for the next frame.
 * Returns immediately if no frame is pending.
 *
 * @return TASK_OK Task executed successfully.
 */
static task_status_t m__pkt_logger_task(void)
{
    if (!m__pkt_ready) {
        return TASK_OK;
    }

    printf("Captured %u-byte frame:\n", m__pkt_len);

    for (uint16_t i = 0; i < m__pkt_len; i++) {
        printf("%02x ", m__pkt_buf[i]);
        if (((i + 1U) % M_PKT_LOGGER_BYTES_PER_LINE) == 0U) {
            printf("\n");
        }
    }
    if ((m__pkt_len % M_PKT_LOGGER_BYTES_PER_LINE) != 0U) {
        printf("\n");
    }
    printf("\n");

    m__pkt_ready = false;
    return TASK_OK;
}

/**
 * @brief Cleans up state after failed packet logger configuration.
 *
 * @details
 * If initial setup fails after the receive callback has been registered,
 * the callback is unregistered. Monitor mode is disabled only if the
 * packet logger was responsible for enabling it, leaving pre-existing
 * monitor state unchanged.
 *
 * @param[in] monitor_enabled_here true if the packet logger was responsible for enabling monitor mode.
 */
static void m__pkt_logger_cleanup(bool monitor_enabled_here)
{
    if (!m__initialized) {
        k_wifi_monitor_unregister_rx_cb();
    }

    if (monitor_enabled_here) {
        k_wifi_set_monitor(false);
    }
}

/**
 * @brief Initializes or reconfigures packet capture.
 */
bool m_pkt_logger_init(uint8_t channel, uint16_t dwell_ms, bool hop)
{
    if (hop && (channel != 0)) {
        return false;
    }
    if (!hop && ((channel < 1) || (channel > 13) || (dwell_ms != 0))) {
        return false;
    }

    bool monitor_enabled_here = false;

    if (!k_wifi_monitor_is_active()) {
        if (k_wifi_set_monitor(true) != 0) {
            printf("Monitor Mode could not be initialized. It maybe due to STA/AP restrictions\n");
            return false;
        }
        monitor_enabled_here = true;
    }

    if (!m__initialized) {
        if (k_wifi_monitor_register_rx_cb(m__pkt_logger_rx_cb, NULL) != 0) {
            if (monitor_enabled_here) {
                k_wifi_set_monitor(false);
            }
            return false;
        }
    }

    if (hop) {
        if (k_wifi_monitor_hop_start(dwell_ms) != 0) {
            m__pkt_logger_cleanup(monitor_enabled_here);
            return false;
        }
        m__hop = true;
    } else {
        if (m__hop) {
            k_wifi_monitor_hop_stop();
            m__hop = false;
        }

        if (k_wifi_monitor_set_channel(channel) != 0) {
            m__pkt_logger_cleanup(monitor_enabled_here);
            return false;
        }
    }

    if (!m__initialized) {
        k_scheduler_register("pkt_logger", m__pkt_logger_task);
        m__initialized = true;
    }

    return true;
}
