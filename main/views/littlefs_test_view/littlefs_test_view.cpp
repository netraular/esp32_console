#include "littlefs_test_view.h"
#include "controllers/littlefs_manager/littlefs_manager.h"

static const char *TAG = "LITTLEFS_TEST_VIEW";

// --- Lifecycle Methods ---
LittlefsTestView::LittlefsTestView() {
    ESP_LOGI(TAG, "LittlefsTestView constructed");
}

LittlefsTestView::~LittlefsTestView() {
    ESP_LOGI(TAG, "LittlefsTestView destructed");
}

void LittlefsTestView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating LittleFS Test view UI");
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);
    
    setup_ui(container);
    setup_button_handlers();
}

// --- UI Setup ---
void LittlefsTestView::setup_ui(lv_obj_t* parent) {
    // Create a title
    lv_obj_t *title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "LittleFS Test");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

    // Create the main text label
    lv_obj_t* content_label = lv_label_create(parent);
    lv_obj_set_width(content_label, LV_PCT(90));
    lv_obj_align(content_label, LV_ALIGN_CENTER, 0, 10);
    lv_label_set_long_mode(content_label, LV_LABEL_LONG_WRAP);

    // Attempt to read the file from LittleFS
    char* file_content = nullptr;
    size_t file_size = 0;
    if (littlefs_manager_read_file("welcome.txt", &file_content, &file_size) && file_content) {
        ESP_LOGI(TAG, "Successfully read 'welcome.txt'");
        lv_label_set_text(content_label, file_content);
        free(file_content); // Free the buffer after LVGL has copied the text
    } else {
        ESP_LOGE(TAG, "Failed to read 'welcome.txt'");
        lv_label_set_text(content_label, "Error:\nCould not read 'welcome.txt' from LittleFS. Check logs.");
    }
}

// --- Button Handling ---
void LittlefsTestView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, LittlefsTestView::cancel_press_cb, true, this);
}

// --- Instance Methods for Button Actions ---
void LittlefsTestView::on_cancel_press() {
    ESP_LOGI(TAG, "Cancel pressed, returning to menu.");
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Static Callbacks (Bridges) ---
void LittlefsTestView::cancel_press_cb(void* user_data) {
    static_cast<LittlefsTestView*>(user_data)->on_cancel_press();
}