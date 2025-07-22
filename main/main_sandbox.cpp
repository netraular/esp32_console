/**
 * @file main.cpp
 * @brief Test Sandbox Entry Point
 *
 * This file serves as a development and testing environment, independent of the
 * main application's ViewManager. It allows for the isolated initialization
 * of controllers and the creation of simple LVGL UIs to test new features.
 *
 * --- HOW TO USE ---
 * 1. Replace the original main.cpp with this file.
 * 2. In the `create_test_ui()` function, build your experimental LVGL interface.
 * 3. In the `register_test_handlers()` function, connect button events to your UI logic.
 * 4. If your test requires specific controllers (e.g., SD card, WiFi), uncomment
 *    their initialization calls in `app_main`.
 * 5. Build and flash to see your test in action.
 * 6. When finished, revert to the original main.cpp.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lvgl.h"

// --- Core Controller Includes (essential for any UI) ---
#include "controllers/screen_manager/screen_manager.h"
#include "controllers/button_manager/button_manager.h"

// --- Optional Controller Includes (uncomment as needed for testing) ---
// #include "controllers/sd_card_manager/sd_card_manager.h"
// #include "controllers/littlefs_manager/littlefs_manager.h"
// #include "controllers/audio_manager/audio_manager.h"
// #include "controllers/audio_recorder/audio_recorder.h"
// #include "controllers/wifi_manager/wifi_manager.h"
// #include "controllers/data_manager/data_manager.h"
// #include "controllers/stt_manager/stt_manager.h"
// #include "controllers/power_manager/power_manager.h"

// --- LVGL Add-ons (uncomment as needed) ---
// #include "controllers/lvgl_vfs_driver/lvgl_fs_driver.h"
// #include "libs/lodepng/lv_lodepng.h"

static const char *TAG = "TEST_MAIN";

// --- Test UI State & Widgets ---
static lv_obj_t* counter_label = nullptr;
static lv_obj_t* info_label = nullptr;
static int click_count = 0;

// --- Forward Declarations for Test Functions ---
void create_test_ui(lv_obj_t* parent);
void register_test_handlers();

// --- Test Button Callback Handlers ---

/**
 * @brief Handles the OK button press. Increments a counter.
 */
static void handle_ok_press(void* user_data) {
    click_count++;
    if (counter_label) {
        lv_label_set_text_fmt(counter_label, "Count: %d", click_count);
    }
    ESP_LOGI(TAG, "OK button pressed. Count is now %d", click_count);
}

/**
 * @brief Handles the Cancel button press. Resets the counter.
 */
static void handle_cancel_press(void* user_data) {
    click_count = 0;
    if (counter_label) {
        lv_label_set_text_fmt(counter_label, "Count: %d", click_count);
    }
    if (info_label) {
        lv_label_set_text(info_label, "Counter Reset!");
        lv_obj_set_style_text_color(info_label, lv_palette_main(LV_PALETTE_RED), 0);
    }
    ESP_LOGI(TAG, "Cancel button pressed. Counter reset.");
}

/**
 * @brief Handles the Right button press. Changes a label's color.
 */
static void handle_right_press(void* user_data) {
    if (info_label) {
        lv_label_set_text(info_label, "Right Press!");
        lv_obj_set_style_text_color(info_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    }
    ESP_LOGI(TAG, "Right button pressed.");
}

/**
 * @brief Handles the Left button press. Changes a label's color.
 */
static void handle_left_press(void* user_data) {
    if (info_label) {
        lv_label_set_text(info_label, "Left Press!");
        lv_obj_set_style_text_color(info_label, lv_palette_main(LV_PALETTE_BLUE), 0);
    }
    ESP_LOGI(TAG, "Left button pressed.");
}


extern "C" void app_main(void) {
    ESP_LOGI(TAG, "--- Starting Application in SANDBOX MODE ---");

    // --- 1. CORE SYSTEM INITIALIZATION ---
    // Initialize NVS (Non-Volatile Storage), required for WiFi and other components.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize TCP/IP stack and default event loop.
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // --- 2. CORE CONTROLLER INITIALIZATION ---
    // Initialize display (hardware and LVGL). This is ESSENTIAL.
    screen_t* screen = screen_init();
    if (!screen) {
        ESP_LOGE(TAG, "Failed to initialize screen, halting.");
        // In a real scenario, you might want to handle this more gracefully.
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    // Initialize buttons. This is ESSENTIAL for any user interaction.
    button_manager_init();
    ESP_LOGI(TAG, "Core controllers (Screen, Buttons) initialized.");


    // --- 3. OPTIONAL CONTROLLER INITIALIZATION (Uncomment what you need) ---
    /*
    ESP_LOGI(TAG, "Initializing optional controllers for test...");
    
    // NVS Data Manager
    // data_manager_init();
    
    // LittleFS (on-board flash storage)
    // if (littlefs_manager_init("storage")) {
    //     ESP_LOGI(TAG, "LittleFS manager initialized.");
    // }

    // SD Card
    // if (sd_manager_init()) {
    //     ESP_LOGI(TAG, "SD Card manager hardware initialized.");
    //     // Mount it if you need to access files immediately
    //     // sd_manager_mount();
    // }

    // LVGL Filesystem Driver (for LVGL to see SD/LittleFS)
    // lvgl_fs_driver_init('S'); // 'S' for SD, 'F' for flash, etc.

    // PNG Image Decoder
    // lv_lodepng_init();

    // Audio
    // audio_manager_init();
    // audio_recorder_init();

    // Networking
    // wifi_manager_init_sta();
    */

    // --- 4. CREATE TEST UI & REGISTER HANDLERS ---
    ESP_LOGI(TAG, "Creating test UI...");
    create_test_ui(lv_screen_active());
    register_test_handlers();
    ESP_LOGI(TAG, "Test UI created and handlers registered.");

    // --- 5. MAIN APPLICATION LOOP ---
    ESP_LOGI(TAG, "Entering main loop.");
    while (true) {
        // This task is required for LVGL to render animations, handle inputs, etc.
        lv_timer_handler();
        // A short delay to yield to other tasks.
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Creates the simple UI for the test sandbox.
 * @param parent The parent LVGL object (usually the screen).
 */
void create_test_ui(lv_obj_t* parent) {
    // Title Label
    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "LVGL Test Sandbox");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Counter Label (we store a pointer to it to update it later)
    counter_label = lv_label_create(parent);
    lv_label_set_text(counter_label, "Count: 0");
    lv_obj_set_style_text_font(counter_label, &lv_font_montserrat_24, 0);
    lv_obj_center(counter_label);

    // Info Label (for showing button actions)
    info_label = lv_label_create(parent);
    lv_label_set_text(info_label, "Press OK to increment");
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info_label, LV_ALIGN_BOTTOM_MID, 0, -20);
}

/**
 * @brief Registers the button handlers for the test UI.
 */
void register_test_handlers() {
    // We register these as "default" handlers (is_view_handler = false)
    // because we are not using the ViewManager system.
    // The last argument `nullptr` is for user_data, which we don't need in this simple example.
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_ok_press,      false, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press,  false, nullptr);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, handle_right_press,   false, nullptr);
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, handle_left_press,    false, nullptr);
}