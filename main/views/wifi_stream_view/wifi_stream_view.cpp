#include "wifi_stream_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/wifi_streamer/wifi_streamer.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "esp_log.h"

static const char *TAG = "WIFI_STREAM_VIEW";

// UI Widgets
static lv_obj_t* status_label;
static lv_obj_t* ip_label;
static lv_obj_t* icon_label;
static lv_timer_t* ui_update_timer;

// Prototypes
static void ui_update_timer_cb(lv_timer_t* timer);
static void cleanup_wifi_stream_view();
static void handle_ok_press(void* user_data);
static void handle_cancel_press(void* user_data);

// Updates the UI based on the state of the streamer and WiFi
static void update_ui() {
    wifi_stream_state_t stream_state = wifi_streamer_get_state();
    bool wifi_connected = wifi_manager_is_connected();

    // Update IP label
    char ip_buf[20] = "IP: ---.---.---.---";
    if (wifi_connected) {
        char temp_ip[16];
        if (wifi_manager_get_ip_address(temp_ip, sizeof(temp_ip))) {
            snprintf(ip_buf, sizeof(ip_buf), "IP: %s", temp_ip);
        }
    }
    lv_label_set_text(ip_label, ip_buf);

    // Update main status and icon
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
                    lv_label_set_text(status_label, "Press OK to start streaming");
                }
                break;
            case WIFI_STREAM_STATE_CONNECTING:
            case WIFI_STREAM_STATE_STREAMING:
                lv_label_set_text(icon_label, LV_SYMBOL_STOP);
                lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_RED), 0);
                break;
            case WIFI_STREAM_STATE_STOPPING:
                lv_label_set_text(icon_label, LV_SYMBOL_SAVE);
                 lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
                break;
            case WIFI_STREAM_STATE_ERROR:
                lv_label_set_text(icon_label, LV_SYMBOL_WARNING);
                lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_RED), 0);
                break;
        }
    }
}

// Timer to refresh the UI
static void ui_update_timer_cb(lv_timer_t* timer) {
    update_ui();
}

// Cleanup function called when exiting the view
static void cleanup_wifi_stream_view() {
    ESP_LOGI(TAG, "Cleaning up WiFi Stream View...");
    if (ui_update_timer) {
        lv_timer_delete(ui_update_timer);
        ui_update_timer = NULL;
    }
    // Stop the streamer if it is running
    if (wifi_streamer_get_state() != WIFI_STREAM_STATE_IDLE) {
        wifi_streamer_stop();
    }
    // De-initialize WiFi
    wifi_manager_deinit_sta();
    ESP_LOGI(TAG, "Cleanup finished.");
}


// Button handlers
static void handle_ok_press(void* user_data) {
    wifi_stream_state_t state = wifi_streamer_get_state();
    
    if (state == WIFI_STREAM_STATE_IDLE || state == WIFI_STREAM_STATE_ERROR) {
        if(wifi_manager_is_connected()) {
            wifi_streamer_start();
        } else {
             ESP_LOGW(TAG, "OK pressed, but WiFi is not connected yet.");
        }
    } else if (state == WIFI_STREAM_STATE_STREAMING || state == WIFI_STREAM_STATE_CONNECTING) {
        wifi_streamer_stop();
    }
    update_ui(); // Update immediately on press
}

static void handle_cancel_press(void* user_data) {
    cleanup_wifi_stream_view();
    view_manager_load_view(VIEW_ID_MENU);
}


// Main function to create the view
void wifi_stream_view_create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating WiFi Stream View. Initializing WiFi...");
    // --- START WIFI ---
    wifi_manager_init_sta();

    // Main container
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Title
    lv_obj_t* title_label = lv_label_create(cont);
    lv_label_set_text(title_label, "WiFi Audio Stream");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);

    // Icon (Play/Stop/etc)
    icon_label = lv_label_create(cont);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_48, 0);

    // IP label
    ip_label = lv_label_create(cont);
    lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_18, 0);

    // Status label
    status_label = lv_label_create(cont);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, 0);

    // Initial state of the UI
    update_ui();

    // Create a timer to periodically update the UI
    ui_update_timer = lv_timer_create(ui_update_timer_cb, 500, NULL);

    // Register button handlers
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_SINGLE_CLICK, handle_ok_press, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_SINGLE_CLICK, handle_cancel_press, true, nullptr);
}