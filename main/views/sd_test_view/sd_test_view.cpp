#include "sd_test_view.h"
#include "../view_manager.h"
#include "../file_explorer/file_explorer.h" 
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"
#include <string.h>
#include <time.h> 

static const char *TAG = "SD_TEST_VIEW";

static lv_obj_t *view_parent = NULL;
static lv_obj_t *info_label_widget = NULL;

// --- Prototipos de funciones ---
static void create_initial_sd_view();
static void show_file_explorer();
static void handle_ok_press_initial();
static void handle_cancel_press_initial();
static void on_file_selected(const char *path);
static void on_create_action(file_item_type_t action_type, const char *current_path);
static void on_explorer_exit();

// Nuevas funciones para el visor de texto
static void handle_cancel_from_viewer();
static void create_text_viewer(const char* title, char* content);
static void text_viewer_delete_cb(lv_event_t * e);


// --- Implementación de las funciones de la UI ---

/**
 * @brief Callback para liberar la memoria del buffer de texto cuando se elimina el objeto contenedor.
 * Se invoca automáticamente cuando se limpia la pantalla.
 */
static void text_viewer_delete_cb(lv_event_t * e) {
    char* text_content = (char*)lv_event_get_user_data(e);
    if (text_content) {
        free(text_content);
        ESP_LOGI(TAG, "Text viewer content buffer freed.");
    }
}

/**
 * @brief Handler para el botón de cancelar cuando se está en el visor de texto.
 * Vuelve al explorador de archivos.
 */
static void handle_cancel_from_viewer() {
    ESP_LOGI(TAG, "Returning to file explorer.");
    show_file_explorer();
}

/**
 * @brief Crea la UI del visor de texto. Esta función toma posesión del puntero 'content'.
 * @param title El título a mostrar (ej. el nombre del archivo).
 * @param content El contenido de texto a mostrar. Debe ser un buffer alocado con malloc.
 */
static void create_text_viewer(const char* title, char* content) {
    button_manager_unregister_view_handlers(); // Limpia handlers anteriores
    lv_obj_clean(view_parent);

    // Contenedor principal con layout flex para organizar título y contenido
    lv_obj_t* main_cont = lv_obj_create(view_parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Etiqueta para el título (nombre del archivo o "Error")
    lv_obj_t* title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, lv_theme_get_font_large(title_label), 0);
    lv_obj_set_style_margin_top(title_label, 5, 0);
    lv_obj_set_style_margin_bottom(title_label, 5, 0);

    // Contenedor scrollable para el contenido del texto
    lv_obj_t* text_cont = lv_obj_create(main_cont);
    lv_obj_set_size(text_cont, lv_pct(95), lv_pct(85));
    
    // Asocia el puntero 'content' al contenedor para que se libere al eliminarlo
    lv_obj_add_event_cb(text_cont, text_viewer_delete_cb, LV_EVENT_DELETE, content);

    // Etiqueta para el contenido del archivo, dentro del contenedor scrollable
    lv_obj_t* content_label = lv_label_create(text_cont);
    lv_label_set_text(content_label, content);
    lv_label_set_long_mode(content_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(content_label, lv_pct(100));

    // Registra el handler del botón Cancelar para esta vista específica
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_from_viewer);
}

/**
 * @brief Callback que se ejecuta al seleccionar un archivo en el explorador.
 * Lee el archivo y muestra su contenido en el visor de texto.
 */
static void on_file_selected(const char *path) {
    ESP_LOGI(TAG, "File selected to read: %s", path);
    
    char* file_content = NULL;
    size_t file_size = 0;
    const char* filename = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;

    if (sd_manager_read_file(path, &file_content, &file_size)) {
        // Éxito: muestra el contenido del archivo
        create_text_viewer(filename, file_content); // Pasa la propiedad del buffer
    } else {
        // Error: muestra un mensaje de error en el mismo tipo de visor
        char* error_msg = (char*)malloc(128);
        if (error_msg) {
            snprintf(error_msg, 128, "Could not read file.\n\nPlease check SD card\nand press Cancel to return.");
            create_text_viewer("Error", error_msg);
        }
        if (file_content) {
            free(file_content); // Debería ser NULL, pero por si acaso.
        }
    }
}

// Callback que se ejecuta al seleccionar una acción como "[+] Crear Archivo"
static void on_create_action(file_item_type_t action_type, const char *current_path) {
    ESP_LOGI(TAG, "Create action triggered in path: %s", current_path);

    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char basename[32];
    strftime(basename, sizeof(basename), "%H%M%S", &timeinfo);
    
    char full_path[256];

    if (action_type == ITEM_TYPE_ACTION_CREATE_FILE) {
        snprintf(full_path, sizeof(full_path), "%s/%s.txt", current_path, basename);
        if (sd_manager_create_file(full_path)) {
            sd_manager_write_file(full_path, "Esto es un archivo de prueba.");
        }
    } else if (action_type == ITEM_TYPE_ACTION_CREATE_FOLDER) {
        snprintf(full_path, sizeof(full_path), "%s/%s", current_path, basename);
        sd_manager_create_directory(full_path);
    }

    file_explorer_refresh();
}

// Se llama cuando el usuario sale del explorador (presionando Cancelar en la raíz)
static void on_explorer_exit() {
    file_explorer_destroy(); 
    create_initial_sd_view(); 
}

// Lanza el componente del explorador de archivos
static void show_file_explorer() {
    lv_obj_clean(view_parent);

    lv_obj_t * main_cont = lv_obj_create(view_parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, "SD Explorer");
    lv_obj_set_style_text_font(title_label, lv_theme_get_font_large(title_label), 0);
    lv_obj_set_style_margin_bottom(title_label, 10, 0);

    lv_obj_t * explorer_container = lv_obj_create(main_cont);
    lv_obj_remove_style_all(explorer_container);
    lv_obj_set_size(explorer_container, lv_pct(95), lv_pct(85));

    file_explorer_create(
        explorer_container,
        sd_manager_get_mount_point(),
        on_file_selected,
        on_create_action,
        on_explorer_exit
    );
}

// Handler del botón OK en la pantalla inicial
static void handle_ok_press_initial() {
    ESP_LOGI(TAG, "OK button pressed. Checking SD card status...");
    sd_manager_unmount(); 
    if (sd_manager_mount()) {
        ESP_LOGI(TAG, "Mount successful. Opening the explorer.");
        show_file_explorer(); 
    } else {
        ESP_LOGW(TAG, "SD card mount failed.");
        if (info_label_widget) {
            lv_label_set_text(info_label_widget, "Failed to read SD card.\n\n"
                                                 "Check the card and\n"
                                                 "press OK to retry.");
        }
    }
}

// Handler del botón Cancelar en la pantalla inicial
static void handle_cancel_press_initial() {
    view_manager_load_view(VIEW_ID_MENU);
}

// Crea la UI para la vista inicial
static void create_initial_sd_view() {
    lv_obj_clean(view_parent); 

    lv_obj_t *label = lv_label_create(view_parent);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_label_set_text(label, "SD Test");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    info_label_widget = lv_label_create(view_parent);
    lv_obj_set_style_text_align(info_label_widget, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label_widget);
    lv_label_set_text(info_label_widget, "Press OK to\nopen the explorer");

    button_manager_register_view_handler(BUTTON_OK, handle_ok_press_initial);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press_initial);
}

void sd_test_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating SD Test view (initial screen).");
    view_parent = parent; 
    create_initial_sd_view();
}