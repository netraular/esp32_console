#include "sd_test_view.h"
#include "../view_manager.h"
#include "../file_explorer/file_explorer.h" 
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/button_manager/button_manager.h" // [FIX] Añadir esta línea
#include "esp_log.h"

static const char *TAG = "SD_TEST_VIEW";

// --- [NUEVO] Punteros estáticos para gestionar la UI ---
static lv_obj_t *view_parent = NULL;
static lv_obj_t *info_label_widget = NULL;

// --- Prototipos de funciones ---
static void create_initial_sd_view();
static void show_file_explorer();
static void handle_ok_press_initial();
static void handle_cancel_press_initial();
static void on_file_selected(const char *path);
static void on_explorer_exit();

// --- Callbacks para el explorador ---

static void on_file_selected(const char *path) {
    ESP_LOGI(TAG, "Archivo seleccionado en SD Test: %s", path);
    // En una implementación real, aquí se podría hacer algo con el archivo.
    // Por ahora, volvemos a la pantalla inicial del test.
    file_explorer_destroy();
    create_initial_sd_view();
}

static void on_explorer_exit() {
    file_explorer_destroy(); // Limpiamos los recursos del explorador
    // Volvemos a la pantalla inicial de esta vista, no al menú principal.
    // Esto permite volver a intentar abrir el explorador.
    create_initial_sd_view();
}

// --- Lógica para mostrar el explorador ---
static void show_file_explorer() {
    // Limpiar la vista inicial (etiquetas, etc.)
    lv_obj_clean(view_parent);

    // 1. Crear contenedor principal y título
    lv_obj_t * main_cont = lv_obj_create(view_parent);
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


// --- Handlers para la vista inicial ---
static void handle_ok_press_initial() {
    ESP_LOGI(TAG, "Botón OK presionado. Forzando re-verificación de la SD...");

    // Esta es la operación larga y bloqueante.
    // Ahora se ejecuta en un callback, no durante la creación de la vista.
    sd_manager_unmount();
    if (sd_manager_mount()) {
        ESP_LOGI(TAG, "Montaje exitoso. Abriendo el explorador.");
        show_file_explorer(); // Si tiene éxito, mostramos el explorador
    } else {
        ESP_LOGW(TAG, "El montaje de la SD falló.");
        // Si falla, actualizamos la etiqueta para informar al usuario.
        if (info_label_widget) {
            lv_label_set_text(info_label_widget, "Fallo al leer la SD.\n\n"
                                                 "Revise la tarjeta y\n"
                                                 "presione OK para reintentar.");
        }
    }
}

static void handle_cancel_press_initial() {
    // El botón Cancel en la vista inicial siempre vuelve al menú principal.
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Creación de la vista inicial ---
static void create_initial_sd_view() {
    lv_obj_clean(view_parent); // Limpiar por si venimos del explorador

    lv_obj_t *label = lv_label_create(view_parent);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_label_set_text(label, "Test SD");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    info_label_widget = lv_label_create(view_parent);
    lv_obj_set_style_text_align(info_label_widget, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label_widget);
    lv_label_set_text(info_label_widget, "Presiona OK para\nabrir el explorador");

    // Registrar los handlers de botones para esta pantalla inicial
    button_manager_register_view_handler(BUTTON_OK, handle_ok_press_initial);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press_initial);
}


// --- Función Pública Principal ---
void sd_test_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creando la vista de Test SD (pantalla inicial).");
    view_parent = parent; // Guardar el objeto padre
    
    // La creación de la vista ahora solo llama a esta función rápida.
    // No hay operaciones bloqueantes aquí.
    create_initial_sd_view();
}