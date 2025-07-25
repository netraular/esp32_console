#include "components/file_explorer/file_explorer.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "esp_log.h"
#include <string.h>
#include <vector>
#include <string>
#include <algorithm>
#include <strings.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "COMP_FILE_EXPLORER";

// --- UI State Variables ---
static lv_group_t *explorer_group = nullptr;
static lv_style_t style_focused;
static lv_obj_t* list_widget = nullptr;
static char current_path[256];
static char mount_point[32];
static bool in_error_state = false;

// --- External Callbacks and User Data ---
static file_select_callback_t on_file_select_cb = NULL;
static file_long_press_callback_t on_file_long_press_cb = NULL;
static file_action_callback_t on_action_cb = NULL;
static file_explorer_exit_callback_t on_exit_cb = NULL;
static void* s_user_data = nullptr; // --- NEW: Store the user_data passed on creation.

// --- Internal Data Types ---
typedef struct {
    std::string name;
    bool is_dir;
} file_entry_t;

typedef struct {
    file_item_type_t type;
} list_item_data_t;

typedef struct {
    lv_obj_t *list;
    lv_group_t *group;
} add_file_context_t;

// --- Forward Declarations ---
static void repopulate_list_cb(lv_timer_t *timer);
static void schedule_repopulate_list();
static void clear_list_items(bool show_loading);
static void list_item_delete_cb(lv_event_t * e);
static void add_list_entry(add_file_context_t *context, const char *name, const char *icon, file_item_type_t type);
static void collect_fs_entries_cb(const char *name, bool is_dir, void *user_data);
static void focus_changed_cb(lv_group_t * group);
static void handle_cancel_press(void* user_data);

// --- MODIFIED BUTTON HANDLERS ---
// The user_data passed to these handlers is now the global context `s_user_data`.
static void handle_right_press(void* user_data) {
    if (in_error_state) return;
    if (explorer_group) lv_group_focus_next(explorer_group);
}

static void handle_left_press(void* user_data) {
    if (in_error_state) return;
    if (explorer_group) lv_group_focus_prev(explorer_group);
}

static void handle_ok_press(void* user_data) {
    if (in_error_state) return;
    
    lv_obj_t * focused_obj = lv_group_get_focused(explorer_group);
    if (!focused_obj) return;

    list_item_data_t* item_data = static_cast<list_item_data_t*>(lv_obj_get_user_data(focused_obj));
    if (!item_data) return;

    const char* entry_name = lv_list_get_button_text(list_widget, focused_obj);

    switch (item_data->type) {
        case ITEM_TYPE_DIR:
            if (strlen(current_path) > 0 && current_path[strlen(current_path) - 1] != '/') {
                 strcat(current_path, "/");
            }
            strcat(current_path, entry_name);
            schedule_repopulate_list();
            break;
        case ITEM_TYPE_PARENT_DIR:
            handle_cancel_press(user_data); // Same action as going back
            break;
        case ITEM_TYPE_FILE:
            if (on_file_select_cb) {
                char full_path[300];
                snprintf(full_path, sizeof(full_path), "%s/%s", current_path, entry_name);
                on_file_select_cb(full_path, user_data);
            }
            break;
        case ITEM_TYPE_ACTION_CREATE_FILE:
        case ITEM_TYPE_ACTION_CREATE_FOLDER:
            if (on_action_cb) {
                on_action_cb(item_data->type, current_path, user_data);
            }
            break;
    }
}

static void handle_ok_long_press(void* user_data) {
    if (in_error_state || !on_file_long_press_cb) return;

    lv_obj_t * focused_obj = lv_group_get_focused(explorer_group);
    if (!focused_obj) return;

    list_item_data_t* item_data = static_cast<list_item_data_t*>(lv_obj_get_user_data(focused_obj));
    if (!item_data) return;

    if (item_data->type == ITEM_TYPE_FILE) {
        const char* entry_name = lv_list_get_button_text(list_widget, focused_obj);
        char full_path[300];
        snprintf(full_path, sizeof(full_path), "%s/%s", current_path, entry_name);
        on_file_long_press_cb(full_path, user_data);
    }
}

static void handle_cancel_press(void* user_data) {
    if (in_error_state || strcmp(current_path, mount_point) == 0) {
        if (on_exit_cb) on_exit_cb(user_data);
    } else {
        char *last_slash = strrchr(current_path, '/');
        if (last_slash && last_slash > current_path) {
            *last_slash = '\0';
        } else {
            strcpy(current_path, mount_point);
        }
        schedule_repopulate_list();
    }
}


// --- UI Logic (largely unchanged) ---

static void focus_changed_cb(lv_group_t * group) {
    lv_obj_t * focused_obj = lv_group_get_focused(group);
    if (!focused_obj) return;
    lv_obj_scroll_to_view(focused_obj, LV_ANIM_ON);
}

static void list_item_delete_cb(lv_event_t * e) {
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    list_item_data_t* item_data = static_cast<list_item_data_t*>(lv_obj_get_user_data(btn));
    if (item_data) free(item_data);
}

static void clear_list_items(bool show_loading) {
    if (!list_widget) return;
    if (explorer_group) lv_group_remove_all_objs(explorer_group);
    lv_obj_clean(list_widget);
    if (show_loading) lv_list_add_text(list_widget, "Loading...");
}

static void repopulate_list_cb(lv_timer_t *timer) {
    clear_list_items(false);

    if (sd_manager_check_ready()) {
        in_error_state = false;
        add_file_context_t context = { .list = list_widget, .group = explorer_group };

        if (on_action_cb) {
            add_list_entry(&context, "Create File", LV_SYMBOL_PLUS, ITEM_TYPE_ACTION_CREATE_FILE);
            add_list_entry(&context, "Create Folder", LV_SYMBOL_PLUS, ITEM_TYPE_ACTION_CREATE_FOLDER);
        }

        if (strcmp(current_path, mount_point) != 0) {
            add_list_entry(&context, "..", LV_SYMBOL_UP, ITEM_TYPE_PARENT_DIR);
        }

        std::vector<file_entry_t> entries;
        sd_manager_list_files(current_path, collect_fs_entries_cb, &entries);

        std::sort(entries.begin(), entries.end(), [](const file_entry_t& a, const file_entry_t& b) {
            if (a.is_dir && !b.is_dir) return true;
            if (!a.is_dir && b.is_dir) return false;
            return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
        });

        for (const auto& entry : entries) {
            const char *icon = entry.is_dir ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE;
            file_item_type_t type = entry.is_dir ? ITEM_TYPE_DIR : ITEM_TYPE_FILE;
            add_list_entry(&context, entry.name.c_str(), icon, type);
        }

    } else {
        in_error_state = true;
        lv_list_add_text(list_widget, "Error: SD Card not readable");
    }

    if (explorer_group && lv_group_get_obj_count(explorer_group) > 0) {
        lv_group_focus_obj(lv_obj_get_child(list_widget, 0));
    }
    
    if(timer) lv_timer_delete(timer);
}

static void schedule_repopulate_list() {
    clear_list_items(true);
    lv_timer_create(repopulate_list_cb, 10, NULL);
}

static void add_list_entry(add_file_context_t *context, const char *name, const char *icon, file_item_type_t type) {
    lv_obj_t *btn = lv_list_add_button(context->list, icon, name);
    
    list_item_data_t* item_data = static_cast<list_item_data_t*>(malloc(sizeof(list_item_data_t)));
    if (item_data) {
        item_data->type = type;
        lv_obj_set_user_data(btn, item_data);
        lv_obj_add_event_cb(btn, list_item_delete_cb, LV_EVENT_DELETE, NULL);
    }

    lv_obj_add_style(btn, &style_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(context->group, btn);

    lv_obj_t *label = lv_obj_get_child(btn, 1);
    if(label) {
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(label, lv_pct(90));
    }
}

static void collect_fs_entries_cb(const char *name, bool is_dir, void *user_data) {
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return;
    }
    auto* entries_vector = static_cast<std::vector<file_entry_t>*>(user_data);
    entries_vector->push_back({std::string(name), is_dir});
}

// --- Public Functions ---

void file_explorer_destroy(void) {
    if (explorer_group) {
        if (lv_group_get_default() == explorer_group) lv_group_set_default(NULL);
        lv_group_delete(explorer_group);
        explorer_group = nullptr;
    }
    
    list_widget = nullptr;
    on_file_select_cb = NULL;
    on_file_long_press_cb = NULL;
    on_action_cb = NULL;
    on_exit_cb = NULL;
    s_user_data = nullptr; // Clear user data
    in_error_state = false;
    ESP_LOGI(TAG, "File explorer destroyed.");
}

void file_explorer_set_input_active(bool active) {
    if (active) {
        ESP_LOGD(TAG, "Re-activating file explorer input handlers.");
        button_manager_set_dispatch_mode(INPUT_DISPATCH_MODE_QUEUED);
        // --- MODIFIED --- All handlers now pass `s_user_data`
        button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, s_user_data);
        button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_ok_press, true, s_user_data);
        button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, handle_right_press, true, s_user_data);
        button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, handle_left_press, true, s_user_data);
        button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_LONG_PRESS_START, handle_ok_long_press, true, s_user_data);

        if (explorer_group) {
            lv_group_set_default(explorer_group);
        }
    } else {
        ESP_LOGD(TAG, "De-activating file explorer input handlers.");
        button_manager_unregister_view_handlers();
        if (explorer_group && lv_group_get_default() == explorer_group) {
            lv_group_set_default(NULL);
        }
    }
}

// --- MODIFIED: `create` function now accepts user_data ---
void file_explorer_create(lv_obj_t *parent, const char *initial_path, file_select_callback_t on_select, file_long_press_callback_t on_long_press, file_action_callback_t on_action, file_explorer_exit_callback_t on_exit, void* user_data) {
    ESP_LOGI(TAG, "Creating file explorer at path: %s", initial_path);
    
    on_file_select_cb = on_select;
    on_file_long_press_cb = on_long_press;
    on_action_cb = on_action;
    on_exit_cb = on_exit;
    s_user_data = user_data; // --- NEW: Store user data
    strncpy(current_path, initial_path, sizeof(current_path) - 1);
    current_path[sizeof(current_path) - 1] = '\0';
    strncpy(mount_point, initial_path, sizeof(mount_point) - 1);
    mount_point[sizeof(mount_point) - 1] = '\0';
    in_error_state = false;

    explorer_group = lv_group_create();
    lv_group_set_wrap(explorer_group, true);
    lv_group_set_focus_cb(explorer_group, focus_changed_cb);

    lv_style_init(&style_focused);
    lv_style_set_bg_color(&style_focused, lv_palette_main(LV_PALETTE_LIGHT_BLUE));
    lv_style_set_bg_opa(&style_focused, LV_OPA_COVER);

    list_widget = lv_list_create(parent);
    lv_obj_set_size(list_widget, lv_pct(100), lv_pct(100));
    lv_obj_center(list_widget);
    
    schedule_repopulate_list();

    file_explorer_set_input_active(true);
}

void file_explorer_refresh(void) {
    if (list_widget) {
        schedule_repopulate_list();
    }
}