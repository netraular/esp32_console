#include "sd_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "esp_log.h"

// --- Variables estáticas y TAG para el log ---
static const char *TAG = "SD_VIEW";
static lv_group_t *view_group = nullptr;
static lv_style_t style_focused; // Estilo para el elemento seleccionado

// --- Contexto para el callback de archivos ---
// Necesitamos pasar tanto la lista como el grupo al callback que añade los elementos.
typedef struct {
    lv_obj_t *list;
    lv_group_t *group;
} add_file_context_t;


// --- Prototipos de funciones ---
static void cleanup_view();
static void add_file_entry_to_list(const char *name, bool is_dir, void *user_data);
static void focus_changed_cb(lv_group_t * group);
static void handle_right_press();
static void handle_left_press();
static void handle_cancel_press();

// --- Handlers y Callbacks ---

// Usamos focus_next/prev porque ahora el grupo conoce cada botón individualmente.
static void handle_right_press() {
    if (view_group) {
        lv_group_focus_next(view_group);
    }
}

static void handle_left_press() {
    if (view_group) {
        lv_group_focus_prev(view_group);
    }
}

static void handle_cancel_press() {
    cleanup_view();
    view_manager_load_view(VIEW_ID_MENU);
}

// Callback de cambio de foco. Ahora es más robusto para evitar crashes.
static void focus_changed_cb(lv_group_t * group) {
    lv_obj_t * focused_obj = lv_group_get_focused(group);

    // ¡Comprobación de seguridad! Evita el crash si el foco es temporalmente nulo.
    if (!focused_obj) {
        return;
    }

    // Asegurarse de que el objeto enfocado es un botón con una etiqueta.
    if (lv_obj_get_child_count(focused_obj) > 1) {
        lv_obj_t * label = lv_obj_get_child(focused_obj, 1);
        if (label && lv_obj_check_type(label, &lv_label_class)) {
            ESP_LOGI(TAG, "Seleccionado: %s", lv_label_get_text(label));
            // Asegura que el elemento seleccionado sea visible en la lista
            lv_obj_scroll_to_view(focused_obj, LV_ANIM_ON);
        }
    }
}

static void cleanup_view() {
    if (view_group) {
        if (lv_group_get_default() == view_group) {
            lv_group_set_default(NULL);
        }
        lv_group_delete(view_group);
        view_group = nullptr;
    }
    // El estilo es estático, no necesita borrarse si se va a reusar.
    // lv_style_reset(&style_focused);
}

/**
 * @brief Callback para añadir una entrada a la lista.
 * Esta es la función más importante: crea el botón, le aplica el estilo de foco
 * y lo AÑADE AL GRUPO.
 */
static void add_file_entry_to_list(const char *name, bool is_dir, void *user_data) {
    add_file_context_t *context = (add_file_context_t *)user_data;
    if (!context || !context->list || !context->group) return;

    const char *icon = is_dir ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE;
    lv_obj_t *btn = lv_list_add_button(context->list, icon, name);

    // Aplicamos el estilo que definimos para cuando el botón esté enfocado.
    lv_obj_add_style(btn, &style_focused, LV_STATE_FOCUSED);

    // Añadimos el BOTÓN (no la lista) al grupo de navegación.
    lv_group_add_obj(context->group, btn);

    // Permitir que el texto de nombres largos se desplace
    lv_obj_t *label = lv_obj_get_child(btn, 1);
    if(label) {
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(label, lv_pct(90));
    }
}

// --- Función principal de creación de la vista ---

void sd_test_view_create(lv_obj_t *parent) {
    // 1. Crear el grupo para gestionar el foco de los botones
    view_group = lv_group_create();
    lv_group_set_wrap(view_group, true); // ¡CRÍTICO! para que pase del último al primero.
    lv_group_set_focus_cb(view_group, focus_changed_cb);
    lv_group_set_default(view_group);

    // 2. Crear y registrar el estilo para el elemento seleccionado (foco)
    lv_style_init(&style_focused);
    lv_style_set_bg_color(&style_focused, lv_palette_main(LV_PALETTE_LIGHT_BLUE));
    lv_style_set_bg_opa(&style_focused, LV_OPA_COVER);

    // 3. Crear un contenedor principal con layout vertical
    lv_obj_t * main_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 4. Crear un título
    lv_obj_t *title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, "Contenido de la SD");
    lv_obj_set_style_text_font(title_label, lv_theme_get_font_large(title_label), 0);
    lv_obj_set_style_margin_bottom(title_label, 10, 0);

    // 5. Crear el widget de lista (lv_list)
    lv_obj_t *list = lv_list_create(main_cont);
    lv_obj_set_size(list, lv_pct(95), lv_pct(80));
    
    // NO AÑADIR LA LISTA AL GRUPO.
    // lv_group_add_obj(view_group, list); // <-- ESTA LÍNEA SE ELIMINA

    // 6. Poblar la lista. Pasamos el contexto con la lista Y el grupo.
    if (sd_manager_is_mounted()) {
        add_file_context_t context = { .list = list, .group = view_group };
        sd_manager_list_files(sd_manager_get_mount_point(), add_file_entry_to_list, &context);
    } else {
        lv_list_add_text(list, "Error: No se montó la SD");
    }

    // 7. Configurar la navegación por botones físicos
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press);
    button_manager_register_view_handler(BUTTON_RIGHT, handle_right_press);
    button_manager_register_view_handler(BUTTON_LEFT, handle_left_press);

    // 8. Forzar el foco inicial en el primer elemento del grupo
    // Esto es más seguro que acceder a los hijos de la lista directamente.
    // La primera llamada a `focus_next` seleccionará el primer elemento del grupo.
    if (lv_group_get_obj_count(view_group) > 0) {
        lv_group_focus_next(view_group);
    }
}