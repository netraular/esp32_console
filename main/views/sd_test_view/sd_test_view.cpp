#include "sd_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h" // Incluir el nuevo gestor
#include "esp_log.h"

static const char *TAG = "SD_TEST_VIEW";

static void handle_cancel_press() {
    view_manager_load_view(VIEW_ID_MENU);
}

void sd_test_view_create(lv_obj_t *parent) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    
    // --- LÍNEA ELIMINADA ---
    // lv_label_set_recolor(label, true); // ESTA LÍNEA CAUSA EL ERROR Y YA NO ES NECESARIA

    lv_obj_set_width(label, LV_PCT(90));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);

    if (sd_manager_is_mounted()) {
        ESP_LOGI(TAG, "SD Card is mounted. Listing files...");
        // Esto funcionará sin la línea eliminada, ya que el recoloreo es por defecto.
        lv_label_set_text(label, "#00ff00 SD OK#\nListando archivos en consola...");
        sd_manager_list_files(sd_manager_get_mount_point());
    } else {
        ESP_LOGE(TAG, "SD Card is not mounted.");
        lv_label_set_text(label, "#ff0000 Error#\nNo se pudo montar la tarjeta SD. Revisa las conexiones.");
    }

    lv_obj_center(label);

    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press);
}