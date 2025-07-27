#ifndef NOTIFICATION_HISTORY_VIEW_H
#define NOTIFICATION_HISTORY_VIEW_H

#include "../view.h"
#include "lvgl.h"
#include "models/notification_data_model.h"
#include "components/popup_manager/popup_manager.h"
#include <vector>

/**
 * @brief Displays a list of unread notifications.
 *
 * This view fetches the list of unread notifications from the NotificationManager
 * and displays them in a scrollable list. Users can select a notification to
 * view its full details in a popup.
 */
class NotificationHistoryView : public View {
public:
    NotificationHistoryView();
    ~NotificationHistoryView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- UI Widgets ---
    lv_obj_t* list = nullptr;
    lv_group_t* group = nullptr;

    // --- State ---
    std::vector<Notification> unread_notifications;

    // --- Private Methods for Setup ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();
    void populate_list();

    // --- Private Methods for UI Logic ---
    void handle_item_selection();
    void handle_popup_close(popup_result_t result);

    // --- Instance Methods for Button Actions ---
    void on_ok_press();
    void on_cancel_press();
    void on_up_press();
    void on_down_press();

    // --- Static Callbacks (Bridge to C-style APIs) ---
    static void ok_press_cb(void* user_data);
    static void cancel_press_cb(void* user_data);
    static void up_press_cb(void* user_data);
    static void down_press_cb(void* user_data);
    static void item_click_event_cb(lv_event_t* e);
    static void popup_close_cb(popup_result_t result, void* user_data);
};

#endif // NOTIFICATION_HISTORY_VIEW_H