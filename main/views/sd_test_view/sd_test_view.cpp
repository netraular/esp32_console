#include "sd_test_view.h"
#include "../view_manager.h"
#include "components/text_viewer/text_viewer.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "esp_log.h"
#include <cstring>
#include <time.h>
#include <sys/stat.h>

static const char *TAG = "SD_TEST_VIEW";

// --- Lifecycle Methods ---
SdTestView::SdTestView() {
    ESP_LOGI(TAG, "SdTestView constructed");
}

SdTestView::~SdTestView() {
    ESP_LOGI(TAG, "SdTestView destructed, cleaning up resources.");
    
    destroy_action_menu(false);
    reset_action_menu_styles();
}

void SdTestView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating SD Test View UI");
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);
    
    create_initial_view();
}

// --- UI Setup & Logic ---

void SdTestView::setup_initial_button_handlers() {
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, SdTestView::initial_ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, SdTestView::initial_cancel_press_cb, true, this);
}

void SdTestView::create_initial_view() {
    lv_obj_clean(container);

    lv_obj_t* title = lv_label_create(container);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_label_set_text(title, "SD Card Test");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    info_label_widget = lv_label_create(container);
    lv_obj_set_style_text_align(info_label_widget, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label_widget);
    lv_label_set_text(info_label_widget, "Press OK to open\nthe file explorer.");

    setup_initial_button_handlers();
}

void SdTestView::show_file_explorer() {
    lv_obj_clean(container);

    lv_obj_t* main_cont = lv_obj_create(container);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, "SD Explorer");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);

    file_explorer_host_container = lv_obj_create(main_cont);
    lv_obj_remove_style_all(file_explorer_host_container);
    lv_obj_set_size(file_explorer_host_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(file_explorer_host_container, 1);
    lv_obj_clear_flag(file_explorer_host_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(file_explorer_host_container, explorer_cleanup_event_cb, LV_EVENT_DELETE, this);

    // --- MODIFIED --- Pass `this` as user_data.
    file_explorer_create(file_explorer_host_container, sd_manager_get_mount_point(), 
                         file_selected_cb_c, file_long_pressed_cb_c, 
                         create_action_cb_c, explorer_exit_cb_c, this);
}

void SdTestView::create_action_menu(const char* path) {
    if (action_menu_container) return; // Already exists
    
    ESP_LOGI(TAG, "Creating action menu for: %s", path);
    strncpy(this->selected_item_path, path, sizeof(this->selected_item_path) - 1);

    file_explorer_set_input_active(false);
    init_action_menu_styles();

    action_menu_container = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(action_menu_container);
    lv_obj_set_size(action_menu_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(action_menu_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(action_menu_container, LV_OPA_50, 0);

    lv_obj_t* list = lv_list_create(action_menu_container);
    lv_obj_set_size(list, 180, LV_SIZE_CONTENT);
    lv_obj_center(list);

    action_menu_group = lv_group_create();
    const char* actions[][2] = {{"View", LV_SYMBOL_EYE_OPEN}, {"Rename", LV_SYMBOL_EDIT}, {"Delete", LV_SYMBOL_TRASH}};
    for (auto const& action : actions) {
        lv_obj_t* btn = lv_list_add_button(list, action[1], action[0]);
        lv_obj_add_style(btn, &style_action_menu_focused, LV_STATE_FOCUSED);
        lv_group_add_obj(action_menu_group, btn);
    }

    if (lv_obj_get_child_count(list) > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, action_menu_ok_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, action_menu_cancel_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, action_menu_left_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, action_menu_right_cb, true, this);
}

void SdTestView::destroy_action_menu(bool refresh_explorer) {
    if (!action_menu_container) return;

    ESP_LOGD(TAG, "Destroying action menu.");
    if (action_menu_group) {
        lv_group_del(action_menu_group);
        action_menu_group = nullptr;
    }
    lv_obj_del(action_menu_container);
    action_menu_container = nullptr;

    file_explorer_set_input_active(true);
    if (refresh_explorer) {
        file_explorer_refresh();
    }
}

// --- Instance Methods for Actions ---

void SdTestView::on_initial_ok_press() {
    sd_manager_unmount();
    if (sd_manager_mount()) {
        show_file_explorer();
    } else if (info_label_widget) {
        lv_label_set_text(info_label_widget, "Error mounting SD card.\n\nCheck card and press OK\nto retry.");
    }
}

void SdTestView::on_initial_cancel_press() {
    view_manager_load_view(VIEW_ID_MENU);
}

void SdTestView::on_file_selected(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) { // It's a regular file
        char* file_content = nullptr;
        size_t file_size = 0;
        const char* filename = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;

        if (sd_manager_read_file(path, &file_content, &file_size)) {
            destroy_action_menu(false);
            lv_obj_clean(container);
            // --- MODIFIED --- Pass `this` as user_data.
            text_viewer_obj = text_viewer_create(container, filename, file_content, text_viewer_exit_cb_c, this);
        } else {
            ESP_LOGE(TAG, "Failed to read file: %s", path);
            if (file_content) free(file_content);
            destroy_action_menu(true);
        }
    }
}

void SdTestView::on_file_long_pressed(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
        create_action_menu(path);
    }
}

void SdTestView::on_create_action(file_item_type_t action_type, const char* current_path) {
    char full_path[256];
    char basename[32];
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(basename, sizeof(basename), "%H%M%S", &timeinfo);

    if (action_type == ITEM_TYPE_ACTION_CREATE_FILE) {
        snprintf(full_path, sizeof(full_path), "%s/%s.txt", current_path, basename);
        if (sd_manager_create_file(full_path)) {
            sd_manager_write_file(full_path, "New file.");
        }
    } else if (action_type == ITEM_TYPE_ACTION_CREATE_FOLDER) {
        snprintf(full_path, sizeof(full_path), "%s/%s_dir", current_path, basename);
        sd_manager_create_directory(full_path);
    }
    file_explorer_refresh();
}

void SdTestView::on_explorer_exit() {
    create_initial_view();
}

void SdTestView::on_action_menu_ok() {
    if (!action_menu_group) return;
    lv_obj_t* focused_btn = lv_group_get_focused(action_menu_group);
    if (!focused_btn) return;
    
    const char* action_text = lv_list_get_button_text(lv_obj_get_parent(focused_btn), focused_btn);
    ESP_LOGI(TAG, "Action '%s' selected for: %s", action_text, selected_item_path);

    if (strcmp(action_text, "View") == 0) {
        on_file_selected(selected_item_path);
    } else if (strcmp(action_text, "Delete") == 0) {
        sd_manager_delete_item(selected_item_path);
        destroy_action_menu(true);
    } else if (strcmp(action_text, "Rename") == 0) {
        char new_path[256] = {0};
        const char* dir_end = strrchr(selected_item_path, '/');
        size_t dir_len = (dir_end) ? (dir_end - selected_item_path) + 1 : 0;
        if (dir_len > 0) strncpy(new_path, selected_item_path, dir_len);

        char basename[32];
        time_t now = time(NULL);
        strftime(basename, sizeof(basename), "%Y%m%d_%H%M%S", localtime(&now));
        
        const char* ext = strrchr(selected_item_path, '.');
        snprintf(new_path + dir_len, sizeof(new_path) - dir_len, "%s%s", basename, ext ? ext : "");

        ESP_LOGI(TAG, "Renaming '%s' -> '%s'", selected_item_path, new_path);
        sd_manager_rename_item(selected_item_path, new_path);
        destroy_action_menu(true);
    }
}

void SdTestView::on_action_menu_cancel() {
    destroy_action_menu(true);
}

void SdTestView::on_action_menu_nav(bool is_next) {
    if (!action_menu_group) return;
    if (is_next) lv_group_focus_next(action_menu_group);
    else lv_group_focus_prev(action_menu_group);
}

void SdTestView::on_text_viewer_exit() {
    text_viewer_obj = nullptr;
    show_file_explorer();
}

void SdTestView::init_action_menu_styles() {
    if (styles_initialized) return;
    lv_style_init(&style_action_menu_focused);
    lv_style_set_bg_color(&style_action_menu_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_color(&style_action_menu_focused, lv_color_white());
    styles_initialized = true;
}
void SdTestView::reset_action_menu_styles() {
    if (!styles_initialized) return;
    lv_style_reset(&style_action_menu_focused);
    styles_initialized = false;
}

// --- Static Callbacks for Button Manager (Pass `this`) ---
void SdTestView::initial_ok_press_cb(void* user_data) { static_cast<SdTestView*>(user_data)->on_initial_ok_press(); }
void SdTestView::initial_cancel_press_cb(void* user_data) { static_cast<SdTestView*>(user_data)->on_initial_cancel_press(); }
void SdTestView::action_menu_ok_cb(void* user_data) { static_cast<SdTestView*>(user_data)->on_action_menu_ok(); }
void SdTestView::action_menu_cancel_cb(void* user_data) { static_cast<SdTestView*>(user_data)->on_action_menu_cancel(); }
void SdTestView::action_menu_left_cb(void* user_data) { static_cast<SdTestView*>(user_data)->on_action_menu_nav(false); }
void SdTestView::action_menu_right_cb(void* user_data) { static_cast<SdTestView*>(user_data)->on_action_menu_nav(true); }

// --- Static Callbacks for C Components (Use `user_data` to get `this`) ---
void SdTestView::file_selected_cb_c(const char* path, void* user_data) { if (user_data) static_cast<SdTestView*>(user_data)->on_file_selected(path); }
void SdTestView::file_long_pressed_cb_c(const char* path, void* user_data) { if (user_data) static_cast<SdTestView*>(user_data)->on_file_long_pressed(path); }
void SdTestView::create_action_cb_c(file_item_type_t type, const char* path, void* user_data) { if (user_data) static_cast<SdTestView*>(user_data)->on_create_action(type, path); }
void SdTestView::explorer_exit_cb_c(void* user_data) { if (user_data) static_cast<SdTestView*>(user_data)->on_explorer_exit(); }
void SdTestView::text_viewer_exit_cb_c(void* user_data) { if (user_data) static_cast<SdTestView*>(user_data)->on_text_viewer_exit(); }

void SdTestView::explorer_cleanup_event_cb(lv_event_t * e) {
    ESP_LOGD(TAG, "Explorer host container deleted. Calling file_explorer_destroy().");
    file_explorer_destroy();
    auto* instance = static_cast<SdTestView*>(lv_event_get_user_data(e));
    if (instance) {
        instance->file_explorer_host_container = nullptr;
    }
}