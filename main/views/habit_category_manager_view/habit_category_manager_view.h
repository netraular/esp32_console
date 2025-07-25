#ifndef HABIT_CATEGORY_MANAGER_VIEW_H
#define HABIT_CATEGORY_MANAGER_VIEW_H

#include "../view.h"
#include "../view_manager.h"
#include "lvgl.h"
#include <vector>
#include <string>

/**
 * @brief View for creating, viewing, and deleting habit categories using a simple slot-based UI.
 * This view communicates with the HabitDataManager to display and modify data.
 */
class HabitCategoryManagerView : public View {
public:
    HabitCategoryManagerView();
    ~HabitCategoryManagerView() override;

    void create(lv_obj_t* parent) override;

private:
    // --- UI and State Members ---
    lv_obj_t* category_container = nullptr;
    lv_group_t* main_group = nullptr;

    lv_obj_t* action_menu_container = nullptr;
    lv_group_t* action_menu_group = nullptr;

    lv_style_t style_focused;
    bool styles_initialized = false;
    uint32_t selected_category_id = 0; // Stores the ID of the selected category for the action menu

    // --- UI Setup & Management ---
    void setup_ui(lv_obj_t* parent);
    void repopulate_category_slots();
    void init_styles();
    void reset_styles();

    // --- Action Menu Management ---
    void create_action_menu(uint32_t category_id);
    void destroy_action_menu();
    void set_main_input_active(bool active);

    // --- Button Handling ---
    void setup_button_handlers();

    // Instance Methods for Button Actions
    void on_ok_press();
    void on_cancel_press();
    void on_nav_press(bool next);

    void on_action_menu_ok();
    void on_action_menu_cancel();
    void on_action_menu_nav(bool next);

    // Static Callbacks for Button Manager
    static void handle_ok_press_cb(void* user_data);
    static void handle_cancel_press_cb(void* user_data);
    static void handle_left_press_cb(void* user_data);
    static void handle_right_press_cb(void* user_data);

    static void handle_action_menu_ok_cb(void* user_data);
    static void handle_action_menu_cancel_cb(void* user_data);
    static void handle_action_menu_nav_cb(void* user_data);
};

#endif // HABIT_CATEGORY_MANAGER_VIEW_H