#ifndef HABIT_ADD_VIEW_H
#define HABIT_ADD_VIEW_H

#include "../view.h"
#include "../view_manager.h"
#include "lvgl.h"
#include <vector>
#include <string>

/**
 * @brief A view for creating a new habit.
 *
 * This view allows the user to select a category, pick a color, and
 * create a new habit, which is then saved by the HabitDataManager.
 */
class HabitAddView : public View {
public:
    HabitAddView();
    ~HabitAddView() override;

    void create(lv_obj_t* parent) override;

private:
    // --- UI and State Members ---
    lv_group_t* group = nullptr;
    lv_obj_t* category_roller = nullptr;
    lv_obj_t* color_palette_container = nullptr;
    lv_obj_t* name_label = nullptr;
    lv_obj_t* refresh_name_button = nullptr;

    lv_obj_t* create_button = nullptr;

    lv_style_t style_focused;
    lv_style_t style_color_cell_focused;
    bool styles_initialized = false;
    
    std::string selected_color_hex;
    std::string current_habit_name;
    std::vector<std::string> preset_colors;

    // --- UI Setup ---
    void setup_ui(lv_obj_t* parent);
    void populate_category_roller();
    void create_color_palette(lv_obj_t* parent);
    void init_styles();
    void reset_styles();

    // --- Logic ---
    void update_habit_name();

    // --- Button Handling ---
    void setup_button_handlers();
    void on_ok_press();
    void on_cancel_press();
    void on_nav_press(bool next);
    
    // --- Event Handlers ---
    static void color_cell_event_cb(lv_event_t * e);
    static void show_creation_toast_cb(lv_timer_t * timer);
    static void focus_changed_cb(lv_group_t * group); // <-- NEW: Callback for focus changes

    // Static Callbacks for Button Manager
    static void handle_ok_press_cb(void* user_data);
    static void handle_cancel_press_cb(void* user_data);
    static void handle_left_press_cb(void* user_data);
    static void handle_right_press_cb(void* user_data);
};

#endif // HABIT_ADD_VIEW_H