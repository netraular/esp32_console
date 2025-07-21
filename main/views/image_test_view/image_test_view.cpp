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

    lv_obj_t* title_label = lv_label_create(container);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(title_label, "PNG Image Test (SD)");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    info_label = lv_label_create(container);
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label);
    lv_label_set_text(info_label, "Press OK to select a file\nfrom the SD Card (PNG only).");

    image_widget = nullptr; // Ensure no stale pointer
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
    char lvgl_path[260];
    // LVGL's VFS driver expects a drive letter prefix (e.g., "S:/sdcard/image.png")
    snprintf(lvgl_path, sizeof(lvgl_path), "S:%s", path);
    ESP_LOGI(TAG, "Attempting to load image from LVGL path: %s", lvgl_path);

    lv_obj_clean(container); // Clear the screen before displaying the image


    lv_obj_t * img;
    img = lv_image_create(lv_screen_active());
    lv_image_set_src(img, lvgl_path);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);



    /*
    image_widget = lv_img_create(container);
    lv_img_set_src(image_widget, lvgl_path); // Attempt to load the image

    lv_coord_t img_width = lv_obj_get_width(image_widget);
    lv_coord_t img_height = lv_obj_get_height(image_widget);

    if (img_width == 0 || img_height == 0) {
        // CORRECTED: Use %ld for lv_coord_t
        ESP_LOGE(TAG, "Failed to load or decode image (width: %ld, height: %ld). Displaying error.", (long)img_width, (long)img_height);
        // Clean up the partially created image widget
        lv_obj_del(image_widget); 
        image_widget = nullptr;
        // Return to the initial state with an error message
        create_initial_view();
        lv_label_set_text(info_label, "Error: Failed to decode PNG.\nCheck file integrity or memory.\nPress OK to retry.");
        return;
    }

    ESP_LOGI(TAG, "Image loaded successfully. Dimensions: %ldx%ld pixels", (long)img_width, (long)img_height);
    lv_obj_center(image_widget); // Center the image on the screen
    current_image_path = path; // Store the path for potential re-display or context

    // Display image dimensions and instructions
    lv_obj_t *dims_label = lv_label_create(container);
    // CORRECTED: Use %ld for lv_coord_t
    lv_label_set_text_fmt(dims_label, "Dimensions: %ldx%ld px", (long)img_width, (long)img_height);
    lv_obj_align(dims_label, LV_ALIGN_TOP_MID, 0, 10); // Position above the image

    lv_obj_t *instruction_label = lv_label_create(container);
    lv_label_set_text(instruction_label, "Press Cancel to return");
    lv_obj_align(instruction_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    */
    
    // Update button handlers for the image display state
    button_manager_unregister_view_handlers(); // Clear file explorer handlers
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, initial_cancel_press_cb, true, this);
}

// --- Diagnostic function to test LVGL's VFS ---
void ImageTestView::perform_vfs_read_test(const char* path) {
    ESP_LOGW(TAG, "--- STARTING LVGL VFS DIAGNOSTIC TEST ---");
    
    char lvgl_path[260];
    snprintf(lvgl_path, sizeof(lvgl_path), "S:%s", path);
    ESP_LOGI(TAG, "Testing path: %s", lvgl_path);

    lv_fs_file_t f;
    lv_fs_res_t res = lv_fs_open(&f, lvgl_path, LV_FS_MODE_RD);

    if (res != LV_FS_RES_OK) {
        ESP_LOGE(TAG, "lv_fs_open FAILED. Result code: %d", res);
        switch(res) {
            case LV_FS_RES_HW_ERR: ESP_LOGE(TAG, "Reason: Hardware error"); break;
            case LV_FS_RES_FS_ERR: ESP_LOGE(TAG, "Reason: Filesystem error"); break;
            case LV_FS_RES_NOT_EX: ESP_LOGE(TAG, "Reason: File does not exist"); break; 
            case LV_FS_RES_FULL: ESP_LOGE(TAG, "Reason: Filesystem is full"); break;
            case LV_FS_RES_LOCKED: ESP_LOGE(TAG, "Reason: File is locked"); break;
            case LV_FS_RES_DENIED: ESP_LOGE(TAG, "Reason: Permission denied"); break;
            case LV_FS_RES_TOUT: ESP_LOGE(TAG, "Reason: Timeout"); break;
            case LV_FS_RES_NOT_IMP: ESP_LOGE(TAG, "Reason: Not implemented"); break;
            default: ESP_LOGE(TAG, "Reason: Unknown error"); break;
        }
        ESP_LOGW(TAG, "--- LVGL VFS DIAGNOSTIC TEST FAILED ---");
        return;
    }

    ESP_LOGI(TAG, "lv_fs_open SUCCEEDED!");

    uint32_t file_size = 0;
    lv_fs_seek(&f, 0, LV_FS_SEEK_END);
    lv_fs_tell(&f, &file_size);
    lv_fs_seek(&f, 0, LV_FS_SEEK_SET);
    ESP_LOGI(TAG, "File size reported by lv_fs_tell: %" PRIu32 " bytes", file_size);

    char buf[65]; // Read a snippet
    uint32_t bytes_read = 0;
    res = lv_fs_read(&f, buf, 64, &bytes_read); // Read up to 64 bytes
    
    if (res == LV_FS_RES_OK) {
        buf[bytes_read] = '\0'; // Null-terminate the buffer
        ESP_LOGI(TAG, "Read %" PRIu32 " bytes successfully. Content snippet:", bytes_read);
        // Print content in hex and as string for better debugging of binary files
        char hex_dump[193]; // 64 bytes * 3 chars/byte + 1 for null
        hex_dump[0] = '\0';
        for (uint32_t i = 0; i < bytes_read; ++i) {
            snprintf(hex_dump + strlen(hex_dump), sizeof(hex_dump) - strlen(hex_dump), "%02X ", (unsigned char)buf[i]);
        }
        ESP_LOGI(TAG, "Hex: %s", hex_dump);
        ESP_LOGI(TAG, "ASCII: \n---\n%s\n---", buf);
    } else {
        ESP_LOGE(TAG, "lv_fs_read FAILED. Result code: %d", res);
    }
    
    lv_fs_close(&f);
    ESP_LOGW(TAG, "--- LVGL VFS DIAGNOSTIC TEST FINISHED ---");
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
    perform_vfs_read_test(path); // Run diagnostic test for any selected file

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