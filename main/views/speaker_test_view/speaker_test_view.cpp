#include "speaker_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "components/file_explorer/file_explorer.h"
#include "components/audio_player_component/audio_player_component.h" // NUEVO
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SPEAKER_TEST_VIEW";

// --- State ---
static lv_obj_t *view_parent = NULL;
static lv_obj_t *info_label_widget = NULL;

// --- Prototypes ---
static void create_initial_speaker_view();
static void show_file_explorer();
static void on_player_exit();

// --- Callbacks ---
static void on_player_exit() {
    // Cuando el usuario sale del reproductor, volvemos a la pantalla inicial.
    create_initial_speaker_view();
}

static void on_audio_file_selected(const char *path) {
    // Cuando se selecciona un archivo, destruimos el explorador y creamos el reproductor.
    const char *ext = strrchr(path, '.');
    if (ext && (strcasecmp(ext, ".wav") == 0)) {
        lv_obj_clean(view_parent);
        file_explorer_destroy(); 
        audio_player_component_create(view_parent, path, on_player_exit);
    }
}

static void on_explorer_exit_speaker() {
    file_explorer_destroy(); 
    create_initial_speaker_view(); 
}

// --- Button Handlers ---
static void handle_ok_press_initial(void* user_data) {
    sd_manager_unmount();
    if (sd_manager_mount()) {
        show_file_explorer();
    } else if (info_label_widget) {
        lv_label_set_text(info_label_widget, "Failed to read SD card.\nCheck card and press OK.");
    }
}

static void handle_cancel_press_initial(void* user_data) {
    view_manager_load_view(VIEW_ID_MENU);
}

// --- View Creation ---
static void show_file_explorer() {
    lv_obj_clean(view_parent);
    lv_obj_t * explorer_container = lv_obj_create(view_parent);
    lv_obj_remove_style_all(explorer_container);
    lv_obj_set_size(explorer_container, lv_pct(100), lv_pct(100));

    file_explorer_create(explorer_container, sd_manager_get_mount_point(), on_audio_file_selected, nullptr, nullptr, on_explorer_exit_speaker);
}

static void create_initial_speaker_view() {
    lv_obj_clean(view_parent);

    lv_obj_t *label = lv_label_create(view_parent);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_label_set_text(label, "Speaker Test");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    info_label_widget = lv_label_create(view_parent);
    lv_obj_set_style_text_align(info_label_widget, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label_widget);
    lv_label_set_text(info_label_widget, "Press OK to\nselect an audio file");

    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_ok_press_initial, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press_initial, true, nullptr);
}

void speaker_test_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Speaker Test View");
    view_parent = parent;
    create_initial_speaker_view();
}