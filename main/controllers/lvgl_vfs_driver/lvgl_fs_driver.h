// main/controllers/lvgl_vfs_driver/lvgl_fs_driver.h
#ifndef LVGL_FS_DRIVER_H
#define LVGL_FS_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and registers a custom LVGL filesystem driver for the ESP-IDF VFS.
 *
 * This function creates a bridge between LVGL and the underlying ESP-IDF Virtual
 * File System. It allows LVGL to access files on mounted partitions (like SD card or LittleFS)
 * using a drive letter.
 *
 * @param drive_letter The character to use as the drive letter in LVGL paths (e.g., 'S').
 */
void lvgl_fs_driver_init(char drive_letter);

#ifdef __cplusplus
}
#endif

#endif // LVGL_FS_DRIVER_H