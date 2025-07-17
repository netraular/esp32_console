#include "speaker_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "components/file_explorer/file_explorer.h"
#include "components/audio_player_component/audio_player_component.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SPEAKER_TEST_VIEW";

// --- State ---
static lv_obj_t *view_parent = nullptr;
static lv_obj_t *info_label_widget = nullptr;

// --- Prototypes ---
static void create_initial_speaker_view();
static void show_file_explorer();
static void on_player_exit();
static void explorer_cleanup_cb(lv_event_t * e);

// --- Callbacks ---

// **NUEVO**: Callback para limpiar los recursos del explorador cuando su contenedor se elimina.
// Esta es la corrección CRÍTICA para la fuga de recursos.
static void explorer_cleanup_cb(lv_event_t * e) {
    ESP_LOGI(TAG, "Explorer container deleted. Calling file_explorer_destroy().");
    file_explorer_destroy();
}

static void on_player_exit() {
    // Cuando el usuario sale del reproductor, volvemos a la pantalla inicial.
    // La limpieza del reproductor es automática porque es un hijo de la pantalla
    // y view_manager llamará a lv_obj_clean() al crear la nueva vista.
    create_initial_speaker_view();
}

static void on_audio_file_selected(const char *path) {
    // Cuando se selecciona un archivo .wav, destruimos el explorador y creamos el reproductor.
    const char *ext = strrchr(path, '.');
    if (ext && (strcasecmp(ext, ".wav") == 0)) {
        // Limpiar la pantalla actual (esto disparará explorer_cleanup_cb, destruyendo el file_explorer)
        lv_obj_clean(view_parent);
        
        // Crear el componente reproductor.
        audio_player_component_create(view_parent, path, on_player_exit);
    } else {
        ESP_LOGW(TAG, "File selected is not a .wav file: %s", path);
    }
}

static void on_explorer_exit_from_root() {
    // Al salir del explorador desde la raíz, volvemos a la vista inicial.
    // La limpieza de la pantalla en create_initial_speaker_view se encargará
    // de disparar el evento de borrado del contenedor del explorador.
    create_initial_speaker_view();
}

// --- Button Handlers (for initial view) ---
static void handle_ok_press_initial(void* user_data) {
    if (sd_manager_check_ready()) {
        show_file_explorer();
    } else if (info_label_widget) {
        lv_label_set_text(info_label_widget, "Failed to read SD card.\nCheck card and press OK.");
    }
}

static void handle_cancel_press_initial(void* user_data) {
    view_manager_load_view(VIEW_ID_MENU);
}

// --- View Creation Logic ---
static void show_file_explorer() {
    lv_obj_clean(view_parent);

    // Este contenedor alojará el explorador y llevará el callback de limpieza.
    lv_obj_t * explorer_container = lv_obj_create(view_parent);
    lv_obj_remove_style_all(explorer_container);
    lv_obj_set_size(explorer_container, lv_pct(100), lv_pct(100));

    // **LA CORRECCIÓN**: Asociar la función de limpieza al evento de borrado del contenedor.
    lv_obj_add_event_cb(explorer_container, explorer_cleanup_cb, LV_EVENT_DELETE, nullptr);

    // Crear el file_explorer dentro del contenedor que ahora tiene el callback.
    // Se pasa nullptr a los callbacks no necesarios (long_press, action).
    file_explorer_create(
        explorer_container, 
        sd_manager_get_mount_point(), 
        on_audio_file_selected, 
        nullptr, 
        nullptr, 
        on_explorer_exit_from_root
    );
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
    lv_label_set_text(info_label_widget, "Press OK to select\na .wav audio file");

    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_ok_press_initial, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press_initial, true, nullptr);
}

void speaker_test_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Speaker Test View");
    view_parent = parent;
    create_initial_speaker_view();
}