#ifndef SETTINGS_VIEW_H
#define SETTINGS_VIEW_H

#include "views/view.h"
#include "lvgl.h"
#include <string>

/**
 * @brief View for displaying system and user settings.
 *
 * This view uses a colorful, icon-driven, card-based layout to present
 * device information and user preferences in a clear and visually
 * appealing way.
 */
class SettingsView : public View {
public:
    SettingsView();
    ~SettingsView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- UI Widgets ---
    lv_obj_t* m_content_area = nullptr;
    lv_obj_t* m_first_interactive_item = nullptr; // To handle scroll-to-top
    lv_group_t* m_group = nullptr;
    
    // --- LVGL Styles ---
    lv_style_t m_style_card;
    lv_style_t m_style_card_focused;
    lv_style_t m_style_section_title;
    lv_style_t m_style_divider;
    lv_style_t m_style_icon;
    bool m_styles_initialized = false;

    // --- Private Methods for Setup ---
    void setup_ui(lv_obj_t* parent);
    void populate_settings_list();
    void setup_button_handlers();
    void init_styles();
    void reset_styles();

    // --- Private Methods for UI Logic ---
    void create_section_header(const char* title);
    void create_setting_card(const char* icon, const char* name, const std::string& value);
    
    // --- Instance Methods for Button Actions ---
    void on_cancel_press();
    void on_nav_press(bool is_next);

    // --- Static Callbacks (Bridge to C-style APIs) ---
    static void cancel_press_cb(void* user_data);
    static void left_press_cb(void* user_data);
    static void right_press_cb(void* user_data);
};

#endif // SETTINGS_VIEW_H