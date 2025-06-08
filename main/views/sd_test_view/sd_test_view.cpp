#include "sd_test_view.h"
#include "../view_manager.h"
#include "../file_explorer/file_explorer.h" 
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "esp_log.h"

static const char *TAG = "SD_TEST_VIEW";

// --- Callbacks para el explorador ---

// Se llama cuando el explorador selecciona un archivo
static void on_file_selected(const char *path) {
    ESP_LOGI(TAG, "Archivo seleccionado en SD Test: %s", path);
    // Aquí podríamos, por ejemplo, mostrar el contenido si es un .txt
    // o simplemente volver al menú. Por ahora, solo log.
}

// Se llama cuando el explorador se cierra
static void on_explorer_exit() {
    file_explorer_destroy(); // Limpiamos los recursos del explorador
    view_manager_load_view(VIEW_ID_MENU); // Volvemos al menú
}


// --- Creación de la vista ---

void sd_test_view_create(lv_obj_t *parent) {
    // 1. Crear contenedor principal y título
    lv_obj_t * main_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, "Explorador SD");
    lv_obj_set_style_text_font(title_label, lv_theme_get_font_large(title_label), 0);
    lv_obj_set_style_margin_bottom(title_label, 10, 0);

    // 2. Crear un contenedor para el explorador
    lv_obj_t * explorer_container = lv_obj_create(main_cont);
    lv_obj_remove_style_all(explorer_container);
    lv_obj_set_size(explorer_container, lv_pct(95), lv_pct(85));

    // 3. Crear la instancia del explorador de archivos
    file_explorer_create(
        explorer_container,
        sd_manager_get_mount_point(),
        on_file_selected,
        on_explorer_exit
    );
}