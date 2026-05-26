/**
 * @file panic.c
 *
 * @brief Implementation of the kernel panic handler.
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
 * @date 2026-04-26
 * @copyright Copyright (c) 2026 Picokernel Project.
 *            Licensed under the MIT License.
 *
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
