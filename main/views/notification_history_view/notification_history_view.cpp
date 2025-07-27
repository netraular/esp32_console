#include "notification_history_view.h"
#include "views/view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/notification_manager/notification_manager.h"
#include "components/status_bar_component/status_bar_component.h"
#include "esp_log.h"

static const char *TAG = "NOTIF_HIST_VIEW";

NotificationHistoryView::NotificationHistoryView() {
    ESP_LOGI(TAG, "NotificationHistoryView constructed");
}

NotificationHistoryView::~NotificationHistoryView() {
    stop_refresh_timer();
    cleanup_ui();
    ESP_LOGI(TAG, "NotificationHistoryView destructed");
}

void NotificationHistoryView::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    
    status_bar_create(container);
    setup_selector_ui();
}

void NotificationHistoryView::cleanup_ui() {
    if (group) {
        lv_group_del(group);
        group = nullptr;
    }
    if (selector_container) {
        lv_obj_del(selector_container);
        selector_container = nullptr;
    }
    if (list_container) {
        lv_obj_del(list_container);
        list_container = nullptr;
    }
}

void NotificationHistoryView::stop_refresh_timer() {
    if (refresh_timer) {
        ESP_LOGI(TAG, "Stopping refresh timer.");
        lv_timer_del(refresh_timer);
        refresh_timer = nullptr;
    }
}

void NotificationHistoryView::setup_selector_ui() {
    stop_refresh_timer();
    cleanup_ui();
    current_state = STATE_SELECTING;

    selector_container = lv_obj_create(container);
    lv_obj_remove_style_all(selector_container);
    lv_obj_set_size(selector_container, LV_PCT(100), LV_PCT(100) - 20);
    lv_obj_align(selector_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(selector_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(selector_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(selector_container, 20, 0);

    lv_obj_t* title = lv_label_create(selector_container);
    lv_label_set_text(title, "View Notifications");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);

    group = lv_group_create();

    lv_obj_t* pending_btn = lv_list_add_button(selector_container, LV_SYMBOL_REFRESH, "Pending");
    lv_obj_set_user_data(pending_btn, (void*)LIST_TYPE_PENDING);
    lv_group_add_obj(group, pending_btn);

    lv_obj_t* unread_btn = lv_list_add_button(selector_container, LV_SYMBOL_BELL, "Unread");
    lv_obj_set_user_data(unread_btn, (void*)LIST_TYPE_UNREAD);
    lv_group_add_obj(group, unread_btn);
    
    button_manager_unregister_view_handlers();
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, cancel_press_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, left_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, right_press_cb, true, this);
}

void NotificationHistoryView::setup_list_ui() {
    stop_refresh_timer();
    cleanup_ui();
    current_state = STATE_SHOWING_LIST;

    list_container = lv_obj_create(container);
    lv_obj_remove_style_all(list_container);
    lv_obj_set_size(list_container, LV_PCT(100), LV_PCT(100) - 20);
    lv_obj_align(list_container, LV_ALIGN_BOTTOM_MID, 0, 0);

    const char* list_name = "Unknown";
    if (current_list_type == LIST_TYPE_PENDING) {
        list_name = "Pending";
        current_notifications = NotificationManager::get_pending_notifications();
    } else {
        list_name = "Unread";
        current_notifications = NotificationManager::get_unread_notifications();
    }
    ESP_LOGI(TAG, "Setting up list view for '%s' notifications. Found %d items.", list_name, current_notifications.size());

    if (current_notifications.empty()) {
        lv_obj_t* label = lv_label_create(list_container);
        lv_label_set_text(label, current_list_type == LIST_TYPE_PENDING ? "No pending notifications" : "No unread notifications");
        lv_obj_center(label);
    } else {
        lv_obj_t* list = lv_list_create(list_container);
        lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
        lv_obj_center(list);
        group = lv_group_create();
        populate_list();
    }
    
    button_manager_unregister_view_handlers();
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, cancel_press_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, left_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, right_press_cb, true, this);

    ESP_LOGI(TAG, "Starting refresh timer for the list view.");
    refresh_timer = lv_timer_create(refresh_list_cb, 2000, this);
}

void NotificationHistoryView::populate_list() {
    lv_obj_t* list = lv_obj_get_child(list_container, 0);
    if (!list) return;

    ESP_LOGI(TAG, "--- Start of Notification List ---");
    for (size_t i = 0; i < current_notifications.size(); ++i) {
        const auto& notification_item = current_notifications[i];
        
        char time_buf[64];
        struct tm timeinfo;
        localtime_r(&notification_item.timestamp, &timeinfo);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        
        ESP_LOGI(TAG, "Item %d: ID=%lu, Title='%s', Timestamp=%s", 
                 i, notification_item.id, notification_item.title.c_str(), time_buf);

        lv_obj_t* btn;
        if (current_list_type == LIST_TYPE_PENDING) {
            strftime(time_buf, sizeof(time_buf), "%b %d, %H:%M", &timeinfo);
            std::string text = notification_item.title + "\n" + time_buf;
            btn = lv_list_add_button(list, LV_SYMBOL_REFRESH, text.c_str());
        } else {
            btn = lv_list_add_button(list, LV_SYMBOL_BELL, notification_item.title.c_str());
        }
        
        lv_obj_set_user_data(btn, (void*)i);
        lv_obj_add_event_cb(btn, list_event_cb, LV_EVENT_ALL, this);
        lv_group_add_obj(group, btn);
    }
    ESP_LOGI(TAG, "--- End of Notification List ---");
}

void NotificationHistoryView::refresh_list_content() {
    std::vector<Notification> new_notifications;
    if (current_list_type == LIST_TYPE_PENDING) {
        new_notifications = NotificationManager::get_pending_notifications();
    } else {
        new_notifications = NotificationManager::get_unread_notifications();
    }

    if (new_notifications.size() != current_notifications.size()) {
        ESP_LOGI(TAG, "Data has changed (old: %d, new: %d), refreshing list UI.", current_notifications.size(), new_notifications.size());
        setup_list_ui();
    }
}

void NotificationHistoryView::handle_item_selection() {
    if (!group) return;
    lv_obj_t* focused_btn = lv_group_get_focused(group);
    if (!focused_btn) return;

    size_t index = (size_t)lv_obj_get_user_data(focused_btn);

    if (index < current_notifications.size()) {
        const Notification& selected_notif = current_notifications[index];
        ESP_LOGI(TAG, "Showing details for notification ID: %lu", selected_notif.id);

        if (current_list_type == LIST_TYPE_UNREAD) {
            NotificationManager::mark_as_read(selected_notif.id);
        }

        stop_refresh_timer(); // Pause refresh while popup is active
        popup_manager_show_alert(selected_notif.title.c_str(), selected_notif.message.c_str(), popup_close_cb, this);
    }
}

void NotificationHistoryView::handle_popup_close(popup_result_t result) {
    if (current_list_type == LIST_TYPE_UNREAD) {
        ESP_LOGI(TAG, "Unread notification viewed, refreshing list.");
        setup_list_ui(); // This will refresh the list and restart the timer
    } else {
        ESP_LOGI(TAG, "Pending notification viewed, re-enabling input and restarting timer.");
        button_manager_unregister_view_handlers();
        button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, ok_press_cb, true, this);
        button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, cancel_press_cb, true, this);
        button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, left_press_cb, true, this);
        button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, right_press_cb, true, this);
        refresh_timer = lv_timer_create(refresh_list_cb, 2000, this); // Restart timer
    }
}

void NotificationHistoryView::on_ok_press() {
    if (current_state == STATE_SELECTING) {
        lv_obj_t* focused_btn = lv_group_get_focused(group);
        if (focused_btn) {
            current_list_type = (ListType)(size_t)lv_obj_get_user_data(focused_btn);
            setup_list_ui();
        }
    } else if (current_state == STATE_SHOWING_LIST) {
        handle_item_selection();
    }
}

void NotificationHistoryView::on_cancel_press() {
    if (current_state == STATE_SHOWING_LIST) {
        setup_selector_ui();
    } else {
        view_manager_load_view(VIEW_ID_MENU);
    }
}

void NotificationHistoryView::on_nav_press(bool is_next) {
    if (!group) return;
    if (is_next) {
        lv_group_focus_next(group);
    } else {
        lv_group_focus_prev(group);
    }
}

void NotificationHistoryView::ok_press_cb(void* user_data) {
    static_cast<NotificationHistoryView*>(user_data)->on_ok_press();
}

void NotificationHistoryView::cancel_press_cb(void* user_data) {
    static_cast<NotificationHistoryView*>(user_data)->on_cancel_press();
}

void NotificationHistoryView::left_press_cb(void* user_data) {
    static_cast<NotificationHistoryView*>(user_data)->on_nav_press(false);
}

void NotificationHistoryView::right_press_cb(void* user_data) {
    static_cast<NotificationHistoryView*>(user_data)->on_nav_press(true);
}

void NotificationHistoryView::list_event_cb(lv_event_t* e) {
    auto* view = static_cast<NotificationHistoryView*>(lv_event_get_user_data(e));
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        view->handle_item_selection();
    }
}

void NotificationHistoryView::popup_close_cb(popup_result_t result, void* user_data) {
    static_cast<NotificationHistoryView*>(user_data)->handle_popup_close(result);
}

void NotificationHistoryView::refresh_list_cb(lv_timer_t* timer) {
    auto* view = static_cast<NotificationHistoryView*>(lv_timer_get_user_data(timer));
    view->refresh_list_content();
}