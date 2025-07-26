#ifndef POPUP_TEST_VIEW_H
#define POPUP_TEST_VIEW_H

#include "../view.h"
#include "lvgl.h"
#include "components/popup_manager/popup_manager.h"

/**
 * @brief Demonstrates the different popups provided by the PopupManager.
 *
 * This view allows the user to cycle through different popup types:
 * 1. Alert (OK button)
 * 2. Confirmation (Two buttons)
 * 3. Loading (Spinner with a timed callback)
 * It showcases how to handle popup results and re-enable input handlers.
 */
class PopupTestView : public View {
public:
    PopupTestView();
    ~PopupTestView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- State ---
    enum PopupDemoState {
        STATE_DEMO_ALERT,
        STATE_DEMO_CONFIRMATION,
        STATE_DEMO_LOADING,
        STATE_COUNT
    };
    PopupDemoState current_demo_state = STATE_DEMO_ALERT;

    // --- UI Widgets ---
    lv_obj_t* info_label = nullptr;
    lv_timer_t* loading_timer = nullptr;
    lv_timer_t* info_update_timer = nullptr; // <-- ADDED: To track the feedback timer

    // --- Private Methods for Setup ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();
    void update_info_label();

    // --- Private Methods for Popup Logic ---
    void show_next_popup();
    void show_alert_popup();
    void show_confirmation_popup();
    void show_loading_popup();
    void handle_popup_result(popup_result_t result);
    void handle_loading_finished();
    void create_info_update_timer(); // <-- ADDED: Helper to create the timer

    // --- Instance Methods for Button Actions ---
    void on_ok_press();
    void on_cancel_press();

    // --- Static Callbacks (Bridge to C-style APIs) ---
    static void ok_press_cb(void* user_data);
    static void cancel_press_cb(void* user_data);
    static void popup_result_cb(popup_result_t result, void* user_data);
    static void loading_finished_cb(lv_timer_t* timer);
    static void info_update_timer_cb(lv_timer_t* timer); // <-- ADDED: Static callback for the timer
};

#endif // POPUP_TEST_VIEW_H