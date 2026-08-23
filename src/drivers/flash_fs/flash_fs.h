/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025 PicoKernel Contributors
 */

/**
 * @file flash_fs.h
 * @author datenbar
 * @date 15-06-2026
 * @brief Public API for the LittleFS-backed flash filesystem driver.
 * @ingroup drivers
 *
 * @details
 * Exposes lifecycle management and file/directory operations over a 3 MB
 * LittleFS partition beginning at FLASH_FS_OFFSET (1 MB into flash).
 * All hardware access is encapsulated in flash_fs.c
 *
 * Constraints:
 * - d_flash_fs_init() must be called before any other function in this API.
 * - flash_safe_execute_core_init() must be called on both cores before init.
 * - A maximum of 12 files and 12 directories may be open simultaneously
 *   (separate pools, files and directories do not share slots).
 * - File and directory handles are opaque, never cast or dereference them.
 *
 * Security:
 * - All flash offsets are bounds-checked before hardware access.
 * - Alignment of prog/erase offsets is validated before calling SDK functions.
 * - Caller-supplied paths are passed directly to LittleFS with no sanitisation.
 */

#ifndef PICOKERNEL_DRIVERS_FLASH_FS_H
#define PICOKERNEL_DRIVERS_FLASH_FS_H

#include <lfs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/** @brief Seek origin for d_flash_fs_seek(). */
typedef enum {
    D_FS_SEEK_SET, /**< @brief Offset is absolute, from the start of the file. */
    D_FS_SEEK_CUR, /**< @brief Offset is relative to the current file position. */
    D_FS_SEEK_END, /**< @brief Offset is relative to the end of the file. */
} d_fs_whence_t;

/** @brief Type of a filesystem entry, as reported by d_fs_info_t. */
typedef enum {
    D_FS_TYPE_REG,     /**< @brief Regular file. */
    D_FS_TYPE_DIR,     /**< @brief Directory. */
    D_FS_TYPE_UNKNOWN, /**< @brief Type could not be determined; treat as neither file nor directory. */
} d_fs_type_t;

/** @brief Metadata for a single file or directory entry. */
typedef struct {
    d_fs_type_t type;            /**< @brief Entry type: regular file, directory, or unknown. */
    uint32_t size;               /**< @brief Size in bytes. Only meaningful for D_FS_TYPE_REG. */
    char name[LFS_NAME_MAX + 1]; /**< @brief Null-terminated entry name. */
} d_fs_info_t;

/** @brief Open-mode flags for d_flash_fs_open(). Combine with bitwise OR. */
typedef enum {
    D_FS_O_RDONLY = (1 << 0), /**< @brief Open for reading only. */
    D_FS_O_WRONLY = (1 << 1), /**< @brief Open for writing only. */
    D_FS_O_RDWR = (1 << 2),   /**< @brief Open for reading and writing. */
    D_FS_O_CREAT = (1 << 3),  /**< @brief Create the file if it does not exist. */
    D_FS_O_EXCL = (1 << 4),   /**< @brief Fail if the file already exists (used with D_FS_O_CREAT). */
    D_FS_O_TRUNC = (1 << 5),  /**< @brief Truncate the file to zero length if it exists. */
    D_FS_O_APPEND = (1 << 6), /**< @brief Move to the end of the file before every write. */
} d_fs_flags_t;

/** @brief Driver-level error codes, decoupled from LittleFS's own error numbering. */
typedef enum {
    D_FS_ERR_OK = 0,            /**< @brief No error. */
    D_FS_ERR_IO = -1,           /**< @brief Error during a flash device operation. */
    D_FS_ERR_CORRUPT = -2,      /**< @brief Filesystem corruption detected. */
    D_FS_ERR_NOENT = -3,        /**< @brief No such file or directory. */
    D_FS_ERR_EXIST = -4,        /**< @brief Entry already exists. */
    D_FS_ERR_NOTDIR = -5,       /**< @brief Entry is not a directory. */
    D_FS_ERR_ISDIR = -6,        /**< @brief Entry is a directory. */
    D_FS_ERR_NOTEMPTY = -7,     /**< @brief Directory is not empty. */
    D_FS_ERR_BADF = -8,         /**< @brief Bad file handle. */
    D_FS_ERR_FBIG = -9,         /**< @brief File too large. */
    D_FS_ERR_INVAL = -10,       /**< @brief Invalid parameter, including NULL pointers. */
    D_FS_ERR_NOSPC = -11,       /**< @brief No space left on the filesystem. */
    D_FS_ERR_NOMEM = -12,       /**< @brief No memory available (e.g. pool exhausted). */
    D_FS_ERR_NOATTR = -13,      /**< @brief No such attribute. */
    D_FS_ERR_NAMETOOLONG = -14, /**< @brief Name exceeds LFS_NAME_MAX. */
} d_fs_err_t;

/**
 * @brief Mount the LittleFS partition, formatting it on first boot if necessary.
 *
 * @details
 * Attempts lfs_mount() and flips FS_MOUNTED flag on success.
 * On failure, calls lfs_format() then retries mount.
 * If the retry also fails, calls k_panic() the filesystem is considered
 * essential and the system cannot proceed without it.
 *
 * @note Requires flash_safe_execute_core_init() to have been called on both
 * cores before this function is invoked (handled by kinit.c).
 *
 * @warning Formatting erases all data in the 3 MB LittleFS partition.
 * This occurs silently on first boot or after filesystem corruption.
 */
void d_flash_fs_init(void);

/**
 * @brief Unmount the LittleFS partition and release filesystem state.
 *
 * @details
 * Calls lfs_unmount() and clears the FS_MOUNTED flag. Safe to call even
 * if the filesystem is already unmounted, returns immediately in that case.
 */
void d_flash_fs_deinit(void);

/**
 * @brief Open or create a file at the given path.
 *
 * @details
 * Allocates a slot from the internal file pool and calls
 * lfs_file_open(). The returned handle is opaque, pass it back to
 * d_flash_fs_close(), d_flash_fs_read(), d_flash_fs_write(), and
 * d_flash_fs_seek() only.
 *
 * @param[in] path  Absolute path to the file (e.g. "/logs/boot.log").
 * @param[in] flags file open mode flags.
 * @return Opaque file handle on success, NULL if no pool slot is available
 * or lfs_file_open() fails.
 *
 * @note At most 12 files may be open simultaneously.
 * @warning the returned handle must never be casted or dereferenced.
 */
void *d_flash_fs_open(const char *path, d_fs_flags_t flags);

/**
 * @brief Close an open file and release its pool slot.
 *
 * @param[in] file Opaque file handle returned by d_flash_fs_open().
 * @return D_FS_ERR_OK on success, D_FS_ERR_INVAL if file is NULL, or another negative d_fs_err_t value on failure.
 *
 * @warning Passing an invalid or already-closed handle is undefined behaviour.
 */
d_fs_err_t d_flash_fs_close(void *file);

/**
 * @brief Read bytes from an open file into a buffer.
 *
 * @param[in]  file   Opaque file handle returned by d_flash_fs_open().
 * @param[out] buffer Destination buffer for the data read.
 * @param[in]  size   Number of bytes to read.
 * @return Number of bytes read on success, D_FS_ERR_INVAL if file or buffer is NULL, or another negative d_fs_err_t value on failure.
 */
int d_flash_fs_read(void *file, void *buffer, size_t size);

/**
 * @brief Write bytes from a buffer into an open file.
 *
 * @param[in] file   Opaque file handle returned by d_flash_fs_open().
 * @param[in] buffer Source buffer containing data to write.
 * @param[in] size   Number of bytes to write.
 * @return Number of bytes written on success, D_FS_ERR_INVAL if file or buffer is NULL, or another negative d_fs_err_t value on failure.
 */
int d_flash_fs_write(void *file, const void *buffer, size_t size);

/* NOLINTNEXTLINE(bugprone-easily-swappable-parameters) - offset and whence
 * follow the standard fseek()-style (file, offset, whence) convention;
 * not wrapped in a struct to preserve that familiar signature. */

/**
 * @brief Reposition the read/write offset within an open file.
 *
 * @param[in] file   Opaque file handle returned by d_flash_fs_open().
 * @param[in] offset Byte offset relative to whence.
 * @param[in] whence Seek origin: set, current position, or end of file.
 * @return New absolute offset from the start of the file on success, D_FS_ERR_INVAL if file is NULL or whence is invalid, or another negative d_fs_err_t value on failure.
 */
int d_flash_fs_seek(void *file, int32_t offset, d_fs_whence_t whence);

/**
 * @brief Remove a file at the given path.
 *
 * @param[in] path Absolute path to the file to remove.
 * @return D_FS_ERR_OK on success, D_FS_ERR_ISDIR if path is a directory, D_FS_ERR_INVAL if path is NULL, or another negative d_fs_err_t value on failure.
 *
 * @warning The file must not be open when this is called.
 */
d_fs_err_t d_flash_fs_remove(const char *path);

/**
 * @brief Retrieve metadata for a file or directory.
 *
 * @param[in]  path Absolute path to the file or directory.
 * @param[out] info Pointer to a d_fs_info_t struct populated on success.
 * @return D_FS_ERR_OK on success, D_FS_ERR_INVAL if path or info is NULL, or another negative d_fs_err_t value on failure.
 */
d_fs_err_t d_flash_fs_stat(const char *path, d_fs_info_t *info);

/**
 * @brief Create a directory at the given path.
 *
 * @param[in] path Absolute path of the directory to create.
 * @return D_FS_ERR_OK on success, D_FS_ERR_INVAL if path is NULL, or another negative d_fs_err_t value on failure.
 */
d_fs_err_t d_flash_fs_mkdir(const char *path);

/**
 * @brief Open a directory for iteration.
 *
 * @param[in] path Absolute path to the directory to open.
 * @return Opaque directory handle on success, NULL on failure.
 *
 * @note At most 12 directories may be open simultaneously, tracked in a pool separate from open files.
 */
void *d_flash_fs_opendir(const char *path);

/**
 * @brief Read the next entry from an open directory.
 *
 * @param[in]  dir  Opaque directory handle returned by d_flash_fs_opendir().
 * @param[out] info Pointer to a d_fs_info_t struct populated with the
 * next entry's name, type, and size.
 *
 * @return 1 if an entry was read, 0 at end of directory, D_FS_ERR_INVAL if dir or info is NULL, or another negative d_fs_err_t value on failure.
 *
 * @warning Reuse a single stack-local d_fs_info_t across a directory
 * listing loop rather than accumulating one per entry each call
 * overwrites it, and name[] alone is LFS_NAME_MAX+1 (256) bytes, so
 * storing an array of entries multiplies that cost per entry.
 */
int d_flash_fs_readdir(void *dir, d_fs_info_t *info);

/**
 * @brief Close an open directory handle and release its pool slot.
 *
 * @param[in] dir Opaque directory handle returned by d_flash_fs_opendir().
 * @return D_FS_ERR_OK on success, D_FS_ERR_INVAL if dir is NULL, or another negative d_fs_err_t value on failure.
 */
d_fs_err_t d_flash_fs_closedir(void *dir);

/**
 * @brief Remove an empty directory at the given path.
 *
 * @param[in] path Absolute path of the directory to remove.
 * @return D_FS_ERR_OK on success, D_FS_ERR_NOTDIR if path is not a directory, D_FS_ERR_INVAL if path is NULL, or another negative d_fs_err_t value on failure.
 *
 * @warning Directory must be empty before removal.
 */
d_fs_err_t d_flash_fs_rmdir(const char *path);

/**
 * @brief Query used and total filesystem space in bytes.
 *
 * @param[out] used  Populated with the number of bytes currently in use.
 * @param[out] total Populated with the total filesystem capacity in bytes.
 * @return 0 on success, negative d_fs_err_t values on failure.
 */
d_fs_err_t d_flash_fs_size(int32_t *used, int32_t *total);

/**
 * @brief Check whether a file or directory exists at the given path.
 *
 * @param[in] path Absolute path to check.
 * @return true if the entry exists, false otherwise.
 */
bool d_flash_fs_exists(const char *path);

#endif
