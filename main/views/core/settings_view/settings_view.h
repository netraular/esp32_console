#ifndef SETTINGS_VIEW_H
#define SETTINGS_VIEW_H

#include "views/view.h"
#include "lvgl.h"
#include <string>

/**
 * @brief View for displaying system and user settings.
 *
 * This view shows a read-only list of various settings grouped by category.
 * It serves as a central place to view device information and user preferences.
 */
class SettingsView : public View {
public:
    SettingsView();
    ~SettingsView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- UI Widgets ---
    lv_obj_t* list = nullptr;
    lv_group_t* group = nullptr;
    lv_style_t style_focused;
    bool styles_initialized = false;

    // --- Private Methods for Setup ---
    void setup_ui(lv_obj_t* parent);
    void populate_settings_list();
    void setup_button_handlers();
    void init_styles();
    void reset_styles();

    // --- Private Methods for UI Logic ---
    void add_setting_item(const char* name, const std::string& value);
    
    // --- Instance Methods for Button Actions ---
    void on_ok_press();
    void on_cancel_press();
    void on_nav_press(bool is_next);

    // --- Static Callbacks (Bridge to C-style APIs) ---
    static void ok_press_cb(void* user_data);
    static void cancel_press_cb(void* user_data);
    static void left_press_cb(void* user_data);
    static void right_press_cb(void* user_data);
};

#endif // SETTINGS_VIEW_H