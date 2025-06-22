#include "sd_test_view.h"
#include "../view_manager.h"
#include "../file_explorer/file_explorer.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>
#include <sys/stat.h>

static const char *TAG = "SD_TEST_VIEW";

// State variables for the view
static lv_obj_t *view_parent = NULL;
static lv_obj_t *info_label_widget = NULL;

// State variables for the file action menu (Read, Rename, Delete)
static lv_obj_t *action_menu_container = NULL;
static lv_group_t *action_menu_group = NULL;
static char selected_item_path[256];
static lv_style_t style_action_menu_focused;

// --- Function Prototypes ---
static void create_initial_sd_view();
static void show_file_explorer();
static void on_explorer_exit();
static void on_file_or_dir_selected(const char *path);
static void on_create_action(file_item_type_t action_type, const char *current_path);
static void destroy_action_menu();
static void create_text_viewer(const char* title, char* content);


/**********************
 *  ACTION MENU LOGIC
 **********************/

static void handle_action_menu_input(button_id_t button) {
    if (!action_menu_group) return;

    if (button == BUTTON_LEFT) {
        lv_group_focus_prev(action_menu_group);
    } else if (button == BUTTON_RIGHT) {
        lv_group_focus_next(action_menu_group);
    } else if (button == BUTTON_OK) {
        lv_obj_t* selected_btn = lv_group_get_focused(action_menu_group);
        if (!selected_btn) return;

        lv_obj_t* list = lv_obj_get_parent(selected_btn);
        const char* action_text = lv_list_get_button_text(list, selected_btn);
        ESP_LOGI(TAG, "Action '%s' selected for: %s", action_text, selected_item_path);

        if (strcmp(action_text, "Leer") == 0) {
            char* file_content = NULL;
            size_t file_size = 0;
            const char* filename = strrchr(selected_item_path, '/') ? strrchr(selected_item_path, '/') + 1 : selected_item_path;

            if (sd_manager_read_file(selected_item_path, &file_content, &file_size)) {
                // The text viewer will handle freeing the file_content buffer
                create_text_viewer(filename, file_content);
            } else {
                if(file_content) free(file_content);
            }
        }
        else if (strcmp(action_text, "Renombrar") == 0) {
            char new_path[256] = {0};
            const char* dir_end = strrchr(selected_item_path, '/');
            size_t dir_len = (dir_end) ? (dir_end - selected_item_path) + 1 : 0;
            if(dir_len > 0) strncpy(new_path, selected_item_path, dir_len);

            time_t now = time(NULL);
            struct tm timeinfo;
            localtime_r(&now, &timeinfo);
            char basename[32];
            strftime(basename, sizeof(basename), "%Y%m%d_%H%M%S", &timeinfo);

            const char* ext = strrchr(selected_item_path, '.');
            snprintf(new_path + dir_len, sizeof(new_path) - dir_len, "%s%s", basename, ext ? ext : "");

            ESP_LOGI(TAG, "Renaming '%s' -> '%s'", selected_item_path, new_path);
            sd_manager_rename_item(selected_item_path, new_path);
            destroy_action_menu();
        }
        else if (strcmp(action_text, "Eliminar") == 0) {
            sd_manager_delete_item(selected_item_path);
            destroy_action_menu();
        }
    } else if (button == BUTTON_CANCEL) {
        destroy_action_menu();
    }
}

static void handle_action_menu_ok()     { handle_action_menu_input(BUTTON_OK);     }
static void handle_action_menu_cancel() { handle_action_menu_input(BUTTON_CANCEL); }
static void handle_action_menu_left()   { handle_action_menu_input(BUTTON_LEFT);   }
static void handle_action_menu_right()  { handle_action_menu_input(BUTTON_RIGHT);  }

static void create_action_menu(const char* path) {
    if (action_menu_container) return;
    strncpy(selected_item_path, path, sizeof(selected_item_path) - 1);

    file_explorer_set_input_active(false); // Pause file explorer input

    lv_style_init(&style_action_menu_focused);
    lv_style_set_bg_color(&style_action_menu_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_opa(&style_action_menu_focused, LV_OPA_COVER);

    action_menu_container = lv_obj_create(view_parent);
    lv_obj_remove_style_all(action_menu_container);
    lv_obj_set_size(action_menu_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(action_menu_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(action_menu_container, LV_OPA_50, 0);

    lv_obj_t* menu_box = lv_obj_create(action_menu_container);
    lv_obj_set_width(menu_box, lv_pct(80));
    lv_obj_set_height(menu_box, LV_SIZE_CONTENT);
    lv_obj_center(menu_box);

    lv_obj_t* list = lv_list_create(menu_box);
    lv_obj_set_size(list, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_center(list);

    action_menu_group = lv_group_create();
    const char* actions[][2] = {{LV_SYMBOL_EYE_OPEN, "Leer"}, {LV_SYMBOL_EDIT, "Renombrar"}, {LV_SYMBOL_TRASH, "Eliminar"}};
    for(auto const& action : actions) {
        lv_obj_t* btn = lv_list_add_button(list, action[0], action[1]);
        lv_obj_add_style(btn, &style_action_menu_focused, LV_STATE_FOCUSED);
        lv_group_add_obj(action_menu_group, btn);
    }

    lv_group_set_default(action_menu_group);
    lv_group_focus_obj(lv_obj_get_child(list, 0));

    button_manager_register_view_handler(BUTTON_OK, handle_action_menu_ok);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_action_menu_cancel);
    button_manager_register_view_handler(BUTTON_LEFT, handle_action_menu_left);
    button_manager_register_view_handler(BUTTON_RIGHT, handle_action_menu_right);
}

static void destroy_action_menu() {
    if (action_menu_container) {
        if(action_menu_group) {
             lv_group_del(action_menu_group);
             action_menu_group = NULL;
        }
        lv_obj_del(action_menu_container);
        action_menu_container = NULL;

        file_explorer_set_input_active(true);
        file_explorer_refresh();
    }
}


/*********************
 *  TEXT VIEWER LOGIC
 *********************/

static void text_viewer_delete_cb(lv_event_t * e) {
    char* text_content = (char*)lv_event_get_user_data(e);
    if (text_content) {
        free(text_content);
    }
    destroy_action_menu(); // Also destroy the action menu that opened the viewer
}

static void handle_cancel_from_viewer() {
    show_file_explorer(); // This will clean the screen, triggering the delete callback
}

static void create_text_viewer(const char* title, char* content) {
    button_manager_unregister_view_handlers();
    lv_obj_clean(view_parent);

    lv_obj_t* main_cont = lv_obj_create(view_parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, lv_theme_get_font_large(title_label), 0);
    lv_obj_set_style_margin_bottom(title_label, 5, 0);

    lv_obj_t* text_area = lv_textarea_create(main_cont);
    lv_obj_set_size(text_area, lv_pct(95), lv_pct(85));
    lv_textarea_set_text(text_area, content);
    lv_obj_add_event_cb(text_area, text_viewer_delete_cb, LV_EVENT_DELETE, content);

    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_from_viewer);
}


/********************************
 *  FILE EXPLORER EVENT HANDLERS
 ********************************/

static void on_file_or_dir_selected(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0 && !S_ISDIR(st.st_mode)) {
        create_action_menu(path);
    } else {
        ESP_LOGD(TAG, "Directory selected, explorer will handle it.");
    }
}

static void on_create_action(file_item_type_t action_type, const char *current_path) {
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


/***********************
 *  MAIN VIEW LOGIC
 ***********************/

static void show_file_explorer() {
    lv_obj_clean(view_parent);

    lv_obj_t* main_cont = lv_obj_create(view_parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, "SD Explorer");
    lv_obj_set_style_text_font(title_label, lv_theme_get_font_large(title_label), 0);
    lv_obj_set_style_margin_bottom(title_label, 10, 0);

    lv_obj_t* explorer_container = lv_obj_create(main_cont);
    lv_obj_remove_style_all(explorer_container);
    lv_obj_set_size(explorer_container, lv_pct(95), lv_pct(85));

    file_explorer_create(
        explorer_container,
        sd_manager_get_mount_point(),
        on_file_or_dir_selected,
        on_create_action,
        on_explorer_exit
    );
}

static void on_explorer_exit() {
    file_explorer_destroy();
    create_initial_sd_view();
}

static void handle_initial_ok_press() {
    sd_manager_unmount();
    if (sd_manager_mount()) {
        show_file_explorer();
    } else if (info_label_widget) {
        lv_label_set_text(info_label_widget, "Error mounting SD card.\n\nCheck card and press OK\nto retry.");
    }
}

static void handle_initial_cancel_press() {
    view_manager_load_view(VIEW_ID_MENU);
}

static void create_initial_sd_view() {
    lv_obj_clean(view_parent);

    lv_obj_t* title = lv_label_create(view_parent);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_label_set_text(title, "SD Card Test");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    info_label_widget = lv_label_create(view_parent);
    lv_obj_set_style_text_align(info_label_widget, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label_widget);
    lv_label_set_text(info_label_widget, "Press OK to open\nthe file explorer");

    button_manager_register_view_handler(BUTTON_OK, handle_initial_ok_press);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_initial_cancel_press);
    button_manager_register_view_handler(BUTTON_LEFT, NULL);
    button_manager_register_view_handler(BUTTON_RIGHT, NULL);
}

void sd_test_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating SD Test View");
    view_parent = parent;
    create_initial_sd_view();
}