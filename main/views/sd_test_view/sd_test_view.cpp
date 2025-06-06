#include "sd_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "esp_log.h"

// --- Callbacks y Handlers (sin cambios) ---

static void add_file_entry_to_menu(const char *name, bool is_dir, void *user_data) {
    lv_obj_t *page = (lv_obj_t *)user_data;
    if (!page) return;

    lv_obj_t *cont = lv_menu_cont_create(page);
    lv_obj_t *label = lv_label_create(cont);
    
    if (is_dir) {
        lv_label_set_text_fmt(label, "%s  %s", LV_SYMBOL_DIRECTORY, name);
    } else {
        lv_label_set_text_fmt(label, "%s  %s", LV_SYMBOL_FILE, name);
    }
}

static void handle_cancel_press() {
    view_manager_load_view(VIEW_ID_MENU);
}

static void back_event_handler(lv_event_t * e) {
    lv_obj_t * obj = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t * menu = lv_obj_get_parent(obj);

    if(lv_menu_back_button_is_root(menu, obj)) {
        view_manager_load_view(VIEW_ID_MENU);
    }
}


// --- Función principal de creación de la vista (CORREGIDA) ---

void sd_test_view_create(lv_obj_t *parent) {
    // 1. Crear el objeto de menú principal
    lv_obj_t *menu = lv_menu_create(parent);
    lv_obj_set_size(menu, lv_pct(100), lv_pct(100));
    lv_obj_center(menu);

    // 2. Crear LA ÚNICA PÁGINA que usaremos
    lv_obj_t *page = lv_menu_page_create(menu, NULL);

    // 3. Añadir el título COMO PRIMER ELEMENTO DENTRO DE LA PÁGINA
    lv_obj_t *title_cont = lv_menu_cont_create(page);
    lv_obj_remove_flag(title_cont, LV_OBJ_FLAG_CLICKABLE); // Hacemos que el título no sea seleccionable
    lv_obj_t *title_label = lv_label_create(title_cont);
    lv_label_set_text(title_label, "Contenido de la SD");

    // 4. Añadir un separador DENTRO DE LA PÁGINA, debajo del título
    lv_menu_separator_create(page);

    // 5. Poblar el resto de la página con los archivos
    if (sd_manager_is_mounted()) {
        // La función de callback ahora añade los elementos a la misma 'page'
        sd_manager_list_files(sd_manager_get_mount_point(), add_file_entry_to_menu, page);
    } else {
        // El mensaje de error también se añade a la 'page'
        lv_obj_t *err_cont = lv_menu_cont_create(page);
        lv_obj_t *err_label = lv_label_create(err_cont);
        lv_label_set_text(err_label, "#ff0000 Error: No se montó la SD#");
    }

    // 6. Configurar la navegación
    lv_obj_t * back_btn = lv_menu_get_main_header_back_button(menu);
    lv_obj_add_event_cb(back_btn, back_event_handler, LV_EVENT_CLICKED, NULL);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press);

    // 7. Establecer nuestra página como la página principal
    lv_menu_set_page(menu, page);
}