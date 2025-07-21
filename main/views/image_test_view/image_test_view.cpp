#include "image_test_view.h"

static const char *TAG = "IMAGE_TEST_VIEW";

// --- Lifecycle Methods ---
ImageTestView::ImageTestView() {
    ESP_LOGI(TAG, "ImageTestView constructed");
}

ImageTestView::~ImageTestView() {
    ESP_LOGI(TAG, "ImageTestView destructed");
    // No dynamic resources to clean up. LVGL widgets are handled by the parent.
}

void ImageTestView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Image Test view UI");
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);
    
    setup_ui(container);
    setup_button_handlers();
}

// --- UI Setup ---
void ImageTestView::setup_ui(lv_obj_t* parent) {
    // Create a title
    lv_obj_t *title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Test Image");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    // Create the LVGL image object
    lv_obj_t *img = lv_img_create(parent);

    // Assign the image source from our .c asset file
    lv_img_set_src(img, &icon_1);
    
    // Center the image on the screen
    lv_obj_center(img);

    // Create an instruction label
    lv_obj_t *info_label = lv_label_create(parent);
    lv_label_set_text(info_label, "Press Cancel to return");
    lv_obj_align(info_label, LV_ALIGN_BOTTOM_MID, 0, -20);
}

// --- Button Handling ---
void ImageTestView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, ImageTestView::cancel_press_cb, true, this);
}

// --- Instance Methods for Button Actions ---
void ImageTestView::on_cancel_press() {
    ESP_LOGI(TAG, "Cancel pressed, returning to menu.");
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Static Callbacks (Bridges) ---
void ImageTestView::cancel_press_cb(void* user_data) {
    // Cast the user_data back to a pointer to our class instance and call the member function.
    static_cast<ImageTestView*>(user_data)->on_cancel_press();
}