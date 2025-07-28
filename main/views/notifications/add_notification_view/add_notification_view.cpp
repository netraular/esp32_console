#include "add_notification_view.h"
#include "views/view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/notification_manager/notification_manager.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "ADD_NOTIF_VIEW";

// --- Lifecycle Methods ---
AddNotificationView::AddNotificationView() : 
    input_group(nullptr), 
    feedback_label(nullptr), 
    feedback_timer(nullptr), 
    styles_initialized(false) 
{
    ESP_LOGI(TAG, "AddNotificationView constructed");
}

AddNotificationView::~AddNotificationView() {
    // Crucially, delete the timer if it's still active.
    // This prevents it from firing after the view object is destroyed.
    cleanup_feedback_ui();

    if (input_group) {
        lv_group_del(input_group);
        input_group = nullptr;
    }
    ESP_LOGI(TAG, "AddNotificationView destructed");
}

void AddNotificationView::create(lv_obj_t* parent) {
    container = parent;
    setup_ui(parent);
    setup_button_handlers();
}

// --- UI & Handler Setup ---
void AddNotificationView::init_styles() {
    if (styles_initialized) return;

    lv_style_init(&style_btn_default);
    lv_style_set_bg_color(&style_btn_default, lv_palette_lighten(LV_PALETTE_GREY, 2));
    lv_style_set_border_color(&style_btn_default, lv_palette_darken(LV_PALETTE_GREY, 3));
    lv_style_set_border_width(&style_btn_default, 2);

    lv_style_init(&style_btn_focused);
    lv_style_set_bg_color(&style_btn_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_border_color(&style_btn_focused, lv_palette_darken(LV_PALETTE_BLUE, 3));

    styles_initialized = true;
}

void AddNotificationView::setup_ui(lv_obj_t* parent) {
    init_styles();

    lv_obj_t* title = lv_label_create(parent);
    lv_label_set_text(title, "Add Test Notification");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    lv_obj_t* main_cont = lv_obj_create(parent);
    lv_obj_set_size(main_cont, 200, 120);
    lv_obj_center(main_cont);
    lv_obj_set_layout(main_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    input_group = lv_group_create();
    lv_group_set_wrap(input_group, true);

    save_10s_button = lv_button_create(main_cont);
    lv_obj_set_size(save_10s_button, 180, 40);
    lv_obj_add_style(save_10s_button, &style_btn_default, 0);
    lv_obj_add_style(save_10s_button, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(save_10s_button, save_10s_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* label_10s = lv_label_create(save_10s_button);
    lv_label_set_text(label_10s, "Test Notif. in 10s");
    lv_obj_center(label_10s);
    lv_group_add_obj(input_group, save_10s_button);

    save_1min_button = lv_button_create(main_cont);
    lv_obj_set_size(save_1min_button, 180, 40);
    lv_obj_add_style(save_1min_button, &style_btn_default, 0);
    lv_obj_add_style(save_1min_button, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(save_1min_button, save_1min_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_t* label_1min = lv_label_create(save_1min_button);
    lv_label_set_text(label_1min, "Test Notif. in 1min");
    lv_obj_center(label_1min);
    lv_group_add_obj(input_group, save_1min_button);
}

void AddNotificationView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, cancel_press_cb, true, this);
    // Navigation is handled by the group
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, [](void* user_data){
        auto* view = static_cast<AddNotificationView*>(user_data);
        if (view && view->input_group) lv_group_focus_prev(view->input_group);
    }, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, [](void* user_data){
        auto* view = static_cast<AddNotificationView*>(user_data);
        if (view && view->input_group) lv_group_focus_next(view->input_group);
    }, true, this);
}

// --- Instance Methods for Actions ---

void AddNotificationView::cleanup_feedback_ui() {
    if (feedback_timer) {
        lv_timer_delete(feedback_timer);
        feedback_timer = nullptr;
    }
    if (feedback_label) {
        lv_obj_del(feedback_label);
        feedback_label = nullptr;
    }
}

void AddNotificationView::save_notification(int delay_seconds) {
    time_t now = time(nullptr);
    time_t target_time = now + delay_seconds;

    std::string title = "Test Notification";
    std::string message = "This is a test notification scheduled for " + std::to_string(delay_seconds) + " seconds from now.";

    NotificationManager::add_notification(title, message, target_time);
    
    // If a feedback message is already showing, clean it up before showing a new one.
    cleanup_feedback_ui();

    // Create a temporary feedback label and store the handle
    feedback_label = lv_label_create(container);
    lv_label_set_text(feedback_label, "Notification Saved!");
    lv_obj_set_style_bg_color(feedback_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_bg_opa(feedback_label, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(feedback_label, lv_color_white(), 0);
    lv_obj_set_style_pad_all(feedback_label, 5, 0);
    lv_obj_set_style_radius(feedback_label, 3, 0);
    lv_obj_align(feedback_label, LV_ALIGN_BOTTOM_MID, 0, -5);

    // Create a one-shot timer to delete the label and store its handle.
    // The user_data for the timer now points to this view instance.
    feedback_timer = lv_timer_create(feedback_timer_cb, 1500, this);
    lv_timer_set_repeat_count(feedback_timer, 1);
}

void AddNotificationView::on_ok_press() {
    if (input_group) {
        lv_obj_t* focused_obj = lv_group_get_focused(input_group);
        if (focused_obj) {
            lv_obj_send_event(focused_obj, LV_EVENT_CLICKED, nullptr);
        }
    }
}

void AddNotificationView::on_cancel_press() {
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Static Callbacks ---
void AddNotificationView::feedback_timer_cb(lv_timer_t* timer) {
    auto* view = static_cast<AddNotificationView*>(lv_timer_get_user_data(timer));
    if (view) {
        // The timer is one-shot, so it deletes itself. We just need to clean up our UI.
        view->cleanup_feedback_ui();
    }
}

void AddNotificationView::ok_press_cb(void* user_data) {
    static_cast<AddNotificationView*>(user_data)->on_ok_press();
}

void AddNotificationView::cancel_press_cb(void* user_data) {
    static_cast<AddNotificationView*>(user_data)->on_cancel_press();
}

void AddNotificationView::save_10s_event_cb(lv_event_t* e) {
    auto* view = static_cast<AddNotificationView*>(lv_event_get_user_data(e));
    if (view) {
        view->save_notification(10);
    }
}

void AddNotificationView::save_1min_event_cb(lv_event_t* e) {
    auto* view = static_cast<AddNotificationView*>(lv_event_get_user_data(e));
    if (view) {
        view->save_notification(60);
    }
}