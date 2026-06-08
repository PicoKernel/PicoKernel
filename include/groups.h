/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file groups.h
 * @brief Doxygen group definitions for PicoKernel.
 *
 * This file contains no code. It exists solely to define
 * documentation groups for the Doxygen output.
 *
 * Do not include this file in any source file.
 */

#ifndef DOXYGEN
#error "This file is for Doxygen only and must not be included in source files."
#endif

/**
 * @defgroup kernel Kernel
 * @brief Core OS primitives : memory, scheduling, IPC, and panic handling.
 */

/**
 * @defgroup drivers Drivers
 * @brief Thin hardware abstraction layer over the Pico SDK.
 */

/**
 * @defgroup modules Modules
 * @brief Application-level subsystems registered with the kernel scheduler.
 */

/**
 * @defgroup interface Interface
 * @brief Outward-facing components routing requests through kernel APIs.
 */
