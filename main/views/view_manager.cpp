#include "view_manager.h"
#include "esp_log.h"
#include "controllers/button_manager/button_manager.h"

#include "menu_view/menu_view.h"
#include "mic_test_view/mic_test_view.h"
#include "speaker_test_view/speaker_test_view.h"
#include "sd_test_view/sd_test_view.h"
#include "image_test_view/image_test_view.h"
#include "buzzer_test_view/buzzer_test_view.h"

static const char *TAG = "VIEW_MGR";
static view_id_t current_view_id;

void view_manager_init(void) {
    ESP_LOGI(TAG, "Initializing View Manager and loading initial view.");
    view_manager_load_view(VIEW_ID_MENU);
}

void view_manager_load_view(view_id_t view_id) {
    if (view_id >= VIEW_ID_COUNT) {
        ESP_LOGE(TAG, "Invalid view ID: %d", view_id);
        return;
    }

    ESP_LOGI(TAG, "Loading view %d", view_id);

    // Unregister all view-specific button handlers to restore default behavior.
    button_manager_unregister_view_handlers();

    // Clean the current screen before drawing the new view.
    lv_obj_t *scr = lv_screen_active();
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
        case VIEW_ID_BUZZER_TEST:
            buzzer_test_view_create(scr);
            break;
        default:
            break;
    }

    current_view_id = view_id;
    ESP_LOGI(TAG, "View %d loaded successfully.", view_id);
}