#include "image_test_view.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include <cstring> // For strrchr, strcasecmp
#include <cstdio>  // For snprintf

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
    container = parent; // The parent (active screen) becomes our container
    create_initial_view();
}

// --- UI & State Management ---

void ImageTestView::create_initial_view() {
    current_image_path.clear();
    lv_obj_clean(container); // Clear any existing widgets on the screen

    // Reset widget pointers
    image_widget = nullptr;
    image_info_label = nullptr;

    lv_obj_t* title_label = lv_label_create(container);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(title_label, "PNG Image Test (SD)");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    info_label = lv_label_create(container);
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label);
    lv_label_set_text(info_label, "Press OK to select a file\nfrom the SD Card (.png only).");

    setup_initial_button_handlers();
}

void ImageTestView::show_file_explorer() {
    lv_obj_clean(container); // Clear the current screen content

    // Create a host container for the file explorer to manage its lifecycle
    file_explorer_host_container = lv_obj_create(container);
    lv_obj_remove_style_all(file_explorer_host_container); // Make it transparent
    lv_obj_set_size(file_explorer_host_container, LV_PCT(100), LV_PCT(100));
    // Register an event callback to destroy the file explorer when its host is deleted
    lv_obj_add_event_cb(file_explorer_host_container, explorer_cleanup_event_cb, LV_EVENT_DELETE, this);

    file_explorer_create(
        file_explorer_host_container,
        sd_manager_get_mount_point(), // Start browsing from SD card root
        file_selected_cb_c,           // Callback for file selection
        nullptr,                      // No long press handler for this view
        nullptr,                      // No action handler for this view
        explorer_exit_cb_c,           // Callback for explorer exit
        this                          // Pass 'this' as user_data
    );
}

void ImageTestView::display_image_from_path(const char* path) {
    lv_obj_clean(container); // Clear the screen before displaying the image
    current_image_path = path; // Store the path for context

    char lvgl_path[260];
    // LVGL's VFS driver expects a drive letter prefix (e.g., "S:/sdcard/image.png")
    snprintf(lvgl_path, sizeof(lvgl_path), "S:%s", path);
    ESP_LOGI(TAG, "Attempting to load image from LVGL path: %s", lvgl_path);

    // Create the image widget and assign it to our member variable
    image_widget = lv_image_create(container);
    lv_image_set_src(image_widget, lvgl_path);
    
    // Check if the image was loaded successfully by checking its dimensions.
    // If the decoder fails, width/height will be 0.
    // MODIFIED: Use the correct LVGL v9 functions as suggested by the compiler.
    int32_t width = lv_image_get_src_width(image_widget);
    int32_t height = lv_image_get_src_height(image_widget);

    if (width > 0 && height > 0) {
        ESP_LOGI(TAG, "Image loaded successfully! Dimensions: %" PRIi32 "x%" PRIi32, width, height);
        lv_obj_align(image_widget, LV_ALIGN_CENTER, 0, 0);

        // Create a label to display image info
        image_info_label = lv_label_create(container);
        lv_label_set_long_mode(image_info_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(image_info_label, lv_pct(90));
        lv_obj_set_style_text_align(image_info_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(image_info_label, LV_ALIGN_BOTTOM_MID, 0, -5);
        lv_label_set_text_fmt(image_info_label, "%s\n%" PRIi32 " x %" PRIi32, path, width, height);

    } else {
        ESP_LOGE(TAG, "Failed to decode or load image. Dimensions are 0x0.");
        create_initial_view(); // Go back to the initial view
        lv_label_set_text(info_label, "Error: Failed to load PNG.\nIs the file valid?\nPress OK to try again.");
        return;
    }
    
    // Update button handlers for the image display state
    button_manager_unregister_view_handlers(); // Clear file explorer handlers
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, initial_cancel_press_cb, true, this);
}


// --- Button Handling & Callbacks ---
void ImageTestView::setup_initial_button_handlers() {
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, initial_ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, initial_cancel_press_cb, true, this);
}

void ImageTestView::on_initial_ok_press() {
    // Check if SD card is ready and then show file explorer
    if (sd_manager_check_ready()) {
        show_file_explorer();
    } else if (info_label) {
        // Update info label if SD card is not ready
        lv_label_set_text(info_label, "Failed to read SD card.\nCheck card and press OK to retry.");
    }
}

void ImageTestView::on_initial_cancel_press() {
    // If no image is displayed, return to the menu; otherwise, return to the initial view.
    if (current_image_path.empty()) {
        view_manager_load_view(VIEW_ID_MENU);
    } else {
        create_initial_view();
    }
}

void ImageTestView::on_file_selected(const char* path) {
    const char* ext = strrchr(path, '.'); // Get file extension
    if (ext && (strcasecmp(ext, ".png") == 0)) {
        ESP_LOGI(TAG, "Selected file is a PNG, attempting to display...");
        display_image_from_path(path);
    } else {
        ESP_LOGI(TAG, "Selected file is not a PNG. Returning to initial view.");
        create_initial_view(); // Go back to the initial view
        lv_label_set_text(info_label, "Selected file was not a .png\nPress OK to try again.");
    }
}

void ImageTestView::on_explorer_exit() {
    ESP_LOGI(TAG, "Exited file explorer. Returning to initial view.");
    create_initial_view(); // Go back to the initial view
}

// --- Static Callback Bridges ---
// These static functions simply cast the user_data back to a class instance
// and call the appropriate member function.
void ImageTestView::initial_ok_press_cb(void* user_data) { static_cast<ImageTestView*>(user_data)->on_initial_ok_press(); }
void ImageTestView::initial_cancel_press_cb(void* user_data) { static_cast<ImageTestView*>(user_data)->on_initial_cancel_press(); }
void ImageTestView::file_selected_cb_c(const char* path, void* user_data) { if (user_data) static_cast<ImageTestView*>(user_data)->on_file_selected(path); }
void ImageTestView::explorer_exit_cb_c(void* user_data) { if (user_data) static_cast<ImageTestView*>(user_data)->on_explorer_exit(); }

void ImageTestView::explorer_cleanup_event_cb(lv_event_t * e) {
    ESP_LOGD(TAG, "Explorer host container deleted. Calling file_explorer_destroy().");
    file_explorer_destroy(); // Call the component's destructor when its parent is deleted
    auto* instance = static_cast<ImageTestView*>(lv_event_get_user_data(e));
    if (instance) instance->file_explorer_host_container = nullptr; // Clear the pointer
}