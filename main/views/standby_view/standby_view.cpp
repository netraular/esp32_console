#include "standby_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/power_manager/power_manager.h"
#include "components/status_bar_component/status_bar_component.h"
#include "esp_log.h"
#include <time.h>
#include <cstring>

// NOTE: We DO NOT need any extra LVGL includes. lvgl.h is sufficient.

static const char *TAG = "StandbyView";
#define VOLUME_REPEAT_PERIOD_MS 200

// Define the volume callback as a static function
static void volume_timer_callback(lv_timer_t* t) {
    // --- FIX: Use the public API function to get user data ---
    auto* direction = static_cast<int*>(lv_timer_get_user_data(t));
    if (*direction > 0) {
        audio_manager_volume_up();
    } else {
        audio_manager_volume_down();
    }
    status_bar_update_volume_display();
}

// --- View Lifecycle ---

void StandbyView::create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Standby View");
    is_time_synced = false;

    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, lv_pct(100), lv_pct(100));
    lv_obj_center(container);
    
    status_bar_create(container);

    loading_label = lv_label_create(container);
    lv_obj_set_style_text_font(loading_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(loading_label, "Loading...");
    lv_obj_center(loading_label);

    center_time_label = lv_label_create(container);
    lv_obj_set_style_text_font(center_time_label, &lv_font_montserrat_48, 0);
    lv_obj_align(center_time_label, LV_ALIGN_CENTER, 0, -20);
    lv_obj_add_flag(center_time_label, LV_OBJ_FLAG_HIDDEN);

    center_date_label = lv_label_create(container);
    lv_obj_set_style_text_font(center_date_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(center_date_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(center_date_label, LV_ALIGN_CENTER, 0, 25);
    lv_obj_add_flag(center_date_label, LV_OBJ_FLAG_HIDDEN);

    update_timer = lv_timer_create([](lv_timer_t* timer) {
        // --- FIX: Use the public API function to get user data ---
        auto* view = static_cast<StandbyView*>(lv_timer_get_user_data(timer));
        if (view) view->update_center_clock_task();
    }, 1000, this);
    update_center_clock_task();

    register_main_view_handlers();
}

StandbyView::~StandbyView() {
    ESP_LOGI(TAG, "Destroying Standby View resources.");
    
    destroy_shutdown_popup();

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
    reset_popup_styles();
}

// --- Private Methods ---

void StandbyView::update_center_clock_task() {
    if (!center_time_label || !center_date_label || !loading_label) return;

    if (wifi_manager_is_connected()) {
        if (!is_time_synced) {
            is_time_synced = true;
            lv_obj_add_flag(loading_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(center_time_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(center_date_label, LV_OBJ_FLAG_HIDDEN);
        }
        char time_str[16], date_str[64];
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

void StandbyView::register_main_view_handlers() {
    button_manager_unregister_view_handlers();

    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_LONG_PRESS_START, [](void* d){ static_cast<StandbyView*>(d)->create_shutdown_popup(); }, true, this);
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_TAP, [](void*d){
        auto* view = static_cast<StandbyView*>(d);
        if (view->volume_up_timer) { lv_timer_del(view->volume_up_timer); view->volume_up_timer = nullptr; }
        if (view->volume_down_timer) { lv_timer_del(view->volume_down_timer); view->volume_down_timer = nullptr; }
        power_manager_enter_light_sleep();
        button_manager_pause_for_wake_up(200);
    }, true, this);

    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, [](void*d){
        ESP_LOGI(TAG, "Cancel pressed. Menu view not implemented yet.");
    }, true, this);
    
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_LONG_PRESS_START, [](void*d){ 
        auto* v = static_cast<StandbyView*>(d); 
        audio_manager_volume_up(); 
        status_bar_update_volume_display(); 
        static int dir=1; 
        v->volume_up_timer = lv_timer_create(volume_timer_callback, VOLUME_REPEAT_PERIOD_MS, &dir);
    }, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_LONG_PRESS_START, [](void*d){ 
        auto* v = static_cast<StandbyView*>(d); 
        audio_manager_volume_down(); 
        status_bar_update_volume_display(); 
        static int dir=-1; 
        v->volume_down_timer = lv_timer_create(volume_timer_callback, VOLUME_REPEAT_PERIOD_MS, &dir);
    }, true, this);

    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_PRESS_UP, [](void*d){ auto* v = static_cast<StandbyView*>(d); if(v->volume_up_timer){lv_timer_del(v->volume_up_timer); v->volume_up_timer=nullptr;}}, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_PRESS_UP, [](void*d){ auto* v = static_cast<StandbyView*>(d); if(v->volume_down_timer){lv_timer_del(v->volume_down_timer); v->volume_down_timer=nullptr;}}, true, this);
}

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

void StandbyView::create_shutdown_popup() {
    if (shutdown_popup_container) return;
    init_popup_styles();

    shutdown_popup_container = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(shutdown_popup_container);
    lv_obj_set_size(shutdown_popup_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(shutdown_popup_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(shutdown_popup_container, LV_OPA_70, 0);

    lv_obj_t *msgbox = lv_msgbox_create(shutdown_popup_container);
    lv_msgbox_add_title(msgbox, "Turn Off Device");
    lv_msgbox_add_text(msgbox, "Reset button will be needed to start the device again.");
    lv_obj_t* btn_cancel = lv_msgbox_add_footer_button(msgbox, "Cancel");
    lv_obj_t* btn_ok = lv_msgbox_add_footer_button(msgbox, "Turn Off");
    lv_obj_center(msgbox);

    lv_obj_add_style(btn_cancel, &style_popup_normal, LV_STATE_DEFAULT);
    lv_obj_add_style(btn_cancel, &style_popup_focused, LV_STATE_FOCUSED);
    lv_obj_add_style(btn_ok, &style_popup_normal, LV_STATE_DEFAULT);
    lv_obj_add_style(btn_ok, &style_popup_focused, LV_STATE_FOCUSED);

    shutdown_popup_group = lv_group_create();
    lv_group_add_obj(shutdown_popup_group, btn_cancel);
    lv_group_add_obj(shutdown_popup_group, btn_ok);
    lv_group_set_default(shutdown_popup_group);
    lv_group_focus_obj(btn_cancel);

    button_manager_unregister_view_handlers();
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, [](void* d){
        auto* view = static_cast<StandbyView*>(d);
        if (view->shutdown_popup_group) {
            lv_obj_t *focused = lv_group_get_focused(view->shutdown_popup_group);
            const char* txt = lv_label_get_text(lv_obj_get_child(focused, 0));
            if (strcmp(txt, "Turn Off") == 0) {
                power_manager_enter_deep_sleep();
            } else {
                view->destroy_shutdown_popup();
            }
        }
    }, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, [](void* d){ static_cast<StandbyView*>(d)->destroy_shutdown_popup(); }, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, [](void* d){ if(static_cast<StandbyView*>(d)->shutdown_popup_group) lv_group_focus_prev(static_cast<StandbyView*>(d)->shutdown_popup_group); }, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, [](void* d){ if(static_cast<StandbyView*>(d)->shutdown_popup_group) lv_group_focus_next(static_cast<StandbyView*>(d)->shutdown_popup_group); }, true, this);
}

void StandbyView::destroy_shutdown_popup() {
    if (!shutdown_popup_container) return;
    
    if (shutdown_popup_group) {
        if (lv_group_get_default() == shutdown_popup_group) lv_group_set_default(NULL);
        lv_group_del(shutdown_popup_group);
        shutdown_popup_group = nullptr;
    }
    lv_obj_del(shutdown_popup_container);
    shutdown_popup_container = nullptr;
    
    register_main_view_handlers();
}