#include "mic_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"

static void handle_cancel_press() {
    // Return to the menu view
    view_manager_load_view(VIEW_ID_MENU);
}

void mic_test_view_create(lv_obj_t *parent) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_label_set_text(label, "Test Microphone");
    lv_obj_center(label);

    // Register the handler for the CANCEL button
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press);
}