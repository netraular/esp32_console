#ifndef HABIT_HISTORY_VIEW_H
#define HABIT_HISTORY_VIEW_H

#include "../view.h"
#include "../view_manager.h"
#include "lvgl.h"
#include "models/habit_data_models.h"
#include <vector>
#include <string>

// Defines the steps/screens within this view
enum class HabitHistoryStep {
    SELECT_HABIT,
    SHOW_HISTORY
};

/**
 * @brief A view for displaying a habit's completion history.
 *
 * This view has two screens:
 * 1. A list, grouped by category, to select an active habit.
 * 2. A GitHub-style calendar grid showing completion history for the last few weeks.
 */
class HabitHistoryView : public View {
public:
    HabitHistoryView();
    ~HabitHistoryView() override;

    void create(lv_obj_t* parent) override;

private:
    // --- UI Panels and State ---
    HabitHistoryStep current_step;
    uint32_t selected_habit_id = 0;
    std::string selected_habit_name;

    lv_obj_t* panel_select_habit = nullptr;
    lv_obj_t* panel_show_history = nullptr;

    // --- UI Elements ---
    // Selection panel
    lv_obj_t* habit_list_container = nullptr;
    lv_group_t* group = nullptr;
    
    // History panel
    // --- MODIFIED: Added a pointer for the color indicator circle ---
    lv_obj_t* history_color_indicator = nullptr; 
    lv_obj_t* history_title_label = nullptr;
    lv_obj_t* history_content_container = nullptr; // This will hold the calendar grid
    lv_obj_t* streak_label = nullptr;            // Label for the current streak

    // --- Style Management ---
    lv_style_t style_list_item_focused;
    lv_style_t style_category_header;
    lv_style_t style_calendar_cell;
    lv_style_t style_calendar_cell_today; // Style for the current day's cell
    bool styles_initialized = false;

    // --- UI Setup ---
    void setup_ui(lv_obj_t* parent);
    void create_selection_panel(lv_obj_t* parent);
    void create_history_panel(lv_obj_t* parent);
    void switch_to_step(HabitHistoryStep new_step);
    void init_styles();
    void reset_styles();

    // --- Logic ---
    void populate_habit_selector();
    void update_history_display();
    int calculate_streak(const std::vector<time_t>& completed_dates) const;

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

#endif // HABIT_HISTORY_VIEW_H