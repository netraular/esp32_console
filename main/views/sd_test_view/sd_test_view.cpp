#include "sd_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"

static void handle_cancel_press() {
    view_manager_load_view(VIEW_ID_MENU);
}

void sd_test_view_create(lv_obj_t *parent) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_label_set_text(label, "Test SD");
    lv_obj_center(label);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press);
}