#include "file_explorer.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "FILE_EXPLORER";
static lv_group_t *explorer_group = nullptr;
static lv_style_t style_focused;
static lv_obj_t* list_widget = nullptr;
static char current_path[256];
static char mount_point[32];
static bool in_error_state = false; // Flag to manage the UI when the SD card is not mounted.

// User-provided callbacks
static file_select_callback_t on_file_select_cb = NULL;
static file_explorer_exit_callback_t on_exit_cb = NULL;

// Internal data structures
typedef struct { bool is_dir; } list_item_data_t;
typedef struct { lv_obj_t *list; lv_group_t *group; } add_file_context_t;

// Internal Prototypes
static void repopulate_list_cb(lv_timer_t *timer);
static void schedule_repopulate_list();
static void clear_list_items(bool show_loading);
static void list_item_delete_cb(lv_event_t * e);
static void add_file_entry_to_list(const char *name, bool is_dir, void *user_data);
static void focus_changed_cb(lv_group_t * group);
static void handle_right_press();
static void handle_left_press();
static void handle_cancel_press();
static void handle_ok_press();

static void handle_right_press() {
    if (in_error_state) return; // Ignore navigation in error state
    if (explorer_group) lv_group_focus_next(explorer_group);
}

static void handle_left_press() {
    if (in_error_state) return; // Ignore navigation in error state
    if (explorer_group) lv_group_focus_prev(explorer_group);
}

static void handle_ok_press() {
    // If in error state, "OK" becomes a retry button.
    if (in_error_state) {
        ESP_LOGI(TAG, "Retrying SD card mount...");
        sd_manager_unmount(); // Force unmount just in case
        if (sd_manager_mount()) {
            ESP_LOGI(TAG, "Mount successful. Reloading file list.");
            in_error_state = false; // Exit error state
            schedule_repopulate_list();
        } else {
            ESP_LOGW(TAG, "Mount failed again.");
            // Keep the error message on screen
        }
        return;
    }
    
    // Normal operation: select an item.
    lv_obj_t * focused_obj = lv_group_get_focused(explorer_group);
    if (!focused_obj) return;

    list_item_data_t* item_data = static_cast<list_item_data_t*>(lv_obj_get_user_data(focused_obj));
    if (!item_data) return;

    const char* entry_name = lv_list_get_button_text(list_widget, focused_obj);

    if (item_data->is_dir) {
        // Navigate into a subdirectory
        ESP_LOGI(TAG, "Entering directory: %s", entry_name);
        if (current_path[strlen(current_path) - 1] != '/') {
            strcat(current_path, "/");
        }
        strcat(current_path, entry_name);
        schedule_repopulate_list();
    } else {
        // It's a file, notify the caller via callback.
        ESP_LOGI(TAG, "File selected: %s", entry_name);
        if (on_file_select_cb) {
            char full_path[300];
            snprintf(full_path, sizeof(full_path), "%s/%s", current_path, entry_name);
            on_file_select_cb(full_path);
        }
    }
}

static void handle_cancel_press() {
    // "Cancel" always works: either exits the explorer or navigates up.
    if (in_error_state || strcmp(current_path, mount_point) == 0) {
        // If at the root or in an error state, notify to exit.
        ESP_LOGI(TAG, "Exiting file explorer.");
        if (on_exit_cb) {
            on_exit_cb();
        }
        return;
    }
    
    // Navigate to the parent directory.
    ESP_LOGI(TAG, "Navigating to parent from: %s", current_path);
    char *last_slash = strrchr(current_path, '/');
    if (last_slash > current_path && (size_t)(last_slash - current_path) >= strlen(mount_point)) {
         *last_slash = '\0';
    } else {
         strcpy(current_path, mount_point);
    }
    ESP_LOGI(TAG, "New path: %s", current_path);

    schedule_repopulate_list();
}

// Scrolls the list to keep the focused item in view.
static void focus_changed_cb(lv_group_t * group) {
    lv_obj_t * focused_obj = lv_group_get_focused(group);
    if (!focused_obj) return;
    lv_obj_scroll_to_view(focused_obj, LV_ANIM_ON);
}

// Frees the user data associated with a list item when it's deleted.
static void list_item_delete_cb(lv_event_t * e) {
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    list_item_data_t* item_data = static_cast<list_item_data_t*>(lv_obj_get_user_data(btn));
    if (item_data) {
        free(item_data);
    }
}

static void clear_list_items(bool show_loading) {
    if (!list_widget) return;
    lv_group_remove_all_objs(explorer_group);
    lv_obj_clean(list_widget);
    if (show_loading) {
        lv_list_add_text(list_widget, "Loading...");
    }
}

// Asynchronously repopulates the file list. Called via an lv_timer.
static void repopulate_list_cb(lv_timer_t *timer) {
    clear_list_items(false);

    if (sd_manager_is_mounted()) {
        in_error_state = false;
        add_file_context_t context = { .list = list_widget, .group = explorer_group };
        sd_manager_list_files(current_path, add_file_entry_to_list, &context);
    } else {
        // If SD is not mounted, enter error state and show a message.
        in_error_state = true;
        lv_obj_t* label = lv_label_create(list_widget);
        lv_label_set_text(label, "Error: SD Card not found.\n\n"
                                 "Press OK to retry.\n"
                                 "Press Cancel to exit.");
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(label, lv_pct(95));
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(label);
    }

    if (lv_group_get_obj_count(explorer_group) > 0) {
        lv_group_focus_next(explorer_group);
    }
    
    lv_timer_delete(timer);
}

// Schedules a deferred update of the file list.
static void schedule_repopulate_list() {
    clear_list_items(true);
    // Use a short timer to allow the "Loading..." message to render first.
    lv_timer_create(repopulate_list_cb, 10, NULL);
}

// Callback for sd_manager_list_files, adds one entry to the LVGL list.
static void add_file_entry_to_list(const char *name, bool is_dir, void *user_data) {
    add_file_context_t *context = static_cast<add_file_context_t *>(user_data);
    if (!context || !context->list || !context->group) return;

    const char *icon = is_dir ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE;
    lv_obj_t *btn = lv_list_add_button(context->list, icon, name);

    // Store whether the item is a directory in the button's user data.
    list_item_data_t* item_data = static_cast<list_item_data_t*>(malloc(sizeof(list_item_data_t)));
    if (item_data) {
        item_data->is_dir = is_dir;
        lv_obj_set_user_data(btn, item_data);
        lv_obj_add_event_cb(btn, list_item_delete_cb, LV_EVENT_DELETE, NULL);
    }

    lv_obj_add_style(btn, &style_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(context->group, btn);

    // Allow long filenames to scroll.
    lv_obj_t *label = lv_obj_get_child(btn, 1);
    if(label) {
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(label, lv_pct(90));
    }
}

// --- Public Function Implementations ---

void file_explorer_destroy(void) {
    // Unregister button handlers to prevent calls to a destroyed component.
    button_manager_unregister_view_handlers();

    if (explorer_group) {
        if (lv_group_get_default() == explorer_group) {
            lv_group_set_default(NULL);
        }
        lv_group_delete(explorer_group);
        explorer_group = nullptr;
    }
    // `list_widget` is a child of the parent screen and will be deleted with it,
    // so we just null the pointer.
    list_widget = nullptr;
    on_file_select_cb = NULL;
    on_exit_cb = NULL;
    in_error_state = false;
    ESP_LOGI(TAG, "File explorer destroyed.");
}

void file_explorer_create(lv_obj_t *parent, const char *initial_path, file_select_callback_t on_select, file_explorer_exit_callback_t on_exit) {
    ESP_LOGI(TAG, "Creating file explorer at path: %s", initial_path);
    
    // Store callbacks and initial path
    on_file_select_cb = on_select;
    on_exit_cb = on_exit;
    strncpy(current_path, initial_path, sizeof(current_path) - 1);
    strncpy(mount_point, initial_path, sizeof(mount_point) - 1);
    in_error_state = false;

    // Set up an LVGL group for button navigation
    explorer_group = lv_group_create();
    lv_group_set_wrap(explorer_group, true);
    lv_group_set_focus_cb(explorer_group, focus_changed_cb);
    lv_group_set_default(explorer_group);

    // Style for the focused list item
    lv_style_init(&style_focused);
    lv_style_set_bg_color(&style_focused, lv_palette_main(LV_PALETTE_LIGHT_BLUE));
    lv_style_set_bg_opa(&style_focused, LV_OPA_COVER);

    // Create the list widget
    list_widget = lv_list_create(parent);
    lv_obj_set_size(list_widget, lv_pct(100), lv_pct(100));
    lv_obj_center(list_widget);

    // Populate the list asynchronously
    schedule_repopulate_list();

    // Register physical button handlers for this view
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press);
    button_manager_register_view_handler(BUTTON_OK, handle_ok_press);
    button_manager_register_view_handler(BUTTON_RIGHT, handle_right_press);
    button_manager_register_view_handler(BUTTON_LEFT, handle_left_press);
}