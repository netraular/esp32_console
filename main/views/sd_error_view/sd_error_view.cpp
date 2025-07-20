#include "sd_error_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"

static const char* TAG = "SdErrorView";

void SdErrorView::create(lv_obj_t* parent) {
    ESP_LOGW(TAG, "Creating SD Card Error view. System halted until SD is present.");

    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, lv_pct(100), lv_pct(100));
    lv_obj_center(container);

    lv_obj_t* title_label = lv_label_create(container);
    lv_label_set_text(title_label, "SD Card Required");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t* body_label = lv_label_create(