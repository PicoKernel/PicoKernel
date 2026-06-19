cmake_minimum_required(VERSION 3.20)

# ─── Build options ────────────────────────────────────────────────────────────

# Target board. pico2_w = RP2350 + CYW43 Wi-Fi/BT chip.
# Override: cmake -DPICO_BOARD=pico2 .. (for non-wireless builds)
set(PICO_BOARD pico2_w CACHE STRING "Board Type")

# ─── Pico SDK bootstrap ───────────────────────────────────────────────────────
#
# Must happen before project(). Uses pico-sdk as a git submodule.
# Override the path by setting PICO_SDK_PATH as an env or CMake variable:
#   cmake -DPICO_SDK_PATH=/path/to/other/sdk ..
#
if(NOT DEFINED ENV{PICO_SDK_PATH} AND NOT DEFINED PICO_SDK_PATH)
  set(PICO_SDK_PATH "${CMAKE_SOURCE_DIR}/lib/pico-sdk" CACHE PATH
        "Path to pico-sdk (defaults to submodule)")
endif()
include(${CMAKE_SOURCE_DIR}/lib/pico-sdk/external/pico_sdk_import.cmake)

# ─── Project ──────────────────────────────────────────────────────────────────
project(PicoKernel
    VERSION     0.3.0
    DESCRIPTION "A bare-metal cooperative OS for the Raspberry Pi Pico 2W"
    LANGUAGES   C CXX ASM
)

set(CMAKE_C_STANDARD          11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS        ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── USB Delay ────────────────────────────────────────────────────────────

if (NOT DEFINED PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS)
  set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 3000)
endif()

# Kernel debug instrumentation: heap dumps (kdump), verbose panic output.
# Gated by #ifdef KERNEL_DEBUG in source never active in release builds.
# Enable: cmake -DKERNEL_DEBUG=ON ..
option(KERNEL_DEBUG "Enable kernel debug instrumentation (kdump, verbose logging)" OFF)

# ─── Pico SDK init ────────────────────────────────────────────────────────────
pico_sdk_init()

# ─── Source collection ────────────────────────────────────────────────────────
#
# GLOB_RECURSE with CONFIGURE_DEPENDS:
#   CMake re-globs automatically when the filesystem changes at build time.
#   However, on a fresh cmake invocation (e.g., after `rm -rf build/`),
#   you may need to re-run cmake if sources were added since the last run.
#   Touch this file or run `cmake -B build` again to force re-evaluation.
#
file(GLOB_RECURSE KERNEL_SOURCES    CONFIGURE_DEPENDS src/kernel/*.c)
file(GLOB_RECURSE DRIVER_SOURCES    CONFIGURE_DEPENDS src/drivers/*.c)
file(GLOB_RECURSE MODULE_SOURCES    CONFIGURE_DEPENDS src/modules/*.c)
file(GLOB_RECURSE INTERFACE_SOURCES CONFIGURE_DEPENDS src/interface/*.c)

# Fail loudly if the source tree is missing or not initialised.
if(NOT KERNEL_SOURCES)
  message(FATAL_ERROR
        "No kernel sources found under src/kernel/. "
        "Did you forget to run: git submodule update --init --recursive?")
endif()

if(NOT EXISTS ${CMAKE_SOURCE_DIR}/src/main.c)
  message(FATAL_ERROR "src/main.c not found, every PicoKernel build needs an entry point.")
endif()

set(PICOKERNEL_SOURCES
    src/main.c
    ${KERNEL_SOURCES}
    ${DRIVER_SOURCES}
    ${MODULE_SOURCES}
    ${INTERFACE_SOURCES}
)

# ─── Executable target ────────────────────────────────────────────────────────
add_executable(picokernel ${PICOKERNEL_SOURCES})

set_target_properties(picokernel PROPERTIES
    OUTPUT_NAME "PicoKernel"
)

# ─── Include directories ──────────────────────────────────────────────────────
target_include_directories(picokernel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interface
)

# ─── Compiler warnings ────────────────────────────────────────────────────────
target_compile_options(picokernel PRIVATE
    -Wall
    -Wextra
    -Werror
    # Peripheral init callbacks often have fixed signatures that include
    # parameters unused in our implementation. Suppress globally.
    -Wno-unused-parameter
)

# ─── Compile definitions ──────────────────────────────────────────────────────
if(KERNEL_DEBUG)
  target_compile_definitions(picokernel PRIVATE KERNEL_DEBUG=1)
  message(STATUS "PicoKernel: KERNEL_DEBUG ON - heap dump and verbose instrumentation active")
endif()

# ─── Pico SDK libraries ───────────────────────────────────────────────────────
target_link_libraries(picokernel
    pico_stdlib
    pico_cyw43_arch_none
    hardware_timer
)

# ─── stdio routing ────────────────────────────────────────────────────────────
# UART: GP0 (TX) / GP1 (RX) at 115200 baud. Use minicom -b 115200 -D /dev/ttyUSB0
# USB:  Appears as /dev/ttyACM0. Swap values below if using USB-CDC.
pico_enable_stdio_uart(picokernel 0)
pico_enable_stdio_usb(picokernel  1)

# ─── Binary outputs ───────────────────────────────────────────────────────────
# Produces: picokernel.uf2  (drag-and-drop flash)
#           picokernel.elf  (openocd / gdb)
#           picokernel.bin  (raw binary)
#           picokernel.dis  (disassembly)
pico_add_extra_outputs(picokernel)
