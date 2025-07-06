#include "view_manager.h"
#include "esp_log.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h" // Necesario para la comprobación

#include "menu_view/menu_view.h"
#include "mic_test_view/mic_test_view.h"
#include "speaker_test_view/speaker_test_view.h"
#include "sd_test_view/sd_test_view.h"
#include "image_test_view/image_test_view.h"
#include "button_test_view/button_test_view.h"
#include "multi_click_test_view/multi_click_test_view.h"
#include "wifi_stream_view/wifi_stream_view.h"
#include "pomodoro_view/pomodoro_view.h"
#include "click_counter_view/click_counter_view.h"

static const char *TAG = "VIEW_MGR";
static view_id_t current_view_id;

// --- Funciones para la pantalla de error de la SD Card ---

/**
 * @brief Manejador del botón OK en la pantalla de error. Intenta volver al menú.
 * Si la SD sigue fallando, la comprobación en view_manager_load_view volverá
 * a mostrar la pantalla de error.
 */
static void handle_error_view_retry(void* user_data) {
    ESP_LOGI(TAG, "Retry button pressed from error screen. Attempting to load menu.");
    view_manager_load_view(VIEW_ID_MENU);
}

/**
 * @brief Crea una pantalla de error global para cuando la SD no está disponible.
 * @param parent El objeto padre sobre el que crear la vista de error.
 */
static void load_sd_error_view(lv_obj_t* parent) {
    // Asegurarse de que no queden manejadores de vistas anteriores
    button_manager_unregister_view_handlers();
    lv_obj_clean(parent);

    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "SD Card Required");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t* body_label = lv_label_create(parent);
    lv_label_set_text(body_label, "Could not detect SD Card.\n\n"
                                  "Please insert the card.\n\n"
                                  "Press OK to retry.");
    lv_obj_set_style_text_align(body_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(body_label);

    // Solo el botón OK (Reintentar) está activo en esta pantalla.
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_error_view_retry, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, NULL, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, NULL, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, NULL, true, nullptr);

    current_view_id = VIEW_ID_SD_CARD_ERROR;
    ESP_LOGW(TAG, "Loaded SD Card Error view. System halted until SD is present.");
}


void view_manager_init(void) {
    ESP_LOGI(TAG, "Initializing View Manager and loading initial view.");
    view_manager_load_view(VIEW_ID_MENU);
}

void view_manager_load_view(view_id_t view_id) {
    if (view_id >= VIEW_ID_COUNT) {
        ESP_LOGE(TAG, "Invalid view ID: %d", view_id);
        return;
    }

    ESP_LOGI(TAG, "Request to load view %d", view_id);
    lv_obj_t *scr = lv_screen_active();

    // --- REGLA GLOBAL: COMPROBACIÓN DE SD OBLIGATORIA ---
    // Antes de cargar CUALQUIER vista (excepto la de error misma para evitar bucles),
    // se comprueba si la tarjeta SD está lista.
    if (view_id != VIEW_ID_SD_CARD_ERROR) {
        if (!sd_manager_check_ready()) {
            ESP_LOGE(TAG, "CRITICAL: SD Card not ready. Halting normal operation and loading error screen.");
            load_sd_error_view(scr);
            return; // Detiene la carga de la vista solicitada y muestra el error.
        }
    }
    // --- FIN DE LA REGLA GLOBAL ---

    // Unregister all view-specific button handlers to restore default behavior.
    button_manager_unregister_view_handlers();

    // Clean the current screen before drawing the new view.
    lv_obj_clean(scr);

    // Create the UI for the selected view.
    switch (view_id) {
        case VIEW_ID_MENU:
            menu_view_create(scr);
            break;
        case VIEW_ID_MIC_TEST:
            mic_test_view_create(scr);
            break;
        case VIEW_ID_SPEAKER_TEST:
            speaker_test_view_create(scr);
            break;
        case VIEW_ID_SD_TEST:
            sd_test_view_create(scr);
            break;
        case VIEW_ID_IMAGE_TEST:
            image_test_view_create(scr);
            break;
        case VIEW_ID_BUTTON_DISPATCH_TEST:
            button_test_view_create(scr);
            break;
        case VIEW_ID_MULTI_CLICK_TEST:
            multi_click_test_view_create(scr);
            break;
        case VIEW_ID_WIFI_STREAM_TEST:
            wifi_stream_view_create(scr);
            break;
        case VIEW_ID_POMODORO:
            pomodoro_view_create(scr);
            break;
        case VIEW_ID_CLICK_COUNTER_TEST:
            click_counter_view_create(scr);
            break;
        // No hay 'case' para VIEW_ID_SD_CARD_ERROR porque se carga de forma especial.
        default:
            // Si por alguna razón se llama con un ID no manejado, mostrar un error simple.
            lv_label_set_text(lv_label_create(scr), "Error: View not found!");
            break;
    }

    current_view_id = view_id;
    ESP_LOGI(TAG, "View %d loaded successfully.", view_id);
}