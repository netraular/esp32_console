#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"

// Contains the handles needed to manage the screen and LVGL
typedef struct {
    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_panel_handle_t panel_handle;
    lv_display_t *lvgl_disp;
    lv_color_t *lvgl_buf1;
    lv_color_t *lvgl_buf2;
} screen_t;


/**
 * @brief Initializes the display hardware and LVGL.
 *
 * @return Pointer to the screen_t struct with the necessary handles, or NULL on failure.
 */
screen_t* screen_init();

/**
 * @brief Deinitializes the display and releases resources.
 *
 * @param screen Pointer to the screen_t struct to deinitialize.
 */
void screen_deinit(screen_t* screen);

#endif