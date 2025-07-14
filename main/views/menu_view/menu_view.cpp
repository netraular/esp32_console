// main\views\menu_view\menu_view.cpp
#include "menu_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "components/status_bar_component/status_bar_component.h"
#include "esp_log.h"

static const char *TAG = "MENU_VIEW";

static lv_obj_t *main_label;
static int selected_view_index = 0;

// Navigation options (names displayed on the screen)
static const char *view_options[] = {
    "Test Microphone",
    "Test Speaker",
    "Test SD",
    "Test Image",
    "Test Button Events",
    "WiFi Audio Stream",
    "Pomodoro Clock",
    "Click Counter Test",
    "Voice Notes",
    "Volume Tester"
};
static const int num_options = sizeof(view_options) / sizeof(view_options[0]);

// The array of IDs must have the same number of elements as the names array.
static const view_id_t view_ids[] = {
    VIEW_ID_MIC_TEST,
    VIEW_ID_SPEAKER_TEST,
    VIEW_ID_SD_TEST,
    VIEW_ID_IMAGE_TEST,
    VIEW_ID_MULTI_CLICK_TEST,
    VIEW_ID_WIFI_STREAM_TEST,
    VIEW_ID_POMODORO,
    VIEW_ID_CLICK_COUNTER_TEST,
    VIEW_ID_VOICE_NOTE,
    VIEW_ID_VOLUME_TESTER
};

static void update_menu_label() {
    lv_label_set_text_fmt(main_label, "< %s >", view_options[selected_view_index]);
}

static void handle_left_press(void* user_data) {
    selected_view_index--;
    if (selected_view_index < 0) {
        selected_view_index = num_options - 1; // Wrap around
    }
    update_menu_label();
}

static void handle_right_press(void* user_data) {
    selected_view_index++;
    if (selected_view_index >= num_options) {
        selected_view_index = 0; // Wrap around
    }
    update_menu_label();
}

static void handle_ok_press(void* user_data) {
    // Load the currently selected view
    view_manager_load_view(view_ids[selected_view_index]);
}

static void handle_cancel_press(void* user_data) {
    // Load the standby view
    view_manager_load_view(VIEW_ID_STANDBY);
}

void menu_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Menu View");

    // Crear un contenedor principal para la vista. Esto ayuda a agrupar todos los elementos
    // y hace que la limpieza sea consistente con otras vistas como standby_view.
    lv_obj_t *view_container = lv_obj_create(parent);
    lv_obj_remove_style_all(view_container);
    lv_obj_set_size(view_container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(view_container);

    // 1. Crear la barra de estado en la parte superior del contenedor de la vista
    status_bar_create(view_container);

    // 2. Crear la etiqueta principal del menú dentro del contenedor de la vista
    main_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(main_label, &lv_font_montserrat_24, 0);
    lv_obj_center(main_label); // Centrarla dentro del contenedor

    // 3. Empezar con el primer elemento seleccionado
    selected_view_index = 0; 
    update_menu_label();

    // 4. Registrar los manejadores de los botones
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_left_press, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_right_press, true, nullptr);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_press, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, nullptr);
}