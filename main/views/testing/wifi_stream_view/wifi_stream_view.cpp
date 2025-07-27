#include "wifi_stream_view.h"
#include "views/view_manager.h"
#include <string>

static const char *TAG = "WIFI_STREAM_VIEW";

// --- Lifecycle Methods ---

WifiStreamView::WifiStreamView() {
    ESP_LOGI(TAG, "WifiStreamView constructed.");
    // The WiFi manager is now a global resource, initialized in main.
    // We just need to ensure the streamer task is started if needed.
    // However, the user will trigger this with the OK button.
}

WifiStreamView::~WifiStreamView() {
    ESP_LOGI(TAG, "WifiStreamView destructed, cleaning up resources.");
    
    // Stop the UI timer
    if (ui_update_timer) {
        lv_timer_del(ui_update_timer);
        ui_update_timer = nullptr;
    }
    
    // Stop the streamer task if it's running when the view is destroyed
    if (wifi_streamer_get_state() != WIFI_STREAM_STATE_IDLE) {
        wifi_streamer_stop();
    }
    
    // We NO LONGER de-initialize the WiFi manager here. It stays on.
}

void WifiStreamView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating WiFi Stream View UI");
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, lv_pct(100), lv_pct(100));
    
    setup_ui(container);
    setup_button_handlers();
}

// --- UI & Handler Setup ---

void WifiStreamView::setup_ui(lv_obj_t* parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "WiFi Audio Stream");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);

    icon_label = lv_label_create(parent);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_48, 0);

    ip_label = lv_label_create(parent);
    lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_18, 0);

    status_label = lv_label_create(parent);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);

    update_ui();
    ui_update_timer = lv_timer_create(ui_update_timer_cb, 500, this);
}

void WifiStreamView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, WifiStreamView::ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, WifiStreamView::cancel_press_cb, true, this);
}

// --- UI Logic & Instance Methods ---

void WifiStreamView::update_ui() {
    if (!status_label || !ip_label || !icon_label) return;
    
    wifi_stream_state_t stream_state = wifi_streamer_get_state();
    bool wifi_connected = wifi_manager_is_connected();
    char status_buf[128];
    wifi_streamer_get_status_message(status_buf, sizeof(status_buf));
    lv_label_set_text(status_label, status_buf);

    if (wifi_connected) {
        char temp_ip[16];
        if (wifi_manager_get_ip_address(temp_ip, sizeof(temp_ip))) {
            lv_label_set_text_fmt(ip_label, "IP: %s", temp_ip);
        } else {
            lv_label_set_text(ip_label, "IP: Acquiring...");
        }
    } else {
        lv_label_set_text(ip_label, "IP: Disconnected");
    }

    switch (stream_state) {
        case WIFI_STREAM_STATE_IDLE:
            lv_label_set_text(icon_label, LV_SYMBOL_PLAY);
            lv_obj_set_style_text_color(icon_label, lv_color_white(), 0);
            if (!wifi_connected) {
                lv_label_set_text(status_label, "Waiting for WiFi...");
            } else {
                lv_label_set_text(status_label, "Press OK to connect to server");
            }
            break;
        case WIFI_STREAM_STATE_CONNECTING:
            lv_label_set_text(icon_label, LV_SYMBOL_WIFI);
            lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
            break;
        case WIFI_STREAM_STATE_CONNECTED_IDLE:
            lv_label_set_text(icon_label, LV_SYMBOL_OK);
            lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_BLUE), 0);
            break;
        case WIFI_STREAM_STATE_STREAMING:
            lv_label_set_text(icon_label, LV_SYMBOL_AUDIO);
            lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_GREEN), 0);
            break;
        case WIFI_STREAM_STATE_STOPPING:
            lv_label_set_text(icon_label, LV_SYMBOL_STOP);
            lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
            break;
        case WIFI_STREAM_STATE_ERROR:
            lv_label_set_text(icon_label, LV_SYMBOL_WARNING);
            lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_RED), 0);
            break;
    }
}

void WifiStreamView::on_ok_press() {
    wifi_stream_state_t state = wifi_streamer_get_state();
    
    if (state == WIFI_STREAM_STATE_IDLE || state == WIFI_STREAM_STATE_ERROR) {
        if(wifi_manager_is_connected()) {
            ESP_LOGI(TAG, "OK pressed. Starting streamer task.");
            wifi_streamer_start();
        } else {
            ESP_LOGW(TAG, "OK pressed, but WiFi is not connected yet.");
            if (status_label) {
                lv_label_set_text(status_label, "Waiting for WiFi...");
            }
        }
    } else {
        ESP_LOGI(TAG, "OK pressed, but streamer is already active (state: %d). No action taken.", state);
    }
    update_ui();
}

void WifiStreamView::on_cancel_press() {
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Static Callbacks (Bridges) ---
void WifiStreamView::ok_press_cb(void* user_data) {
    static_cast<WifiStreamView*>(user_data)->on_ok_press();
}
void WifiStreamView::cancel_press_cb(void* user_data) {
    static_cast<WifiStreamView*>(user_data)->on_cancel_press();
}
void WifiStreamView::ui_update_timer_cb(lv_timer_t* timer) {
    auto* instance = static_cast<WifiStreamView*>(lv_timer_get_user_data(timer));
    if (instance) instance->update_ui();
}