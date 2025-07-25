#ifndef HABIT_ADD_VIEW_H
#define HABIT_ADD_VIEW_H

#include "../view.h"
#include "../view_manager.h"
#include "lvgl.h"
#include <vector>
#include <string>

// Defines the steps in the habit creation wizard
enum class HabitAddStep {
    STEP_CATEGORY,
    STEP_NAME,
    STEP_COLOR_CREATE
};

/**
 * @brief A view for creating a new habit, implemented as a 3-step wizard.
 *
 * This view manages three distinct UI panels within a single class, guiding the
 * user through selecting a category, setting a name, and choosing a color
 * before creating the habit.
 */
class HabitAddView : public View {
public:
    HabitAddView();
    ~HabitAddView() override;

    void create(lv_obj_t* parent) override;

private:
    // --- UI Panels and Groups ---
    lv_obj_t* panel_category = nullptr;
    lv_obj_t* panel_name = nullptr;
    lv_obj_t* panel_color_create = nullptr;

    lv_group_t* name_panel_group = nullptr;
    lv_group_t* color_panel_group = nullptr;

    HabitAddStep current_step;

    // --- UI Elements ---
    lv_obj_t* category_roller = nullptr;
    lv_obj_t* name_label = nullptr;
    lv_obj_t* btn_generate_name = nullptr;
    lv_obj_t* btn_next_name = nullptr;
    lv_obj_t* btn_create_habit = nullptr;

    // --- Style Management ---
    lv_style_t style_button_focused;      // For focused buttons on the name screen
    lv_style_t style_color_cell_focused;  // For focused color cells
    lv_style_t style_color_cell_checked;  // For the selected color cell
    bool styles_initialized = false;

    // --- Data Storage During Creation ---
    uint32_t selected_category_id = 0;
    std::string current_habit_name;
    std::string selected_color_hex;
    std::vector<std::string> preset_colors;
    bool color_is_selected = false;

    // --- UI Setup ---
    void setup_ui(lv_obj_t* parent);
    void create_category_panel(lv_obj_t* parent);
    void create_name_panel(lv_obj_t* parent);
    void create_color_create_panel(lv_obj_t* parent);
    void populate_category_roller();
    void init_styles();
    void reset_styles();
    void switch_to_step(HabitAddStep new_step);

    // --- Logic ---
    void update_habit_name();
    void handle_create_habit();
    void show_creation_toast();
    void go_to_next_step();
    void go_to_previous_step();

    // --- Button and Event Handling ---
    void setup_button_handlers();
    void on_ok_press();
    void on_cancel_press();
    void on_left_press();
    void on_right_press();

    // --- Static Callbacks ---
    static void return_to_manager_cb(lv_timer_t * timer);
    static void handle_ok_press_cb(void* user_data);
    static void handle_cancel_press_cb(void* user_data);
    static void handle_left_press_cb(void* user_data);
    static void handle_right_press_cb(void* user_data);
};

#endif // HABIT_ADD_VIEW_H