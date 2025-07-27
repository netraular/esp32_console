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

// A struct to pass all necessary data to the FreeRTOS task.
struct NotificationTaskParams {
    std::string title;
    std::string message;
    int delay_seconds;
};

// --- Constructor & Destructor ---
AddNotificationView::AddNotificationView() {
    ESP_LOGI(TAG, "AddNotificationView constructed");
}

AddNotificationView::~AddNotificationView() {
    if (input_group) {
        lv_group_del(input_group);
        input_group = nullptr;
    }
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

    init_styles();
    setup_ui(container);
    setup_button_handlers();
}

// --- Style Initialization ---
void AddNotificationView::init_styles() {
    lv_style_init(&style_btn_default);
    lv_style_set_radius(&style_btn_default, 6);
    lv_style_set_bg_color(&style_btn_default, lv_color_white());
    lv_style_set_bg_opa(&style_btn_default, LV_OPA_100);
    lv_style_set_border_color(&style_btn_default, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_border_width(&style_btn_default, 2);
    lv_style_set_text_color(&style_btn_default, lv_palette_main(LV_PALETTE_BLUE));

    lv_style_init(&style_btn_focused);
    lv_style_set_bg_color(&style_btn_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_color(&style_btn_focused, lv_color_white());
}

// --- UI & Handler Setup ---
void AddNotificationView::setup_ui(lv_obj_t* parent) {
    status_bar_create(parent);
    
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_center(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 20, 0);

    lv_obj_t* info_label = lv_label_create(cont);
    lv_label_set_text(info_label, "Create a test notification");
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_20, 0);

    // Button for 10-second delay
    save_10s_button = lv_button_create(cont);
    lv_obj_t* save_10s_label = lv_label_create(save_10s_button);
    lv_label_set_text(save_10s_label, "Create (10s Delay)");
    lv_obj_add_event_cb(save_10s_button, save_10s_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_style(save_10s_button, &style_btn_default, LV_STATE_DEFAULT);
    lv_obj_add_style(save_10s_button, &style_btn_focused, LV_STATE_FOCUSED);

    // Button for 1-minute delay
    save_1min_button = lv_button_create(cont);
    lv_obj_t* save_1min_label = lv_label_create(save_1min_button);
    lv_label_set_text(save_1min_label, "Create (1 min Delay)");
    lv_obj_add_event_cb(save_1min_button, save_1min_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_style(save_1min_button, &style_btn_default, LV_STATE_DEFAULT);
    lv_obj_add_style(save_1min_button, &style_btn_focused, LV_STATE_FOCUSED);

    input_group = lv_group_create();
    lv_group_add_obj(input_group, save_10s_button);
    lv_group_add_obj(input_group, save_1min_button);
    lv_group_set_wrap(input_group, true);
}

void AddNotificationView::setup_button_handlers() {
    button_manager_unregister_view_handlers();
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, cancel_press_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, [](void* d) { lv_group_focus_prev(static_cast<AddNotificationView*>(d)->input_group); }, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, [](void* d) { lv_group_focus_next(static_cast<AddNotificationView*>(d)->input_group); }, true, this);
}

// --- UI Logic ---
void AddNotificationView::save_notification(int delay_seconds) {
    ESP_LOGI(TAG, "Scheduling notification with %d-second delay.", delay_seconds);

    // Task function to be run in the background
    auto task_func = [](void* params) {
        auto* task_params = static_cast<NotificationTaskParams*>(params);
        ESP_LOGI(TAG, "[Delayed Task] Started, waiting %d seconds...", task_params->delay_seconds);
        
        vTaskDelay(pdMS_TO_TICKS(task_params->delay_seconds * 1000));
        
        ESP_LOGI(TAG, "[Delayed Task] Adding notification now.");
        NotificationManager::add_notification(task_params->title, task_params->message);

        delete task_params; // Clean up the allocated data
        ESP_LOGI(TAG, "[Delayed Task] Finished, deleting task.");
        vTaskDelete(NULL); // Delete the task
    };

    // We must copy the data to the heap so it's valid when the task runs.
    auto* params = new NotificationTaskParams();
    params->title = "Notification at " + std::to_string(time(NULL) + delay_seconds);
    params->message = "This notification was scheduled.";
    params->delay_seconds = delay_seconds;

    xTaskCreate(task_func, "notif_delay_task", 4096, params, 5, NULL);
    
    // Show a temporary confirmation message before going back
    lv_obj_t* label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Notification Scheduled!");
    lv_obj_center(label);
    
    lv_timer_create([](lv_timer_t* t){
        view_manager_load_view(VIEW_ID_MENU);
    }, 1500, NULL);
    lv_timer_set_repeat_count(lv_timer_get_next(NULL), 1);
}

void AddNotificationView::on_ok_press() {
    if (!input_group) return;
    lv_obj_t* focused_obj = lv_group_get_focused(input_group);
    if (focused_obj) {
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

void AddNotificationView::save_10s_event_cb(lv_event_t* e) {
    auto* view = static_cast<AddNotificationView*>(lv_event_get_user_data(e));
    view->save_notification(10);
}

void AddNotificationView::save_1min_event_cb(lv_event_t* e) {
    auto* view = static_cast<AddNotificationView*>(lv_event_get_user_data(e));
    view->save_notification(60);
}