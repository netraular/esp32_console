#include "file_explorer.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "esp_log.h"
#include <string.h>

// --- Variables estáticas del componente ---
static const char *TAG = "FILE_EXPLORER";
static lv_group_t *explorer_group = nullptr;
static lv_style_t style_focused;
static lv_obj_t* list_widget = nullptr;
static char current_path[256];
static char mount_point[32];

// [CAMBIO] Variable para gestionar el estado de error (SD no montada)
static bool in_error_state = false;

// Callbacks proporcionados por el usuario del componente
static file_select_callback_t on_file_select_cb = NULL;
static file_explorer_exit_callback_t on_exit_cb = NULL;

// Estructuras de datos internas
typedef struct { bool is_dir; } list_item_data_t;
typedef struct { lv_obj_t *list; lv_group_t *group; } add_file_context_t;

// --- Prototipos Internos ---
static void repopulate_list_cb(lv_timer_t *timer);
static void schedule_repopulate_list();
static void clear_list_items(bool show_loading);
static void list_item_delete_cb(lv_event_t * e);
static void add_file_entry_to_list(const char *name, bool is_dir, void *user_data);
static void focus_changed_cb(lv_group_t * group);
static void handle_right_press();
static void handle_left_press();
static void handle_cancel_press();
static void handle_ok_press();

// --- Handlers y Callbacks ---

static void handle_right_press() {
    // [CAMBIO] No hacer nada si estamos en estado de error
    if (in_error_state) return;
    if (explorer_group) lv_group_focus_next(explorer_group);
}

static void handle_left_press() {
    // [CAMBIO] No hacer nada si estamos en estado de error
    if (in_error_state) return;
    if (explorer_group) lv_group_focus_prev(explorer_group);
}

static void handle_ok_press() {
    // [CAMBIO] Lógica de recuperación si estamos en estado de error
    if (in_error_state) {
        ESP_LOGI(TAG, "Reintentando montaje de la tarjeta SD...");
        sd_manager_unmount(); // Forzar desmontaje por si acaso
        if (sd_manager_mount()) {
            ESP_LOGI(TAG, "Montaje exitoso. Recargando lista de archivos.");
            in_error_state = false; // Salir del estado de error
            schedule_repopulate_list();
        } else {
            ESP_LOGW(TAG, "El montaje falló de nuevo.");
            // Mantenemos el mensaje de error en la pantalla
        }
        return;
    }
    
    // Lógica original si no hay error
    lv_obj_t * focused_obj = lv_group_get_focused(explorer_group);
    if (!focused_obj) return;

    list_item_data_t* item_data = static_cast<list_item_data_t*>(lv_obj_get_user_data(focused_obj));
    if (!item_data) return;

    const char* entry_name = lv_list_get_button_text(list_widget, focused_obj);

    if (item_data->is_dir) {
        // Navegar a un subdirectorio
        ESP_LOGI(TAG, "Entrando en el directorio: %s", entry_name);
        if (current_path[strlen(current_path) - 1] != '/') {
            strcat(current_path, "/");
        }
        strcat(current_path, entry_name);
        schedule_repopulate_list();
    } else {
        // Es un archivo, notificar al llamador
        ESP_LOGI(TAG, "Archivo seleccionado: %s", entry_name);
        if (on_file_select_cb) {
            char full_path[300];
            snprintf(full_path, sizeof(full_path), "%s/%s", current_path, entry_name);
            on_file_select_cb(full_path);
        }
    }
}

static void handle_cancel_press() {
    // [CAMBIO] La cancelación siempre debe funcionar para poder salir
    if (in_error_state || strcmp(current_path, mount_point) == 0) {
        // Estamos en la raíz o en estado de error, notificar para salir
        ESP_LOGI(TAG, "Saliendo del explorador.");
        if (on_exit_cb) {
            on_exit_cb();
        }
        return;
    }
    
    // Navegar al directorio padre
    ESP_LOGI(TAG, "Volviendo al directorio padre desde: %s", current_path);
    char *last_slash = strrchr(current_path, '/');
    if (last_slash > current_path && (size_t)(last_slash - current_path) >= strlen(mount_point)) {
         *last_slash = '\0';
    } else {
         strcpy(current_path, mount_point);
    }
    ESP_LOGI(TAG, "Nueva ruta: %s", current_path);

    schedule_repopulate_list();
}

static void focus_changed_cb(lv_group_t * group) {
    lv_obj_t * focused_obj = lv_group_get_focused(group);
    if (!focused_obj) return;
    lv_obj_scroll_to_view(focused_obj, LV_ANIM_ON);
}

static void list_item_delete_cb(lv_event_t * e) {
    lv_obj_t* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    list_item_data_t* item_data = static_cast<list_item_data_t*>(lv_obj_get_user_data(btn));
    if (item_data) {
        free(item_data);
    }
}

static void clear_list_items(bool show_loading) {
    if (!list_widget) return;
    lv_group_remove_all_objs(explorer_group);
    lv_obj_clean(list_widget);
    if (show_loading) {
        lv_list_add_text(list_widget, "Cargando...");
    }
}

static void repopulate_list_cb(lv_timer_t *timer) {
    clear_list_items(false);

    if (sd_manager_is_mounted()) {
        in_error_state = false; // [CAMBIO] Asegurarse de que no estamos en estado de error
        add_file_context_t context = { .list = list_widget, .group = explorer_group };
        sd_manager_list_files(current_path, add_file_entry_to_list, &context);
    } else {
        in_error_state = true; // [CAMBIO] Entrar en estado de error
        // [CAMBIO] Mostrar un mensaje de error más informativo
        lv_obj_t* label = lv_label_create(list_widget);
        lv_label_set_text(label, "Error: Tarjeta SD no encontrada.\n\n"
                                 "Presione OK para reintentar.\n"
                                 "Presione CANCEL para salir.");
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(label, lv_pct(95));
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(label);
    }

    if (lv_group_get_obj_count(explorer_group) > 0) {
        lv_group_focus_next(explorer_group);
    }
    
    lv_timer_delete(timer);
}

static void schedule_repopulate_list() {
    clear_list_items(true);
    lv_timer_create(repopulate_list_cb, 10, NULL);
}

static void add_file_entry_to_list(const char *name, bool is_dir, void *user_data) {
    add_file_context_t *context = static_cast<add_file_context_t *>(user_data);
    if (!context || !context->list || !context->group) return;

    const char *icon = is_dir ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE;
    lv_obj_t *btn = lv_list_add_button(context->list, icon, name);

    list_item_data_t* item_data = static_cast<list_item_data_t*>(malloc(sizeof(list_item_data_t)));
    if (item_data) {
        item_data->is_dir = is_dir;
        lv_obj_set_user_data(btn, item_data);
        lv_obj_add_event_cb(btn, list_item_delete_cb, LV_EVENT_DELETE, NULL);
    }

    lv_obj_add_style(btn, &style_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(context->group, btn);

    lv_obj_t *label = lv_obj_get_child(btn, 1);
    if(label) {
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(label, lv_pct(90));
    }
}

// --- Implementación de Funciones Públicas ---

void file_explorer_destroy(void) {
    // Desregistrar handlers para evitar llamadas a un componente destruido
    button_manager_unregister_view_handlers();

    if (explorer_group) {
        if (lv_group_get_default() == explorer_group) {
            lv_group_set_default(NULL);
        }
        lv_group_delete(explorer_group);
        explorer_group = nullptr;
    }
    // `list_widget` se elimina cuando se limpia su padre, no hace falta eliminarlo manualmente.
    list_widget = nullptr;
    on_file_select_cb = NULL;
    on_exit_cb = NULL;
    in_error_state = false; // [CAMBIO] Resetear estado al destruir
    ESP_LOGI(TAG, "File explorer destroyed.");
}

void file_explorer_create(lv_obj_t *parent, const char *initial_path, file_select_callback_t on_select, file_explorer_exit_callback_t on_exit) {
    ESP_LOGI(TAG, "Creating file explorer at path: %s", initial_path);
    
    // Guardar callbacks y ruta inicial
    on_file_select_cb = on_select;
    on_exit_cb = on_exit;
    strncpy(current_path, initial_path, sizeof(current_path) - 1);
    strncpy(mount_point, initial_path, sizeof(mount_point) - 1);
    
    in_error_state = false; // [CAMBIO] Inicializar estado

    // Configurar grupo de LVGL para navegación por botones
    explorer_group = lv_group_create();
    lv_group_set_wrap(explorer_group, true);
    lv_group_set_focus_cb(explorer_group, focus_changed_cb);
    lv_group_set_default(explorer_group);

    // Estilo para el elemento enfocado
    lv_style_init(&style_focused);
    lv_style_set_bg_color(&style_focused, lv_palette_main(LV_PALETTE_LIGHT_BLUE));
    lv_style_set_bg_opa(&style_focused, LV_OPA_COVER);

    // Crear la lista
    list_widget = lv_list_create(parent);
    lv_obj_set_size(list_widget, lv_pct(100), lv_pct(100));
    lv_obj_center(list_widget);

    // Llenar la lista de forma asíncrona
    schedule_repopulate_list();

    // Registrar los handlers de los botones físicos
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press);
    button_manager_register_view_handler(BUTTON_OK, handle_ok_press);
    button_manager_register_view_handler(BUTTON_RIGHT, handle_right_press);
    button_manager_register_view_handler(BUTTON_LEFT, handle_left_press);
}