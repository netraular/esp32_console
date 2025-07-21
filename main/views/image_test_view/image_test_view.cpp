#include "image_test_view.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include <cstring>

static const char *TAG = "IMAGE_TEST_VIEW";

// --- Lifecycle Methods ---
ImageTestView::ImageTestView() {
    ESP_LOGI(TAG, "ImageTestView constructed");
}

ImageTestView::~ImageTestView() {
    ESP_LOGI(TAG, "ImageTestView destructed");
}

void ImageTestView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Image Test View");
    container = parent;
    create_initial_view();
}

// --- UI & State Management ---

void ImageTestView::create_initial_view() {
    current_image_path.clear();
    lv_obj_clean(container);

    lv_obj_t* title_label = lv_label_create(container);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(title_label, "PNG Image Test (SD)");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    info_label = lv_label_create(container);
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label);
    lv_label_set_text(info_label, "Press OK to select a .png file\nfrom the SD Card.");

    image_widget = nullptr;
    setup_initial_button_handlers();
}

void ImageTestView::show_file_explorer() {
    lv_obj_clean(container);

    file_explorer_host_container = lv_obj_create(container);
    lv_obj_remove_style_all(file_explorer_host_container);
    lv_obj_set_size(file_explorer_host_container, LV_PCT(100), LV_PCT(100));
    lv_obj_add_event_cb(file_explorer_host_container, explorer_cleanup_event_cb, LV_EVENT_DELETE, this);

    file_explorer_create(
        file_explorer_host_container,
        sd_manager_get_mount_point(),
        file_selected_cb_c,
        nullptr, 
        nullptr, 
        explorer_exit_cb_c,
        this
    );
}

void ImageTestView::display_image_from_path(const char* path) {
    const char* mount_point = sd_manager_get_mount_point();
    const char* relative_path = strstr(path, mount_point);
    if (relative_path) {
        relative_path += strlen(mount_point);
    } else {
        relative_path = path;
    }
    
    char lvgl_path[260];
    snprintf(lvgl_path, sizeof(lvgl_path), "S:%s", relative_path);
    ESP_LOGI(TAG, "Attempting to load image from LVGL path: %s", lvgl_path);

    lv_obj_clean(container);
    image_widget = lv_img_create(container);
    lv_img_set_src(image_widget, lvgl_path);

    if(lv_obj_get_width(image_widget) == 0 || lv_obj_get_height(image_widget) == 0) {
        ESP_LOGE(TAG, "Failed to load or decode image. Displaying error.");
        create_initial_view();
        lv_label_set_text(info_label, "Error: Failed to load image.\nCheck file or console for errors.");
        return;
    }

    lv_obj_center(image_widget);
    current_image_path = path;

    lv_obj_t *info_label_local = lv_label_create(container);
    lv_label_set_text(info_label_local, "Press Cancel to return");
    lv_obj_align(info_label_local, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, initial_cancel_press_cb, true, this);
}

// --- Button Handling & Callbacks ---
void ImageTestView::setup_initial_button_handlers() {
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, initial_ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, initial_cancel_press_cb, true, this);
}

void ImageTestView::on_initial_ok_press() {
    if (sd_manager_check_ready()) {
        show_file_explorer();
    } else if (info_label) {
        lv_label_set_text(info_label, "Failed to read SD card.\nCheck card and press OK to retry.");
    }
}

void ImageTestView::on_initial_cancel_press() {
    if (current_image_path.empty()) {
        view_manager_load_view(VIEW_ID_MENU);
    } else {
        create_initial_view();
    }
}

void ImageTestView::on_file_selected(const char* path) {
    const char* ext = strrchr(path, '.');
    if (ext && (strcasecmp(ext, ".png") == 0)) {
        ESP_LOGI(TAG, "PNG file selected: %s. Displaying...", path);
        display_image_from_path(path);
    } else {
        ESP_LOGW(TAG, "Non-PNG file selected: %s", path);
        file_explorer_refresh();
    }
}

void ImageTestView::on_explorer_exit() {
    ESP_LOGI(TAG, "Exited file explorer. Returning to initial view.");
    create_initial_view();
}

void ImageTestView::initial_ok_press_cb(void* user_data) { static_cast<ImageTestView*>(user_data)->on_initial_ok_press(); }
void ImageTestView::initial_cancel_press_cb(void* user_data) { static_cast<ImageTestView*>(user_data)->on_initial_cancel_press(); }
void ImageTestView::file_selected_cb_c(const char* path, void* user_data) { if (user_data) static_cast<ImageTestView*>(user_data)->on_file_selected(path); }
void ImageTestView::explorer_exit_cb_c(void* user_data) { if (user_data) static_cast<ImageTestView*>(user_data)->on_explorer_exit(); }
void ImageTestView::explorer_cleanup_event_cb(lv_event_t * e) {
    ESP_LOGD(TAG, "Explorer host container deleted. Calling file_explorer_destroy().");
    file_explorer_destroy();
    auto* instance = static_cast<ImageTestView*>(lv_event_get_user_data(e));
    if (instance) instance->file_explorer_host_container = nullptr;
}