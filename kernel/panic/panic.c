/* SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 PicoKernel Project
 */

/**
 * @file panic.c
 *
 * @brief Implementation of the kernel panic handler.
 * @date 26-04-2026
 *
 * @details
 * Provides the implementation of kernel panic service, which handles
 * unrecoverable system errors.
 *
 * @warning
 * - Interrupts are globally disabled.
 *
 * @ingroup kernel
 * @author rootmnt
 * @version 0.1.0
 */

#include "panic.h"
#include "pico/stdlib.h"
#include <stdio.h>

noreturn void kernel_panic(const char *reason) {
  __disable_irq();
  if (reason == NULL)
    printf("KERNEL PANIC: (no reason passed)\n");
  else
    printf("KERNEL PANIC: %s\n", reason);
  while (1) {
    tight_loop_contents();
  };
}
