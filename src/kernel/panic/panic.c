/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file panic.c
 * @author rootmnt
 * @date 26-04-2026
 * @brief Implementation of the kernel panic handler.
 * @ingroup kernel
 *
 * @details
 * Provides the implementation of kernel panic service, which handles
 * unrecoverable system errors.
 *
 * @warning
 * - Interrupts are globally disabled.
 */

#include "panic.h"
#include <hardware/sync.h>
#include <stdio.h>

/**
 * @brief Triggers a kernel panic. Talks directly to the SDK. Doesn't return.
 */
noreturn void k_panic(const char *reason)
{
    save_and_disable_interrupts();
    if (reason == NULL) {
        printf("KERNEL PANIC: (no reason passed)\n");
    } else {
        printf("KERNEL PANIC: %s\n", reason);
    }
    while (1) {
        tight_loop_contents();
    };
}
