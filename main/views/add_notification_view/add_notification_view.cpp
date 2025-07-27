#include "add_notification_view.h"
#include "views/view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/notification_manager/notification_manager.h"
#include "components/status_bar_component/status_bar_component.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring> // <-- ADDED: This fixes the 'strlen' error

static const char *TAG = "ADD_NOTIF_VIEW";

// --- Constructor & Destructor ---
AddNotificationView::AddNotificationView() {
    ESP_LOGI(TAG, "AddNotificationView constructed");
}

AddNotificationView::~AddNotificationView() {
    if (input_group) {
        // LVGL groups are not objects, so we don't need to check for lv_obj_is_valid.
        // The group is automatically invalidated if the screen is cleared,
        // but it's good practice to delete it if we created it.
        lv_group_del(input_group);
        input_group = nullptr;
    }
    ESP_LOGI(TAG, "AddNotificationView destructed");
}

// --- View Lifecycle ---
void AddNotificationView::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);

    setup_ui(container);
    setup_button_handlers();
}

// --- UI & Handler Setup ---
void AddNotificationView::setup_ui(lv_obj_t* parent) {
    status_bar_create(parent);
    
    // Create a container for the form elements
    lv_obj_t* form_cont = lv_obj_create(parent);
    lv_obj_set_size(form_cont, LV_PCT(95), LV_PCT(80));
    lv_obj_align(form_cont, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_flex_flow(form_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(form_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(form_cont, 10, 0);

    // Title Text Area
    lv_obj_t* title_label = lv_label_create(form_cont);
    lv_label_set_text(title_label, "Title");
    title_textarea = lv_textarea_create(form_cont);
    lv_textarea_set_one_line(title_textarea, true);
    lv_obj_set_width(title_textarea, LV_PCT(100));
    lv_textarea_set_placeholder_text(title_textarea, "Notification Title");

    // Message Text Area
    lv_obj_t* msg_label = lv_label_create(form_cont);
    lv_label_set_text(msg_label, "Message");
    message_textarea = lv_textarea_create(form_cont);
    lv_obj_set_width(message_textarea, LV_PCT(100));
    lv_textarea_set_placeholder_text(message_textarea, "Your message here...");

    // Buttons container
    lv_obj_t* btn_cont = lv_obj_create(form_cont);
    lv_obj_remove_style_all(btn_cont);
    lv_obj_set_width(btn_cont, LV_PCT(100));
    lv_obj_set_height(btn_cont, LV_SIZE_CONTENT);
    lv_obj_set_layout(btn_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Save Button
    save_button = lv_button_create(btn_cont);
    lv_obj_t* save_label = lv_label_create(save_button);
    lv_label_set_text(save_label, "Save Now");
    lv_obj_add_event_cb(save_button, save_event_cb, LV_EVENT_CLICKED, this);

    // Save Later Button
    save_later_button = lv_button_create(btn_cont);
    lv_obj_t* save_later_label = lv_label_create(save_later_button);
    lv_label_set_text(save_later_label, "Save (10s)");
    lv_obj_add_event_cb(save_later_button, save_later_event_cb, LV_EVENT_CLICKED, this);

    // Create a group to manage focus between the input elements
    input_group = lv_group_create();
    lv_group_add_obj(input_group, title_textarea);
    lv_group_add_obj(input_group, message_textarea);
    lv_group_add_obj(input_group, save_button);
    lv_group_add_obj(input_group, save_later_button);
    lv_group_set_wrap(input_group, true); // Allow focus to wrap around
}

void AddNotificationView::setup_button_handlers() {
    button_manager_unregister_view_handlers();
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, cancel_press_cb, true, this);
    // Use Left/Right for navigation between form elements
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, left_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, right_press_cb, true, this);
}

// --- UI Logic ---

void AddNotificationView::save_notification(bool is_delayed) {
    const char* title = lv_textarea_get_text(title_textarea);
    const char* message = lv_textarea_get_text(message_textarea);

    if (strlen(title) == 0 || strlen(message) == 0) {
        // Optional: Show a popup to the user indicating fields are empty
        ESP_LOGW(TAG, "Title or message is empty. Not saving.");
        return;
    }
    
    if (is_delayed) {
        ESP_LOGI(TAG, "Saving notification with 10s delay.");
        // We use a simple FreeRTOS task to handle the delay.
        // This is a bit heavy for a simple delay, a timer would also work,
        // but this demonstrates a pattern that could be used for more complex background tasks.
        auto task_func = [](void* params) {
            auto* notif_data = static_cast<std::pair<std::string, std::string>*>(params);
            vTaskDelay(pdMS_TO_TICKS(10000));
            NotificationManager::add_notification(notif_data->first, notif_data->second);
            delete notif_data; // Clean up the allocated data
            vTaskDelete(NULL); // Delete the task
        };
        
        // We must copy the strings to the heap so they are valid when the task runs.
        auto* data = new std::pair<std::string, std::string>(title, message);
        xTaskCreate(task_func, "notif_delay_task", 2048, data, 5, NULL);

    } else {
        ESP_LOGI(TAG, "Saving notification now.");
        NotificationManager::add_notification(title, message);
    }
    
    // After saving, go back to the menu
    view_manager_load_view(VIEW_ID_MENU);
}

void AddNotificationView::navigate_focus(bool forward) {
    if (!input_group) return;
    if (forward) {
        lv_group_focus_next(input_group);
    } else {
        lv_group_focus_prev(input_group);
    }
}

// --- Instance Methods for Button Actions ---
void AddNotificationView::on_ok_press() {
    if (!input_group) return;
    lv_obj_t* focused_obj = lv_group_get_focused(input_group);
    if (focused_obj) {
        // Send a click event to the focused object (e.g., a button)
        lv_obj_send_event(focused_obj, LV_EVENT_CLICKED, nullptr);
    }
}

void AddNotificationView::on_cancel_press() {
    ESP_LOGI(TAG, "Cancel pressed, returning to menu.");
    view_manager_load_view(VIEW_ID_MENU);
}

void AddNotificationView::on_nav_press(bool forward) {
    navigate_focus(forward);
}

// --- Static Callback Bridges ---
void AddNotificationView::ok_press_cb(void* user_data) {
    static_cast<AddNotificationView*>(user_data)->on_ok_press();
}

void AddNotificationView::cancel_press_cb(void* user_data) {
    static_cast<AddNotificationView*>(user_data)->on_cancel_press();
}

void AddNotificationView::left_press_cb(void* user_data) {
    static_cast<AddNotificationView*>(user_data)->on_nav_press(false);
}

void AddNotificationView::right_press_cb(void* user_data) {
    static_cast<AddNotificationView*>(user_data)->on_nav_press(true);
}

void AddNotificationView::save_event_cb(lv_event_t* e) {
    auto* view = static_cast<AddNotificationView*>(lv_event_get_user_data(e));
    view->save_notification(false);
}

void AddNotificationView::save_later_event_cb(lv_event_t* e) {
    auto* view = static_cast<AddNotificationView*>(lv_event_get_user_data(e));
    view->save_notification(true);
}