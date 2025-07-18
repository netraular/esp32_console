#include "sd_test_view.h"
#include "../view_manager.h"
#include "components/file_explorer/file_explorer.h"
#include "components/text_viewer/text_viewer.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>
#include <sys/stat.h>

static const char *TAG = "SD_TEST_VIEW";

// --- State variables for the view ---
static lv_obj_t *view_container = NULL;      // The main container for the entire view
static lv_obj_t *info_label_widget = NULL; // Label on the initial screen

// --- State variables for the file action pop-up menu ---
static lv_obj_t *action_menu_container = NULL;
static lv_group_t *action_menu_group = NULL;
static char selected_item_path[256];
static lv_style_t style_action_menu_focused;

// --- Forward declarations ---
static void create_initial_sd_view();
static void show_file_explorer();
static void on_explorer_exit();
static void on_file_selected(const char *path);
static void on_file_long_pressed(const char *path);
static void on_create_action(file_item_type_t action_type, const char *current_path);
static void destroy_action_menu(bool refresh_file_explorer_after);
static void on_text_viewer_exit();
static void create_action_menu(const char *path);


/***************************************************
 *  VIEW LIFECYCLE & CLEANUP
 ***************************************************/

/**
 * @brief Master cleanup handler for the entire SD Test View.
 *
 * This function is attached to the view's main container and is guaranteed to be called
 * when the container is deleted (e.g., when the view_manager switches to another view).
 * It is responsible for freeing any non-LVGL resources, like the input group.
 */
static void sd_view_cleanup_event_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "SD Test View is being deleted, cleaning up all resources.");

    // Clean up the non-LVGL input group resource if it exists.
    if (action_menu_group) {
        if (lv_group_get_default() == action_menu_group) {
            lv_group_set_default(NULL); // Avoid a dangling default group pointer
        }
        lv_group_del(action_menu_group);
        ESP_LOGD(TAG, "Action menu group deleted.");
    }

    // Reset all static pointers to ensure a clean state for the next time the view is created.
    view_container = NULL;
    info_label_widget = NULL;
    action_menu_container = NULL;
    action_menu_group = NULL;
}

/**
 * @brief Callback for when the file explorer's container is deleted.
 *
 * This is a critical link to ensure the file_explorer component can free its internal
 * resources (like timers or memory) when its UI is removed from the screen.
 */
static void explorer_cleanup_event_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "Explorer container deleted. Calling file_explorer_destroy().");
    file_explorer_destroy();
}

/***************************************************
 *  ACTION MENU BUTTON HANDLERS
 ***************************************************/
static void handle_action_menu_left_press(void *user_data) {
    if (action_menu_group) lv_group_focus_prev(action_menu_group);
}
static void handle_action_menu_right_press(void *user_data) {
    if (action_menu_group) lv_group_focus_next(action_menu_group);
}
static void handle_action_menu_cancel_press(void *user_data) {
    destroy_action_menu(true);
}
static void handle_action_menu_ok_press(void *user_data) {
    if (!action_menu_group) return;

    lv_obj_t *selected_btn = lv_group_get_focused(action_menu_group);
    if (!selected_btn) return;

    // --- FIX ---
    // The focused object is a button within a list. To get its text, we need
    // the list object (its parent) and the button itself.
    lv_obj_t *list = lv_obj_get_parent(selected_btn);
    const char *action_text = lv_list_get_button_text(list, selected_btn);

    ESP_LOGI(TAG, "Action '%s' selected for: %s", action_text, selected_item_path);

    if (strcmp(action_text, "Read") == 0) {
        on_file_selected(selected_item_path); // Reuse the open-file function
    } else if (strcmp(action_text, "Rename") == 0) {
        char new_path[256] = {0};
        const char *dir_end = strrchr(selected_item_path, '/');
        size_t dir_len = (dir_end) ? (dir_end - selected_item_path) + 1 : 0;
        if (dir_len > 0) strncpy(new_path, selected_item_path, dir_len);

        char basename[32];
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        strftime(basename, sizeof(basename), "%Y%m%d_%H%M%S", &timeinfo);

        const char *ext = strrchr(selected_item_path, '.');
        snprintf(new_path + dir_len, sizeof(new_path) - dir_len, "%s%s", basename, ext ? ext : "");

        ESP_LOGI(TAG, "Renaming '%s' -> '%s'", selected_item_path, new_path);
        sd_manager_rename_item(selected_item_path, new_path);
        destroy_action_menu(true);
    } else if (strcmp(action_text, "Delete") == 0) {
        sd_manager_delete_item(selected_item_path);
        destroy_action_menu(true);
    }
}

/***************************************************
 *  ACTION MENU LOGIC
 ***************************************************/
static void create_action_menu(const char *path) {
    if (action_menu_container) {
        destroy_action_menu(false);
    }
    ESP_LOGI(TAG, "Creating action menu for: %s", path);
    strncpy(selected_item_path, path, sizeof(selected_item_path) - 1);

    file_explorer_set_input_active(false); // Deactivate explorer input

    lv_style_init(&style_action_menu_focused);
    lv_style_set_bg_color(&style_action_menu_focused, lv_palette_main(LV_PALETTE_BLUE));

    action_menu_container = lv_obj_create(view_container); // Parent to main container
    lv_obj_remove_style_all(action_menu_container);
    lv_obj_set_size(action_menu_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(action_menu_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(action_menu_container, LV_OPA_50, 0);
    lv_obj_clear_flag(action_menu_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *list = lv_list_create(action_menu_container);
    lv_obj_set_size(list, lv_pct(80), LV_SIZE_CONTENT);
    lv_obj_center(list);

    action_menu_group = lv_group_create();
    const char *actions[][2] = {{LV_SYMBOL_EYE_OPEN, "Read"}, {LV_SYMBOL_EDIT, "Rename"}, {LV_SYMBOL_TRASH, "Delete"}};
    for (auto const &action : actions) {
        lv_obj_t *btn = lv_list_add_button(list, action[0], action[1]);
        lv_obj_add_style(btn, &style_action_menu_focused, LV_STATE_FOCUSED);
        lv_group_add_obj(action_menu_group, btn);
    }

    lv_group_set_default(action_menu_group);
    if (lv_obj_get_child_count(list) > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_action_menu_ok_press, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_action_menu_cancel_press, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_action_menu_left_press, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_action_menu_right_press, true, nullptr);
}

static void destroy_action_menu(bool refresh_file_explorer_after) {
    if (action_menu_container) {
        ESP_LOGI(TAG, "Destroying action menu. Refresh explorer: %s", refresh_file_explorer_after ? "yes" : "no");
        if (action_menu_group) {
            if (lv_group_get_default() == action_menu_group) lv_group_set_default(NULL);
            lv_group_del(action_menu_group);
            action_menu_group = NULL;
        }
        lv_obj_del(action_menu_container);
        action_menu_container = NULL;
        file_explorer_set_input_active(true);
        if (refresh_file_explorer_after) {
            file_explorer_refresh();
        }
    }
}

/***************************************************
 *  COMPONENT & VIEW EVENT HANDLERS
 ***************************************************/

// Callback from the text_viewer when the user exits.
static void on_text_viewer_exit() {
    show_file_explorer(); // Return to the file explorer view
}

// Called on a normal tap on a file. Action: open the file in the text viewer.
static void on_file_selected(const char *path) {
    struct stat st;
    // Ensure it's a file before trying to read it.
    if (stat(path, &st) == 0 && !S_ISDIR(st.st_mode)) {
        char *file_content = NULL;
        size_t file_size = 0;
        const char *filename = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;

        if (sd_manager_read_file(path, &file_content, &file_size)) {
            destroy_action_menu(false); // Destroy the menu if it was open
            lv_obj_clean(view_container);  // Clean the screen (will trigger explorer_cleanup_event_cb)
            // Create the text viewer. It will take ownership of the file_content pointer.
            text_viewer_create(view_container, filename, file_content, on_text_viewer_exit);
        } else {
            ESP_LOGE(TAG, "Failed to read file for viewer: %s", path);
            if (file_content) free(file_content); // Free memory if reading failed but allocated buffer
            destroy_action_menu(true);      // Close menu and refresh in case of error
        }
    }
}

// Called on a long press on a file. Action: open the context menu (Read, Rename, Delete).
static void on_file_long_pressed(const char *path) {
    struct stat st;
    // Only show the menu for files, not directories.
    if (stat(path, &st) == 0 && !S_ISDIR(st.st_mode)) {
        create_action_menu(path);
    }
}

// Callback for when a create action is selected in the explorer.
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

// Callback for when the user exits the explorer (by navigating back from the root).
static void on_explorer_exit() {
    create_initial_sd_view();
}

/***************************************************
 *  MAIN VIEW LOGIC
 ***************************************************/
static void show_file_explorer() {
    if (action_menu_container) {
        destroy_action_menu(false);
    }
    lv_obj_clean(view_container);

    lv_obj_t *main_cont = lv_obj_create(view_container);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(main_cont, 5, 0);

    lv_obj_t *title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, "SD Explorer");
    lv_obj_set_style_text_font(title_label, lv_theme_get_font_large(title_label), 0);
    lv_obj_set_style_margin_bottom(title_label, 5, 0);

    // This container hosts the explorer. We attach the cleanup callback to it.
    lv_obj_t *explorer_container = lv_obj_create(main_cont);
    lv_obj_remove_style_all(explorer_container);
    lv_obj_set_size(explorer_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(explorer_container, 1);
    lv_obj_clear_flag(explorer_container, LV_OBJ_FLAG_SCROLLABLE);

    // THE FIX: Register the cleanup callback on the container's delete event.
    // When lv_obj_clean() or lv_obj_del() removes this container, explorer_cleanup_event_cb will be called.
    lv_obj_add_event_cb(explorer_container, explorer_cleanup_event_cb, LV_EVENT_DELETE, NULL);

    // Pass on_file_long_pressed as the fourth argument.
    file_explorer_create(explorer_container, sd_manager_get_mount_point(), on_file_selected,
                         on_file_long_pressed, on_create_action, on_explorer_exit);
}

static void handle_initial_ok_press(void *user_data) {
    sd_manager_unmount();
    if (sd_manager_mount()) {
        show_file_explorer();
    } else if (info_label_widget) {
        lv_label_set_text(info_label_widget, "Error mounting SD card.\n\nCheck card and press OK\nto retry.");
    }
}

static void handle_initial_cancel_press(void *user_data) {
    view_manager_load_view(VIEW_ID_MENU);
}

static void create_initial_sd_view() {
    lv_obj_clean(view_container);
    action_menu_container = NULL;
    action_menu_group = NULL;

    lv_obj_t *title = lv_label_create(view_container);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_label_set_text(title, "SD Card Test");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    info_label_widget = lv_label_create(view_container);
    lv_obj_set_style_text_align(info_label_widget, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label_widget);
    lv_label_set_text(info_label_widget, "Press OK to open\nthe file explorer.");

    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_initial_ok_press, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_initial_cancel_press, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, NULL, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, NULL, true, nullptr);
}

void sd_test_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating SD Test View");

    // Create a single container for the entire view's lifecycle.
    view_container = lv_obj_create(parent);
    lv_obj_remove_style_all(view_container);
    lv_obj_set_size(view_container, lv_pct(100), lv_pct(100));
    lv_obj_center(view_container);

    // Attach the master cleanup handler. This is the key to robust resource management.
    lv_obj_add_event_cb(view_container, sd_view_cleanup_event_cb, LV_EVENT_DELETE, NULL);

    // Now, create the initial content of the view.
    create_initial_sd_view();
}