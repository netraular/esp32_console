#include "add_notification_view.h"
#include "views/view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/notification_manager/notification_manager.h"
#include "components/status_bar_component/status_bar_component.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>
#include <string>

static const char *TAG = "ADD_NOTIF_VIEW";

// --- Constructor & Destructor ---
AddNotificationView::AddNotificationView() {
    ESP_LOGI(TAG, "AddNotificationView constructed");
}

AddNotificationView::~AddNotificationView() {
    if (input_group) {
        lv_group_del(input_group);
        input_group = nullptr;
    }
    // Clean up the styles we created
    lv_style_reset(&style_btn_default);
    lv_style_reset(&style_btn_focused);
    ESP_LOGI(TAG, "AddNotificationView destructed");
}

// --- View Lifecycle ---
void AddNotificationView::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);

    init_styles(); // Initialize styles first
    setup_ui(container);
    setup_button_handlers();
}

// --- Style Initialization ---
void AddNotificationView::init_styles() {
    // This style is very similar to the one in popup_manager.
    // In a larger project, you might move this to a global "Theme" manager.
    
    // --- Default button style (white with blue border) ---
    lv_style_init(&style_btn_default);
    lv_style_set_radius(&style_btn_default, 6);
    lv_style_set_bg_color(&style_btn_default, lv_color_white());
    lv_style_set_bg_opa(&style_btn_default, LV_OPA_100);
    lv_style_set_border_color(&style_btn_default, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_border_width(&style_btn_default, 2);
    lv_style_set_text_color(&style_btn_default, lv_palette_main(LV_PALETTE_BLUE));

    // --- Focused button style (blue with white text) ---
    lv_style_init(&style_btn_focused);
    lv_style_set_bg_color(&style_btn_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_color(&style_btn_focused, lv_color_white());
}

// --- UI & Handler Setup ---
void AddNotificationView::setup_ui(lv_obj_t* parent) {
    status_bar_create(parent);
    
    // Main container for the buttons
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_center(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 20, 0);

    // Info Label
    lv_obj_t* info_label = lv_label_create(cont);
    lv_label_set_text(info_label, "Create a test notification");
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_20, 0);

    // Save Now Button
    save_now_button = lv_button_create(cont);
    lv_obj_t* save_label = lv_label_create(save_now_button);
    lv_label_set_text(save_label, "Create Now");
    lv_obj_add_event_cb(save_now_button, save_now_event_cb, LV_EVENT_CLICKED, this);
    // Apply custom styles
    lv_obj_add_style(save_now_button, &style_btn_default, LV_STATE_DEFAULT);
    lv_obj_add_style(save_now_button, &style_btn_focused, LV_STATE_FOCUSED);

    // Save Later Button
    save_later_button = lv_button_create(cont);
    lv_obj_t* save_later_label = lv_label_create(save_later_button);
    lv_label_set_text(save_later_label, "Create (10s Delay)");
    lv_obj_add_event_cb(save_later_button, save_later_event_cb, LV_EVENT_CLICKED, this);
    // Apply custom styles
    lv_obj_add_style(save_later_button, &style_btn_default, LV_STATE_DEFAULT);
    lv_obj_add_style(save_later_button, &style_btn_focused, LV_STATE_FOCUSED);

    // Create a group to manage focus between the buttons
    input_group = lv_group_create();
    lv_group_add_obj(input_group, save_now_button);
    lv_group_add_obj(input_group, save_later_button);
    lv_group_set_wrap(input_group, true);
}

void AddNotificationView::setup_button_handlers() {
    button_manager_unregister_view_handlers();
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, cancel_press_cb, true, this);
    // Use Left/Right to navigate between the two buttons
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, [](void* d) { lv_group_focus_prev(static_cast<AddNotificationView*>(d)->input_group); }, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, [](void* d) { lv_group_focus_next(static_cast<AddNotificationView*>(d)->input_group); }, true, this);
}

// --- UI Logic ---

void AddNotificationView::save_notification(bool is_delayed) {
    // Generate the content automatically
    time_t now = time(NULL);
    std::string title = "Notification at " + std::to_string(now);
    std::string message = "This is a test notification message.";
    
    if (is_delayed) {
        ESP_LOGI(TAG, "Creating notification with 10s delay.");
        // Use a simple FreeRTOS task to handle the delay.
        auto task_func = [](void* params) {
            ESP_LOGI(TAG, "[Delayed Task] Started, waiting 10s...");
            auto* notif_data = static_cast<std::pair<std::string, std::string>*>(params);
            vTaskDelay(pdMS_TO_TICKS(10000));
            
            ESP_LOGI(TAG, "[Delayed Task] Adding notification now.");
            NotificationManager::add_notification(notif_data->first, notif_data->second);

            delete notif_data; // Clean up the allocated data
            ESP_LOGI(TAG, "[Delayed Task] Finished, deleting task.");
            vTaskDelete(NULL); // Delete the task
        };
        
        // We must copy the strings to the heap so they are valid when the task runs.
        auto* data = new std::pair<std::string, std::string>(title, message);

        // Increased stack size to prevent stack overflow.
        // File I/O and C++ libraries in the call chain require more stack.
        xTaskCreate(task_func, "notif_delay_task", 4096, data, 5, NULL);

    } else {
        ESP_LOGI(TAG, "Creating notification now.");
        NotificationManager::add_notification(title, message);
    }
    
    // Show a temporary confirmation message before going back
    lv_obj_t* label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Notification Created!");
    lv_obj_center(label);
    
    // Create a one-shot timer to go back to the menu after a short delay
    lv_timer_create([](lv_timer_t* t){
        view_manager_load_view(VIEW_ID_MENU);
    }, 1500, NULL);
    lv_timer_set_repeat_count(lv_timer_get_next(NULL), 1);
}

// --- Instance Methods for Button Actions ---
void AddNotificationView::on_ok_press() {
    if (!input_group) return;
    lv_obj_t* focused_obj = lv_group_get_focused(input_group);
    if (focused_obj) {
        // Send a click event to the focused object (the button)
        lv_obj_send_event(focused_obj, LV_EVENT_CLICKED, nullptr);
    }
}

void AddNotificationView::on_cancel_press() {
    ESP_LOGI(TAG, "Cancel pressed, returning to menu.");
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Static Callback Bridges ---
void AddNotificationView::ok_press_cb(void* user_data) {
    static_cast<AddNotificationView*>(user_data)->on_ok_press();
}

void AddNotificationView::cancel_press_cb(void* user_data) {
    static_cast<AddNotificationView*>(user_data)->on_cancel_press();
}

void AddNotificationView::save_now_event_cb(lv_event_t* e) {
    auto* view = static_cast<AddNotificationView*>(lv_event_get_user_data(e));
    view->save_notification(false);
}

void AddNotificationView::save_later_event_cb(lv_event_t* e) {
    auto* view = static_cast<AddNotificationView*>(lv_event_get_user_data(e));
    view->save_notification(true);
}