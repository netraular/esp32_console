#include "image_test_view.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include <cstring> // For strrchr, strcasecmp
#include <cstdio>  // For snprintf
#include "esp_heap_caps.h" // For memory monitoring functions

static const char *TAG = "IMAGE_TEST_VIEW";

/**
 * @brief Logs the current usage of internal RAM and PSRAM in a single line.
 * @param context A string describing when the log is being taken.
 */
static void log_memory_status(const char* context) {
    // --- RAM Calculations ---
    size_t total_ram = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t free_ram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t used_ram = total_ram - free_ram;
    float ram_usage_percent = (total_ram > 0) ? ((float)used_ram / total_ram * 100.0f) : 0;

    // --- PSRAM Calculations ---
    size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    
    // --- Formatted Logging ---
    if (total_psram > 0) {
        size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t used_psram = total_psram - free_psram;
        float psram_usage_percent = (float)used_psram / total_psram * 100.0f;
        
        ESP_LOGI(TAG, "[Mem Status: %s] RAM: %6zu B (%5.2f%%) | PSRAM: %7zu B (%5.2f%%)", 
                 context, used_ram, ram_usage_percent, used_psram, psram_usage_percent);
    } else {
        ESP_LOGI(TAG, "[Mem Status: %s] RAM: %6zu B (%5.2f%%) | PSRAM: N/A",
                 context, used_ram, ram_usage_percent);
    }
}


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
    lv_obj_clean(container); // Mark old objects for deletion

    // This call forces LVGL to actually delete the old image widget and free its memory.
    lv_timer_handler();
    log_memory_status("In initial view (after cleanup)");

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
    lv_obj_clean(container);
    lv_timer_handler(); // Process cleanup before showing explorer

    file_explorer_host_container = lv_obj_create(container);
    lv_obj_remove_style_all(file_explorer_host_container);
    lv_obj_set_size(file_explorer_host_container, LV_PCT(100), LV_PCT(100));
    lv_obj_add_event_cb(file_explorer_host_container, explorer_cleanup_event_cb, LV_EVENT_DELETE, this);

    file_explorer_create(
        file_explorer_host_container, sd_manager_get_mount_point(),
        file_selected_cb_c, nullptr, nullptr, explorer_exit_cb_c, this
    );
}

void ImageTestView::display_image_from_path(const char* path) {
    current_image_path = path;
    
    // 1. Mark old widgets for deletion.
    lv_obj_clean(container); 
    
    // 2. Force LVGL to process the deletion and free the memory from the previous image.
    lv_timer_handler(); 

    // 3. Log memory status *after* cleanup but *before* new allocation.
    log_memory_status("Before new image load");
    
    // 4. Create the new image widget and set its source. This allocates new memory (likely in PSRAM).
    char lvgl_path[260];
    snprintf(lvgl_path, sizeof(lvgl_path), "S:%s", path);
    ESP_LOGI(TAG, "Attempting to load image from LVGL path: %s", lvgl_path);
    image_widget = lv_image_create(container);
    lv_image_set_src(image_widget, lvgl_path);

    // 5. Force LVGL to process the new image allocation.
    lv_timer_handler();

    int32_t width = lv_image_get_src_width(image_widget);
    int32_t height = lv_image_get_src_height(image_widget);

    if (width > 0 && height > 0) {
        ESP_LOGI(TAG, "Image loaded successfully! Dimensions: %" PRIi32 "x%" PRIi32, width, height);
        lv_obj_align(image_widget, LV_ALIGN_CENTER, 0, 0);

        image_info_label = lv_label_create(container);
        lv_label_set_long_mode(image_info_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(image_info_label, lv_pct(90));
        lv_obj_set_style_text_align(image_info_label, LV_TEXT_ALIGN_CENTER, 0);
        
        // --- MODIFICACIÓN: Añadir estilo para el fondo de la etiqueta de información ---
        lv_obj_set_style_bg_color(image_info_label, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(image_info_label, LV_OPA_70, 0); // 70% de opacidad
        lv_obj_set_style_text_color(image_info_label, lv_color_white(), 0);
        lv_obj_set_style_pad_all(image_info_label, 5, 0);
        lv_obj_set_style_radius(image_info_label, 5, 0);
        // --- FIN DE LA MODIFICACIÓN ---

        lv_obj_align(image_info_label, LV_ALIGN_BOTTOM_MID, 0, -5);
        lv_label_set_text_fmt(image_info_label, "%s\n%" PRIi32 " x %" PRIi32, path, width, height);
        // 6. Log the "after" state. Memory usage should now be higher.
        log_memory_status("After image load");

    } else {
        ESP_LOGE(TAG, "Failed to decode or load image. Dimensions are 0x0.");
        create_initial_view();
        lv_label_set_text(info_label, "Error: Failed to load PNG.\nIs the file valid?\nPress OK to try again.");
        return;
    }

    button_manager_unregister_view_handlers();
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
        ESP_LOGI(TAG, "Selected file is a PNG, attempting to display...");
        display_image_from_path(path);
    } else {
        ESP_LOGI(TAG, "Selected file is not a PNG. Returning to initial view.");
        create_initial_view();
        lv_label_set_text(info_label, "Selected file was not a .png\nPress OK to try again.");
    }
}

void ImageTestView::on_explorer_exit() {
    ESP_LOGI(TAG, "Exited file explorer. Returning to initial view.");
    create_initial_view();
}

// --- Static Callback Bridges ---
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