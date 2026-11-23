/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file console.h
 * @author datenbar
 * @date 22-11-2026
 * @brief Console that reads and writes bytes over USB CDC.
 * @ingroup interface
 *
 * @details
 * Bridges kernel text output and a USB Communications Device Class (CDC) serial port. Output goes
 * through the Pico SDK stdio USB backend. A stdio callback wakes the core when input arrives. The
 * input functions then move bytes from the SDK into a fixed-size receive ring for the caller.
 * After i_console_init(), k_print() output is routed to this console.
 *
 * Constraints:
 * - i_console_init() runs once, after the USB stdio backend is initialised (stdio_init_all() or
 *   stdio_usb_init()). The console does not initialise the backend. Never call it again on a USB
 *   reconnect.
 * - Every function here is thread context only. None is safe to call from an interrupt.
 * - Input is read only through i_console_has_input() and i_console_getc_nb(). Unread bytes stay in
 *   the SDK buffer until one of them is called.
 * - Input functions run from one context only (typically the main loop). Each call both fills the
 *   ring (producer) and empties it (consumer), so a second caller context breaks both sides of
 *   the single-producer, single-consumer (SPSC) rule of the ring.
 * - When the receive ring is full, the console stops reading. Unread bytes stay in the USB
 *   buffer and hold the host back. The console never drops input.
 * - Output is raw. There is no "\n" to "\r\n" translation, so callers send "\r\n" themselves.
 *
 * Security:
 * - The CDC channel is plaintext and unauthenticated. Anyone with physical access to the USB
 *   port can read output and send input.
 * - Received bytes are untrusted. The console returns them unchanged, including control bytes
 *   and escape sequences. Callers must validate them before use.
 * - Output is not filtered. Sensitive values written to k_print() appear on the serial port.
 */

#ifndef PICOKERNEL_INTERFACES_CONSOLE_H
#define PICOKERNEL_INTERFACES_CONSOLE_H

#include "kernel/print/kprint.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Initialise the receive ring and install i_console_write() as the k_print() sink.
 *
 * @details
 * Sets up the receive ring over its static storage and registers the stdio callback that wakes a
 * core waiting in Wait For Event (WFE) when input arrives. Then calls k_print_set_sink() with
 * i_console_write(). The previous sink is discarded, not restored. If the ring cannot be set up, no
 * callback and no sink are installed.
 *
 * @return KPRINT_ERR_OK on success.
 *         KPRINT_ERR_IO if the receive ring could not be initialised.
 *
 * @note Context: thread, once during boot, on core 0, after the USB stdio backend is initialised.
 *       Call it before any other context reads or writes the console.
 *
 * @warning Calling this again resets the receive ring and discards buffered input. Do not call
 *          it on a USB reconnect.
 */
k_print_err_t i_console_init(void);

/**
 * @brief Check whether at least one received byte is waiting.
 *
 * @details
 * Moves available bytes from the SDK buffer into the receive ring, as far as the ring has space,
 * then checks whether the ring holds any bytes. Does not block.
 *
 * @return true if i_console_getc_nb() would return a byte.
 *         false if no input is waiting.
 *
 * @note Context: thread, single caller context only (see the file constraints).
 */
bool i_console_has_input(void);

/**
 * @brief Read the oldest received byte without blocking.
 *
 * @details
 * Moves available bytes from the SDK buffer into the receive ring, then removes the oldest byte.
 * Returns at once if the ring is empty.
 *
 * @return The received byte as a value from 0 to 255.
 *         -1 if no input is waiting.
 *
 * @note Context: thread, single caller context only (see the file constraints). The returned
 *       byte is untrusted.
 *
 * @warning Store the result in an int. A uint8_t makes -1 indistinguishable from 0xFF.
 */
int i_console_getc_nb(void);

/**
 * @brief Write a byte range to the USB CDC port.
 *
 * @details
 * Sends @p len bytes from @p buf through the SDK stdio backend. Adds no newline, translates no
 * line endings and forces no flush. This function is the sink that k_print() uses after
 * i_console_init().
 *
 * @param[in] buf Bytes to send. May be NULL only when @p len is zero.
 * @param[in] len Number of bytes to send. Must not exceed INT_MAX. Zero is a successful no-op.
 *
 * @return KPRINT_ERR_OK if the bytes were handed to the SDK, or if @p len is zero.
 *         KPRINT_ERR_INVAL if @p buf is NULL with a non-zero @p len, or @p len exceeds INT_MAX.
 *         KPRINT_ERR_IO if the SDK reported a write error.
 *
 * @note Context: thread only. May block while the SDK waits on the USB stack. Not safe from an
 *       interrupt.
 *
 * @warning Short writes and USB failures are not detected, so KPRINT_ERR_OK does not confirm
 *          that the bytes reached the host. This does not yet meet the k_print sink contract
 *          (write all bytes or return an error).
 */
k_print_err_t i_console_write(const char *buf, size_t len);

/**
 * @brief Check whether a USB host has opened the CDC port.
 *
 * @details
 * Reports the connection state of the SDK USB stdio backend. The state is false when no host is
 * attached and before the USB stack is running. It can change at any time, for example on
 * unplug or when the host closes the port.
 *
 * @return true if a USB host has the CDC port open.
 *         false otherwise.
 *
 * @note Context: thread. True does not guarantee that the next write reaches the host.
 */
bool i_console_ready(void);

#endif
