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
 * - A maximum of 4 files may be open simultaneously.
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

typedef struct lfs_info d_fs_info_t;

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
 * Allocates a slot from the internal 4-entry file pool and calls
 * lfs_file_open(). The returned handle is opaque pass it back to
 * d_flash_fs_close(), d_flash_fs_read(), d_flash_fs_write(), and
 * d_flash_fs_seek() only.
 *
 * @param[in] path  Absolute path to the file (e.g. "/logs/boot.log").
 * @param[in] flags file open mode flags.
 * @return Opaque file handle on success, NULL if no pool slot is available
 * or lfs_file_open() fails.
 *
 * @note At most 4 files may be open simultaneously.
 */
void *d_flash_fs_open(const char *path, int flags);

/**
 * @brief Close an open file and release its pool slot.
 *
 * @param[in] file Opaque file handle returned by d_flash_fs_open().
 * @return 0 on success, negative error code on failure.
 *
 * @warning Passing an invalid or already-closed handle is undefined behaviour.
 */
int d_flash_fs_close(void *file);

/**
 * @brief Read bytes from an open file into a buffer.
 *
 * @param[in]  file   Opaque file handle returned by d_flash_fs_open().
 * @param[out] buffer Destination buffer for the data read.
 * @param[in]  size   Number of bytes to read.
 * @return Number of bytes read on success, negative error code on failure.
 */
int d_flash_fs_read(void *file, void *buffer, size_t size);

/**
 * @brief Write bytes from a buffer into an open file.
 *
 * @param[in] file   Opaque file handle returned by d_flash_fs_open().
 * @param[in] buffer Source buffer containing data to write.
 * @param[in] size   Number of bytes to write.
 * @return Number of bytes written on success, negative error code on failure.
 */
int d_flash_fs_write(void *file, const void *buffer, size_t size);

/**
 * @brief Reposition the read/write offset within an open file.
 *
 * @param[in] file   Opaque file handle returned by d_flash_fs_open().
 * @param[in] offset Byte offset relative to whence.
 * @param[in] whence Seek origin: set, current position, or end of file.
 * @return New absolute offset from the start of the file on success, negative error code on failure.
 */
int d_flash_fs_seek(void *file, int32_t offset, int whence);

/**
 * @brief Remove a file at the given path.
 *
 * @param[in] path Absolute path to the file to remove.
 * @return 0 on success, negative error code on failure.
 *
 * @warning The file must not be open when this is called.
 */
int d_flash_fs_remove(const char *path);

/**
 * @brief Retrieve metadata for a file or directory.
 *
 * @param[in]  path Absolute path to the file or directory.
 * @param[out] info Pointer to a d_fs_info_t struct populated on success.
 * @return 0 on success, negative error code on failure.
 */
int d_flash_fs_stat(const char *path, d_fs_info_t *info);

/**
 * @brief Create a directory at the given path.
 *
 * @param[in] path Absolute path of the directory to create.
 * @return 0 on success, negative error code on failure.
 */
int d_flash_fs_mkdir(const char *path);

/**
 * @brief Open a directory for iteration.
 *
 * @param[in] path Absolute path to the directory to open.
 * @return Opaque directory handle on success, NULL on failure.
 *
 * @note At most 4 directories may be open simultaneously (shared pool).
 */
void *d_flash_fs_opendir(const char *path);

/**
 * @brief Read the next entry from an open directory.
 *
 * @param[in]  dir  Opaque directory handle returned by d_flash_fs_opendir().
 * @param[out] info Pointer to a d_fs_info_t struct populated with the
 * next entry's name, type, and size.
 *
 * @return 1 if an entry was read, 0 at end of directory, negative error code on failure.
 */
int d_flash_fs_readdir(void *dir, d_fs_info_t *info);

/**
 * @brief Close an open directory handle and release its pool slot.
 *
 * @param[in] dir Opaque directory handle returned by d_flash_fs_opendir().
 * @return 0 on success, negative error code on failure.
 */
int d_flash_fs_closedir(void *dir);

/**
 * @brief Remove an empty directory at the given path.
 *
 * @param[in] path Absolute path of the directory to remove.
 * @return 0 on success, negative error code on failure.
 *
 * @warning Directory must be empty before removal.
 */
int d_flash_fs_rmdir(const char *path);

/**
 * @brief Query used and total filesystem space in bytes.
 *
 * @param[out] used  Populated with the number of bytes currently in use.
 * @param[out] total Populated with the total filesystem capacity in bytes.
 * @return 0 on success, negative error code on failure.
 */
int d_flash_fs_size(lfs_ssize_t *used, lfs_ssize_t *total);

/**
 * @brief Check whether a file or directory exists at the given path.
 *
 * @param[in] path Absolute path to check.
 * @return true if the entry exists, false otherwise.
 */
bool d_flash_fs_exists(const char *path);

#endif
