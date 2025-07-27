#include "notification_history_view.h"
#include "views/view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/notification_manager/notification_manager.h"
#include "components/status_bar_component/status_bar_component.h"
#include "esp_log.h"

static const char *TAG = "NOTIF_HIST_VIEW";

// --- Constructor & Destructor ---
NotificationHistoryView::NotificationHistoryView() {
    ESP_LOGI(TAG, "NotificationHistoryView constructed");
}

NotificationHistoryView::~NotificationHistoryView() {
    if (group) {
        lv_group_del(group);
        group = nullptr;
    }
    ESP_LOGI(TAG, "NotificationHistoryView destructed");
}

// --- View Lifecycle ---
void NotificationHistoryView::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);
    
    setup_ui(container);
    setup_button_handlers();
}

// --- UI & Handler Setup ---
void NotificationHistoryView::setup_ui(lv_obj_t* parent) {
    status_bar_create(parent);
    
    // Fetch notifications first to decide what to display
    unread_notifications = NotificationManager::get_unread_notifications();

    if (unread_notifications.empty()) {
        // Display a message if there are no notifications
        lv_obj_t* label = lv_label_create(parent);
        lv_label_set_text(label, "No unread notifications");
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(label);
    } else {
        // Create the list widget
        list = lv_list_create(parent);
        lv_obj_set_size(list, LV_PCT(100), LV_PCT(100) - 20); // Full size minus status bar
        lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);

        group = lv_group_create();
        populate_list();
    }
}

void NotificationHistoryView::populate_list() {
    ESP_LOGI(TAG, "Populating list with %d notifications.", unread_notifications.size());
    for (size_t i = 0; i < unread_notifications.size(); ++i) {
        const auto& notif = unread_notifications[i];

        // Create a button in the list for each notification
        lv_obj_t* btn = lv_list_add_button(list, LV_SYMBOL_BELL, notif.title.c_str());
        
        // Store the index of the notification in the button's user data
        lv_obj_set_user_data(btn, (void*)i);
        
        // Add an event callback for clicks
        lv_obj_add_event_cb(btn, item_click_event_cb, LV_EVENT_CLICKED, this);
        
        // Add the button to the navigation group
        lv_group_add_obj(group, btn);
    }
}

void NotificationHistoryView::setup_button_handlers() {
    button_manager_unregister_view_handlers();
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, cancel_press_cb, true, this);
    // For list navigation, up/down is more intuitive than left/right
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, up_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, down_press_cb, true, this);
}

// --- UI Logic ---
void NotificationHistoryView::handle_item_selection() {
    if (!group) return;

    lv_obj_t* focused_btn = lv_group_get_focused(group);
    if (!focused_btn) return;

    // Retrieve the index stored in the button's user data
    size_t index = (size_t)lv_obj_get_user_data(focused_btn);

    if (index < unread_notifications.size()) {
        const Notification& selected_notif = unread_notifications[index];
        ESP_LOGI(TAG, "Showing details for notification ID: %lu", selected_notif.id);

        // Show the full notification in a popup
        popup_manager_show_alert(
            selected_notif.title.c_str(),
            selected_notif.message.c_str(),
            popup_close_cb,
            this
        );
    }
}

void NotificationHistoryView::handle_popup_close(popup_result_t result) {
    ESP_LOGI(TAG, "Notification detail popup closed. Re-enabling view input.");
    // After the popup is closed, we must re-register our button handlers
    setup_button_handlers();
}

// --- Instance Methods for Button Actions ---
void NotificationHistoryView::on_ok_press() {
    handle_item_selection();
}

void NotificationHistoryView::on_cancel_press() {
    ESP_LOGI(TAG, "Cancel pressed, returning to menu.");
    view_manager_load_view(VIEW_ID_MENU);
}

void NotificationHistoryView::on_up_press() {
    if (group) lv_group_focus_prev(group);
}

void NotificationHistoryView::on_down_press() {
    if (group) lv_group_focus_next(group);
}

// --- Static Callback Bridges ---
void NotificationHistoryView::ok_press_cb(void* user_data) {
    static_cast<NotificationHistoryView*>(user_data)->on_ok_press();
}

void NotificationHistoryView::cancel_press_cb(void* user_data) {
    static_cast<NotificationHistoryView*>(user_data)->on_cancel_press();
}

void NotificationHistoryView::up_press_cb(void* user_data) {
    static_cast<NotificationHistoryView*>(user_data)->on_up_press();
}

void NotificationHistoryView::down_press_cb(void* user_data) {
    static_cast<NotificationHistoryView*>(user_data)->on_down_press();
}

void NotificationHistoryView::item_click_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        static_cast<NotificationHistoryView*>(lv_event_get_user_data(e))->handle_item_selection();
    }
}

void NotificationHistoryView::popup_close_cb(popup_result_t result, void* user_data) {
    static_cast<NotificationHistoryView*>(user_data)->handle_popup_close(result);
}