/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file flash_fs.c
 * @author datenbar
 * @date 15-06-2026
 * @brief Implementation of the LittleFS flash filesystem driver.
 * @ingroup drivers
 *
 * @details
 * Implements the HAL callbacks required by LittleFS and the public filesystem
 * API declared in flash_fs.h. Flash reads are performed via XIP memory mapping
 * program and erase operations are wrapped in flash_safe_execute() to halt
 * core 1 and disable IRQs for the duration of the operation.
 *
 * Design notes:
 * - HAL callbacks are static and co-located with flash_fs.c they have
 *   exactly one consumer (lfs_config) and no meaningful portability boundary.
 * - A 4-slot static file pool is used in place of dynamic allocation.
 *   Pool state is tracked in bits 1-4 of d__fs_state.
 * - flash_safe_execute() parameters are passed via on-stack structs cast
 *   to void * stack lifetime is guaranteed for the duration of the call.
 *
 * Known limitations:
 * - File/directory operations are stubbed implementation deferred to M9.
 * - Format-or-prompt user interaction deferred to M9 (requires serial shell).
 * - Maximum 4 simultaneously open files/directories (shared pool).
 *
 * @todo [Driver][Enhancement] Add serial prompt on mount failure.
 * @todo [Driver][Enhancement] Implement adaptive block_cycles tuning based on observed per-block erase counts post-V1.
 * @todo [Driver][Enhancement] Implement file pool operations.
 */

#include "flash_fs.h"
#include "panic/panic.h"
#include <hardware/flash.h>
#include <pico/flash.h>
#include <hardware/regs/addressmap.h>
#include <pico/platform/sections.h>
#include <string.h>

#define FLASH_FS_OFFSET               (1024 * 1024)                                                               /**< @brief Byte offset of the LittleFS partition from the start of flash (1 MB). */
#define FLASH_BASE_ADDR               ((const uint8_t *)(XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE + FLASH_FS_OFFSET)) /**< @brief Base XIP address of the LittleFS partition. */
#define FLASH_FS_SIZE                 (768 * FLASH_SECTOR_SIZE)                                                   /**< @brief Total size of the LittleFS partition in bytes (3 MB, 768 sectors). */
#define FLASH_SAFE_EXECUTE_TIMEOUT_MS 1000                                                                        /**< @brief Timeout in milliseconds for flash_safe_execute() enter/exit. */

#define FS_MOUNTED (1u << 0) /**< @brief Bitmask: filesystem is mounted. */
#define FS_SLOT_0  (1u << 1) /**< @brief Bitmask: file pool slot 0 is in use. */
#define FS_SLOT_1  (1u << 2) /**< @brief Bitmask: file pool slot 1 is in use. */
#define FS_SLOT_2  (1u << 3) /**< @brief Bitmask: file pool slot 2 is in use. */
#define FS_SLOT_3  (1u << 4) /**< @brief Bitmask: file pool slot 3 is in use. */

static uint8_t d__fs_state;                                /**< @brief Combined filesystem and file pool state bitmask. See FS_MOUNTED and FS_SLOT_* masks. */
static lfs_file_t d__file_pool[4] __attribute__((unused)); /**< @brief Static pool of LittleFS file descriptors. Pool state tracked in d__fs_state bits 1-4. */
static lfs_t d__lfs;                                       /**< @brief LittleFS instance. Holds all internal filesystem state after mount. */

/**
 * @brief Read bytes from flash via XIP memory mapping.
 *
 * @details
 * Translates a LittleFS block/offset pair to an absolute XIP address and
 * copies the requested bytes into the caller's buffer. No flash_safe_execute()
 * wrapper is needed XIP reads are memory-mapped and do not modify flash.
 *
 * @param[in]  c      LittleFS configuration struct (block_size used for offset calculation).
 * @param[in]  block  LittleFS block index (0 to block_count - 1).
 * @param[in]  off    Byte offset within the block.
 * @param[out] buffer Destination buffer for the data read.
 * @param[in]  size   Number of bytes to read.
 * @return LFS_ERR_OK on success, LFS_ERR_INVAL if the request exceeds
 *         the partition bounds.
 */
static int d__flash_hal_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);

/**
 * @brief Program a page-aligned region of flash via flash_safe_execute().
 *
 * @details
 * Validates alignment and bounds, then delegates to flash_range_program()
 * via flash_safe_execute() to ensure core 1 is halted and IRQs are disabled
 * for the duration of the operation.
 *
 * @param[in] c      LittleFS configuration struct.
 * @param[in] block  LittleFS block index.
 * @param[in] off    Byte offset within the block. Must be FLASH_PAGE_SIZE-aligned.
 * @param[in] buffer Source data to program into flash.
 * @param[in] size   Number of bytes to program. Must be a multiple of FLASH_PAGE_SIZE.
 * @return LFS_ERR_OK on success, LFS_ERR_INVAL on alignment or bounds violation,
 *         LFS_ERR_IO on flash_safe_execute() failure.
 *
 * @warning flash_safe_execute_core_init() must have been called on both cores
 *          before this function is reachable.
 */
static int d__flash_hal_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);

/**
 * @brief Erase a sector-aligned flash block via flash_safe_execute().
 *
 * @details
 * Validates sector alignment and bounds, then delegates to flash_range_erase()
 * via flash_safe_execute(). Erasing sets all bits in the block to 1.
 *
 * @param[in] c     LittleFS configuration struct (block_size used for erase count).
 * @param[in] block LittleFS block index. The corresponding flash offset must be
 *                  FLASH_SECTOR_SIZE-aligned.
 * @return LFS_ERR_OK on success, LFS_ERR_INVAL on alignment or bounds violation,
 *         LFS_ERR_IO on flash_safe_execute() failure.
 *
 * @warning flash_safe_execute_core_init() must have been called on both cores
 *          before this function is reachable.
 */
static int d__flash_hal_erase(const struct lfs_config *c, lfs_block_t block);

/**
 * @brief No-op sync callback for LittleFS.
 *
 * @details
 * flash_range_program() writes synchronously to flash hardware with no
 * intermediate write buffer. There is nothing to flush.
 *
 * @param[in] c LittleFS configuration struct (unused).
 * @return LFS_ERR_OK always.
 */
static int d__flash_hal_sync(const struct lfs_config *c);

/** @brief LittleFS configuration struct wiring HAL callbacks and partition parameters. */
const static struct lfs_config d__cfg = {
    .context = NULL,                 /**< No per-instance context needed. */
    .read = d__flash_hal_read,       /**< XIP memory-mapped read callback. */
    .prog = d__flash_hal_prog,       /**< Page-aligned program callback. */
    .erase = d__flash_hal_erase,     /**< Sector-aligned erase callback. */
    .sync = d__flash_hal_sync,       /**< No-op sync callback. */
    .read_size = 1,                  /**< XIP allows byte-granularity reads. */
    .prog_size = FLASH_PAGE_SIZE,    /**< flash_range_program page constraint. */
    .block_size = FLASH_SECTOR_SIZE, /**< flash_range_erase sector constraint. */
    .block_count = 768,              /**< 3 MB partition / 4 KB sector = 768 blocks. */
    .block_cycles = 500,             /**< Wear levelling hint; tuned later. */
    .cache_size = FLASH_PAGE_SIZE,   /**< Must be a multiple of prog_size. */
    .lookahead_size = 16,            /**< Lookahead buffer size in bytes, multiple of 8. */
};

/**
 * @brief Parameters passed to d__flash_erase_callback() via flash_safe_execute().
 */
typedef struct {
    uint32_t flash_offs; /**< Flash-relative offset of the erase target. */
    size_t count;        /**< Number of bytes to erase. */
} flash_erase_params_t;

/**
 * @brief Parameters passed to d__flash_prog_callback() via flash_safe_execute().
 */
typedef struct {
    uint32_t flash_offs; /**< Flash-relative offset of the program target. */
    const uint8_t *data; /**< Source data buffer to program into flash. */
    size_t count;        /**< Number of bytes to program. */
} flash_prog_params_t;

/** @brief Mount the filesystem, formatting on first boot if necessary. */
void d_flash_fs_init(void)
{
    int mount = lfs_mount(&d__lfs, &d__cfg);
    if (mount) {
        lfs_format(&d__lfs, &d__cfg);
        mount = lfs_mount(&d__lfs, &d__cfg);
        if (!mount) {
            d__fs_state |= FS_MOUNTED;
            return;
        }
        k_panic("FS: Failed to format and mount");
    }
    d__fs_state |= FS_MOUNTED;
}

/** @brief Unmount the filesystem and clear mounted state. */
void d_flash_fs_deinit(void)
{
    if (d__fs_state & FS_MOUNTED) {
        lfs_unmount(&d__lfs);
        d__fs_state &= ~FS_MOUNTED;
    }
}

/** @brief Read bytes from flash via XIP memory mapping. */
static int d__flash_hal_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    uint32_t offset = (block * c->block_size) + off; /* The asterisk here is for multiplication. */

    if ((uint64_t)offset + (uint64_t)size > (uint64_t)FLASH_FS_SIZE) {
        return LFS_ERR_INVAL;
    }

    const uint8_t *xip_addr = (const uint8_t *)(FLASH_BASE_ADDR + offset);

    memcpy(buffer, xip_addr, size);

    return LFS_ERR_OK;
}

/** @brief flash_safe_execute() callback that invokes flash_range_program(). */
static void __no_inline_not_in_flash_func(d__flash_prog_callback)(void *params)
{
    flash_prog_params_t *p = (flash_prog_params_t *)params;
    flash_range_program(p->flash_offs, p->data, p->count);
}

/** @brief Program a page-aligned flash region via flash_safe_execute(). */
static int d__flash_hal_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    flash_prog_params_t params = {
        .flash_offs = FLASH_FS_OFFSET + (block * c->block_size) + off,
        .data = (const uint8_t *)buffer,
        .count = size,
    };

    if (params.flash_offs & (FLASH_PAGE_SIZE - 1) || size & (FLASH_PAGE_SIZE - 1)) {
        return LFS_ERR_INVAL;
    }

    if (params.flash_offs + size > FLASH_FS_OFFSET + FLASH_FS_SIZE) {
        return LFS_ERR_INVAL;
    }

    int rc = flash_safe_execute(d__flash_prog_callback, (void *)&params, FLASH_SAFE_EXECUTE_TIMEOUT_MS);

    return rc == PICO_OK ? LFS_ERR_OK : LFS_ERR_IO;
}

/** @brief flash_safe_execute() callback that invokes flash_range_erase(). */
static void __no_inline_not_in_flash_func(d__flash_erase_callback)(void *params)
{
    flash_erase_params_t *p = (flash_erase_params_t *)params;
    flash_range_erase(p->flash_offs, p->count);
}

/** @brief Erase a sector-aligned flash block via flash_safe_execute(). */
static int d__flash_hal_erase(const struct lfs_config *c, lfs_block_t block)
{
    flash_erase_params_t params = {
        .flash_offs = FLASH_FS_OFFSET + (block * c->block_size),
        .count = c->block_size,
    };

    if (params.flash_offs & (FLASH_SECTOR_SIZE - 1) || params.count & (FLASH_SECTOR_SIZE - 1)) {
        return LFS_ERR_INVAL;
    }

    if (params.flash_offs + params.count > FLASH_FS_OFFSET + FLASH_FS_SIZE) {
        return LFS_ERR_INVAL;
    }

    int rc = flash_safe_execute(d__flash_erase_callback, (void *)&params, FLASH_SAFE_EXECUTE_TIMEOUT_MS);

    return rc == PICO_OK ? LFS_ERR_OK : LFS_ERR_IO;
}

/** @brief No-op sync flash_range_program() writes synchronously. */
static int d__flash_hal_sync(const struct lfs_config *c)
{
    (void)c;
    return LFS_ERR_OK;
}
