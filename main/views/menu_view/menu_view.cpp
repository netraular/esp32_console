#include "menu_view.h"
#include "../view_manager.h" // Usamos "../" para subir un nivel en el directorio
#include "controllers/button_manager/button_manager.h"

// --- Variables estáticas de la vista ---
static lv_obj_t *main_label;
static int selected_view_index = 0;

// Opciones de navegación (nombres que se muestran en pantalla)
static const char *view_options[] = {
    "Test Microphone",
    "Test Speaker",
    "Test SD",
    "Test Image",
    "Test Buzzer"
};
static const int num_options = sizeof(view_options) / sizeof(view_options[0]);

// **CORRECCIÓN IMPORTANTE**:
// El array de IDs debe tener el mismo número de elementos que el array de nombres.
static const view_id_t view_ids[] = {
    VIEW_ID_MIC_TEST,
    VIEW_ID_SPEAKER_TEST,
    VIEW_ID_SD_TEST,
    VIEW_ID_IMAGE_TEST,
    VIEW_ID_BUZZER_TEST
};


// --- Funciones auxiliares y de UI ---

static void update_menu_label() {
    lv_label_set_text_fmt(main_label, "< %s >", view_options[selected_view_index]);
}

// --- Handlers de botones específicos de esta vista ---

static void handle_left_press() {
    selected_view_index--;
    if (selected_view_index < 0) {
        selected_view_index = num_options - 1; // Wrap around
    }
    update_menu_label();
}

static void handle_right_press() {
    selected_view_index++;
    if (selected_view_index >= num_options) {
        selected_view_index = 0; // Wrap around
    }
    update_menu_label();
}

static void handle_ok_press() {
    // Cargar la vista que está actualmente seleccionada
    // Ahora esto es seguro porque 'view_ids' tiene 5 elementos.
    view_manager_load_view(view_ids[selected_view_index]);
}

// --- Función principal de creación de la vista ---

void menu_view_create(lv_obj_t *parent) {
    // Crear el label principal
    main_label = lv_label_create(parent);
    lv_obj_set_style_text_font(main_label, &lv_font_montserrat_24, 0);
    lv_obj_center(main_label);
    
    // Iniciar con el primer elemento seleccionado
    selected_view_index = 0; 
    update_menu_label();

    // Registrar los handlers de botón para esta vista
    button_manager_register_view_handler(BUTTON_LEFT, handle_left_press);
    button_manager_register_view_handler(BUTTON_RIGHT, handle_right_press);
    button_manager_register_view_handler(BUTTON_OK, handle_ok_press);
}