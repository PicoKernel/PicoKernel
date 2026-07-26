/**
 * @file pkt_logger.c
 * @author rootmnt
 * @date 18-07-2026
 * @brief Implements the raw IEEE 802.11 packet capture module.
 * @ingroup modules
 *
 * @details
 * Implements the API declared in pkt_logger.h. Captures raw IEEE 802.11
 * frames via the kernel WiFi monitor-mode interface and prints each
 * captured frame to serial as a plain hexadecimal dump. Frame capture and
 * processing are performed by a cooperative scheduler task.
 *
 * Design notes:
 * - Acts as a consumer of the kernel WiFi subsystem.
 * - Refer to kernel/wifi/wifi.h for the WiFi-related structures, enums,
 *   and APIs used by this module.
 * - The capture channel (1-13) is selected during initialisation.
 * - Captured frames are printed as plain hexadecimal dumps with no
 *   protocol parsing or interpretation.
 *
 * Known limitations:
 * - Frame contents are printed as a flat hexadecimal dump only; no
 *   IEEE 802.11 header parsing is performed in this milestone.
 *
 * @warning This module assumes cooperative, single-core execution. Its
 *          design must be revisited if preemptive scheduling or multicore
 *          execution is introduced.
 */

#include "pkt_logger.h"
#include "scheduler/scheduler.h"
#include "wifi/wifi.h"
#include <stdio.h>
#include <string.h>

#define M_PKT_LOGGER_BUF_SIZE (2560U) /**< @brief Size of the module-owned receive buffer, in bytes. Sized for the largest supported IEEE 802.11 frame (~2346 bytes) with additional headroom. */

static uint8_t m__pkt_buf[M_PKT_LOGGER_BUF_SIZE]; /**< @brief Module-owned receive buffer for captured IEEE 802.11 frames. */

static uint16_t m__pkt_len = 0; /**< @brief Length, in bytes, of the buffered frame awaiting processing. */

static bool m__pkt_ready = false; /**< @brief Indicates whether a captured frame is awaiting processing by the scheduler task. */

/**
 * @brief Receives captured IEEE 802.11 frames.
 *
 * @details
 * Registered with the kernel WiFi monitor-mode interface during
 * pkt_logger_init(). Invoked for each captured IEEE 802.11 frame.
 * Copies the received frame into the module-owned buffer and marks it
 * ready for processing by the scheduler task.
 *
 * Frames larger than M_PKT_LOGGER_BUF_SIZE are silently discarded.
 *
 * @note This callback performs only minimal work to minimize receive latency.
 *       Frame printing is deferred to the scheduler task.
 *
 * @param[in] buf Pointer to the received frame.
 * @param[in] len Length of the received frame, in bytes.
 * @param[in] ctx User-supplied callback context. Unused.
 */
static void m__pkt_logger_rx_cb(const uint8_t *buf, size_t len, void *ctx)
{
    (void)ctx;
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
        if (((i + 1) % 16) == 0) {
            printf("\n");
        }
    }
    if ((m__pkt_len % 16) != 0) {
        printf("\n");
    }
    printf("\n");

    m__pkt_ready = false;
    return TASK_OK;
}

/**
 * @brief Initializes the packet logger module.
 *
 * @details
 * Enables WiFi monitor mode, selects the specified IEEE 802.11 capture
 * channel, registers the receive callback, and registers the packet
 * logger task with the cooperative scheduler.
 *
 * @param[in] channel IEEE 802.11 channel to capture on (1-13).
 *
 * @return true Module initialized successfully.
 * @return false Monitor mode could not be enabled, the capture channel
 *               could not be configured, or the receive callback could
 *               not be registered.
 */
bool pkt_logger_init(uint8_t channel)
{
    if ((channel < 1) || (channel > 13)) {
        return false;
    }

    if (k_wifi_set_monitor(true) != 0) {
        return false;
    }

    if (k_wifi_monitor_set_channel(channel) != 0) {
        return false;
    }

    if (k_wifi_monitor_register_rx_cb(m__pkt_logger_rx_cb, NULL) != 0) {
        return false;
    }

    k_scheduler_register("pkt_logger", m__pkt_logger_task);
    return true;
}
