#ifndef TRACK_HABITS_VIEW_H
#define TRACK_HABITS_VIEW_H

#include "views/view.h"
#include "views/view_manager.h"
#include "lvgl.h"
#include "models/habit_data_models.h" // For Habit and HabitCategory
#include <vector>
#include <string>

/**
 * @brief A view for tracking all daily habits in a unified list.
 *
 * This view provides an efficient and responsive interface for users to mark their
 * habits as completed for the current day. It displays a single, scrollable list
 * of all active habits, grouped by their category, for quick and easy tracking.
 */
class TrackHabitsView : public View {
public:
    TrackHabitsView();
    ~TrackHabitsView() override;

    void create(lv_obj_t* parent) override;

private:
    // --- Private Data Structure for UI ---
    /**
     * @brief A helper struct to cache all data needed to render a single habit row.
     * This avoids repeated calls to the data manager during UI interaction.
     */
    struct HabitRenderData {
        Habit habit;
        std::string category_name; // Added to show category context if needed
        bool is_done_today;
    };

    // --- UI and State Members ---
    lv_obj_t* habit_list_container = nullptr;
    lv_group_t* group = nullptr;
    lv_style_t style_list_item_focused;
    lv_style_t style_category_header;
    bool styles_initialized = false;

    // --- Data Cache ---
    std::vector<HabitRenderData> m_habit_render_data;

    // --- UI Setup ---
    void setup_ui(lv_obj_t* parent);
    void populate_habit_list();
    void init_styles();
    void reset_styles();

    // --- Button and Event Handling ---
    void setup_button_handlers();
    void on_ok_press();
    void on_cancel_press();
    void on_nav_press(bool next);

    // --- Static Callbacks ---
    static void handle_ok_press_cb(void* user_data);
    static void handle_cancel_press_cb(void* user_data);
    static void handle_left_press_cb(void* user_data);
    static void handle_right_press_cb(void* user_data);
};

#endif // TRACK_HABITS_VIEW_H