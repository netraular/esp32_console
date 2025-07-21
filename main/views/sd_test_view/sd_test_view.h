#ifndef SD_TEST_VIEW_H
#define SD_TEST_VIEW_H

#include "../view.h"
#include "components/file_explorer/file_explorer.h"
#include "controllers/button_manager/button_manager.h"
#include "lvgl.h"

/**
 * @brief Represents the SD Card Test view.
 *
 * This view provides a comprehensive interface for interacting with the SD card,
 * including a file explorer, file viewer, and file management actions (create,
 * rename, delete).
 */
class SdTestView : public View {
public:
    SdTestView();
    ~SdTestView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- UI Widgets ---
    lv_obj_t* info_label_widget = nullptr;
    lv_obj_t* action_menu_container = nullptr;
    lv_obj_t* text_viewer_obj = nullptr;
    lv_obj_t* file_explorer_host_container = nullptr;

    // --- State ---
    lv_group_t* action_menu_group = nullptr;
    lv_style_t style_action_menu_focused;
    bool styles_initialized = false;
    char selected_item_path[256] = {0};

    // --- UI Setup & Logic ---
    void create_initial_view();
    void show_file_explorer();
    void create_action_menu(const char* path);
    void destroy_action_menu(bool refresh_explorer);
    void init_action_menu_styles();
    void reset_action_menu_styles();
    void setup_initial_button_handlers();
    
    // --- Instance Methods for Button/Component Actions ---
    void on_initial_ok_press();
    void on_initial_cancel_press();
    
    void on_file_selected(const char* path);
    void on_file_long_pressed(const char* path);
    void on_create_action(file_item_type_t action_type, const char* current_path);
    void on_explorer_exit();
    
    void on_action_menu_ok();
    void on_action_menu_cancel();
    void on_action_menu_nav(bool is_next);
    
    void on_text_viewer_exit();

    // --- Static Callbacks for Button Manager (Bridge to instance methods) ---
    static void initial_ok_press_cb(void* user_data);
    static void initial_cancel_press_cb(void* user_data);
    static void action_menu_ok_cb(void* user_data);
    static void action_menu_cancel_cb(void* user_data);
    static void action_menu_left_cb(void* user_data);
    static void action_menu_right_cb(void* user_data);

    // --- Static Callbacks for C Components (Bridge to instance methods) ---
    static void file_selected_cb_c(const char* path, void* user_data);
    static void file_long_pressed_cb_c(const char* path, void* user_data);
    static void create_action_cb_c(file_item_type_t type, const char* path, void* user_data);
    static void explorer_exit_cb_c(void* user_data);
    static void text_viewer_exit_cb_c(void* user_data);
    static void explorer_cleanup_event_cb(lv_event_t * e);
};

#endif // SD_TEST_VIEW_H