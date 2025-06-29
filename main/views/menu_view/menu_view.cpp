#include "menu_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"

static lv_obj_t *main_label;
static int selected_view_index = 0;

// Navigation options (names displayed on the screen)
static const char *view_options[] = {
    "Test Microphone",
    "Test Speaker",
    "Test SD",
    "Test Image",
    "Test Button Events",
    "WiFi Audio Stream" // --> AÑADIDO
};
static const int num_options = sizeof(view_options) / sizeof(view_options[0]);

// The array of IDs must have the same number of elements as the names array.
static const view_id_t view_ids[] = {
    VIEW_ID_MIC_TEST,
    VIEW_ID_SPEAKER_TEST,
    VIEW_ID_SD_TEST,
    VIEW_ID_IMAGE_TEST,
    VIEW_ID_MULTI_CLICK_TEST,
    VIEW_ID_WIFI_STREAM_TEST // --> AÑADIDO
};

static void update_menu_label() {
    lv_label_set_text_fmt(main_label, "< %s >", view_options[selected_view_index]);
}

// Button handlers for this view
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
    // Load the currently selected view
    view_manager_load_view(view_ids[selected_view_index]);
}

void menu_view_create(lv_obj_t *parent) {
    // Create the main label
    main_label = lv_label_create(parent);
    lv_obj_set_style_text_font(main_label, &lv_font_montserrat_24, 0);
    lv_obj_center(main_label);
    
    // Start with the first item selected
    selected_view_index = 0; 
    update_menu_label();

    // Register button handlers for this view using the new API
    // The last parameter 'false' indicates these are default handlers, not view-specific
    // but for the menu, we register them as 'view-specific' to ensure they are cleared when leaving.
    // --- CAMBIO: Se usa BUTTON_EVENT_TAP para una respuesta inmediata sin esperar al timeout del doble click. ---
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_left_press, true);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_right_press, true);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_press, true);
}