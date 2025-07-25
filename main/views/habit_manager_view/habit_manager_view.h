#ifndef HABIT_MANAGER_VIEW_H
#define HABIT_MANAGER_VIEW_H

#include "../view.h"
#include "../view_manager.h"
#include "lvgl.h"

/**
 * @brief The main hub for the Habit Tracker feature.
 *
 * This view provides a menu to navigate to different habit-related screens,
 * such as tracking today's habits, managing habits and categories, and viewing history.
 */
class HabitManagerView : public View {
public:
    HabitManagerView();
    ~HabitManagerView() override;

    void create(lv_obj_t* parent) override;

private:
    // --- UI and State Members ---
    lv_obj_t* list_menu = nullptr;
    lv_group_t* group = nullptr;
    lv_style_t style_focused;
    bool styles_initialized = false;

    // --- UI Setup ---
    void setup_ui(lv_obj_t* parent);
    void init_styles();
    void reset_styles();

    // --- Button Handling ---
    void setup_button_handlers();

    // Instance Methods for Button Actions
    void on_ok_press();
    void on_cancel_press();
    void on_nav_press(bool next);

    // Static Callbacks for Button Manager
    static void handle_ok_press_cb(void* user_data);
    static void handle_cancel_press_cb(void* user_data);
    static void handle_left_press_cb(void* user_data);
    static void handle_right_press_cb(void* user_data);
};

#endif // HABIT_MANAGER_VIEW_H