/**
 * @file screen_manager.h
 * @brief Handles all low-level initialization for the display hardware and LVGL.
 *
 * This controller acts as the bridge between the UI software and the physical
 * hardware, configuring the SPI bus, the ST7789 display controller, and the
 * LVGL graphics library.
 */
#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"

/**
 * @brief Contains all handles needed to manage the screen and LVGL.
 */
typedef struct {
    esp_lcd_panel_io_handle_t io_handle;   //!< Handle for panel IO communication (SPI).
    esp_lcd_panel_handle_t panel_handle;   //!< Handle for the LCD panel driver (ST7789).
    lv_display_t *lvgl_disp;               //!< Pointer to the LVGL display object.
    lv_color_t *lvgl_buf1;                 //!< Pointer to the first LVGL draw buffer.
    lv_color_t *lvgl_buf2;                 //!< Pointer to the second LVGL draw buffer.
} screen_t;


/**
 * @brief Initializes display hardware (SPI, LCD) and the LVGL library.
 *
 * Sets up the LVGL tick timer, drawing buffers, and flush callback.
 * After calling this, the main application loop must periodically call `lv_timer_handler()`.
 *
 * @return Pointer to a screen_t struct with necessary handles, or NULL on failure.
 */
screen_t* screen_init();

/**
 * @brief Deinitializes the display, LVGL, and releases all associated resources.
 *
 * @param screen Pointer to the screen_t struct returned by screen_init().
 */
void screen_deinit(screen_t* screen);

#endif