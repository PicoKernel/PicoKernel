// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 PicoKernel Contributors

/**
 * @file main.c
 * @brief Firmware entry point.
 *
 */

#include <pico/stdlib.h>

/**
 * @brief Firmware entry point.
 */
int main(void)
{
    stdio_init_all();

    while (true) {
        tight_loop_contents();
    }

    return 0;
}
