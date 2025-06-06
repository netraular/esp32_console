#include "view_manager.h"
#include "esp_log.h"
#include "controllers/button_manager/button_manager.h"

// Incluir las cabeceras de todas las vistas que este gestor puede manejar
#include "menu_view/menu_view.h"
#include "mic_test_view/mic_test_view.h"
#include "speaker_test_view/speaker_test_view.h"

static const char *TAG = "VIEW_MGR";
static view_id_t current_view_id;

void view_manager_init(void) {
    ESP_LOGI(TAG, "Initializing View Manager and loading initial view.");
    // La primera vista es el menú
    view_manager_load_view(VIEW_ID_MENU);
}

void view_manager_load_view(view_id_t view_id) {
    if (view_id >= VIEW_ID_COUNT) {
        ESP_LOGE(TAG, "Invalid view ID: %d", view_id);
        return;
    }

    ESP_LOGI(TAG, "Loading view %d", view_id);

    // 1. Des-registrar los handlers de la vista anterior para evitar comportamientos no deseados.
    button_manager_unregister_view_handlers();

    // 2. Limpiar la pantalla actual de todos sus objetos.
    // lv_obj_clean() es una forma eficiente de eliminar todos los hijos de un objeto.
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);

    // 3. Crear la nueva vista y registrar sus handlers de botón.
    switch (view_id) {
        case VIEW_ID_MENU:
            menu_view_create(scr);
            break;
        case VIEW_ID_MIC_TEST:
            mic_test_view_create(scr);
            break;
        case VIEW_ID_SPEAKER_TEST:
            speaker_test_view_create(scr);
            break;
        default:
            break;
    }

    // 4. Actualizar el ID de la vista actual.
    current_view_id = view_id;
    ESP_LOGI(TAG, "View %d loaded successfully.", view_id);
}