#include "standby_view.h"
#include "views/view_manager.h" 
#include "controllers/button_manager/button_manager.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/power_manager/power_manager.h"
#include "components/status_bar_component/status_bar_component.h"
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

    // The ViewManager will automatically unregister all view-specific button handlers.
    // We only need to clean up resources created by this view that are not
    // children of the view's main container.

    // Delete timers
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
    
    // The shutdown popup is an LVGL object (and its children) parented to the screen.
    // If it exists when the view is destroyed, ViewManager's lv_obj_clean() will
    // delete it. We just need to ensure its non-widget resources (group, styles) are freed.
    if (shutdown_popup_group) {
        lv_group_del(shutdown_popup_group);
        shutdown_popup_group = nullptr;
    }

    // Reset styles to free any allocated memory.
    reset_popup_styles();
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

void StandbyView::setup_popup_button_handlers() {
    button_manager_unregister_view_handlers();
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, StandbyView::popup_ok_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, StandbyView::popup_cancel_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, StandbyView::popup_nav_left_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, StandbyView::popup_nav_right_cb, true, this);
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
    // Stop any active volume timers before sleeping
    stop_volume_repeat_timer(&volume_up_timer);
    stop_volume_repeat_timer(&volume_down_timer);
    power_manager_enter_light_sleep();
    ESP_LOGI(TAG, "Woke up from light sleep. Pausing button input momentarily.");
    button_manager_pause_for_wake_up(200);
}

void StandbyView::on_shutdown_long_press() {
    create_shutdown_popup();
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

// --- Shutdown Popup Logic ---
void StandbyView::create_shutdown_popup() {
    if (shutdown_popup_container) return;
    ESP_LOGI(TAG, "Creating shutdown confirmation popup.");
    init_popup_styles();

    shutdown_popup_container = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(shutdown_popup_container);
    lv_obj_set_size(shutdown_popup_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(shutdown_popup_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(shutdown_popup_container, LV_OPA_70, 0);

    // --- FIX: LVGL v9 Message Box Creation ---
    // 1. Create the base message box object
    lv_obj_t* msgbox = lv_msgbox_create(shutdown_popup_container);

    // 2. Add content to it
    lv_msgbox_add_title(msgbox, "Turn Off Device");
    lv_msgbox_add_text(msgbox, "Reset button will be needed to start the device again.");
    
    // 3. Add footer buttons and capture their pointers
    lv_obj_t* btn_cancel_obj = lv_msgbox_add_footer_button(msgbox, "Cancel");
    lv_obj_t* btn_ok_obj = lv_msgbox_add_footer_button(msgbox, "Turn Off");
    // --- End of FIX ---
    
    lv_obj_center(msgbox);
    lv_obj_set_width(msgbox, 200);

    lv_obj_add_style(btn_cancel_obj, &style_popup_normal, LV_STATE_DEFAULT);
    lv_obj_add_style(btn_cancel_obj, &style_popup_focused, LV_STATE_FOCUSED);
    lv_obj_add_style(btn_ok_obj, &style_popup_normal, LV_STATE_DEFAULT);
    lv_obj_add_style(btn_ok_obj, &style_popup_focused, LV_STATE_FOCUSED);

    shutdown_popup_group = lv_group_create();
    lv_group_add_obj(shutdown_popup_group, btn_cancel_obj);
    lv_group_add_obj(shutdown_popup_group, btn_ok_obj);
    lv_group_focus_obj(btn_cancel_obj);

    setup_popup_button_handlers();
}

void StandbyView::destroy_shutdown_popup() {
    if (!shutdown_popup_container) return;
    ESP_LOGI(TAG, "Destroying shutdown popup and restoring main handlers.");

    // The group must be deleted before its objects
    if (shutdown_popup_group) {
        lv_group_del(shutdown_popup_group);
        shutdown_popup_group = nullptr;
    }
    // Delete the top-level container; its children (msgbox, buttons) will be deleted automatically.
    lv_obj_del(shutdown_popup_container);
    shutdown_popup_container = nullptr;
    
    // Restore the main button handlers for the view
    setup_main_button_handlers();
}

void StandbyView::on_popup_ok() {
    if (!shutdown_popup_group) return;
    lv_obj_t* focused_btn = lv_group_get_focused(shutdown_popup_group);
    if (!focused_btn) return;
    const char* action_text = lv_label_get_text(lv_obj_get_child(focused_btn, 0));
    if (strcmp(action_text, "Turn Off") == 0) {
        ESP_LOGI(TAG, "User confirmed shutdown. Entering deep sleep.");
        power_manager_enter_deep_sleep();
    } else {
        destroy_shutdown_popup();
    }
}
void StandbyView::on_popup_cancel() { destroy_shutdown_popup(); }
void StandbyView::on_popup_nav_left() { if (shutdown_popup_group) lv_group_focus_prev(shutdown_popup_group); }
void StandbyView::on_popup_nav_right() { if (shutdown_popup_group) lv_group_focus_next(shutdown_popup_group); }

void StandbyView::init_popup_styles() {
    if (popup_styles_initialized) return;
    lv_style_init(&style_popup_normal);
    lv_style_set_bg_color(&style_popup_normal, lv_color_white());
    lv_style_set_text_color(&style_popup_normal, lv_color_black());
    lv_style_set_border_width(&style_popup_normal, 1);
    lv_style_init(&style_popup_focused);
    lv_style_set_bg_color(&style_popup_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_color(&style_popup_focused, lv_color_white());
    popup_styles_initialized = true;
}

void StandbyView::reset_popup_styles() {
    if (!popup_styles_initialized) return;
    lv_style_reset(&style_popup_normal);
    lv_style_reset(&style_popup_focused);
    popup_styles_initialized = false;
}

// --- Static Callback Bridges ---
void StandbyView::update_clock_cb(lv_timer_t* timer) {
    void* user_data = lv_timer_get_user_data(timer);
    static_cast<StandbyView*>(user_data)->update_clock();
}
void StandbyView::menu_press_cb(void* user_data) { static_cast<StandbyView*>(user_data)->on_menu_press(); }
void StandbyView::sleep_press_cb(void* user_data) { static_cast<StandbyView*>(user_data)->on_sleep_press(); }
void StandbyView::shutdown_long_press_cb(void* user_data) { static_cast<StandbyView*>(user_data)->on_shutdown_long_press(); }
void StandbyView::volume_up_long_press_start_cb(void* user_data) { static_cast<StandbyView*>(user_data)->on_volume_up_long_press_start(); }
void StandbyView::volume_down_long_press_start_cb(void* user_data) { static_cast<StandbyView*>(user_data)->on_volume_down_long_press_start(); }
void StandbyView::volume_up_press_up_cb(void* user_data) { auto self = static_cast<StandbyView*>(user_data); self->stop_volume_repeat_timer(&self->volume_up_timer); }
void StandbyView::volume_down_press_up_cb(void* user_data) { auto self = static_cast<StandbyView*>(user_data); self->stop_volume_repeat_timer(&self->volume_down_timer); }
void StandbyView::popup_ok_cb(void* user_data) { static_cast<StandbyView*>(user_data)->on_popup_ok(); }
void StandbyView::popup_cancel_cb(void* user_data) { static_cast<StandbyView*>(user_data)->on_popup_cancel(); }
void StandbyView::popup_nav_left_cb(void* user_data) { static_cast<StandbyView*>(user_data)->on_popup_nav_left(); }
void StandbyView::popup_nav_right_cb(void* user_data) { static_cast<StandbyView*>(user_data)->on_popup_nav_right(); }