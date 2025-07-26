#ifndef STANDBY_VIEW_H
#define STANDBY_VIEW_H

#include "../view.h"
#include "lvgl.h"
#include "components/popup_manager/popup_manager.h" // <-- ADDED for popup_result_t

/**
 * @brief The StandbyView class represents the main idle/clock screen.
 *
 * It displays the current time and date, provides volume controls, and handles
 * power management actions like light sleep and deep sleep (shutdown). It serves
 * as the entry point to the rest of the application's UI.
 */
class StandbyView : public View {
public:
    /**
     * @brief Constructs a StandbyView.
     */
    StandbyView();

    /**
     * @brief Destroys the StandbyView and cleans up all its non-widget resources.
     *
     * This is automatically called by the ViewManager. It ensures that all
     * manually created timers are freed, preventing memory leaks. LVGL widgets
     * are cleaned up by the ViewManager itself.
     */
    ~StandbyView() override;

    /**
     * @brief Creates the UI for the StandbyView.
     * @param parent The parent LVGL object to build the UI on.
     */
    void create(lv_obj_t* parent) override;

private:
    // --- UI Widgets ---
    lv_obj_t* center_time_label = nullptr;
    lv_obj_t* center_date_label = nullptr;
    lv_obj_t* loading_label = nullptr;
    
    // --- Timers ---
    lv_timer_t* update_timer = nullptr;
    lv_timer_t* volume_up_timer = nullptr;
    lv_timer_t* volume_down_timer = nullptr;

    // --- State ---
    bool is_time_synced = false;

    // --- Private Methods for Setup ---
    void setup_ui(lv_obj_t* parent);
    void setup_main_button_handlers();

    // --- Private Methods for UI Logic ---
    void update_clock();

    // --- Instance Methods for Button Actions ---
    void on_menu_press();
    void on_sleep_press();
    void on_shutdown_long_press();
    void on_volume_up_long_press_start();
    void on_volume_down_long_press_start();
    void stop_volume_repeat_timer(lv_timer_t** timer_ptr);

    // --- Instance method to handle popup results ---
    void handle_shutdown_result(popup_result_t result);

    // --- Static Callbacks (Bridge to C-style APIs) ---
    static void update_clock_cb(lv_timer_t* timer);
    static void menu_press_cb(void* user_data);
    static void sleep_press_cb(void* user_data);
    static void shutdown_long_press_cb(void* user_data);
    static void volume_up_long_press_start_cb(void* user_data);
    static void volume_down_long_press_start_cb(void* user_data);
    static void volume_up_press_up_cb(void* user_data);
    static void volume_down_press_up_cb(void* user_data);
    // Static callback for the popup manager
    static void shutdown_popup_cb(popup_result_t result, void* user_data);
};

#endif // STANDBY_VIEW_H