/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file kprint.h
 * @author datenbar
 * @date 22-11-2026
 * @brief Kernel text output service that forwards bytes to a pluggable sink.
 * @ingroup kernel
 *
 * @details
 * A sink function is registered at runtime with k_print_set_sink(). Every k_print() and
 * k_print_write() call is then forwarded to that sink. For example, a Universal Asynchronous
 * Receiver-Transmitter (UART) driver registers its transmit function during boot.
 *
 * Constraints:
 * - Register a sink before producing output. Without one, every non-empty write with a valid
 *   buffer returns KPRINT_ERR_NO_SINK.
 * - Only one sink is active at a time. Registering a new sink replaces the previous one.
 * - Writes run synchronously in the caller's context. By project rule, call from thread context
 *   only: interrupts queue events and the main loop prints. Blocking and buffering depend on the
 *   registered sink.
 * - A sink must write all requested bytes or return an error. Short writes are not allowed.
 * - k_print_set_sink() is not synchronised. Call it during initialisation, before other contexts
 *   print.
 *
 * Security:
 * - k_print() trusts the caller to pass a NUL-terminated string. Without a terminator, strlen()
 *   reads past the end of the buffer.
 * - Output is not filtered or redacted. Sensitive data passed to k_print() reaches the sink
 *   unchanged.
 */

#ifndef PICOKERNEL_KERNEL_KPRINT_H
#define PICOKERNEL_KERNEL_KPRINT_H

#include <stddef.h>

/**
 * @brief Result codes returned by the kprint API.
 *
 * @ingroup kernel
 */
typedef enum {
    KPRINT_ERR_OK = 0,       /**< Bytes were accepted by the sink. */
    KPRINT_ERR_INVAL = -1,   /**< A pointer argument was NULL. */
    KPRINT_ERR_NO_SINK = -2, /**< No sink is registered. Output was dropped. */
    KPRINT_ERR_IO = -3,      /**< The sink failed to transmit. Returned by sinks only. */
} k_print_err_t;

/**
 * @brief Output sink function. Receives the bytes to emit.
 *
 * @details
 * The sink transmits @p len bytes starting at @p buf. It runs in the context of the caller of
 * k_print_write(), so it must be safe to call from there. It must either emit all @p len bytes
 * or return an error.
 *
 * @param[in] buf Bytes to emit. Valid only for the duration of the call.
 * @param[in] len Number of bytes in @p buf. Never zero when called by k_print_write().
 *
 * @return KPRINT_ERR_OK if all bytes were emitted.
 *         KPRINT_ERR_IO if transmission failed.
 *         Other codes are passed through to the caller.
 *
 * @note Called synchronously. Must not call k_print() or k_print_write(), which would recurse.
 */
typedef k_print_err_t (*kprint_sink_t)(const char *buf, size_t len);

/**
 * @brief Register the output sink, replacing the current one.
 *
 * @details
 * Stores @p sink as the destination for all later writes. Passing NULL disables output. The
 * previous sink is returned so a caller can restore it later.
 *
 * @param[in] sink New sink function, or NULL to disable output.
 *
 * @return The previously registered sink, or NULL if none was set.
 *
 * @note Context: thread, during initialisation or while no other context is writing. Not
 *       synchronised.
 *
 * @warning A write that is already inside the old sink finishes with the old sink. The new sink
 *          applies only to writes that start after this call returns.
 */
kprint_sink_t k_print_set_sink(kprint_sink_t sink);

/**
 * @brief Send a byte range to the registered sink.
 *
 * @details
 * Passes @p buf and @p len to the sink unchanged. No copy is made, no NUL check is performed,
 * and no newline is added. A zero @p len succeeds at once and the sink is not called, even when
 * @p buf is NULL or no sink is registered. The NULL and sink checks run before the sink is called.
 *
 * @param[in] buf Bytes to emit. May be NULL only when @p len is zero. Must remain valid until
 *                the sink returns.
 * @param[in] len Number of bytes in @p buf. Not checked against the size of @p buf.
 *
 * @return KPRINT_ERR_OK if the sink accepted the bytes, or if @p len is zero.
 *         KPRINT_ERR_INVAL if @p buf is NULL and @p len is non-zero. The sink is not called.
 *         KPRINT_ERR_NO_SINK if no sink is registered and @p len is non-zero. Output is dropped.
 *         Any other value is the code the sink returned, typically KPRINT_ERR_IO.
 *
 * @note Context: thread only. Requires a prior k_print_set_sink(). Blocking behaviour is that of
 *       the registered sink.
 *
 * @warning The return value must be checked. Ignoring it drops output silently.
 */
k_print_err_t k_print_write(const char *buf, size_t len);

/**
 * @brief Send a NUL-terminated string to the registered sink.
 *
 * @details
 * Measures @p str with strlen() and calls k_print_write() with the length. The terminating NUL
 * is not sent. No newline is added or translated, so a serial terminal needs @c "\r\n" in the
 * string to start a new line.
 *
 * @param[in] str NUL-terminated string. Must not be NULL.
 *
 * @return KPRINT_ERR_OK if the sink accepted the string, or if the string is empty.
 *         KPRINT_ERR_INVAL if @p str is NULL.
 *         KPRINT_ERR_NO_SINK if no sink is registered and the string is not empty.
 *         Any other value is the code the sink returned.
 *
 * @note Context: thread only.
 *
 * @warning There is no length limit. A string without a NUL terminator causes an out-of-bounds
 *          read. Use k_print_write() for buffers that are not guaranteed to be terminated.
 */
k_print_err_t k_print(const char *str);

#endif
