#include "wifi_stream_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/wifi_streamer/wifi_streamer.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "esp_log.h"
#include <string>

static const char *TAG = "WIFI_STREAM_VIEW";

// --- UI Widgets ---
static lv_obj_t* status_label = nullptr;
static lv_obj_t* ip_label = nullptr;
static lv_obj_t* icon_label = nullptr;
static lv_timer_t* ui_update_timer = nullptr;

// --- Prototypes ---
static void update_ui();
static void handle_ok_press(void* user_data);
static void handle_cancel_press(void* user_data);
static void wifi_stream_view_delete_cb(lv_event_t* e);

/**
 * @brief Event callback triggered when the view's main container is deleted.
 *
 * This is the centralized cleanup function. It ensures that all resources
 * (timers, background tasks, network connections) are properly released,
 * regardless of how the view is exited.
 * @param e The LVGL event data.
 */
static void wifi_stream_view_delete_cb(lv_event_t* e) {
    ESP_LOGI(TAG, "WiFi Stream View is being deleted. Cleaning up resources...");
    
    // 1. Stop and delete the LVGL timer
    if (ui_update_timer) {
        lv_timer_delete(ui_update_timer);
        ui_update_timer = nullptr;
    }

    // 2. Stop the audio streamer task if it's running
    if (wifi_streamer_get_state() != WIFI_STREAM_STATE_IDLE) {
        wifi_streamer_stop();
    }
    
    // 3. De-initialize the WiFi manager to disconnect and free resources
    wifi_manager_deinit_sta();

    // 4. Nullify static pointers to prevent use-after-free issues
    status_label = nullptr;
    ip_label = nullptr;
    icon_label = nullptr;

    ESP_LOGI(TAG, "WiFi Stream View cleanup finished.");
}


/**
 * @brief Updates all UI elements based on the current state of WiFi and the streamer.
 */
static void update_ui() {
    // If the labels have been deleted, do nothing.
    if (!status_label || !ip_label || !icon_label) {
        return;
    }
    
    wifi_stream_state_t stream_state = wifi_streamer_get_state();
    bool wifi_connected = wifi_manager_is_connected();

    // Update IP Address Label
    std::string ip_str = "IP: Connecting...";
    if (wifi_connected) {
        char temp_ip[16];
        if (wifi_manager_get_ip_address(temp_ip, sizeof(temp_ip))) {
            ip_str = "IP: " + std::string(temp_ip);
        } else {
            ip_str = "IP: Acquiring...";
        }
    }
    lv_label_set_text(ip_label, ip_str.c_str());

    // Update Main Status Label and Icon
    char status_buf[128];
    if (!wifi_connected && stream_state < WIFI_STREAM_STATE_STREAMING) {
        lv_label_set_text(status_label, "Connecting to WiFi...");
        lv_label_set_text(icon_label, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_GREY), 0);
    } else {
        wifi_streamer_get_status_message(status_buf, sizeof(status_buf));
        lv_label_set_text(status_label, status_buf);

        switch (stream_state) {
            case WIFI_STREAM_STATE_IDLE:
                lv_label_set_text(icon_label, LV_SYMBOL_PLAY);
                lv_obj_set_style_text_color(icon_label, lv_color_white(), 0);
                if (wifi_connected) {
                    lv_label_set_text(status_label, "Press OK to stream audio");
                }
                break;
            case WIFI_STREAM_STATE_CONNECTING:
            case WIFI_STREAM_STATE_STREAMING:
                lv_label_set_text(icon_label, LV_SYMBOL_STOP);
                lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_RED), 0);
                break;
            case WIFI_STREAM_STATE_STOPPING:
                lv_label_set_text(icon_label, LV_SYMBOL_SAVE); // Using "save" as a stand-in for stopping
                lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
                break;
            case WIFI_STREAM_STATE_ERROR:
                lv_label_set_text(icon_label, LV_SYMBOL_WARNING);
                lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_RED), 0);
                break;
        }
    }
}

/**
 * @brief LVGL timer callback to periodically refresh the UI.
 * @param timer Pointer to the timer.
 */
static void ui_update_timer_cb(lv_timer_t* timer) {
    update_ui();
}

/**
 * @brief Handles the OK button press to start or stop streaming.
 */
static void handle_ok_press(void* user_data) {
    wifi_stream_state_t state = wifi_streamer_get_state();
    
    if (state == WIFI_STREAM_STATE_IDLE || state == WIFI_STREAM_STATE_ERROR) {
        if(wifi_manager_is_connected()) {
            ESP_LOGI(TAG, "OK pressed. Starting stream.");
            wifi_streamer_start();
        } else {
            ESP_LOGW(TAG, "OK pressed, but WiFi is not connected yet.");
        }
    } else if (state == WIFI_STREAM_STATE_STREAMING || state == WIFI_STREAM_STATE_CONNECTING) {
        ESP_LOGI(TAG, "OK pressed. Stopping stream.");
        wifi_streamer_stop();
    }
    update_ui(); // Update immediately on press for instant feedback
}

/**
 * @brief Handles the Cancel button press to return to the main menu.
 */
static void handle_cancel_press(void* user_data) {
    // We no longer call the cleanup function here directly.
    // The view_manager will delete the view's objects, which triggers
    // the LV_EVENT_DELETE and runs our cleanup callback automatically.
    view_manager_load_view(VIEW_ID_MENU);
}


/**
 * @brief Main function to create the view's UI and initialize its logic.
 */
void wifi_stream_view_create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating WiFi Stream View. Initializing WiFi...");
    
    // --- Initialize WiFi for this view ---
    wifi_manager_init_sta();

    // Create a main container for this view
    lv_obj_t* view_container = lv_obj_create(parent);
    lv_obj_remove_style_all(view_container);
    lv_obj_set_size(view_container, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(view_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(view_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // *** Attach the cleanup callback to the container's delete event ***
    lv_obj_add_event_cb(view_container, wifi_stream_view_delete_cb, LV_EVENT_DELETE, NULL);

    // --- Create UI Elements as children of the container ---

    // Title Label
    lv_obj_t* title_label = lv_label_create(view_container);
    lv_label_set_text(title_label, "WiFi Audio Stream");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);

    // Icon (Play/Stop/etc.)
    icon_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_48, 0);

    // IP Address Label
    ip_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_18, 0);

    // Status Label
    status_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);

    // Set the initial state of the UI
    update_ui();

    // Create a timer to periodically update the UI
    ui_update_timer = lv_timer_create(ui_update_timer_cb, 500, NULL);

    // Register button handlers for this view
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_ok_press, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, nullptr);
}