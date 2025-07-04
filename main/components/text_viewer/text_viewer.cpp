#include "components/text_viewer/text_viewer.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"

static const char *TAG = "COMP_TEXT_VIEWER";

// Structure to store the state and callbacks of the component
typedef struct {
    char* content_to_free;
    text_viewer_exit_callback_t on_exit_cb;
} text_viewer_data_t;

// --- Internal event and button handlers ---

// MODIFIED: Now uses user_data to get the context, which is more robust.
static void handle_exit_press(void* user_data) {
    text_viewer_data_t* data = (text_viewer_data_t*)user_data;
    if (data && data->on_exit_cb) {
        data->on_exit_cb();
    }
}

// All cleanup logic is centralized in the callback of the main container.
static void viewer_container_delete_cb(lv_event_t * e) {
    text_viewer_data_t* data = (text_viewer_data_t*)lv_event_get_user_data(e);
    if (data) {
        ESP_LOGD(TAG, "Cleaning up text viewer component resources.");
        // 1. Free the file content
        if (data->content_to_free) {
            ESP_LOGD(TAG, "Freeing text content memory.");
            free(data->content_to_free);
            data->content_to_free = NULL;
        }
        // 2. Free the component's data structure
        ESP_LOGD(TAG, "Freeing viewer user_data struct.");
        free(data);
    }
}


// --- Public Functions ---

lv_obj_t* text_viewer_create(lv_obj_t* parent, const char* title, char* content, text_viewer_exit_callback_t on_exit) {
    ESP_LOGI(TAG, "Creating text viewer for: %s", title);

    // Create the data structure for the component
    text_viewer_data_t* data = (text_viewer_data_t*)malloc(sizeof(text_viewer_data_t));
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate memory for viewer data");
        free(content); // Free the content to prevent leaks
        return NULL;
    }
    data->content_to_free = content;
    data->on_exit_cb = on_exit;

    // Main container
    lv_obj_t* main_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    // Assign the single cleanup callback to the main container
    lv_obj_add_event_cb(main_cont, viewer_container_delete_cb, LV_EVENT_DELETE, data);

    // Title label
    lv_obj_t* title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, lv_theme_get_font_large(title_label), 0);
    lv_obj_set_style_margin_bottom(title_label, 5, 0);

    // Text area
    lv_obj_t* text_area = lv_textarea_create(main_cont);
    lv_obj_set_size(text_area, lv_pct(95), lv_pct(85));
    lv_textarea_set_text(text_area, content);

    // Register button handlers
    button_manager_unregister_view_handlers(); // Clear previous handlers
    // MODIFIED: We pass the 'data' pointer as user_data to the handler.
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_exit_press, true, data);
    // Nullify other buttons to prevent unwanted actions
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, NULL, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, NULL, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, NULL, true, nullptr);

    return main_cont;
}

void text_viewer_destroy(lv_obj_t* viewer) {
    if (viewer) {
        ESP_LOGI(TAG, "Destroying text viewer component.");
        // Calling lv_obj_del will automatically trigger the LV_EVENT_DELETE
        // and our 'viewer_container_delete_cb' will do all the cleanup.
        lv_obj_del(viewer);
    }
}