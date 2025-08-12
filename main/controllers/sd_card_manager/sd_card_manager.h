/**
 * @file sd_card_manager.h
 * @brief Manages a Micro SD card on a dedicated SPI bus.
 *
 * Handles hardware initialization, FAT filesystem mounting, and provides a
 * comprehensive interface for file and directory operations.
 */
#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <stdbool.h>
#include <stddef.h> // For size_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback function for iterating through files and directories.
 * @param name The name of the file or directory.
 * @param is_dir True if the entry is a directory, false if it is a file.
 * @param user_data A pointer to user data passed to sd_manager_list_files.
 */
typedef void (*file_iterator_cb_t)(const char* name, bool is_dir, void* user_data);

/** @brief Initializes the dedicated SPI bus for the SD card. */
bool sd_manager_init(void);

/** @brief Mounts the SD card FAT filesystem at the default mount point. */
bool sd_manager_mount(void);

/** @brief Unmounts the SD card filesystem. */
void sd_manager_unmount(void);

/** @brief Unmounts the card and deinitializes the SPI bus. */
void sd_manager_deinit(void);

/** @brief Checks if the SD card filesystem is currently mounted. */
bool sd_manager_is_mounted(void);

/**
 * @brief Checks if a file exists on the SD card.
 * @param path The full path to the file.
 * @return true if the file exists, false otherwise.
 */
bool sd_manager_file_exists(const char* path);

/**
 * @brief Checks if the SD card is ready for use, attempting to mount if necessary.
 * @return true if the card is mounted and accessible, false otherwise.
 */
bool sd_manager_check_ready(void);

/**
 * @brief Gets the mount point path string (e.g., "/sdcard").
 */
const char* sd_manager_get_mount_point(void);

/**
 * @brief Lists all files and directories in a given path via a callback.
 * @param path The absolute path to the directory to list.
 * @param cb The callback function to be called for each entry found.
 * @param user_data User data to be passed to the callback function.
 * @return true on success, false if the path could not be opened.
 */
bool sd_manager_list_files(const char* path, file_iterator_cb_t cb, void* user_data);

/**
 * @brief Deletes a file or an empty directory.
 * @param path The full path to the item to delete.
 * @return true on success, false on failure.
 */
bool sd_manager_delete_item(const char* path);

/**
 * @brief Renames or moves a file or directory.
 * @param old_path The current full path of the item.
 * @param new_path The new full path for the item.
 * @return true on success, false on failure.
 */
bool sd_manager_rename_item(const char* old_path, const char* new_path);

/**
 * @brief Creates a new directory.
 * @param path The full path of the directory to create.
 * @return true on success, false on failure.
 */
bool sd_manager_create_directory(const char* path);

/**
 * @brief Creates a new, empty file.
 * @param path The full path of the file to create.
 * @return true on success, false if the file already exists or on error.
 */
bool sd_manager_create_file(const char* path);

/**
 * @brief Reads the entire content of a file into a dynamically allocated buffer.
 *
 * @warning The caller is responsible for freeing the allocated `*buffer` with `free()`.
 *
 * @param path The full path of the file to read.
 * @param buffer A pointer to a char* that will be allocated to store the content.
 * @param size A pointer to a size_t variable where the file size will be stored.
 * @return true on success, false on failure.
 */
bool sd_manager_read_file(const char* path, char** buffer, size_t* size);

/**
 * @brief Writes text content to a file, overwriting it if it exists.
 * @param path Full path of the file.
 * @param content The null-terminated string content to write.
 * @return true on success, false on error.
 */
bool sd_manager_write_file(const char* path, const char* content);

#ifdef __cplusplus
}
#endif

#endif // SD_CARD_MANAGER_H