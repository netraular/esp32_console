#ifndef ADD_NOTIFICATION_VIEW_H
#define ADD_NOTIFICATION_VIEW_H

#include "views/view.h"
#include "lvgl.h"

/**
 * @brief A view for creating new test notifications with a delay.
 *
 * This view provides buttons to create notifications that will be dispatched
 * after a specified delay, allowing for easy testing of the notification system.
 */
class AddNotificationView : public View {
public:
    AddNotificationView();
    ~AddNotificationView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- UI Widgets ---
    lv_obj_t* save_10s_button = nullptr;
    lv_obj_t* save_1min_button = nullptr;
    lv_group_t* input_group = nullptr;

    // --- Style Objects ---
    lv_style_t style_btn_default;
    lv_style_t style_btn_focused;

    // --- Private Methods for Setup ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();
    void init_styles();

    // --- Private Methods for UI Logic ---
    void save_notification(int delay_seconds);
    
    // --- Instance Methods for Button Actions ---
    void on_ok_press();
    void on_cancel_press();

    // --- Static Callbacks (Bridge to C-style APIs) ---
    static void ok_press_cb(void* user_data);
    static void cancel_press_cb(void* user_data);
    static void save_10s_event_cb(lv_event_t* e);
    static void save_1min_event_cb(lv_event_t* e);
};

#endif // ADD_NOTIFICATION_VIEW_H