#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool sd_manager_init(void);
bool sd_manager_mount(void);
void sd_manager_unmount(void);
void sd_manager_deinit(void);
bool sd_manager_is_mounted(void);

/**
 * @brief Checks if the SD card is ready for use.
 * This function attempts to mount the card if it isn't already and verifies that it is readable.
 * If the card is mounted but not responsive, it will unmount it to allow for a clean retry.
 * @return true if the card is mounted and readable, false otherwise.
 */
bool sd_manager_check_ready(void);

const char* sd_manager_get_mount_point(void);

typedef void (*file_iterator_cb_t)(const char* name, bool is_dir, void* user_data);

bool sd_manager_list_files(const char* path, file_iterator_cb_t cb, void* user_data);

#ifdef __cplusplus
}
#endif

#endif // SD_CARD_MANAGER_H