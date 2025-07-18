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

static const char *TAG = "STANDBY_VIEW";

// --- View Widgets ---
static lv_obj_t *center_time_label = nullptr;
static lv_obj_t *center_date_label = nullptr;
static lv_obj_t *loading_label = nullptr;
static lv_timer_t *update_timer = nullptr;
static bool is_time_synced = false;

// --- Volume Control State ---
#define VOLUME_REPEAT_PERIOD_MS 200
static lv_timer_t *volume_up_timer = nullptr;
static lv_timer_t *volume_down_timer = nullptr;

// --- Shutdown Popup State ---
static lv_obj_t *shutdown_popup_container = nullptr;
static lv_group_t *shutdown_popup_group = nullptr;
static lv_style_t style_popup_focused;
static lv_style_t style_popup_normal;
static bool popup_styles_initialized = false;

// --- Forward Declarations ---
static void destroy_shutdown_popup();
static void register_main_view_handlers();

// --- Clock Update Task ---
static void update_center_clock_task(lv_timer_t *timer) {
    if (!center_time_label || !center_date_label || !loading_label) return;

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

// --- Main Button Handlers ---
static void handle_cancel_press(void *user_data) {
    ESP_LOGI(TAG, "CANCEL pressed, loading menu.");
    view_manager_load_view(VIEW_ID_MENU);
}

static void handle_on_off_tap_for_sleep(void* user_data) {
    ESP_LOGI(TAG, "ON/OFF TAP detected, entering light sleep.");
    // Stop any active volume timers before sleeping to prevent issues on wake-up
    if (volume_up_timer) { lv_timer_del(volume_up_timer); volume_up_timer = nullptr; }
    if (volume_down_timer) { lv_timer_del(volume_down_timer); volume_down_timer = nullptr; }

    // Put the device into light sleep. Execution will block here until wake-up.
    power_manager_enter_light_sleep();

    // After waking up, briefly pause button management.
    // This prevents accidental clicks if the button is held down for a moment after wake-up.
    ESP_LOGI(TAG, "Woke up from light sleep. Pausing button input momentarily.");
    button_manager_pause_for_wake_up(200); // Pause for 200 ms
}

// --- Shutdown Popup Logic ---

static void init_popup_styles() {
    if (popup_styles_initialized) return;

    lv_style_init(&style_popup_normal);
    lv_style_set_bg_color(&style_popup_normal, lv_color_white());
    lv_style_set_text_color(&style_popup_normal, lv_color_black());
    lv_style_set_border_color(&style_popup_normal, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_border_width(&style_popup_normal, 1);

    lv_style_init(&style_popup_focused);
    lv_style_set_bg_color(&style_popup_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_color(&style_popup_focused, lv_color_white());

    popup_styles_initialized = true;
}

static void reset_popup_styles() {
    if (!popup_styles_initialized) return;
    lv_style_reset(&style_popup_normal);
    lv_style_reset(&style_popup_focused);
    popup_styles_initialized = false;
}

static void handle_popup_left_press(void *user_data) {
    if (shutdown_popup_group) lv_group_focus_prev(shutdown_popup_group);
}
static void handle_popup_right_press(void *user_data) {
    if (shutdown_popup_group) lv_group_focus_next(shutdown_popup_group);
}
static void handle_popup_cancel(void *user_data) {
    destroy_shutdown_popup();
}

static void handle_popup_ok(void *user_data) {
    if (!shutdown_popup_group) return;
    lv_obj_t *focused_btn = lv_group_get_focused(shutdown_popup_group);
    if (!focused_btn) return;

    lv_obj_t *label = lv_obj_get_child(focused_btn, 0);
    const char *action_text = lv_label_get_text(label);

    if (strcmp(action_text, "Turn Off") == 0) {
        ESP_LOGI(TAG, "User confirmed shutdown. Entering deep sleep.");
        power_manager_enter_deep_sleep(); // This function does not return
    } else {
        destroy_shutdown_popup();
    }
}

static void create_shutdown_popup() {
    if (shutdown_popup_container) return;
    ESP_LOGI(TAG, "Creating shutdown confirmation popup.");

    init_popup_styles();

    shutdown_popup_container = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(shutdown_popup_container);
    lv_obj_set_size(shutdown_popup_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(shutdown_popup_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(shutdown_popup_container, LV_OPA_70, 0);
    lv_obj_clear_flag(shutdown_popup_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *msgbox = lv_msgbox_create(shutdown_popup_container);
    lv_msgbox_add_title(msgbox, "Turn Off Device");
    lv_msgbox_add_text(msgbox, "Reset button will be needed to start the device again.");
    lv_obj_t* btn_cancel = lv_msgbox_add_footer_button(msgbox, "Cancel");
    lv_obj_t* btn_ok = lv_msgbox_add_footer_button(msgbox, "Turn Off");
    lv_obj_center(msgbox);
    lv_obj_set_width(msgbox, 200);

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
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_popup_ok, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_popup_cancel, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, handle_popup_left_press, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, handle_popup_right_press, true, nullptr);
}

static void destroy_shutdown_popup() {
    if (!shutdown_popup_container) return;
    ESP_LOGI(TAG, "Destroying shutdown popup and restoring main handlers.");

    if (shutdown_popup_group) {
        if (lv_group_get_default() == shutdown_popup_group) lv_group_set_default(NULL);
        lv_group_del(shutdown_popup_group);
        shutdown_popup_group = nullptr;
    }
    lv_obj_del(shutdown_popup_container);
    shutdown_popup_container = nullptr;

    // Restore the main view's button handlers
    register_main_view_handlers();
}

// --- Centralized Handler Registration ---
static void register_main_view_handlers() {
    button_manager_unregister_view_handlers();

    // ON/OFF Button Logic: TAP for sleep, LONG_PRESS for shutdown popup
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_LONG_PRESS_START, [](void*d){ create_shutdown_popup(); }, true, nullptr);
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_TAP, handle_on_off_tap_for_sleep, true, nullptr);

    // Other Buttons
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, nullptr);

    // Volume Control Logic: LONG_PRESS to start repeating, PRESS_UP to stop.
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_LONG_PRESS_START, [](void*d){ audio_manager_volume_up(); status_bar_update_volume_display(); volume_up_timer = lv_timer_create([](lv_timer_t*t){audio_manager_volume_up(); status_bar_update_volume_display();}, VOLUME_REPEAT_PERIOD_MS, NULL);}, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT,  BUTTON_EVENT_LONG_PRESS_START, [](void*d){ audio_manager_volume_down(); status_bar_update_volume_display(); volume_down_timer = lv_timer_create([](lv_timer_t*t){audio_manager_volume_down(); status_bar_update_volume_display();}, VOLUME_REPEAT_PERIOD_MS, NULL);}, true, nullptr);

    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_PRESS_UP, [](void*d){ if(volume_up_timer){lv_timer_del(volume_up_timer); volume_up_timer=nullptr;}}, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT,  BUTTON_EVENT_PRESS_UP, [](void*d){ if(volume_down_timer){lv_timer_del(volume_down_timer); volume_down_timer=nullptr;}}, true, nullptr);
}

// --- Cleanup Callback ---
static void cleanup_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        ESP_LOGI(TAG, "Standby view container is being deleted, cleaning up all resources.");
        
        // Ensure popup is destroyed and its handlers are cleared
        destroy_shutdown_popup();
        
        // Delete any active timers
        if (update_timer) { lv_timer_del(update_timer); update_timer = nullptr; }
        if (volume_up_timer) { lv_timer_del(volume_up_timer); volume_up_timer = nullptr; }
        if (volume_down_timer) { lv_timer_del(volume_down_timer); volume_down_timer = nullptr; }
        
        // Clean up styles
        reset_popup_styles();

        // Nullify pointers to prevent use-after-free
        center_time_label = nullptr;
        center_date_label = nullptr;
        loading_label = nullptr;
        is_time_synced = false;
    }
}

// --- View Creation ---
void standby_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Standby View");
    is_time_synced = false;
    shutdown_popup_container = nullptr;
    shutdown_popup_group = nullptr;
    volume_up_timer = nullptr;
    volume_down_timer = nullptr;

    lv_obj_t *view_container = lv_obj_create(parent);
    lv_obj_remove_style_all(view_container);
    lv_obj_set_size(view_container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(view_container);
    lv_obj_add_event_cb(view_container, cleanup_event_cb, LV_EVENT_DELETE, NULL);

    status_bar_create(view_container);

    loading_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(loading_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(loading_label, "Loading...");
    lv_obj_center(loading_label);

    center_time_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(center_time_label, &lv_font_montserrat_48, 0);
    lv_obj_align(center_time_label, LV_ALIGN_CENTER, 0, -20);
    lv_obj_add_flag(center_time_label, LV_OBJ_FLAG_HIDDEN);

    center_date_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(center_date_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(center_date_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(center_date_label, LV_ALIGN_CENTER, 0, 25);
    lv_obj_add_flag(center_date_label, LV_OBJ_FLAG_HIDDEN);

    update_timer = lv_timer_create(update_center_clock_task, 1000, NULL);
    update_center_clock_task(update_timer);

    register_main_view_handlers();
}