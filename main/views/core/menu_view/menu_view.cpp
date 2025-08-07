#include "menu_view.h"
#include "controllers/button_manager/button_manager.h"
#include "components/status_bar_component/status_bar_component.h"
#include "esp_log.h"

static const char *TAG = "MENU_VIEW";

// --- Static Member Initialization ---
// The list of views available from the menu.
const char *MenuView::view_options[] = {
    "Pet Status", "Test Microphone", "Test Speaker", "Test SD Card",
    "Test Image", "Test LittleFS", "Test Button Events", "WiFi Audio Stream",
    "Pomodoro Clock", "Click Counter", "Voice Notes", "Test Popups",
    "Volume Tester", "Habit Tracker", "Add Notification", "Notification History"
};

// The corresponding view IDs for each option. The order must match view_options.
const view_id_t MenuView::view_ids[] = {
    VIEW_ID_PET_VIEW, VIEW_ID_MIC_TEST, VIEW_ID_SPEAKER_TEST, VIEW_ID_SD_TEST,
    VIEW_ID_IMAGE_TEST, VIEW_ID_LITTLEFS_TEST, VIEW_ID_MULTI_CLICK_TEST, VIEW_ID_WIFI_STREAM_TEST,
    VIEW_ID_POMODORO, VIEW_ID_CLICK_COUNTER_TEST, VIEW_ID_VOICE_NOTE, VIEW_ID_POPUP_TEST,
    VIEW_ID_VOLUME_TESTER, VIEW_ID_HABIT_MANAGER, VIEW_ID_ADD_NOTIFICATION, VIEW_ID_NOTIFICATION_HISTORY
};

// Calculate the number of options at compile time.
const int MenuView::num_options = sizeof(MenuView::view_options) / sizeof(MenuView::view_options[0]);

// ... (rest of the file remains unchanged)
MenuView::MenuView() {
    ESP_LOGI(TAG, "MenuView constructed");
}

MenuView::~MenuView() {
    ESP_LOGI(TAG, "MenuView destructed");
}

void MenuView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Menu View UI");
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

void MenuView::setup_ui(lv_obj_t* parent) {
    status_bar_create(parent);

    main_label = lv_label_create(parent);
    lv_obj_set_style_text_font(main_label, &lv_font_montserrat_24, 0);
    lv_obj_center(main_label);

    selected_view_index = 0; 
    update_menu_label();
}

void MenuView::update_menu_label() {
    lv_label_set_text_fmt(main_label, "< %s >", view_options[selected_view_index]);
}

void MenuView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, MenuView::handle_left_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, MenuView::handle_right_press_cb, true, this);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, MenuView::handle_ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, MenuView::handle_cancel_press_cb, true, this);
}

void MenuView::on_left_press() {
    selected_view_index--;
    if (selected_view_index < 0) {
        selected_view_index = num_options - 1;
    }
    update_menu_label();
}

void MenuView::on_right_press() {
    selected_view_index++;
    if (selected_view_index >= num_options) {
        selected_view_index = 0;
    }
    update_menu_label();
}

void MenuView::on_ok_press() {
    ESP_LOGI(TAG, "OK pressed, loading view: %s", view_options[selected_view_index]);
    view_manager_load_view(view_ids[selected_view_index]);
}

void MenuView::on_cancel_press() {
    ESP_LOGI(TAG, "Cancel pressed, returning to Standby view.");
    view_manager_load_view(VIEW_ID_STANDBY);
}

void MenuView::handle_left_press_cb(void* user_data) {
    static_cast<MenuView*>(user_data)->on_left_press();
}

void MenuView::handle_right_press_cb(void* user_data) {
    static_cast<MenuView*>(user_data)->on_right_press();
}

void MenuView::handle_ok_press_cb(void* user_data) {
    static_cast<MenuView*>(user_data)->on_ok_press();
}

void MenuView::handle_cancel_press_cb(void* user_data) {
    static_cast<MenuView*>(user_data)->on_cancel_press();
}