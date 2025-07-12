#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <stdbool.h>
#include <stddef.h> // For size_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the dedicated SPI bus for the SD card.
 * @return true on success, false on failure.
 */
bool sd_manager_init(void);

/**
 * @brief Mounts the SD card FAT filesystem.
 * @return true if the card is successfully mounted, false otherwise.
 */
bool sd_manager_mount(void);

/**
 * @brief Unmounts the SD card filesystem.
 */
void sd_manager_unmount(void);

/**
 * @brief Unmounts the card and deinitializes the SPI bus.
 */
void sd_manager_deinit(void);

/**
 * @brief Checks if the SD card filesystem is currently mounted.
 * @return true if mounted, false otherwise.
 */
bool sd_manager_is_mounted(void);

/**
 * @brief Checks if the SD card is ready for use.
 * This will attempt to mount the card if it isn't already. It also performs a basic check to see if the card is responsive.
 * @return true if the card is mounted and accessible, false otherwise.
 */
bool sd_manager_check_ready(void);

/**
 * @brief Gets the mount point path string.
 * @return A const char* to the mount point (e.g., "/sdcard").
 */
const char* sd_manager_get_mount_point(void);

/**
 * @brief Callback function type for iterating through files and directories.
 * @param name The name of the file or directory.
 * @param is_dir True if the entry is a directory, false if it is a file.
 * @param user_data A pointer to user data passed to sd_manager_list_files.
 */
typedef void (*file_iterator_cb_t)(const char* name, bool is_dir, void* user_data);

/**
 * @brief Lists all files and directories in a given path.
 * @param path The absolute path to the directory to list.
 * @param cb The callback function to be called for each entry found.
 * @param user_data User data to be passed to the callback function.
 * @return true on success, false if the path could not be opened.
 */
bool sd_manager_list_files(const char* path, file_iterator_cb_t cb, void* user_data);

/**
 * @brief Deletes a file or an empty directory.
 * @param path The full path to the item to delete.
 * @return true on success, false on failure (e.g., directory not empty).
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
 * @return true on success, false on failure.
 */
bool sd_manager_create_file(const char* path);

/**
 * @brief Reads the entire content of a file into a dynamically allocated buffer.
 * @param path The full path of the file to read.
 * @param buffer A pointer to a char* that will be allocated to store the content. The caller is responsible for freeing this buffer with `free()`.
 * @param size A pointer to a size_t variable where the size of the file will be stored.
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