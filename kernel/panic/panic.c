/* SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 PicoKernel Project
 */

/**
 * @file panic.c
 * @author rootmnt
 * @date 26-04-2026
 * @brief Implementation of the kernel panic handler.
 * @ingroup kernel
 * @version 0.1.0
 *
 * @details
 * Provides the implementation of kernel panic service, which handles
 * unrecoverable system errors.
 *
 * @warning
 * - Interrupts are globally disabled.
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
