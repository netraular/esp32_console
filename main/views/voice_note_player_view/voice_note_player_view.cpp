#include "voice_note_player_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "controllers/stt_manager/stt_manager.h"
#include "components/file_explorer/file_explorer.h"
#include "components/audio_player_component/audio_player_component.h"
#include "components/text_viewer/text_viewer.h"
#include "esp_log.h"
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "VOICE_NOTE_PLAYER_VIEW";
static const char *NOTES_DIR = "/sdcard/notes";

// --- State ---
static lv_obj_t *view_parent = NULL;
static lv_obj_t *loading_indicator = NULL;

// --- Action Menu State (MODIFIED: Style like sd_test_view) ---
static lv_obj_t *action_menu_container = NULL;
static lv_group_t *action_menu_group = NULL;
static char selected_item_path[256];
static lv_style_t style_action_menu_focused;

// --- Estructura para pasar datos a la llamada asíncrona de LVGL ---
typedef struct {
    bool success;
    char* result_text;
} transcription_result_t;


// --- Prototypes ---
static void show_file_explorer();
static void destroy_action_menu_internal(bool refresh_explorer);
static void on_transcription_complete(bool success, char* result);
static void on_transcription_complete_ui_thread(void *user_data);
static void on_viewer_exit();


// --- Loading Indicator ---
static void show_loading_indicator(const char* text) {
    if (loading_indicator) return;
    loading_indicator = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(loading_indicator);
    lv_obj_set_size(loading_indicator, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(loading_indicator, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(loading_indicator, LV_OPA_70, 0);
    lv_obj_clear_flag(loading_indicator, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *spinner = lv_spinner_create(loading_indicator);
    lv_obj_center(spinner);

    lv_obj_t* label = lv_label_create(loading_indicator);
    lv_label_set_text(label, text);
    lv_obj_align_to(label, spinner, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
}

static void hide_loading_indicator() {
    if (loading_indicator) {
        lv_obj_del(loading_indicator);
        loading_indicator = NULL;
    }
}


// --- Action Menu (MODIFIED: Style like sd_test_view) ---
static void handle_action_menu_left_press(void* user_data) {
    if (action_menu_group) lv_group_focus_prev(action_menu_group);
}
static void handle_action_menu_right_press(void* user_data) {
    if (action_menu_group) lv_group_focus_next(action_menu_group);
}
static void handle_action_menu_cancel(void* user_data) {
    destroy_action_menu_internal(false);
}

static void handle_action_menu_ok(void* user_data) {
    if (!action_menu_group) return;

    lv_obj_t* selected_btn = lv_group_get_focused(action_menu_group);
    if (!selected_btn) return;

    lv_obj_t* list = lv_obj_get_parent(selected_btn);
    const char* action_text = lv_list_get_button_text(list, selected_btn);
    ESP_LOGI(TAG, "Action '%s' selected for: %s", action_text, selected_item_path);

    if (strcmp(action_text, "Eliminar") == 0) {
        sd_manager_delete_item(selected_item_path);
        destroy_action_menu_internal(true);
    } else if (strcmp(action_text, "Transcribir") == 0) {
        if (!wifi_manager_is_connected()) {
             wifi_manager_init_sta(); // Attempt to connect if not already
        }
        destroy_action_menu_internal(false);
        show_loading_indicator("Transcribing...");
        if (!stt_manager_transcribe(selected_item_path, on_transcription_complete)) {
            hide_loading_indicator();
            ESP_LOGE(TAG, "Failed to start transcription task.");
            show_file_explorer();
        }
    } else {
        destroy_action_menu_internal(false);
    }
}

static void create_action_menu(const char* path) {
    if (action_menu_container) return;
    ESP_LOGI(TAG, "Creating action menu for: %s", path);
    strncpy(selected_item_path, path, sizeof(selected_item_path) - 1);
    
    file_explorer_set_input_active(false);

    lv_style_init(&style_action_menu_focused);
    lv_style_set_bg_color(&style_action_menu_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_opa(&style_action_menu_focused, LV_OPA_COVER);

    action_menu_container = lv_obj_create(view_parent);
    lv_obj_remove_style_all(action_menu_container);
    lv_obj_set_size(action_menu_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(action_menu_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(action_menu_container, LV_OPA_50, 0);

    lv_obj_t* menu_box = lv_obj_create(action_menu_container);
    lv_obj_set_width(menu_box, lv_pct(80));
    lv_obj_set_height(menu_box, LV_SIZE_CONTENT);
    lv_obj_center(menu_box);

    lv_obj_t* list = lv_list_create(menu_box);
    lv_obj_set_size(list, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_center(list);

    action_menu_group = lv_group_create();

    const char* actions[][2] = {
        {LV_SYMBOL_TRASH, "Eliminar"},
        {LV_SYMBOL_EDIT, "Transcribir"}
    };
    for(auto const& action : actions) {
        lv_obj_t* btn = lv_list_add_button(list, action[0], action[1]);
        lv_obj_add_style(btn, &style_action_menu_focused, LV_STATE_FOCUSED);
        lv_group_add_obj(action_menu_group, btn);
    }

    lv_group_set_default(action_menu_group);
    if(lv_obj_get_child_cnt(list) > 0){
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_action_menu_ok, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_action_menu_cancel, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, handle_action_menu_left_press, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, handle_action_menu_right_press, true, nullptr);
}

static void destroy_action_menu_internal(bool refresh_explorer) {
    if (action_menu_container) {
        if(action_menu_group) {
            if (lv_group_get_default() == action_menu_group) lv_group_set_default(NULL);
            lv_group_del(action_menu_group);
            action_menu_group = NULL;
        }
        lv_obj_del(action_menu_container);
        action_menu_container = NULL;
        file_explorer_set_input_active(true);
        if (refresh_explorer) file_explorer_refresh();
    }
}

// --- Component Callbacks ---

// Callback from text_viewer when user exits
static void on_viewer_exit() {
    show_file_explorer();
}

// ESTA FUNCIÓN SE EJECUTA EN EL HILO DE LVGL
static void on_transcription_complete_ui_thread(void *user_data) {
    transcription_result_t* result_data = (transcription_result_t*)user_data;
    
    hide_loading_indicator();
    
    if(result_data->success) {
        ESP_LOGI(TAG, "UI THREAD: Transcription success. Showing result.");
        lv_obj_clean(view_parent);
        // text_viewer_create toma posesión del puntero result_data->result_text
        text_viewer_create(view_parent, "Transcripción", result_data->result_text, on_viewer_exit);
    } else {
        ESP_LOGE(TAG, "UI THREAD: Transcription failed: %s", result_data->result_text);
        // El text viewer no se crea, así que debemos liberar la memoria del mensaje de error
        free(result_data->result_text);
        // Aquí podrías mostrar un popup de error. Por ahora, solo volvemos al explorador.
        show_file_explorer();
    }

    // Liberar el contenedor de datos
    free(result_data);
}

// ESTE CALLBACK SE EJECUTA EN EL HILO DE STT
// Su única misión es empaquetar los datos y enviarlos al hilo de LVGL.
static void on_transcription_complete(bool success, char* result) {
    transcription_result_t* result_data = (transcription_result_t*)malloc(sizeof(transcription_result_t));
    if (result_data) {
        result_data->success = success;
        result_data->result_text = result; // El puntero del resultado se transfiere
        lv_async_call(on_transcription_complete_ui_thread, result_data);
    } else {
        ESP_LOGE(TAG, "Failed to allocate memory for async call, result will be lost.");
        free(result);
    }
}

static void on_player_exit() {
    show_file_explorer();
}

static void on_audio_file_selected(const char *path) {
    lv_obj_clean(view_parent);
    file_explorer_destroy();
    audio_player_component_create(view_parent, path, on_player_exit);
}

static void on_file_long_pressed(const char *path) {
    create_action_menu(path);
}

static void on_explorer_exit() {
    destroy_action_menu_internal(false); 
    file_explorer_destroy(); 
    view_manager_load_view(VIEW_ID_VOICE_NOTE);
}

static void show_file_explorer() {
    lv_obj_clean(view_parent);
    
    struct stat st;
    if (stat(NOTES_DIR, &st) != 0) {
        lv_obj_t* label = lv_label_create(view_parent);
        lv_label_set_text(label, "No voice notes found.\n\nPress Cancel to go back.");
        lv_obj_center(label);
        button_manager_unregister_view_handlers();
        button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, [](void* d){ on_explorer_exit(); }, true, nullptr);
        return;
    }

    lv_obj_t * explorer_container = lv_obj_create(view_parent);
    lv_obj_remove_style_all(explorer_container);
    lv_obj_set_size(explorer_container, lv_pct(100), lv_pct(100));
    
    file_explorer_create(explorer_container, NOTES_DIR, on_audio_file_selected, on_file_long_pressed, NULL, on_explorer_exit);
}

// --- Entry Point ---
void voice_note_player_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Voice Note Player View");
    view_parent = parent;
    show_file_explorer();
}