#include "sd_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "esp_log.h"
#include <string.h>

// --- Variables estáticas, TAG, y estructuras ---
static const char *TAG = "SD_VIEW";
static lv_group_t *view_group = nullptr;
static lv_style_t style_focused;
static lv_obj_t* list_widget = nullptr;
static char current_path[256];

typedef struct { bool is_dir; } list_item_data_t;
typedef struct { lv_obj_t *list; lv_group_t *group; } add_file_context_t;

// --- Prototipos ---
static void cleanup_view();
static void repopulate_list_cb(lv_timer_t *timer); // Ahora es un callback de timer
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
    if (view_group) lv_group_focus_next(view_group);
}

static void handle_left_press() {
    if (view_group) lv_group_focus_prev(view_group);
}

static void handle_ok_press() {
    lv_obj_t * focused_obj = lv_group_get_focused(view_group);
    if (!focused_obj) return;

    // Ignorar si el foco está en la propia lista (p.ej. directorio vacío)
    if (focused_obj == list_widget) return;

    list_item_data_t* item_data = static_cast<list_item_data_t*>(lv_obj_get_user_data(focused_obj));
    if (!item_data || !item_data->is_dir) {
        // Es un archivo, no un directorio
        const char* file_name = lv_list_get_button_text(list_widget, focused_obj);
        ESP_LOGI(TAG, "Archivo seleccionado: %s (acción no definida)", file_name);
        return;
    }

    // Es un directorio, procedemos a navegar
    const char* dir_name = lv_list_get_button_text(list_widget, focused_obj);
    ESP_LOGI(TAG, "Entrando en el directorio: %s", dir_name);

    if (current_path[strlen(current_path) - 1] != '/') {
        strcat(current_path, "/");
    }
    strcat(current_path, dir_name);
    
    // En lugar de llamar directamente, programamos la actualización
    schedule_repopulate_list();
}

static void handle_cancel_press() {
    const char *mount_point = sd_manager_get_mount_point();

    if (strcmp(current_path, mount_point) == 0) {
        cleanup_view();
        view_manager_load_view(VIEW_ID_MENU);
        return;
    }
    
    ESP_LOGI(TAG, "Volviendo al directorio padre desde: %s", current_path);
    char *last_slash = strrchr(current_path, '/');
    // Asegurarse de que no nos salgamos del punto de montaje
    if (last_slash > current_path && (size_t)(last_slash - current_path) >= strlen(mount_point)) {
         *last_slash = '\0';
    } else {
         strcpy(current_path, mount_point);
    }
    ESP_LOGI(TAG, "Nueva ruta: %s", current_path);

    // En lugar de llamar directamente, programamos la actualización
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

// *** LÓGICA DE ACTUALIZACIÓN ASÍNCRONA ***

// Limpia la lista y opcionalmente muestra "Cargando..."
static void clear_list_items(bool show_loading) {
    if (!list_widget) return;
    lv_group_remove_all_objs(view_group);
    lv_obj_clean(list_widget);

    // Añadimos la propia lista al grupo como fallback de foco
    lv_group_add_obj(view_group, list_widget);

    if (show_loading) {
        lv_list_add_text(list_widget, "Cargando...");
    }
}

// Esta es la función que hace el trabajo pesado. Se ejecuta como un callback de timer.
static void repopulate_list_cb(lv_timer_t *timer) {
    // 1. Limpiamos la lista (sin el "Cargando", que ya estaba)
    clear_list_items(false);

    // 2. Leemos la SD y añadimos los botones
    if (sd_manager_is_mounted()) {
        add_file_context_t context = { .list = list_widget, .group = view_group };
        sd_manager_list_files(current_path, add_file_entry_to_list, &context);
    } else {
        lv_list_add_text(list_widget, "Error: No se montó la SD");
    }

    // 3. Restauramos el foco
    if (lv_group_get_obj_count(view_group) > 1) { // >1 porque la lista siempre está
        lv_group_focus_next(view_group); // Enfoca el primer botón
    } else {
        lv_group_focus_obj(list_widget); // Enfoca la lista si está vacía
    }

    // 4. Borramos el timer para que solo se ejecute una vez.
    lv_timer_delete(timer);
}

// Esta función se llama desde los handlers de botones.
// Su única misión es preparar la UI y crear el timer que hará el trabajo.
static void schedule_repopulate_list() {
    // 1. Limpiamos la lista y mostramos "Cargando..." de inmediato.
    // Esta es una operación rápida que no bloquea.
    clear_list_items(true);

    // 2. Usamos el método de creación estándar de timers.
    // lv_timer_create(callback, periodo_en_ms, datos_de_usuario)
    // El callback (repopulate_list_cb) se encargará de borrar el timer.
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

static void cleanup_view() {
    clear_list_items(false);
    list_widget = nullptr;

    if (view_group) {
        if (lv_group_get_default() == view_group) {
            lv_group_set_default(NULL);
        }
        lv_group_delete(view_group);
        view_group = nullptr;
    }
}

// --- Creación de la vista ---

void sd_test_view_create(lv_obj_t *parent) {
    strcpy(current_path, sd_manager_get_mount_point());
    
    view_group = lv_group_create();
    lv_group_set_wrap(view_group, true);
    lv_group_set_focus_cb(view_group, focus_changed_cb);
    lv_group_set_default(view_group);

    lv_style_init(&style_focused);
    lv_style_set_bg_color(&style_focused, lv_palette_main(LV_PALETTE_LIGHT_BLUE));
    lv_style_set_bg_opa(&style_focused, LV_OPA_COVER);

    lv_obj_t * main_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, "Explorador SD");
    lv_obj_set_style_text_font(title_label, lv_theme_get_font_large(title_label), 0);
    lv_obj_set_style_margin_bottom(title_label, 10, 0);

    list_widget = lv_list_create(main_cont);
    lv_obj_set_size(list_widget, lv_pct(95), lv_pct(80));

    // La población inicial también se hace de forma asíncrona
    schedule_repopulate_list();

    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press);
    button_manager_register_view_handler(BUTTON_OK, handle_ok_press);
    button_manager_register_view_handler(BUTTON_RIGHT, handle_right_press);
    button_manager_register_view_handler(BUTTON_LEFT, handle_left_press);
}