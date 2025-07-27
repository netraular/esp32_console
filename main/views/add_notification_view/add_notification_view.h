#ifndef ADD_NOTIFICATION_VIEW_H
#define ADD_NOTIFICATION_VIEW_H

#include "../view.h"
#include "lvgl.h"

/**
 * @brief A view for creating new notifications.
 *
 * This view provides a user interface with text areas for a title and message,
 * and buttons to save the notification. It demonstrates how to interact with
 * the NotificationManager from a UI component.
 */
class AddNotificationView : public View {
public:
    AddNotificationView();
    ~AddNotificationView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- UI Widgets ---
    lv_obj_t* title_textarea = nullptr;
    lv_obj_t* message_textarea = nullptr;
    lv_obj_t* save_button = nullptr;
    lv_obj_t* save_later_button = nullptr;
    lv_group_t* input_group = nullptr; // To manage focus between textareas and buttons

    // --- Private Methods for Setup ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();

    // --- Private Methods for UI Logic ---
    void save_notification(bool is_delayed);
    void navigate_focus(bool forward);

    // --- Instance Methods for Button Actions ---
    void on_ok_press();
    void on_cancel_press();
    void on_nav_press(bool forward);

    // --- Static Callbacks (Bridge to C-style APIs) ---
    static void ok_press_cb(void* user_data);
    static void cancel_press_cb(void* user_data);
    static void left_press_cb(void* user_data);
    static void right_press_cb(void* user_data);
    static void save_event_cb(lv_event_t* e);
    static void save_later_event_cb(lv_event_t* e);
};

#endif // ADD_NOTIFICATION_VIEW_H