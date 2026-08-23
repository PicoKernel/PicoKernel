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
 * - Separate 12-slot static pools are used for files and directories,
 *   in place of dynamic allocation. Pool and mount state are tracked in d__fs_state: bit 31 is FS_MOUNTED,
 *   file pool slots occupy bits FS_FILE_BASE..FS_FILE_BASE+11, directory pool slots occupy bits FS_DIR_BASE..FS_DIR_BASE+11,
 *   remaining bits are reserved for future use.
 * - flash_safe_execute() parameters are passed via on-stack structs cast
 *   to void * stack lifetime is guaranteed for the duration of the call.
 *
 * Known limitations:
 * - Maximum 12 simultaneously open files and 12 simultaneously open
 *   directories (separate pools, do not share slots).
 * - Recursive modules, if implemented recursively with
 *   held directory handles rather than an iterative worklist, is
 *   limited to 12 levels of nesting by the directory pool size.
 *
 * @todo [Driver][Enhancement] Add serial prompt on mount failure.
 * @todo [Driver][Enhancement] Implement adaptive block_cycles tuning based on observed per-block erase counts post-V1.
 * @todo [Driver][Enhancement] Redirect lfs_malloc/lfs_free to k_alloc/k_free (heap-backed dynamic file/dir handles) post-V1.
 */

#include "flash_fs.h"
#include "lfs.h"
#include "panic/panic.h"
#include <hardware/flash.h>
#include <pico/flash.h>
#include <hardware/regs/addressmap.h>
#include <pico/platform/sections.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FLASH_FS_OFFSET               (1024 * 1024)                                                               /**< @brief Byte offset of the LittleFS partition from the start of flash (1 MB). */
#define FLASH_BASE_ADDR               ((const uint8_t *)(XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE + FLASH_FS_OFFSET)) /**< @brief Base XIP address of the LittleFS partition. */
#define FLASH_FS_SIZE                 (768 * FLASH_SECTOR_SIZE)                                                   /**< @brief Total size of the LittleFS partition in bytes (3 MB, 768 sectors). */
#define FLASH_SAFE_EXECUTE_TIMEOUT_MS 1000                                                                        /**< @brief Timeout in milliseconds for flash_safe_execute() enter/exit. */

#define FS_MOUNTED   (1U << 31) /**< @brief Bitmask: filesystem is mounted. */
#define FS_FILE_BASE 0          /**< @brief File pool start. */
#define FS_DIR_BASE  12         /**< @brief Directory pool start. */

static uint32_t d__fs_state;        /**< @brief Combined filesystem and file and directory pool state bitmask. See FS_MOUNTED, FS_FILE_BASE and FS_DIR_BASE masks. */
static lfs_file_t d__file_pool[12]; /**< @brief Static pool of LittleFS file descriptors. Pool state tracked in d__fs_state bits 1-12. */
static lfs_dir_t d__dir_pool[12];   /**< @brief Static pool of LittleFS directory handles. Pool state tracked in d__fs_state bits 13-25. */
static lfs_t d__lfs;                /**< @brief LittleFS instance. Holds all internal filesystem state after mount. */

/**
 * @brief Translate a LittleFS error code into the driver's own error type.
 *
 * @details
 * Maps every value in enum lfs_error to the corresponding d_fs_err_t
 * constant so that raw LittleFS error codes never cross the driver
 * boundary into modules or interfaces.
 *
 * @param[in] lfs_rc Raw return value from a LittleFS API call (an
 *                    lfs_error value, or LFS_ERR_OK on success).
 * @return The corresponding d_fs_err_t value. Returns D_FS_ERR_IO if
 *         lfs_rc does not match any known lfs_error code.
 */
static d_fs_err_t d__lfs_err_to_fs_err(int lfs_rc);

/**
 * @brief Translate the driver's own seek origin into a LittleFS whence value.
 *
 * @details
 * Maps each d_fs_whence_t constant to its corresponding LFS_SEEK_*
 * value expected by lfs_file_seek().
 *
 * @param[in] whence Caller-supplied seek origin.
 * @return The equivalent LFS_SEEK_* value on success, or
 *         D_FS_ERR_INVAL (cast to int) if whence does not match a
 *         known d_fs_whence_t value.
 */
static int d__fs_whence_to_lfs(d_fs_whence_t whence);

/**
 * @brief Translate the driver's own open flags into LittleFS open flags.
 *
 * @details
 * Maps each set bit in a d_fs_flags_t value to its corresponding
 * LFS_O_* constant and combines them with bitwise OR into the raw int
 * flags value expected by lfs_file_open().
 *
 * If both D_FS_O_RDONLY and D_FS_O_WRONLY are set, they are treated
 * as equivalent to D_FS_O_RDWR. If neither D_FS_O_RDONLY,
 * D_FS_O_WRONLY, nor D_FS_O_RDWR is set, D_FS_O_RDWR is assumed.
 *
 * @param[in] flags Caller-supplied open flags.
 * @return The equivalent LFS_O_* bitmask on success, or
 *         D_FS_ERR_INVAL (cast to int) if flags contains any bit
 *         outside the recognized d_fs_flags_t set.
 */
static int d__fs_flags_to_lfs(d_fs_flags_t flags);

/**
 * @brief Translate a LittleFS file type into the driver's own type enum.
 *
 * @details
 * Maps LFS_TYPE_REG and LFS_TYPE_DIR to the corresponding d_fs_type_t
 * constant. LittleFS's internally-used type values (LFS_TYPE_SPLICE,
 * LFS_TYPE_STRUCT, etc.) are never returned by lfs_stat() or
 * lfs_dir_read() and are not expected here.
 *
 * @param[in] lfs_type Raw type value from struct lfs_info::type.
 * @return The corresponding d_fs_type_t value. Returns D_FS_TYPE_UNKNOWN
 *         if lfs_type does not match a known public file type.
 */
static d_fs_type_t d__lfs_type_to_fs_type(uint8_t lfs_type);

/**
 * @brief Convert a raw LittleFS info struct into the driver's own info type.
 *
 * @details
 * Translates struct lfs_info (as populated by lfs_stat() or
 * lfs_dir_read()) into d_fs_info_t so that raw LittleFS types never
 * cross the driver boundary into modules or interfaces.
 *
 * @param[in]  raw  Populated struct lfs_info from a prior LittleFS call.
 * @param[out] info Destination d_fs_info_t to populate.
 */
static void d__lfs_info_to_fs_info(const struct lfs_info *raw, d_fs_info_t *info);

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
static const struct lfs_config d__cfg = {
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

/** @brief Open or create a file at the given path. */
void *d_flash_fs_open(const char *path, d_fs_flags_t flags)
{
    int free = -1;
    for (int i = 0; i < 12; i++) {
        if (!(d__fs_state & (1U << (FS_FILE_BASE + i)))) {
            free = i;
            break;
        }
    }
    if (free == -1) {
        return NULL;
    }
    int lfs_flags = d__fs_flags_to_lfs(flags);
    if (lfs_flags < 0) {
        return NULL;
    }
    int rc = lfs_file_open(&d__lfs, &d__file_pool[free], path, lfs_flags);
    if (rc < 0) {
        return NULL;
    }
    d__fs_state |= (1U << (FS_FILE_BASE + free));
    return (void *)(&d__file_pool[free]);
}

/** @brief Close an open file and release its pool slot. */
d_fs_err_t d_flash_fs_close(void *file)
{
    lfs_file_t *f = (lfs_file_t *)file;
    int slot = f - d__file_pool;
    int rc = lfs_file_close(&d__lfs, f);
    d__fs_state &= ~(1U << (FS_FILE_BASE + slot));
    return d__lfs_err_to_fs_err(rc);
}

/** @brief Read bytes from an open file into a buffer. */
int d_flash_fs_read(void *file, void *buffer, size_t size)
{
    if (file == NULL || buffer == NULL) {
        return D_FS_ERR_INVAL;
    }
    lfs_file_t *f = (lfs_file_t *)file;
    lfs_ssize_t rc = lfs_file_read(&d__lfs, f, buffer, (lfs_size_t)size);
    if (rc < 0) {
        return d__lfs_err_to_fs_err(rc);
    }
    return (int)rc;
}

/** @brief Write bytes from a buffer into an open file. */
int d_flash_fs_write(void *file, const void *buffer, size_t size)
{
    if (file == NULL || buffer == NULL) {
        return D_FS_ERR_INVAL;
    }
    lfs_file_t *f = (lfs_file_t *)file;
    lfs_ssize_t rc = lfs_file_write(&d__lfs, f, buffer, (lfs_size_t)size);
    if (rc < 0) {
        return d__lfs_err_to_fs_err(rc);
    }
    return (int)rc;
}

/** @brief Reposition the read/write offset within an open file. */
int d_flash_fs_seek(void *file, int32_t offset, d_fs_whence_t whence) // NOLINT(bugprone-easily-swappable-parameters)
{
    if (file == NULL) {
        return D_FS_ERR_INVAL;
    }
    int lfs_whence = d__fs_whence_to_lfs(whence);
    if (lfs_whence < 0) {
        return D_FS_ERR_INVAL;
    }

    lfs_file_t *f = (lfs_file_t *)file;
    lfs_soff_t rc = lfs_file_seek(&d__lfs, f, offset, lfs_whence);
    if (rc < 0) {
        return d__lfs_err_to_fs_err((int)rc);
    }

    return (int)rc;
}

/** @brief Remove a file at the given path. */
d_fs_err_t d_flash_fs_remove(const char *path)
{
    struct lfs_info info;
    int rc = lfs_stat(&d__lfs, path, &info);
    if (rc != LFS_ERR_OK) {
        return d__lfs_err_to_fs_err(rc);
    }
    if (info.type != LFS_TYPE_REG) {
        return D_FS_ERR_ISDIR;
    }
    return d__lfs_err_to_fs_err(lfs_remove(&d__lfs, path));
}

/** @brief Retrieve metadata for a file or directory. */
d_fs_err_t d_flash_fs_stat(const char *path, d_fs_info_t *info)
{
    struct lfs_info raw;
    int rc = lfs_stat(&d__lfs, path, &raw);
    if (rc != LFS_ERR_OK) {
        return d__lfs_err_to_fs_err(rc);
    }
    d__lfs_info_to_fs_info(&raw, info);
    return D_FS_ERR_OK;
}

/** @brief Create a directory at the given path. */
d_fs_err_t d_flash_fs_mkdir(const char *path)
{
    if (path == NULL) {
        return D_FS_ERR_INVAL;
    }
    return d__lfs_err_to_fs_err(lfs_mkdir(&d__lfs, path));
}

/** @brief Open a directory for iteration. */
void *d_flash_fs_opendir(const char *path)
{
    int free = -1;
    for (int i = 0; i < 12; i++) {
        if (!(d__fs_state & (1U << (FS_DIR_BASE + i)))) {
            free = i;
            break;
        }
    }
    if (free == -1) {
        return NULL;
    }
    int rc = lfs_dir_open(&d__lfs, &d__dir_pool[free], path);
    if (rc < 0) {
        return NULL;
    }
    d__fs_state |= (1U << (FS_DIR_BASE + free));
    return (void *)(&d__dir_pool[free]);
}

/** @brief Read the next entry from an open directory. */
int d_flash_fs_readdir(void *dir, d_fs_info_t *info)
{
    if (dir == NULL || info == NULL) {
        return D_FS_ERR_INVAL;
    }
    lfs_dir_t *d = (lfs_dir_t *)dir;
    struct lfs_info lfs_info;
    int rc = lfs_dir_read(&d__lfs, d, &lfs_info);
    if (rc > 0) {
        d__lfs_info_to_fs_info(&lfs_info, info);
        return 1;
    }
    if (rc == 0) {
        return 0;
    }
    return d__lfs_err_to_fs_err(rc);
}

/** @brief Close an open directory handle and release its pool slot. */
d_fs_err_t d_flash_fs_closedir(void *dir)
{
    lfs_dir_t *d = (lfs_dir_t *)dir;
    int slot = d - d__dir_pool;
    int rc = lfs_dir_close(&d__lfs, d);
    d__fs_state &= ~(1U << (FS_DIR_BASE + slot));
    return d__lfs_err_to_fs_err(rc);
}

/** @brief Remove an empty directory at the given path. */
d_fs_err_t d_flash_fs_rmdir(const char *path)
{
    struct lfs_info info;
    int rc = lfs_stat(&d__lfs, path, &info);
    if (rc != LFS_ERR_OK) {
        return d__lfs_err_to_fs_err(rc);
    }
    if (info.type != LFS_TYPE_DIR) {
        return D_FS_ERR_NOTDIR;
    }
    return d__lfs_err_to_fs_err(lfs_remove(&d__lfs, path));
}

/** @brief Query used and total filesystem space in bytes. */
d_fs_err_t d_flash_fs_size(int32_t *used, int32_t *total) // NOLINT(bugprone-easily-swappable-parameters)
{
    lfs_ssize_t used_blocks = lfs_fs_size(&d__lfs);
    if (used_blocks < 0) {
        return d__lfs_err_to_fs_err(used_blocks);
    }
    *used = used_blocks * (int32_t)FLASH_SECTOR_SIZE;
    *total = FLASH_FS_SIZE;
    return D_FS_ERR_OK;
}

/** @brief Check whether a file or directory exists at the given path. */
bool d_flash_fs_exists(const char *path)
{
    struct lfs_info info;
    return lfs_stat(&d__lfs, path, &info) == LFS_ERR_OK;
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

static d_fs_err_t d__lfs_err_to_fs_err(int lfs_rc)
{
    switch (lfs_rc) {
    case LFS_ERR_OK:
        return D_FS_ERR_OK;
    case LFS_ERR_IO:
        return D_FS_ERR_IO;
    case LFS_ERR_CORRUPT:
        return D_FS_ERR_CORRUPT;
    case LFS_ERR_NOENT:
        return D_FS_ERR_NOENT;
    case LFS_ERR_EXIST:
        return D_FS_ERR_EXIST;
    case LFS_ERR_NOTDIR:
        return D_FS_ERR_NOTDIR;
    case LFS_ERR_ISDIR:
        return D_FS_ERR_ISDIR;
    case LFS_ERR_NOTEMPTY:
        return D_FS_ERR_NOTEMPTY;
    case LFS_ERR_BADF:
        return D_FS_ERR_BADF;
    case LFS_ERR_FBIG:
        return D_FS_ERR_FBIG;
    case LFS_ERR_INVAL:
        return D_FS_ERR_INVAL;
    case LFS_ERR_NOSPC:
        return D_FS_ERR_NOSPC;
    case LFS_ERR_NOMEM:
        return D_FS_ERR_NOMEM;
    case LFS_ERR_NOATTR:
        return D_FS_ERR_NOATTR;
    case LFS_ERR_NAMETOOLONG:
        return D_FS_ERR_NAMETOOLONG;
    default:
        return D_FS_ERR_IO;
    }
}

static d_fs_type_t d__lfs_type_to_fs_type(uint8_t lfs_type)
{
    switch (lfs_type) {
    case LFS_TYPE_DIR:
        return D_FS_TYPE_DIR;
    case LFS_TYPE_REG:
        return D_FS_TYPE_REG;
    default:
        return D_FS_TYPE_UNKNOWN;
    }
}

static void d__lfs_info_to_fs_info(const struct lfs_info *raw, d_fs_info_t *info)
{
    info->type = d__lfs_type_to_fs_type(raw->type);
    info->size = raw->size;
    memcpy(info->name, raw->name, (LFS_NAME_MAX + 1));
}

static int d__fs_flags_to_lfs(d_fs_flags_t flags)
{
    const uint8_t all_valid = D_FS_O_RDONLY | D_FS_O_WRONLY | D_FS_O_RDWR | D_FS_O_CREAT | D_FS_O_EXCL | D_FS_O_TRUNC | D_FS_O_APPEND;

    if (flags & ~all_valid) {
        return D_FS_ERR_INVAL;
    }

    int lfs_flags = 0;

    bool rdonly = (flags & D_FS_O_RDONLY) != 0;
    bool wronly = (flags & D_FS_O_WRONLY) != 0;
    bool rdwr = (flags & D_FS_O_RDWR) != 0;

    if (rdwr || (rdonly && wronly) || (!rdonly && !wronly)) {
        lfs_flags |= LFS_O_RDWR;
    } else if (wronly) {
        lfs_flags |= LFS_O_WRONLY;
    } else if (rdonly) {
        lfs_flags |= LFS_O_RDONLY;
    }

    if (flags & D_FS_O_CREAT) {
        lfs_flags |= LFS_O_CREAT;
    }
    if (flags & D_FS_O_EXCL) {
        lfs_flags |= LFS_O_EXCL;
    }
    if (flags & D_FS_O_TRUNC) {
        lfs_flags |= LFS_O_TRUNC;
    }
    if (flags & D_FS_O_APPEND) {
        lfs_flags |= LFS_O_APPEND;
    }

    return lfs_flags;
}

static int d__fs_whence_to_lfs(d_fs_whence_t whence)
{
    switch (whence) {
    case D_FS_SEEK_SET:
        return LFS_SEEK_SET;
    case D_FS_SEEK_CUR:
        return LFS_SEEK_CUR;
    case D_FS_SEEK_END:
        return LFS_SEEK_END;
    default:
        return D_FS_ERR_INVAL;
    }
}
