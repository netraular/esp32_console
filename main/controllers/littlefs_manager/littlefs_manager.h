/**
 * @file littlefs_manager.h
 * @brief Manages the LittleFS filesystem on the internal flash partition.
 *
 * Handles mounting, formatting, and provides a simple interface for file operations.
 */
#ifndef LITTLEFS_MANAGER_H
#define LITTLEFS_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and mounts the LittleFS partition.
 * 
 * It will format the partition if it's not mounted correctly on the first try.
 * 
 * @param partition_label The label of the partition to use, as defined in partitions.csv.
 * @return true on successful mount, false on failure.
 */
bool littlefs_manager_init(const char* partition_label);

/**
 * @brief Deinitializes the LittleFS manager and unmounts the filesystem.
 */
void littlefs_manager_deinit(void);

/**
 * @brief Gets the mount point path string (e.g., "/fs").
 * @return The mount point path.
 */
const char* littlefs_manager_get_mount_point(void);

/**
 * @brief Checks if a directory exists, and creates it if it does not.
 * 
 * @param relative_path The path of the directory relative to the mount point (e.g., "my_dir").
 * @return true if the directory exists or was created successfully, false on error.
 */
bool littlefs_manager_ensure_dir_exists(const char* relative_path);

/**
 * @brief Checks if a file exists in the LittleFS filesystem.
 * @param filename The name of the file (without the mount point).
 * @return true if the file exists, false otherwise.
 */
bool littlefs_manager_file_exists(const char* filename);

/**
 * @brief Reads the entire content of a file into a dynamically allocated buffer.
 *
 * @warning The caller is responsible for freeing the allocated `*buffer` with `free()`.
 *
 * @param filename The name of the file to read (without the mount point).
 * @param buffer A pointer to a char* that will be allocated to store the content.
 * @param size A pointer to a size_t variable where the file size will be stored.
 * @return true on success, false on failure.
 */
bool littlefs_manager_read_file(const char* filename, char** buffer, size_t* size);

/**
 * @brief Writes text content to a file, overwriting it if it exists.
 * @param filename The name of the file (without the mount point).
 * @param content The null-terminated string content to write.
 * @return true on success, false on error.
 */
bool littlefs_manager_write_file(const char* filename, const char* content);


#ifdef __cplusplus
}
#endif

#endif // LITTLEFS_MANAGER_H