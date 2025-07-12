#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"

/**
 * @brief Contains all the handles needed to manage the screen hardware and LVGL.
 */
typedef struct {
    esp_lcd_panel_io_handle_t io_handle;       // Handle for the panel IO communication (SPI)
    esp_lcd_panel_handle_t panel_handle;       // Handle for the LCD panel driver (ST7789)
    lv_display_t *lvgl_disp;                   // Pointer to the LVGL display object
    lv_color_t *lvgl_buf1;                     // Pointer to the first LVGL draw buffer
    lv_color_t *lvgl_buf2;                     // Pointer to the second LVGL draw buffer
} screen_t;


/**
 * @brief Initializes the display hardware (SPI, LCD controller) and the LVGL graphics library.
 * This function handles all low-level setup required to get the screen working.
 * @return Pointer to a screen_t struct containing the necessary handles, or NULL on failure. The caller does not own this memory.
 */
screen_t* screen_init();

/**
 * @brief Deinitializes the display, LVGL, and releases all associated resources.
 *
 * @param screen Pointer to the screen_t struct that was returned by screen_init().
 */
void screen_deinit(screen_t* screen);

#endif