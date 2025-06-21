#include "sd_test_view.h"
#include "../view_manager.h"
#include "../file_explorer/file_explorer.h" 
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"
#include <string.h>
#include <time.h> 
#include <sys/stat.h>

static const char *TAG = "SD_TEST_VIEW";

// --- Variables de estado de la vista ---
static lv_obj_t *view_parent = NULL;
static lv_obj_t *info_label_widget = NULL;

// --- Variables para el menú de acciones ---
static lv_obj_t *action_menu_container = NULL; 
static lv_group_t *action_menu_group = NULL;   
static char selected_item_path[256];
static lv_style_t style_action_menu_focused;

// --- Prototipos de funciones ---
static void create_initial_sd_view();
static void show_file_explorer();
static void on_file_selected(const char *path);
static void handle_cancel_from_viewer();
static void destroy_action_menu();
static void handle_action_menu_ok();
static void handle_action_menu_left();
static void handle_action_menu_right();


// =================================================================
// Implementación del Menú de Acciones
// =================================================================

static void handle_action_menu_left() {
    if (action_menu_group) {
        lv_group_focus_prev(action_menu_group);
    }
}

static void handle_action_menu_right() {
    if (action_menu_group) {
        lv_group_focus_next(action_menu_group);
    }
}

static void handle_action_menu_ok() {
    lv_obj_t* selected_btn = lv_group_get_focused(action_menu_group);
    if (!selected_btn) return;
    
    // <<< CORRECCIÓN: Obtener la lista (padre del botón) y luego el texto del botón
    lv_obj_t* list = lv_obj_get_parent(selected_btn);
    const char* action_text = lv_list_get_button_text(list, selected_btn);

    ESP_LOGI(TAG, "Acción '%s' seleccionada para: %s", action_text, selected_item_path);

    bool should_destroy_menu = true;

    if (strcmp(action_text, "Leer") == 0) {
        on_file_selected(selected_item_path);
        // El lector de texto se encargará de destruir el menú al limpiar la pantalla
        should_destroy_menu = false; 
    } 
    else if (strcmp(action_text, "Renombrar") == 0) {
        char new_path[256] = {0};
        const char* dir_end = strrchr(selected_item_path, '/');
        if (dir_end) {
            strncpy(new_path, selected_item_path, dir_end - selected_item_path + 1);
        }

        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        char basename[32];
        strftime(basename, sizeof(basename), "%Y%m%d_%H%M%S", &timeinfo);
        
        const char* ext = strrchr(selected_item_path, '.');
        if (!ext) ext = "";

        strncat(new_path, basename, sizeof(new_path) - strlen(new_path) - 1);
        strncat(new_path, ext, sizeof(new_path) - strlen(new_path) - 1);

        ESP_LOGI(TAG, "Renombrando '%s' -> '%s'", selected_item_path, new_path);
        sd_manager_rename_item(selected_item_path, new_path);
    } 
    else if (strcmp(action_text, "Eliminar") == 0) {
        sd_manager_delete_item(selected_item_path);
    }

    if (should_destroy_menu) {
        destroy_action_menu();
    }
}

/**
 * @brief Crea el menú emergente con las acciones para un archivo.
 * @param path La ruta completa del archivo seleccionado.
 */
static void create_action_menu(const char* path) {
    if (action_menu_container) {
        return; 
    }
    strncpy(selected_item_path, path, sizeof(selected_item_path) - 1);

    // Estilo para el feedback visual
    lv_style_init(&style_action_menu_focused);
    lv_style_set_bg_color(&style_action_menu_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_opa(&style_action_menu_focused, LV_OPA_COVER);
    
    // <<< Telón de fondo semitransparente. Ahora es el padre del contenedor del menú.
    action_menu_container = lv_obj_create(view_parent);
    lv_obj_remove_style_all(action_menu_container);
    lv_obj_set_size(action_menu_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(action_menu_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(action_menu_container, LV_OPA_50, 0);
    
    // <<< Contenedor para la lista, que simula una ventana modal.
    lv_obj_t* menu_box = lv_obj_create(action_menu_container);
    lv_obj_set_width(menu_box, lv_pct(80));          // Ocupa el 80% del ancho
    lv_obj_set_height(menu_box, LV_SIZE_CONTENT);   // Altura automática según contenido
    lv_obj_center(menu_box);                        // Centrar el contenedor
    lv_obj_set_style_pad_all(menu_box, 10, 0);

    // La lista ahora se crea dentro del 'menu_box'
    lv_obj_t* list = lv_list_create(menu_box);
    lv_obj_set_size(list, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_center(list);

    action_menu_group = lv_group_create();

    // <<< CORRECCIÓN: Añadir cada botón al grupo individualmente
    lv_obj_t* btn;
    btn = lv_list_add_button(list, LV_SYMBOL_EYE_OPEN, "Leer");
    lv_obj_add_style(btn, &style_action_menu_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(action_menu_group, btn);

    btn = lv_list_add_button(list, LV_SYMBOL_EDIT, "Renombrar");
    lv_obj_add_style(btn, &style_action_menu_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(action_menu_group, btn);

    btn = lv_list_add_button(list, LV_SYMBOL_TRASH, "Eliminar");
    lv_obj_add_style(btn, &style_action_menu_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(action_menu_group, btn);


    lv_group_set_default(action_menu_group);
    lv_group_focus_obj(lv_obj_get_child(list, 0)); // Poner foco en el primer botón

    // Registrar manejadores de botones para el menú de acciones
    button_manager_register_view_handler(BUTTON_OK, handle_action_menu_ok);
    button_manager_register_view_handler(BUTTON_CANCEL, destroy_action_menu); 
    button_manager_register_view_handler(BUTTON_LEFT, handle_action_menu_left);
    button_manager_register_view_handler(BUTTON_RIGHT, handle_action_menu_right); 
}

/**
 * @brief Destruye el menú de acciones y restaura los handlers del explorador.
 */
static void destroy_action_menu() {
    if (action_menu_container) {
        lv_group_del(action_menu_group);
        action_menu_group = NULL;
        lv_obj_del(action_menu_container);
        action_menu_container = NULL;

        // Devuelve el control al explorador de archivos sin reconstruirlo
        file_explorer_set_input_active(true);
        // Refresca la lista por si se ha borrado o renombrado algo
        file_explorer_refresh();
    }
}

// =================================================================
// Implementación del Visor de Texto
// =================================================================

static void text_viewer_delete_cb(lv_event_t * e) {
    char* text_content = (char*)lv_event_get_user_data(e);
    if (text_content) {
        free(text_content);
        ESP_LOGI(TAG, "Text viewer content buffer freed.");
    }
}

static void create_text_viewer(const char* title, char* content) {
    // Esta función limpia toda la pantalla, destruyendo el explorador.
    // Es parte del flujo original, pero la vuelta atrás se mejora.
    button_manager_unregister_view_handlers();
    lv_obj_clean(view_parent);

    lv_obj_t* main_cont = lv_obj_create(view_parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, lv_theme_get_font_large(title_label), 0);
    lv_obj_set_style_margin_top(title_label, 5, 0);
    lv_obj_set_style_margin_bottom(title_label, 5, 0);

    lv_obj_t* text_cont = lv_obj_create(main_cont);
    lv_obj_set_size(text_cont, lv_pct(95), lv_pct(85));
    lv_obj_add_event_cb(text_cont, text_viewer_delete_cb, LV_EVENT_DELETE, content);

    lv_obj_t* content_label = lv_label_create(text_cont);
    lv_label_set_text(content_label, content);
    lv_label_set_long_mode(content_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(content_label, lv_pct(100));
    
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_from_viewer);
}

static void handle_cancel_from_viewer() {
    // Al volver del visor, reconstruimos el explorador
    show_file_explorer();
}


// =================================================================
// Lógica Principal de la Vista
// =================================================================

static void on_file_selected(const char *path) {
    char* file_content = NULL;
    size_t file_size = 0;
    const char* filename = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;

    if (sd_manager_read_file(path, &file_content, &file_size)) {
        create_text_viewer(filename, file_content);
    } else {
        char* error_msg = (char*)malloc(128);
        if (error_msg) {
            snprintf(error_msg, 128, "No se pudo leer el archivo.");
            create_text_viewer("Error", error_msg);
        }
        if (file_content) free(file_content);
    }
}

static void on_file_or_dir_selected(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) {
            // Desactivar temporalmente el input del explorador para que no interfiera
            file_explorer_set_input_active(false);
            create_action_menu(path);
        } else {
            ESP_LOGI(TAG, "Directorio seleccionado, no se muestra menú: %s", path);
            // Para entrar en un directorio, el explorador ya lo gestiona con el botón OK.
            // Aquí solo nos interesan las acciones sobre archivos.
        }
    }
}


static void on_create_action(file_item_type_t action_type, const char *current_path) {
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char basename[32];
    strftime(basename, sizeof(basename), "%H%M%S", &timeinfo);
    
    char full_path[256];

    if (action_type == ITEM_TYPE_ACTION_CREATE_FILE) {
        snprintf(full_path, sizeof(full_path), "%s/%s.txt", current_path, basename);
        if (sd_manager_create_file(full_path)) {
            sd_manager_write_file(full_path, "Archivo de prueba.");
        }
    } else if (action_type == ITEM_TYPE_ACTION_CREATE_FOLDER) {
        snprintf(full_path, sizeof(full_path), "%s/%s", current_path, basename);
        sd_manager_create_directory(full_path);
    }
    file_explorer_refresh();
}

static void on_explorer_exit() {
    file_explorer_destroy(); 
    create_initial_sd_view(); 
}

static void show_file_explorer() {
    lv_obj_clean(view_parent);

    lv_obj_t* main_cont = lv_obj_create(view_parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, "SD Explorer");
    lv_obj_set_style_text_font(title_label, lv_theme_get_font_large(title_label), 0);
    lv_obj_set_style_margin_bottom(title_label, 10, 0);

    lv_obj_t* explorer_container = lv_obj_create(main_cont);
    lv_obj_remove_style_all(explorer_container);
    lv_obj_set_size(explorer_container, lv_pct(95), lv_pct(85));

    file_explorer_create(
        explorer_container,
        sd_manager_get_mount_point(),
        on_file_or_dir_selected,
        on_create_action,
        on_explorer_exit
    );
}

static void handle_ok_press_initial() {
    sd_manager_unmount(); 
    if (sd_manager_mount()) {
        show_file_explorer(); 
    } else {
        if (info_label_widget) {
            lv_label_set_text(info_label_widget, "Error al leer la SD.\n\n"
                                                 "Revise la tarjeta y\n"
                                                 "pulse OK para reintentar.");
        }
    }
}

static void handle_cancel_press_initial() {
    view_manager_load_view(VIEW_ID_MENU);
}

static void create_initial_sd_view() {
    lv_obj_clean(view_parent); 

    lv_obj_t* label = lv_label_create(view_parent);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_label_set_text(label, "SD Test");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    info_label_widget = lv_label_create(view_parent);
    lv_obj_set_style_text_align(info_label_widget, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label_widget);
    lv_label_set_text(info_label_widget, "Pulsa OK para\nabrir el explorador");

    button_manager_register_view_handler(BUTTON_OK, handle_ok_press_initial);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press_initial);
}

void sd_test_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creando vista de Test SD (pantalla inicial).");
    view_parent = parent; 
    create_initial_sd_view();
}