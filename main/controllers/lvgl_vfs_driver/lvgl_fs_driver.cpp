// main/controllers/lvgl_vfs_driver/lvgl_fs_driver.cpp
#include "lvgl_fs_driver.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "LVGL_FS_DRV";

// --- Custom LVGL VFS Driver Callbacks ---

static void * fs_open_cb(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode)
{
    const char * mode_str;
    if (mode == LV_FS_MODE_WR) {
        mode_str = "wb";
    } else if (mode == LV_FS_MODE_RD) {
        mode_str = "rb";
    } else if (mode == (LV_FS_MODE_WR | LV_FS_MODE_RD)) {
        mode_str = "rb+";
    } else {
        ESP_LOGE(TAG, "Unknown file open mode: %d", mode);
        return NULL;
    }

    // The path from LVGL already includes the full system path, e.g., "/sdcard/file.txt"
    // The drive letter has been stripped by LVGL before calling this.
    FILE * f = fopen(path, mode_str);
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s (mode: %s)", path, mode_str);
        return NULL;
    }

    // Return a pointer to the standard FILE object
    return f;
}

static lv_fs_res_t fs_close_cb(lv_fs_drv_t * drv, void * file_p)
{
    FILE * f = (FILE *)file_p;
    if (f) {
        fclose(f);
    }
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read_cb(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br)
{
    FILE * f = (FILE *)file_p;
    *br = fread(buf, 1, btr, f);
    // feof() and ferror() can be used here to check for specific read results if needed
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_write_cb(lv_fs_drv_t * drv, void * file_p, const void * buf, uint32_t btw, uint32_t * bw)
{
    FILE * f = (FILE *)file_p;
    *bw = fwrite(buf, 1, btw, f);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek_cb(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence)
{
    FILE * f = (FILE *)file_p;
    int seek_mode;
    switch (whence) {
        case LV_FS_SEEK_SET:
            seek_mode = SEEK_SET;
            break;
        case LV_FS_SEEK_CUR:
            seek_mode = SEEK_CUR;
            break;
        case LV_FS_SEEK_END:
            seek_mode = SEEK_END;
            break;
        default:
            return LV_FS_RES_INV_PARAM;
    }

    if (fseek(f, pos, seek_mode) != 0) {
        return LV_FS_RES_FS_ERR;
    }

    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell_cb(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p)
{
    FILE * f = (FILE *)file_p;
    long pos = ftell(f);
    if (pos == -1) {
        return LV_FS_RES_FS_ERR;
    }
    *pos_p = (uint32_t)pos;
    return LV_FS_RES_OK;
}

void lvgl_fs_driver_init(char drive_letter)
{
    static lv_fs_drv_t fs_drv; // Keep it static to ensure it's not deallocated
    lv_fs_drv_init(&fs_drv);

    fs_drv.letter = drive_letter;
    fs_drv.open_cb = fs_open_cb;
    fs_drv.close_cb = fs_close_cb;
    fs_drv.read_cb = fs_read_cb;
    fs_drv.write_cb = fs_write_cb;
    fs_drv.seek_cb = fs_seek_cb;
    fs_drv.tell_cb = fs_tell_cb;

    // Optional: Add directory handling callbacks if you use lv_fs_dir_... functions
    // fs_drv.dir_open_cb = ...;
    // fs_drv.dir_read_cb = ...;
    // fs_drv.dir_close_cb = ...;

    lv_fs_drv_register(&fs_drv);
    ESP_LOGI(TAG, "Custom LVGL filesystem driver registered for drive '%c'", drive_letter);
}