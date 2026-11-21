/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file kprint.c
 * @author datenbar
 * @date 22-11-2026
 * @brief Implementation of the kernel text output service.
 * @ingroup kernel
 *
 * @details
 * Holds the single registered sink and forwards writes to it. The sink is stored in a static
 * pointer, so the service needs no allocation and no initialisation beyond the first
 * k_print_set_sink() call.
 *
 * Design notes:
 * - k_print_write() copies the sink pointer into a local variable before calling it. A write
 *   therefore always completes with the sink that was current when it started, even if the sink
 *   is replaced mid-call.
 * - @c volatile makes the compiler re-read @c k__curr_sink on every access, so a stale copy
 *   cannot stay cached in a register. It provides no atomicity and no ordering guarantees.
 * - On Cortex-M33 a pointer-sized, aligned load and store are single instructions, so a reader
 *   never sees a half-written pointer. The C language does not guarantee this.
 * - Errors from the sink are returned unchanged. The service adds no retries and no fallback
 *   output.
 *
 * Known limitations:
 * - k_print_set_sink() is not synchronised. Concurrent registration from two contexts has no
 *   defined order.
 * - No output is produced when no sink is registered. The service does not buffer the bytes or
 *   retry later.
 * - No length check is performed on k_print_write(), so the caller is trusted.
 * - An empty write succeeds even when no sink is registered, because there is nothing to emit.
 * - Uses its own error codes instead of the kernel-wide error enum (yet).
 *
 * @todo [Kernel][Enhancement] Make the sink pointer atomic so it can be registered safely from
 *       an ISR.
 * @todo [Kernel][Enhancement] Add a k_printf() allowing for formatted prints.
 * @todo [Kernel][Enhancement] Add a bounded k_print_nw() variant that takes a maximum length.
 * @todo [Kernel][Enhancement] Replace the KPRINT_ERR_* codes with the kernel-wide error enum.
 */

#include "kprint.h"

#include <string.h>

/** @brief Currently registered sink. NULL means no sink is registered. */
static volatile kprint_sink_t k__curr_sink = NULL;

/** @brief Store the new sink and return the previous one. */
kprint_sink_t k_print_set_sink(kprint_sink_t sink)
{
    kprint_sink_t old_sink = k__curr_sink;
    k__curr_sink = sink;
    return old_sink;
}

/** @brief Accept an empty write, validate the buffer, then forward the bytes to the sink. */
k_print_err_t k_print_write(const char *buf, size_t len)
{
    if (len == 0) {
        return KPRINT_ERR_OK;
    }
    if (!buf) {
        return KPRINT_ERR_INVAL;
    }

    kprint_sink_t sink = k__curr_sink;

    if (sink == NULL) {
        return KPRINT_ERR_NO_SINK;
    }
    return sink(buf, len);
}

/** @brief Measure the string and forward it to k_print_write(). */
k_print_err_t k_print(const char *str)
{
    if (!str) {
        return KPRINT_ERR_INVAL;
    }
    size_t len = strlen(str);
    return k_print_write(str, len);
}
