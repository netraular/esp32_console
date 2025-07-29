#include "standby_view.h"
#include "views/view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "controllers/power_manager/power_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "components/status_bar_component/status_bar_component.h"
#include "esp_log.h"
#include <time.h>
#include <sys/stat.h>
#include <string>

static const char *TAG = "STANDBY_VIEW";
const char* NOTIFICATION_SOUND_PATH = "/sdcard/sounds/notification.wav";

// Initialize static member
StandbyView* StandbyView::s_instance = nullptr;

// --- Lifecycle Methods ---
StandbyView::StandbyView() {
    s_instance = this;
    is_time_synced = false;
    update_timer = nullptr;
    volume_up_timer = nullptr;
    volume_down_timer = nullptr;
    ESP_LOGI(TAG, "StandbyView constructed");
}

StandbyView::~StandbyView() {
    if (update_timer) {
        lv_timer_delete(update_timer);
        update_timer = nullptr;
    }
    stop_volume_repeat_timer(&volume_up_timer);
    stop_volume_repeat_timer(&volume_down_timer);
    s_instance = nullptr;
    ESP_LOGI(TAG, "StandbyView destructed");
}

void StandbyView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Standby View");
    container = parent;
    lv_obj_add_event_cb(container, screen_delete_event_cb, LV_EVENT_DELETE, this);
    setup_ui(parent);
    setup_main_button_handlers();
}

// --- UI & Handler Setup ---
void StandbyView::setup_ui(lv_obj_t* parent) {
    status_bar_create(parent);

    // Create a central container to ensure clock and date are always centered together
    lv_obj_t* center_container = lv_obj_create(parent);
    lv_obj_remove_style_all(center_container);
    lv_obj_set_size(center_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(center_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(center_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(center_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(center_container, 10, 0);
    lv_obj_center(center_container);
    
    // Time and Date labels are now children of the center_container
    center_time_label = lv_label_create(center_container);
    lv_obj_set_style_text_font(center_time_label, &lv_font_montserrat_48, 0);

    center_date_label = lv_label_create(center_container);
    lv_obj_set_style_text_font(center_date_label, &lv_font_montserrat_22, 0);
    
    // The "syncing" label remains at the bottom of the screen
    loading_label = lv_label_create(parent);
    lv_obj_align(loading_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_label_set_text(loading_label, "Syncing time...");

    update_clock(); // Initial update

    update_timer = lv_timer_create(update_clock_cb, 500, this);
}

void StandbyView::setup_main_button_handlers() {
    button_manager_unregister_view_handlers();
    // OK button to go to the menu
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, menu_press_cb, true, this);
    // CANCEL button to go to settings
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, settings_press_cb, true, this);
    
    // ON/OFF button for sleep (tap) and shutdown (long press)
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_TAP, sleep_press_cb, true, this);
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_LONG_PRESS_START, shutdown_long_press_cb, true, this);
    
    // RIGHT button long press for volume up
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_LONG_PRESS_START, volume_up_long_press_start_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_PRESS_UP, volume_up_press_up_cb, true, this);
    
    // LEFT button long press for volume down
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_LONG_PRESS_START, volume_down_long_press_start_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_PRESS_UP, volume_down_press_up_cb, true, this);
}

// --- UI Logic ---
void StandbyView::update_clock() {
    time_t now = time(NULL);
    // Check for a valid time (after year 2023)
    if (now > 1672531200) {
        if (!is_time_synced) {
            is_time_synced = true;
            lv_obj_add_flag(loading_label, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "Time has been synchronized.");
        }
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        lv_label_set_text_fmt(center_time_label, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        lv_label_set_text_fmt(center_date_label, "%s, %02d %s", 
            (const char*[]){"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}[timeinfo.tm_wday],
            timeinfo.tm_mday,
            (const char*[]){"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"}[timeinfo.tm_mon]
        );
    } else {
        lv_label_set_text(center_time_label, "--:--");
        lv_label_set_text(center_date_label, "No Time Sync");
    }
}

// --- Instance Methods for Actions ---
void StandbyView::on_menu_press() {
    view_manager_load_view(VIEW_ID_MENU);
}

void StandbyView::on_settings_press() {
    view_manager_load_view(VIEW_ID_SETTINGS);
}

void StandbyView::on_sleep_press() {
    power_manager_enter_light_sleep();
}

void StandbyView::on_shutdown_long_press() {
    popup_manager_show_confirmation(
        "Shutdown", "Turn off the device?", "Shutdown", "Cancel",
        shutdown_popup_cb, this
    );
}

void StandbyView::handle_shutdown_result(popup_result_t result) {
    if (result == POPUP_RESULT_PRIMARY) {
        power_manager_enter_deep_sleep();
    } else {
        // User cancelled, re-register handlers
        setup_main_button_handlers();
    }
}

void StandbyView::on_volume_up_long_press_start() {
    stop_volume_repeat_timer(&volume_up_timer);
    audio_manager_volume_up();
    status_bar_update_volume_display();
    volume_up_timer = lv_timer_create([](lv_timer_t* timer){
        audio_manager_volume_up();
        status_bar_update_volume_display();
    }, 150, nullptr);
}

void StandbyView::on_volume_down_long_press_start() {
    stop_volume_repeat_timer(&volume_down_timer);
    audio_manager_volume_down();
    status_bar_update_volume_display();
    volume_down_timer = lv_timer_create([](lv_timer_t* timer){
        audio_manager_volume_down();
        status_bar_update_volume_display();
    }, 150, nullptr);
}

void StandbyView::stop_volume_repeat_timer(lv_timer_t** timer_ptr) {
    if (timer_ptr && *timer_ptr) {
        lv_timer_delete(*timer_ptr);
        *timer_ptr = nullptr;
    }
}

// --- Public Static Method for Notifications ---
void StandbyView::show_notification_popup(const Notification& notif) {
    if (!s_instance) {
        ESP_LOGE(TAG, "Cannot show notification popup, StandbyView instance is null.");
        return;
    }

    ESP_LOGI(TAG, "Showing notification popup: %s", notif.title.c_str());

    // Play a sound if the file exists on the SD card
    if (sd_manager_is_mounted()) {
        struct stat st;
        if (stat(NOTIFICATION_SOUND_PATH, &st) == 0) {
            audio_manager_play(NOTIFICATION_SOUND_PATH);
        } else {
            ESP_LOGW(TAG, "Notification sound file not found at %s", NOTIFICATION_SOUND_PATH);
        }
    } else {
        ESP_LOGW(TAG, "SD card not mounted, cannot play notification sound.");
    }
    
    // Show the popup
    popup_manager_show_alert(
        notif.title.c_str(),
        notif.message.c_str(),
        notification_popup_closed_cb,
        s_instance
    );
}

// --- Static Callbacks (Bridge to C-style APIs and instance methods) ---
void StandbyView::screen_delete_event_cb(lv_event_t* e) {
    // The destructor will be called by ViewManager's unique_ptr,
    // which will handle timer deletion. We just ensure we don't use the instance after this.
    auto* instance = static_cast<StandbyView*>(lv_event_get_user_data(e));
    if (instance) {
        ESP_LOGD(TAG, "StandbyView screen is being deleted.");
    }
}

void StandbyView::update_clock_cb(lv_timer_t* timer) {
    auto* instance = static_cast<StandbyView*>(lv_timer_get_user_data(timer));
    if (instance) {
        instance->update_clock();
    }
}

void StandbyView::menu_press_cb(void* user_data) {
    static_cast<StandbyView*>(user_data)->on_menu_press();
}

void StandbyView::settings_press_cb(void* user_data) {
    static_cast<StandbyView*>(user_data)->on_settings_press();
}

void StandbyView::sleep_press_cb(void* user_data) {
    static_cast<StandbyView*>(user_data)->on_sleep_press();
}

void StandbyView::shutdown_long_press_cb(void* user_data) {
    static_cast<StandbyView*>(user_data)->on_shutdown_long_press();
}

void StandbyView::volume_up_long_press_start_cb(void* user_data) {
    static_cast<StandbyView*>(user_data)->on_volume_up_long_press_start();
}

void StandbyView::volume_down_long_press_start_cb(void* user_data) {
    static_cast<StandbyView*>(user_data)->on_volume_down_long_press_start();
}

void StandbyView::volume_up_press_up_cb(void* user_data) {
    auto* instance = static_cast<StandbyView*>(user_data);
    if(instance) {
        instance->stop_volume_repeat_timer(&instance->volume_up_timer);
    }
}

void StandbyView::volume_down_press_up_cb(void* user_data) {
    auto* instance = static_cast<StandbyView*>(user_data);
    if(instance) {
        instance->stop_volume_repeat_timer(&instance->volume_down_timer);
    }
}

void StandbyView::shutdown_popup_cb(popup_result_t result, void* user_data) {
    static_cast<StandbyView*>(user_data)->handle_shutdown_result(result);
}

void StandbyView::notification_popup_closed_cb(popup_result_t result, void* user_data) {
    auto* instance = static_cast<StandbyView*>(user_data);
    if (instance) {
        ESP_LOGD(TAG, "Notification popup closed, re-registering standby view handlers.");
        instance->setup_main_button_handlers();
    }
}