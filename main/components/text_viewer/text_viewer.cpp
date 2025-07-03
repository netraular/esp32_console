#include "components/text_viewer/text_viewer.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"

static const char *TAG = "COMP_TEXT_VIEWER";

// Estructura para guardar el estado y los callbacks del componente
typedef struct {
    char* content_to_free;
    text_viewer_exit_callback_t on_exit_cb;
} text_viewer_data_t;

// --- Manejadores de eventos y botones internos ---

static void handle_exit_press(void) {
    // Usamos el user_data de la pantalla activa para encontrar el objeto viewer
    lv_obj_t* viewer = (lv_obj_t*)lv_obj_get_user_data(lv_screen_active());
    if (!viewer) return;

    text_viewer_data_t* data = (text_viewer_data_t*)lv_obj_get_user_data(viewer);
    if (data && data->on_exit_cb) {
        data->on_exit_cb();
    }
}

// --- CORRECCIÓN ---
// Centralizamos toda la lógica de limpieza en el callback del contenedor principal.
static void viewer_container_delete_cb(lv_event_t * e) {
    text_viewer_data_t* data = (text_viewer_data_t*)lv_event_get_user_data(e);
    if (data) {
        ESP_LOGD(TAG, "Cleaning up text viewer component resources.");
        // 1. Liberar el contenido del archivo
        if (data->content_to_free) {
            ESP_LOGD(TAG, "Freeing text content memory.");
            free(data->content_to_free);
            data->content_to_free = NULL;
        }
        // 2. Liberar la estructura de datos del componente
        ESP_LOGD(TAG, "Freeing viewer user_data struct.");
        free(data);
    }
}


// --- Funciones Públicas ---

lv_obj_t* text_viewer_create(lv_obj_t* parent, const char* title, char* content, text_viewer_exit_callback_t on_exit) {
    ESP_LOGI(TAG, "Creating text viewer for: %s", title);

    // Crear la estructura de datos para el componente
    text_viewer_data_t* data = (text_viewer_data_t*)malloc(sizeof(text_viewer_data_t));
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate memory for viewer data");
        free(content); // Liberar el contenido para evitar fugas
        return NULL;
    }
    data->content_to_free = content;
    data->on_exit_cb = on_exit;

    // Contenedor principal
    lv_obj_t* main_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_user_data(main_cont, data);
    // --- CORRECCIÓN ---
    // Asignar el único callback de limpieza al contenedor principal
    lv_obj_add_event_cb(main_cont, viewer_container_delete_cb, LV_EVENT_DELETE, data);

    // Asociar el contenedor a la pantalla para poder recuperarlo en el handler del botón
    lv_obj_set_user_data(lv_screen_active(), main_cont);

    // Etiqueta de título
    lv_obj_t* title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, lv_theme_get_font_large(title_label), 0);
    lv_obj_set_style_margin_bottom(title_label, 5, 0);

    // Área de texto
    lv_obj_t* text_area = lv_textarea_create(main_cont);
    lv_obj_set_size(text_area, lv_pct(95), lv_pct(85));
    lv_textarea_set_text(text_area, content);
    // --- CORRECCIÓN ---
    // Eliminar el callback de borrado del text_area. Su padre se encarga de todo.
    // lv_obj_add_event_cb(text_area, text_area_delete_cb, LV_EVENT_DELETE, data);

    // Registrar handlers de botones
    button_manager_unregister_view_handlers(); // Limpiar handlers anteriores
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_exit_press, true);
    // Anular otros botones para evitar acciones no deseadas
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, NULL, true);
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, NULL, true);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, NULL, true);

    return main_cont;
}

void text_viewer_destroy(lv_obj_t* viewer) {
    if (viewer) {
        ESP_LOGI(TAG, "Destroying text viewer component.");
        // Al llamar a lv_obj_del, se disparará automáticamente el LV_EVENT_DELETE
        // y nuestro callback 'viewer_container_delete_cb' hará toda la limpieza.
        lv_obj_del(viewer);
    }
}