/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file console.c
 * @author datenbar
 * @date 22-11-2026
 * @brief Implementation of the USB CDC console.
 * @ingroup interface
 *
 * @details
 * Implements the console on top of the Pico SDK stdio USB backend and the single-producer,
 * single-consumer (SPSC) ring buffer. Output goes straight to the SDK. Input is copied from the
 * SDK buffer into a CONSOLE_RX_SIZE-byte ring on demand, and the caller reads it from the ring.
 *
 * Design notes:
 * - The stdio callback only wakes a sleeping core with __sev(). It never touches the ring or
 *   reads input, because the SDK input path takes a mutex and the callback may run in IRQ
 *   context.
 * - Input is pulled from the SDK only while the ring has free space. When the ring is full,
 *   bytes stay in the SDK buffer and hold the host back, so input is never dropped.
 * - i__console_drain() fills the ring and the input functions empty it, all in the caller's
 *   context. This keeps the ring within its SPSC rule.
 * - The main loop sleeps in __wfe() while the ring is empty. The callback's __sev() wakes it,
 *   and i_console_has_input() then drains the SDK buffer.
 *
 * Known limitations:
 * - i_console_write() treats any non-negative SDK return value as success, even if fewer bytes
 *   were written than requested.
 * - i_console_init() does not check that the USB backend is running.
 * - Bytes received before i_console_init() stay in the SDK buffer and are not read until the
 *   input functions are called.
 * - Interface code calls the Pico SDK stdio functions directly instead of going through a
 *   driver. This is accepted for the MVP, and this file is the only one allowed to do it.
 * - Nothing here is safe to call from an interrupt.
 *
 * @todo [Interface][Bug] i_console_write() must report short writes instead of treating them as
 *       success.
 * @todo [Interface][Enhancement] Move USB access into a driver and switch to the direct TinyUSB
 *       CDC API after v1. Re-add a reset-to-BOOTSEL path first.
 * @todo [Interface][Enhancement] Disable or authenticate the console in release builds.
 */

#include "console.h"
#include "kernel/print/kprint.h"
#include "kernel/utils/ringbuf.h"

#include <hardware/sync.h>
#include <pico/stdio.h>
#include <pico/stdio_usb.h>

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Size of the receive ring in bytes. Must be a power of two. */
#define CONSOLE_RX_SIZE 256U

_Static_assert(CONSOLE_RX_SIZE != 0 && (CONSOLE_RX_SIZE & (CONSOLE_RX_SIZE - 1U)) == 0, "CONSOLE_RX_SIZE must be a power of two");

/** @brief Backing storage for the receive ring. Owned by this file. */
static uint8_t i__rx_storage[CONSOLE_RX_SIZE];

/** @brief Receive ring. Filled and emptied only from the caller context of the input functions. */
static k_ringbuf_t i__rx_ring;

/**
 * @brief Wake a core waiting in WFE when input arrives.
 *
 * @details
 * Issues a send-event (__sev()) so a core sleeping in Wait For Event (WFE) resumes and polls
 * the console. Does not read input and does not touch the receive ring.
 *
 * @param[in] param Unused.
 *
 * @note Context: called by the stdio backend and may run in IRQ context. Keep it short and
 *       never block in it.
 */
static void i__console_rx_cb(void *param)
{
    (void)param;
    __sev();
}

/**
 * @brief Move bytes from the SDK buffer into the receive ring until either runs out.
 *
 * @details
 * Pulls one byte at a time while the ring has space. Stops when the SDK has no byte ready or
 * the ring is full. Bytes left in the SDK buffer wait for the next call and hold the host back.
 *
 * @note Context: thread, single caller context only. Called by i_console_has_input() and
 *       i_console_getc_nb().
 */
static void i__console_drain(void)
{
    while (k_ring_has_space(&i__rx_ring, 1)) {
        int c = getchar_timeout_us(0);
        if (c < 0) {
            break;
        }
        if (k_ring_push(&i__rx_ring, (uint8_t)c) != RING_ERR_OK) {
            break;
        }
    }
}

/** @brief Set up the receive ring, register the wake callback, and install the k_print sink. */
k_print_err_t i_console_init(void)
{
    if (k_ring_init(&i__rx_ring, i__rx_storage, sizeof i__rx_storage) != RING_ERR_OK) {
        return KPRINT_ERR_IO;
    }
    stdio_set_chars_available_callback(i__console_rx_cb, NULL);
    k_print_set_sink(i_console_write);
    return KPRINT_ERR_OK;
}

/** @brief Drain pending input, then report whether the ring holds any bytes. */
bool i_console_has_input(void)
{
    i__console_drain();
    return (bool)!k_ring_is_empty(&i__rx_ring);
}

/** @brief Drain pending input, then return the oldest byte or -1 if none is waiting. */
int i_console_getc_nb(void)
{
    i__console_drain();
    uint8_t b;
    if (k_ring_pop(&i__rx_ring, &b) != RING_ERR_OK) {
        return -1;
    }
    return (int)b;
}

/** @brief Validate the arguments, then pass the bytes to the SDK stdio backend. */
k_print_err_t i_console_write(const char *buf, size_t len)
{
    if (len == 0) {
        return KPRINT_ERR_OK;
    }
    if (buf == NULL || len > (size_t)INT_MAX) {
        return KPRINT_ERR_INVAL;
    }
    int rc = stdio_put_string(buf, (int)len, false, false);
    return (rc < 0) ? KPRINT_ERR_IO : KPRINT_ERR_OK;
}

/** @brief Report whether the USB stdio backend has an open host connection. */
bool i_console_ready(void)
{
    return stdio_usb_connected();
}
