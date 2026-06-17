/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file kinit.h
 * @author datenbar
 * @date 15-06-2026
 * @brief Kernel boot initialisation sequence.
 * @ingroup kernel
 *
 * @details
 * Exposes the single entry point for the PicoKernel boot sequence.
 * All subsystem initialisation is orchestrated through k_init().
 *
 * Constraints:
 * - k_init() must be the first call in main().
 * - No subsystem API may be called before k_init() returns.
 *
 * Security:
 * - flash_safe_execute_core_init() is called on both cores before any flash operation is attempted.
 */
#ifndef PICOKERNEL_KERNEL_KINIT_H
#define PICOKERNEL_KERNEL_KINIT_H

/**
 * @brief Initialise all kernel and driver subsystems in dependency order.
 *
 * @note Must be called once from main() before any other kernel API.
 *
 * @warning Calls k_panic() if core 1 fails to initialise or the FIFO
 * handshake does not return the expected flag.
 */
void k_init(void);

#endif
