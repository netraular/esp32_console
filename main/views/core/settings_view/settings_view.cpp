#include "settings_view.h"
#include "views/view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "components/status_bar_component/status_bar_component.h"
#include "components/popup_manager/popup_manager.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "secret.h" // For WIFI_SSID
#include "esp_log.h"
#include <string>

static const char *TAG = "SETTINGS_VIEW";

// --- Lifecycle Methods ---
SettingsView::SettingsView() {
    ESP_LOGI(TAG, "SettingsView constructed");
}

SettingsView::~SettingsView() {
    if (group) {
        lv_group_del(group);
        group = nullptr;
    }
    reset_styles();
    ESP_LOGI(TAG, "SettingsView destructed");
}

void SettingsView::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);

    // Explicitly set a solid white background for this view's container
    lv_obj_set_style_bg_color(container, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
    
    setup_ui(container);
    setup_button_handlers();
}

// --- Style Management ---
void SettingsView::init_styles() {
    if (styles_initialized) return;
    lv_style_init(&style_focused);
    lv_style_set_bg_color(&style_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_color(&style_focused, lv_color_white());
    styles_initialized = true;
}

void SettingsView::reset_styles() {
    if (!styles_initialized) return;
    lv_style_reset(&style_focused);
    styles_initialized = false;
}

// --- UI & Handler Setup ---
void SettingsView::setup_ui(lv_obj_t* parent) {
    status_bar_create(parent);
    init_styles();

    list = lv_list_create(parent);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100) - 20); // Account for status bar
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);

    group = lv_group_create();
    lv_group_set_wrap(group, true);

    populate_settings_list();
}

void SettingsView::populate_settings_list() {
    // --- General Section ---
    lv_list_add_text(list, "General");

    add_setting_item("WiFi SSID", WIFI_SSID);

    char ip_buffer[16] = "Not Connected";
    wifi_manager_get_ip_address(ip_buffer, sizeof(ip_buffer));
    add_setting_item("IP Address", ip_buffer);

    // --- User Profile Section ---
    lv_list_add_text(list, "User Profile");
    add_setting_item("Bedtime", "22:00");
    add_setting_item("Birthday", "January 1st");
    add_setting_item("Age", "30");
}

void SettingsView::add_setting_item(const char* name, const std::string& value) {
    lv_obj_t* btn = lv_list_add_button(list, NULL, name);
    lv_obj_add_style(btn, &style_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(group, btn);

    // Capture the first item added to the list for special scroll handling.
    if (!first_setting_item) {
        first_setting_item = btn;
    }

    // This makes the button non-clickable for the user.
    // It's a list item, not a real button. The OK press is handled by the view.
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* value_label = lv_label_create(btn);
    lv_label_set_text(value_label, value.c_str());
    lv_obj_set_style_text_color(value_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(value_label, LV_ALIGN_RIGHT_MID, -10, 0);
}


void SettingsView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, cancel_press_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, left_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, right_press_cb, true, this);
}

// --- Instance Methods for Actions ---
void SettingsView::on_cancel_press() {
    view_manager_load_view(VIEW_ID_STANDBY);
}

void SettingsView::on_nav_press(bool is_next) {
    if (!group) return;

    if (is_next) {
        lv_group_focus_next(group);
    } else {
        lv_group_focus_prev(group);
    }
    
    lv_obj_t* focused_obj = lv_group_get_focused(group);
    if (focused_obj) {
        // If the focused item is the very first one, scroll the whole list to the top.
        // Otherwise, just scroll to bring the focused item into view.
        if (focused_obj == first_setting_item) {
            lv_obj_scroll_to(list, 0, 0, LV_ANIM_ON);
        } else {
            lv_obj_scroll_to_view(focused_obj, LV_ANIM_ON);
        }
    }
}

// --- Static Callbacks ---
void SettingsView::cancel_press_cb(void* user_data) {
    static_cast<SettingsView*>(user_data)->on_cancel_press();
}

void SettingsView::left_press_cb(void* user_data) {
    static_cast<SettingsView*>(user_data)->on_nav_press(false);
}

void SettingsView::right_press_cb(void* user_data) {
    static_cast<SettingsView*>(user_data)->on_nav_press(true);
}