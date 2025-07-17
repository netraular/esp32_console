#include "image_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "lvgl.h"

// External declaration for the image data
// It's good practice to wrap C declarations in extern "C" when in a .cpp file
extern "C" {
    extern const lv_image_dsc_t icon_1;
}

static void handle_cancel_press(void* user_data) {
    view_manager_load_view(VIEW_ID_MENU);
}

void image_test_view_create(lv_obj_t *parent) {
    // Create a title
    lv_obj_t *title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Test Image");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    // Create the LVGL image object
    lv_obj_t *img = lv_img_create(parent);

    // Assign the image source from our .c file
    lv_img_set_src(img, &icon_1);
    
    // Center the image on the screen
    lv_obj_center(img);

    // Create an instruction label
    lv_obj_t *info_label = lv_label_create(parent);
    lv_label_set_text(info_label, "Press Cancel to return");
    lv_obj_align(info_label, LV_ALIGN_BOTTOM_MID, 0, -20);

    // Register a handler for the cancel button.
    // The view_manager will automatically unregister this when the view changes.
    // The 'true' flag marks this as a view-specific handler.
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, nullptr);
}