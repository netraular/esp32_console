#ifndef NOTIFICATION_HISTORY_VIEW_H
#define NOTIFICATION_HISTORY_VIEW_H

#include "../view.h"
#include "lvgl.h"
#include "models/notification_data_model.h"
#include "components/popup_manager/popup_manager.h"
#include <vector>
#include <string>

/**
 * @brief Manages the display of pending and unread notifications.
 *
 * This view first presents a selection screen to choose between "Pending"
 * and "Unread" notifications. It then displays the corresponding list.
 */
class NotificationHistoryView : public View {
public:
    NotificationHistoryView();
    ~NotificationHistoryView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- State Management ---
    enum ViewState {
        STATE_SELECTING,
        STATE_SHOWING_LIST
    };
    ViewState current_state = STATE_SELECTING;

    enum ListType {
        LIST_TYPE_PENDING,
        LIST_TYPE_UNREAD
    };
    ListType current_list_type = LIST_TYPE_PENDING;

    // --- UI Widgets ---
    lv_obj_t* selector_container = nullptr;
    lv_obj_t* list_container = nullptr;
    lv_group_t* group = nullptr;

    // --- Data ---
    std::vector<Notification> current_notifications;

    // --- Private Methods for UI Setup ---
    void setup_selector_ui();
    void setup_list_ui();
    void cleanup_ui();

    // --- Private Methods for UI Logic ---
    void populate_list();
    void handle_item_selection();
    void handle_popup_close(popup_result_t result);

    // --- Button Handlers ---
    void on_ok_press();
    void on_cancel_press();
    void on_nav_press(bool is_next);

    // --- Static Callbacks (Bridge to C-style APIs) ---
    static void ok_press_cb(void* user_data);
    static void cancel_press_cb(void* user_data);
    static void left_press_cb(void* user_data);
    static void right_press_cb(void* user_data);
    static void list_event_cb(lv_event_t* e);
    static void popup_close_cb(popup_result_t result, void* user_data);
};

#endif // NOTIFICATION_HISTORY_VIEW_H