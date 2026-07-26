/**
 * @file pkt_logger.h
 * @author rootmnt
 * @date 18-07-2026
 * @brief Raw IEEE 802.11 packet capture module.
 * @ingroup modules
 *
 * @details
 * Captures raw IEEE 802.11 frames using the kernel WiFi monitor-mode
 * interface and prints every received frame to serial as a hexadecimal
 * dump. Frames are delivered asynchronously through a registered receive
 * callback, while a cooperative scheduler task processes and prints them.
 *
 * Constraints:
 * - pkt_logger_init() must be called exactly once, after kernel WiFi and
 *   scheduler initialization, but before the scheduler begins executing tasks.
 * - The capture channel must be in the range 1-13.
 * - The receive callback must execute quickly and only buffer received
 *   frames for processing by the scheduler task.
 * - Every received frame is preserved until processed, ensuring complete
 *   frame dumps.
 * - No kernel or driver code may include this header. This header is a
 *   leaf: it depends on the kernel WiFi interface, and nothing else depends on it.
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
 * @brief Initializes the packet logger module.
 *
 * @details
 * Configures the kernel WiFi subsystem for monitor mode on the specified
 * IEEE 802.11 channel, registers the packet receive callback, and registers
 * the packet logger task with the scheduler.
 *
 * @param channel IEEE 802.11 channel to capture on (1-13).
 *
 * @retval true  Module initialized successfully.
 * @retval false Initialization failed.
 */
bool pkt_logger_init(uint8_t channel);

#endif
