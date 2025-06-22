// main/views/sd_test_view/sd_test_view.cpp

#include "sd_test_view.h"
#include "../view_manager.h"
#include "../file_explorer/file_explorer.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h" // Necesario para la cola
#include "freertos/queue.h"   // Necesario para la cola

static const char *TAG = "SD_TEST_VIEW";

// State variables for the view
static lv_obj_t *view_parent = NULL;
static lv_obj_t *info_label_widget = NULL;

// State variables for the file action menu (Read, Rename, Delete)
static lv_obj_t *action_menu_container = NULL;
static lv_group_t *action_menu_group = NULL;
static char selected_item_path[256];
static lv_style_t style_action_menu_focused;

// --- NUEVA ARQUITECTURA PARA ENTRADA DEL MENÚ DE ACCIONES ---
typedef enum {
    ACTION_MENU_NAV_LEFT,
    ACTION_MENU_NAV_RIGHT,
    ACTION_MENU_NAV_OK,
    ACTION_MENU_NAV_CANCEL
} action_menu_input_event_type_t;

static QueueHandle_t action_menu_input_queue = NULL;
static lv_timer_t* action_menu_input_processor_timer = NULL;
// --- FIN DE LA NUEVA ARQUITECTURA ---


// --- Function Prototypes ---
static void create_initial_sd_view();
static void show_file_explorer();
static void on_explorer_exit();
static void on_file_or_dir_selected(const char *path);
static void on_create_action(file_item_type_t action_type, const char *current_path);
static void destroy_action_menu(); // Se mantiene para la lógica de destrucción
static void create_text_viewer(const char* title, char* content);
static void process_action_menu_input_cb(lv_timer_t* timer); // NUEVA función callback del timer

// --- CALLBACK PARA TIMER DE DESTRUCCIÓN (se mantiene por si es útil en otros contextos) ---
static void deferred_destroy_action_menu_cb(lv_timer_t * timer) {
    ESP_LOGD(TAG, "Ejecutando deferred_destroy_action_menu_cb");
    destroy_action_menu();
}


/***************************************************
 *  NUEVA LÓGICA DE MANEJO DE ENTRADA PARA EL MENÚ DE ACCIONES
 ***************************************************/

static void handle_action_menu_ok_press() {
    ESP_LOGD(TAG, "Action Menu OK Press -> Enviando evento a la cola");
    action_menu_input_event_type_t event = ACTION_MENU_NAV_OK;
    if (action_menu_input_queue) xQueueSend(action_menu_input_queue, &event, 0);
}

static void handle_action_menu_cancel_press() {
    ESP_LOGD(TAG, "Action Menu Cancel Press -> Enviando evento a la cola");
    action_menu_input_event_type_t event = ACTION_MENU_NAV_CANCEL;
    if (action_menu_input_queue) xQueueSend(action_menu_input_queue, &event, 0);
}

static void handle_action_menu_left_press()   {
    ESP_LOGD(TAG, "Action Menu Left Press -> Enviando evento a la cola");
    action_menu_input_event_type_t event = ACTION_MENU_NAV_LEFT;
    if (action_menu_input_queue) xQueueSend(action_menu_input_queue, &event, 0);
}
static void handle_action_menu_right_press()  {
    ESP_LOGD(TAG, "Action Menu Right Press -> Enviando evento a la cola");
    action_menu_input_event_type_t event = ACTION_MENU_NAV_RIGHT;
    if (action_menu_input_queue) xQueueSend(action_menu_input_queue, &event, 0);
}

static void process_action_menu_input_cb(lv_timer_t* timer) {
    action_menu_input_event_type_t event;

    if (xQueueReceive(action_menu_input_queue, &event, 0) == pdPASS) {
        const char* event_name = "UNKNOWN_ACTION_MENU_EVENT";
        switch(event) {
            case ACTION_MENU_NAV_LEFT:  event_name = "ACTION_MENU_NAV_LEFT"; break;
            case ACTION_MENU_NAV_RIGHT: event_name = "ACTION_MENU_NAV_RIGHT"; break;
            case ACTION_MENU_NAV_OK:    event_name = "ACTION_MENU_NAV_OK"; break;
            case ACTION_MENU_NAV_CANCEL:event_name = "ACTION_MENU_NAV_CANCEL"; break;
        }
        ESP_LOGD(TAG, "LVGL timer procesando evento del menú de acción: %s", event_name);

        if (!action_menu_group && event != ACTION_MENU_NAV_CANCEL) {
             ESP_LOGW(TAG, "Recibido evento para menú de acción, pero el grupo no existe.");
             if (event == ACTION_MENU_NAV_CANCEL) { // Aún así, permitir cancelar para destruir
                destroy_action_menu(); // Llamada directa segura aquí, ya que estamos en el contexto del timer de LVGL
             }
             return;
        }


        switch (event) {
            case ACTION_MENU_NAV_LEFT:
                if (action_menu_group) lv_group_focus_prev(action_menu_group);
                break;
            case ACTION_MENU_NAV_RIGHT:
                if (action_menu_group) lv_group_focus_next(action_menu_group);
                break;
            case ACTION_MENU_NAV_OK: {
                if (!action_menu_group) break;
                lv_obj_t* selected_btn = lv_group_get_focused(action_menu_group);
                if (!selected_btn) break;

                lv_obj_t* list = lv_obj_get_parent(selected_btn);
                const char* action_text = lv_list_get_button_text(list, selected_btn);
                ESP_LOGI(TAG, "Action '%s' selected for: %s", action_text, selected_item_path);

                if (strcmp(action_text, "Leer") == 0) {
                    char* file_content = NULL;
                    size_t file_size = 0;
                    const char* filename = strrchr(selected_item_path, '/') ? strrchr(selected_item_path, '/') + 1 : selected_item_path;

                    if (sd_manager_read_file(selected_item_path, &file_content, &file_size)) {
                        create_text_viewer(filename, file_content); // Esto limpiará y destruirá el menú de acción actual
                    } else {
                        if(file_content) free(file_content);
                        ESP_LOGE(TAG, "Fallo al leer archivo para el visor.");
                        destroy_action_menu(); // Llamada directa segura
                    }
                } else if (strcmp(action_text, "Renombrar") == 0) {
                    char new_path[256] = {0};
                    const char* dir_end = strrchr(selected_item_path, '/');
                    size_t dir_len = (dir_end) ? (dir_end - selected_item_path) + 1 : 0;
                    if(dir_len > 0) strncpy(new_path, selected_item_path, dir_len);

                    char basename[32];
                    time_t now = time(NULL);
                    struct tm timeinfo;

                    if (now == (time_t)(-1)) {
                         strcpy(basename, "renamed_file");
                    } else {
                        localtime_r(&now, &timeinfo);
                        strftime(basename, sizeof(basename), "%Y%m%d_%H%M%S", &timeinfo);
                    }

                    const char* ext = strrchr(selected_item_path, '.');
                    snprintf(new_path + dir_len, sizeof(new_path) - dir_len, "%s%s", basename, ext ? ext : "");

                    ESP_LOGI(TAG, "Renaming '%s' -> '%s'", selected_item_path, new_path);
                    sd_manager_rename_item(selected_item_path, new_path);
                    destroy_action_menu(); // Llamada directa segura
                } else if (strcmp(action_text, "Eliminar") == 0) {
                    sd_manager_delete_item(selected_item_path);
                    destroy_action_menu(); // Llamada directa segura
                }
                break;
            }
            case ACTION_MENU_NAV_CANCEL:
                ESP_LOGD(TAG, "Evento CANCEL del menú de acción recibido.");
                destroy_action_menu(); // Llamada directa segura
                break;
        }
    }
}


/**********************
 *  ACTION MENU LOGIC (Creación/Destrucción)
 **********************/

static void create_action_menu(const char* path) {
    if (action_menu_container) {
        ESP_LOGW(TAG, "create_action_menu llamado pero el menú ya existe.");
        return;
    }
    ESP_LOGI(TAG, "Creando menú de acción para: %s", path);
    strncpy(selected_item_path, path, sizeof(selected_item_path) - 1);

    file_explorer_set_input_active(false);

    // Crear cola y timer para el menú de acciones
    if (!action_menu_input_queue) { // Crear solo si no existe
        action_menu_input_queue = xQueueCreate(5, sizeof(action_menu_input_event_type_t));
    } else {
        xQueueReset(action_menu_input_queue); // Limpiar cola si ya existía
    }

    if (!action_menu_input_processor_timer) { // Crear solo si no existe
        action_menu_input_processor_timer = lv_timer_create(process_action_menu_input_cb, 20, NULL);
    } else {
        lv_timer_reset(action_menu_input_processor_timer); // Reiniciar timer
        lv_timer_resume(action_menu_input_processor_timer); // Asegurar que esté activo
    }


    lv_style_init(&style_action_menu_focused);
    lv_style_set_bg_color(&style_action_menu_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_opa(&style_action_menu_focused, LV_OPA_COVER);

    action_menu_container = lv_obj_create(view_parent);
    lv_obj_remove_style_all(action_menu_container);
    lv_obj_set_size(action_menu_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(action_menu_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(action_menu_container, LV_OPA_50, 0);
    lv_obj_clear_flag(action_menu_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* menu_box = lv_obj_create(action_menu_container);
    lv_obj_set_width(menu_box, lv_pct(80));
    lv_obj_set_height(menu_box, LV_SIZE_CONTENT);
    lv_obj_center(menu_box);

    lv_obj_t* list = lv_list_create(menu_box);
    lv_obj_set_size(list, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_center(list);

    action_menu_group = lv_group_create();
    const char* actions[][2] = {{LV_SYMBOL_EYE_OPEN, "Leer"}, {LV_SYMBOL_EDIT, "Renombrar"}, {LV_SYMBOL_TRASH, "Eliminar"}};
    for(auto const& action : actions) {
        lv_obj_t* btn = lv_list_add_button(list, action[0], action[1]);
        lv_obj_add_style(btn, &style_action_menu_focused, LV_STATE_FOCUSED);
        lv_group_add_obj(action_menu_group, btn);
    }

    lv_group_set_default(action_menu_group);
    if(lv_obj_get_child_cnt(list) > 0){
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    // Registrar los nuevos manejadores de botones que envían a la cola
    button_manager_register_view_handler(BUTTON_OK, handle_action_menu_ok_press);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_action_menu_cancel_press);
    button_manager_register_view_handler(BUTTON_LEFT, handle_action_menu_left_press);
    button_manager_register_view_handler(BUTTON_RIGHT, handle_action_menu_right_press);
}

static void destroy_action_menu() {
    if (action_menu_container) {
        ESP_LOGI(TAG, "Destruyendo el menú de acción");
        button_manager_unregister_view_handlers(); // Quitar los handlers del menú de acción

        if(action_menu_group) {
             if (lv_group_get_default() == action_menu_group) {
                lv_group_set_default(NULL);
             }
             lv_group_del(action_menu_group);
             action_menu_group = NULL;
        }
        lv_obj_del(action_menu_container);
        action_menu_container = NULL;

        // Pausar el timer del menú de acciones en lugar de borrarlo,
        // para poder reutilizarlo. La cola se limpia al crear el menú.
        if (action_menu_input_processor_timer) {
            lv_timer_pause(action_menu_input_processor_timer);
        }
        // La cola (action_menu_input_queue) no se borra aquí, se reutiliza.
        // Se limpia en create_action_menu.

        file_explorer_set_input_active(true);
        file_explorer_refresh();
    } else {
        ESP_LOGD(TAG, "destroy_action_menu llamado pero el menú no existe.");
    }
}


/*********************
 *  TEXT VIEWER LOGIC
 *********************/

static void text_viewer_delete_cb(lv_event_t * e) {
    char* text_content = (char*)lv_event_get_user_data(e);
    ESP_LOGD(TAG, "Visor de texto eliminado (LV_EVENT_DELETE), liberando contenido.");
    if (text_content) {
        free(text_content);
    }
    // Después de cerrar el visor de texto, el menú de acción subyacente ya debería
    // haber sido destruido por create_text_viewer -> lv_obj_clean.
    // Si no, necesitamos asegurarnos de que se destruya.
    // La lógica actual es que create_text_viewer limpia view_parent, lo que destruye el action_menu_container.
    // Y al destruirse action_menu_container, se deberían limpiar sus recursos (grupo, etc.).
    // No necesitamos llamar a destroy_action_menu() explícitamente aquí si lv_obj_clean lo maneja.
}

static void handle_cancel_from_viewer() {
    ESP_LOGD(TAG, "Cancelar desde el visor de texto, volviendo al explorador.");
    // show_file_explorer limpiará la pantalla, lo que disparará LV_EVENT_DELETE en el text_area.
    show_file_explorer();
}

static void create_text_viewer(const char* title, char* content) {
    ESP_LOGI(TAG, "Creando visor de texto para: %s", title);
    
    // Pausar el timer del menú de acción si está activo, ya que el menú se va a destruir.
    if (action_menu_input_processor_timer) {
        lv_timer_pause(action_menu_input_processor_timer);
    }
    // Limpiar los manejadores de botones del menú de acción
    button_manager_unregister_view_handlers();

    lv_obj_clean(view_parent); // Esto destruirá el action_menu_container y su grupo.

    lv_obj_t* main_cont = lv_obj_create(view_parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, lv_theme_get_font_large(title_label), 0);
    lv_obj_set_style_margin_bottom(title_label, 5, 0);

    lv_obj_t* text_area = lv_textarea_create(main_cont);
    lv_obj_set_size(text_area, lv_pct(95), lv_pct(85));
    lv_textarea_set_text(text_area, content);
    lv_obj_add_event_cb(text_area, text_viewer_delete_cb, LV_EVENT_DELETE, content);

    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_from_viewer);
    button_manager_register_view_handler(BUTTON_OK, NULL);
    button_manager_register_view_handler(BUTTON_LEFT, NULL);
    button_manager_register_view_handler(BUTTON_RIGHT, NULL);
}


/********************************
 *  FILE EXPLORER EVENT HANDLERS
 ********************************/

static void on_file_or_dir_selected(const char *path) {
    ESP_LOGD(TAG, "Elemento seleccionado en el explorador: %s", path);
    struct stat st;
    if (stat(path, &st) == 0 && !S_ISDIR(st.st_mode)) {
        ESP_LOGD(TAG, "Es un archivo, mostrando menú de acción.");
        create_action_menu(path);
    } else {
        ESP_LOGD(TAG, "Es un directorio, el explorador lo manejará.");
    }
}

static void on_create_action(file_item_type_t action_type, const char *current_path_from_explorer) {
    ESP_LOGI(TAG, "Acción de creación (%d) solicitada en: %s", action_type, current_path_from_explorer);
    char full_path[256];
    char basename[32];
    time_t now = time(NULL);
    struct tm timeinfo;

    if (now == (time_t)(-1)) {
         strcpy(basename, "new_item");
    } else {
        localtime_r(&now, &timeinfo);
        strftime(basename, sizeof(basename), "%H%M%S", &timeinfo);
    }

    if (action_type == ITEM_TYPE_ACTION_CREATE_FILE) {
        snprintf(full_path, sizeof(full_path), "%s/%s.txt", current_path_from_explorer, basename);
        if (sd_manager_create_file(full_path)) {
            sd_manager_write_file(full_path, "New file.");
        }
    } else if (action_type == ITEM_TYPE_ACTION_CREATE_FOLDER) {
        snprintf(full_path, sizeof(full_path), "%s/%s_dir", current_path_from_explorer, basename);
        sd_manager_create_directory(full_path);
    }
    file_explorer_refresh();
}


/***********************
 *  MAIN VIEW LOGIC
 ***********************/

static void show_file_explorer() {
    ESP_LOGD(TAG, "Mostrando el explorador de archivos.");
    if (action_menu_container) {
        ESP_LOGD(TAG, "Menú de acción existe, destruyéndolo antes de mostrar el explorador.");
        destroy_action_menu(); // Llamada directa segura aquí
    }
    lv_obj_clean(view_parent);

    lv_obj_t* main_cont = lv_obj_create(view_parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, "SD Explorer");
    lv_obj_set_style_text_font(title_label, lv_theme_get_font_large(title_label), 0);
    lv_obj_set_style_margin_bottom(title_label, 10, 0);

    lv_obj_t* explorer_container = lv_obj_create(main_cont);
    lv_obj_remove_style_all(explorer_container);
    lv_obj_set_size(explorer_container, lv_pct(95), lv_pct(85));
    lv_obj_clear_flag(explorer_container, LV_OBJ_FLAG_SCROLLABLE);

    file_explorer_create(
        explorer_container,
        sd_manager_get_mount_point(),
        on_file_or_dir_selected,
        on_create_action,
        on_explorer_exit
    );
}

static void on_explorer_exit() {
    ESP_LOGD(TAG, "Saliendo del explorador, volviendo a la vista inicial de SD.");
    // file_explorer_destroy() es llamado por la propia lógica del explorador.
    create_initial_sd_view();
}

static void handle_initial_ok_press() {
    ESP_LOGD(TAG, "Botón OK en la vista inicial de SD.");
    sd_manager_unmount();
    if (sd_manager_mount()) {
        ESP_LOGD(TAG, "SD montada, mostrando explorador.");
        show_file_explorer();
    } else if (info_label_widget) {
        ESP_LOGE(TAG, "Error al montar SD.");
        lv_label_set_text(info_label_widget, "Error mounting SD card.\n\nCheck card and press OK\nto retry.");
    }
}

static void handle_initial_cancel_press() {
    ESP_LOGD(TAG, "Botón CANCEL en la vista inicial de SD, volviendo al menú principal.");
    // Asegurarse de que los recursos del menú de acción se liberan si estaban activos.
    if (action_menu_input_processor_timer) {
        lv_timer_del(action_menu_input_processor_timer);
        action_menu_input_processor_timer = NULL;
    }
    if (action_menu_input_queue) {
        vQueueDelete(action_menu_input_queue);
        action_menu_input_queue = NULL;
    }
    // file_explorer_destroy() es llamado por view_manager al cambiar de vista si es necesario.
    view_manager_load_view(VIEW_ID_MENU);
}

static void create_initial_sd_view() {
    ESP_LOGI(TAG, "Creando la vista inicial de SD.");
    lv_obj_clean(view_parent);

    lv_obj_t* title = lv_label_create(view_parent);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_label_set_text(title, "SD Card Test");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    info_label_widget = lv_label_create(view_parent);
    lv_obj_set_style_text_align(info_label_widget, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label_widget);
    lv_label_set_text(info_label_widget, "Press OK to open\nthe file explorer");

    button_manager_register_view_handler(BUTTON_OK, handle_initial_ok_press);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_initial_cancel_press);
    button_manager_register_view_handler(BUTTON_LEFT, NULL);
    button_manager_register_view_handler(BUTTON_RIGHT, NULL);
}

void sd_test_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "sd_test_view_create llamado.");
    view_parent = parent;
    // Asegurarse de que los recursos del menú de acción de una instancia anterior se limpian
    // Esto es importante si se vuelve a esta vista sin pasar por una destrucción completa.
    if (action_menu_input_processor_timer) {
        lv_timer_del(action_menu_input_processor_timer);
        action_menu_input_processor_timer = NULL;
    }
    if (action_menu_input_queue) {
        vQueueDelete(action_menu_input_queue);
        action_menu_input_queue = NULL;
    }
    create_initial_sd_view();
}