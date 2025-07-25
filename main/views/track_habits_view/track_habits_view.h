#ifndef TRACK_HABITS_VIEW_H
#define TRACK_HABITS_VIEW_H

#include "../view.h"
#include "../view_manager.h"
#include "lvgl.h"
#include "models/habit_data_models.h" // For Habit struct
#include <vector>

/**
 * @brief A view for tracking daily habit completion.
 *
 * This view displays a list of all active habits. Each habit has a checkbox
 * that can be toggled. The state is saved immediately to the filesystem,
 * allowing the user to track their progress for the current day.
 */
class TrackHabitsView : public View {
public:
    TrackHabitsView();
    ~TrackHabitsView() override;

    void create(lv_obj_t* parent) override;

private:
    // --- UI and State Members ---
    lv_obj_t* list_habits = nullptr;
    lv_group_t* group = nullptr;
    lv_style_t style_focused_list_btn;
    bool styles_initialized = false;

    // A local copy of habits being displayed to map list indices to habit IDs
    std::vector<Habit> displayed_habits;

    // --- UI Setup ---
    void setup_ui(lv_obj_t* parent);
    void populate_list();
    lv_obj_t* create_habit_list_item(lv_obj_t* parent, const Habit& habit);
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

#endif // TRACK_HABITS_VIEW_H