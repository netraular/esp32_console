/**
 * @file data_manager.h
 * @brief A centralized manager for persistent data storage using NVS.
 *
 * Provides a simple, type-safe API to abstract away the underlying
 * ESP-IDF Non-Volatile Storage (NVS) implementation details.
 */
#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h> // For size_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the data manager. Must be called once after `nvs_flash_init()`.
 */
void data_manager_init(void);

/**
 * @brief Saves a 32-bit unsigned integer value to Non-Volatile Storage (NVS).
 *
 * @param key The null-terminated string key to associate with the value.
 * @param value The uint32_t value to save.
 * @return true on success, false on failure.
 */
bool data_manager_set_u32(const char* key, uint32_t value);

/**
 * @brief Retrieves a 32-bit unsigned integer value from NVS.
 *
 * @param key The null-terminated string key of the value to retrieve.
 * @param value Pointer to a uint32_t variable where the retrieved value will be stored.
 * @return true if key was found and value retrieved, false otherwise (not found or error).
 */
bool data_manager_get_u32(const char* key, uint32_t* value);

/**
 * @brief Saves a string value to Non-Volatile Storage (NVS).
 *
 * @param key The null-terminated string key to associate with the value.
 * @param value The string value to save.
 * @return true on success, false on failure.
 */
bool data_manager_set_str(const char* key, const char* value);

/**
 * @brief Retrieves a string value from NVS.
 *
 * @param key The null-terminated string key of the value to retrieve.
 * @param buffer A character buffer to store the retrieved string.
 * @param buffer_size Pointer to the size of the provided buffer. On success, it will be updated with the length of the string.
 * @return true if key was found and value retrieved, false otherwise.
 */
bool data_manager_get_str(const char* key, char* buffer, size_t* buffer_size);

#ifdef __cplusplus
}
#endif

#endif // DATA_MANAGER_H