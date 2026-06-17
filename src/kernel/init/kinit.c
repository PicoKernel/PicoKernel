/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file kinit.c
 * @author datenbar
 * @date 15-06-2026
 * @brief Implementation of the kernel boot initialisation sequence.
 * @ingroup kernel
 *
 * @details
 * Orchestrates the full PicoKernel boot sequence: stdio, multicore flash
 * safety handshake, and all subsystem initialisations in dependency order.
 *
 * Design notes:
 * - Core 1 is launched early to call flash_safe_execute_core_init() before
 *   any flash operation is attempted on core 0.
 * - A FIFO handshake with FLAG = 0xC0FFEE confirms core 1 is ready before
 *   k_init() proceeds. The FIFO is free for flash_safe_execute() after this.
 * - Subsystem init order: allocator → hw timer → kernel time →
 *   scheduler → flash filesystem.
 *
 * Known limitations:
 * - MAX_TASKS is hardcoded to 8; should be configurable via CMake post-V1.
 * - Core 1 spins idle until dual-core work begins post-V1.
 *
 * @todo [Kernel][Enhancement] Dual-core support.
 * @todo [Kernel][Enhancement] Implement a custom flash_safety_helper_t using doorbells to replace the default FIFO-based lockout, freeing the FIFO for inter-core IPC.
 */
#include "kinit.h"
#include "flash_fs/flash_fs.h"
#include "hw_timer/hw_timer.h"
#include "memory/memory.h"
#include "panic/panic.h"
#include "scheduler/scheduler.h"
#include "time/time.h"
#include <pico/flash.h>
#include <pico/multicore.h>
#include <pico/platform/common.h>
#include <pico/stdio.h>
#include <stddef.h>
#include <stdint.h>

#define FLAG      0xC0FFEE /**< @brief Sentinel value used to confirm core 1 completed flash safe init. */
#define MAX_TASKS 8        /**< @brief Maximum number of scheduler tasks. */

/** @brief Launch core 1, initialise flash safety, and signal core 0 via FIFO. */
static void k__core1_entry(void)
{
    multicore_fifo_push_blocking(FLAG);

    bool ret = flash_safe_execute_core_init();

    if (!ret) {
        k_panic("Init: Failed to launch core1 in flash safe mode.");
    }

    while (1) {
        tight_loop_contents();
    }
}

/** @brief Initialise all kernel and driver subsystems in dependency order. */
void k_init(void)
{
    stdio_init_all();
    multicore_launch_core1(k__core1_entry);

    uint32_t flag = multicore_fifo_pop_blocking();
    if (flag != FLAG) {
        k_panic("Init: FLAG from core1 couldn't be retrieved.");
    }

    bool ret = flash_safe_execute_core_init();
    if (!ret) {
        k_panic("Init: Failed to launch core0 in flash safe mode.");
    }

    k_alloc_init();
    d_timer_init();
    k_time_init();
    k_scheduler_init((size_t)MAX_TASKS);
    d_flash_fs_init();
}
