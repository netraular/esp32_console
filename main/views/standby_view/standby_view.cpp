#include "standby_view.h"
#include "views/view_manager.h" 
#include "controllers/button_manager/button_manager.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/power_manager/power_manager.h"
#include "components/status_bar_component/status_bar_component.h"
#include "components/popup_manager/popup_manager.h" // <-- ADDED
#include "esp_log.h"
#include <time.h>
#include <cstring>

static const char *TAG = "STANDBY_VIEW";
#define VOLUME_REPEAT_PERIOD_MS 200

// --- Constructor & Destructor ---
StandbyView::StandbyView() {
    ESP_LOGI(TAG, "StandbyView constructed");
}

StandbyView::~StandbyView() {
    ESP_LOGI(TAG, "StandbyView destructed");

    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = nullptr;
    }
    if (volume_up_timer) {
        lv_timer_del(volume_up_timer);
        volume_up_timer = nullptr;
    }
    if (volume_down_timer) {
        lv_timer_del(volume_down_timer);
        volume_down_timer = nullptr;
    }
}

// --- View Lifecycle ---
void StandbyView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Standby View UI");
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);
    
    setup_ui(container);
    setup_main_button_handlers();
}

// --- UI & Handler Setup ---
void StandbyView::setup_ui(lv_obj_t* parent) {
    status_bar_create(parent);

    loading_label = lv_label_create(parent);
    lv_obj_set_style_text_font(loading_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(loading_label, "Connecting...");
    lv_obj_center(loading_label);

    center_time_label = lv_label_create(parent);
    lv_obj_set_style_text_font(center_time_label, &lv_font_montserrat_48, 0);
    lv_obj_align(center_time_label, LV_ALIGN_CENTER, 0, -20);
    lv_obj_add_flag(center_time_label, LV_OBJ_FLAG_HIDDEN);

    center_date_label = lv_label_create(parent);
    lv_obj_set_style_text_font(center_date_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(center_date_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(center_date_label, LV_ALIGN_CENTER, 0, 25);
    lv_obj_add_flag(center_date_label, LV_OBJ_FLAG_HIDDEN);

    update_timer = lv_timer_create(StandbyView::update_clock_cb, 1000, this);
    update_clock(); // Initial call to set state
}

void StandbyView::setup_main_button_handlers() {
    button_manager_unregister_view_handlers();
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, StandbyView::menu_press_cb, true, this);
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_TAP, StandbyView::sleep_press_cb, true, this);
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_LONG_PRESS_START, StandbyView::shutdown_long_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_LONG_PRESS_START, StandbyView::volume_up_long_press_start_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_LONG_PRESS_START, StandbyView::volume_down_long_press_start_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_PRESS_UP, StandbyView::volume_up_press_up_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_PRESS_UP, StandbyView::volume_down_press_up_cb, true, this);
}


// --- UI Logic ---
void StandbyView::update_clock() {
    if (wifi_manager_is_connected()) {
        if (!is_time_synced) {
            is_time_synced = true;
            lv_obj_add_flag(loading_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(center_time_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(center_date_label, LV_OBJ_FLAG_HIDDEN);
        }
        char time_str[16];
        char date_str[64];
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);
        strftime(date_str, sizeof(date_str), "%A, %d %B", &timeinfo);
        lv_label_set_text(center_time_label, time_str);
        lv_label_set_text(center_date_label, date_str);
    } else {
        if (is_time_synced) {
            is_time_synced = false;
            lv_obj_clear_flag(loading_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(center_time_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(center_date_label, LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(loading_label, "Connecting...");
    }
}

// --- Instance Methods for Button Actions ---
void StandbyView::on_menu_press() {
    ESP_LOGI(TAG, "CANCEL pressed, loading menu.");
    view_manager_load_view(VIEW_ID_MENU);
}

void StandbyView::on_sleep_press() {
    ESP_LOGI(TAG, "ON/OFF TAP detected, entering light sleep.");
    stop_volume_repeat_timer(&volume_up_timer);
    stop_volume_repeat_timer(&volume_down_timer);
    power_manager_enter_light_sleep();
    ESP_LOGI(TAG, "Woke up from light sleep. Pausing button input momentarily.");
    button_manager_pause_for_wake_up(200);
}

void StandbyView::on_shutdown_long_press() {
    ESP_LOGI(TAG, "Showing shutdown confirmation popup.");
    popup_manager_show_confirmation(
        "Turn Off Device", 
        "The device must be reset manually with the RST button.",
        "Turn Off", // Primary button
        "Cancel",   // Secondary button
        StandbyView::shutdown_popup_cb, 
        this
    );
}

void StandbyView::handle_shutdown_result(popup_result_t result) {
    ESP_LOGI(TAG, "Shutdown popup closed with result: %d", result);
    if (result == POPUP_RESULT_PRIMARY) {
        ESP_LOGI(TAG, "User confirmed shutdown. Entering deep sleep.");
        popup_manager_show_loading("Shutting down..."); // Show a final message
        vTaskDelay(pdMS_TO_TICKS(500)); // Give user time to see the message
        power_manager_enter_deep_sleep();
    } else {
        // User cancelled or dismissed. Re-enable the main view's buttons.
        ESP_LOGD(TAG, "Shutdown cancelled, re-registering main button handlers.");
        setup_main_button_handlers();
    }
}


void StandbyView::on_volume_up_long_press_start() {
    audio_manager_volume_up();
    status_bar_update_volume_display();
    if (!volume_up_timer) {
        volume_up_timer = lv_timer_create([](lv_timer_t *t) {
            audio_manager_volume_up();
            status_bar_update_volume_display();
        }, VOLUME_REPEAT_PERIOD_MS, nullptr);
    }
}

void StandbyView::on_volume_down_long_press_start() {
    audio_manager_volume_down();
    status_bar_update_volume_display();
    if (!volume_down_timer) {
        volume_down_timer = lv_timer_create([](lv_timer_t *t) {
            audio_manager_volume_down();
            status_bar_update_volume_display();
        }, VOLUME_REPEAT_PERIOD_MS, nullptr);
    }
}

void StandbyView::stop_volume_repeat_timer(lv_timer_t** timer_ptr) {
    if (timer_ptr && *timer_ptr) {
        lv_timer_del(*timer_ptr);
        *timer_ptr = nullptr;
    }
}

// --- Static Callback Bridges ---
void StandbyView::update_clock_cb(lv_timer_t* timer) {
    static_cast<StandbyView*>(lv_timer_get_user_data(timer))->update_clock();
}
void StandbyView::menu_press_cb(void* user_data) { static_cast<StandbyView*>(user_data)->on_menu_press(); }
void StandbyView::sleep_press_cb(void* user_data) { static_cast<StandbyView*>(user_data)->on_sleep_press(); }
void StandbyView::shutdown_long_press_cb(void* user_data) { static_cast<StandbyView*>(user_data)->on_shutdown_long_press(); }
void StandbyView::volume_up_long_press_start_cb(void* user_data) { static_cast<StandbyView*>(user_data)->on_volume_up_long_press_start(); }
void StandbyView::volume_down_long_press_start_cb(void* user_data) { static_cast<StandbyView*>(user_data)->on_volume_down_long_press_start(); }
void StandbyView::volume_up_press_up_cb(void* user_data) { auto self = static_cast<StandbyView*>(user_data); self->stop_volume_repeat_timer(&self->volume_up_timer); }
void StandbyView::volume_down_press_up_cb(void* user_data) { auto self = static_cast<StandbyView*>(user_data); self->stop_volume_repeat_timer(&self->volume_down_timer); }

void StandbyView::shutdown_popup_cb(popup_result_t result, void* user_data) {
    static_cast<StandbyView*>(user_data)->handle_shutdown_result(result);
}