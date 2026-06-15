/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file panic.h
 * @author rootmnt
 * @date 26-04-2026
 * @brief Provides a kernel panic service.
 * @ingroup kernel
 *
 * @details
 * Provides a single function to handle serious errors.
 * Logs the reason over USB serial and stalls until hardware watchdog fires and
 * reboots.
 *
 * Constraints:
 * - Never returns.
 * - Depends on the hardware watchdog on RP2350.
 * - Must only be used in dire situations.
 * - Doesn't wait for any ISRs or tasks.
 * - Doesn't save tasks, module is responsible for handling context before call.
 *
 * Security:
 * - Caller is responsible for pointer validation.
 */

#ifndef PICOKERNEL_KERNEL_PANIC_H
#define PICOKERNEL_KERNEL_PANIC_H

#include <stdnoreturn.h>
/**
 * @brief Triggers a kernel panic and forces a watchdog-driven reboot.
 *
 * @param[in] reason Pointer to a null-terminated string describing the fault.
 *                   May be NULL.
 *
 * @warning Neither returns nor waits for the ISRs.
 *
 * @return Nothing.
 */
noreturn void k_panic(const char *reason);

#endif
