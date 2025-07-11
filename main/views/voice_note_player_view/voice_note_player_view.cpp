#include "voice_note_player_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "components/file_explorer/file_explorer.h"
#include "components/audio_player_component/audio_player_component.h" // NUEVO
#include "esp_log.h"
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "VOICE_NOTE_PLAYER_VIEW";
static const char *NOTES_DIR = "/sdcard/notes";

// --- State ---
static lv_obj_t *view_parent = NULL;

// --- Action Menu State ---
static lv_obj_t *action_menu_container = NULL;
static lv_group_t *action_menu_group = NULL;
static char selected_item_path[256];
static lv_style_t style_action_menu_focused;

// --- Prototypes ---
static void show_file_explorer();
static void destroy_action_menu_internal(bool refresh_explorer);

// --- Action Menu for Deleting Files ---
static void handle_action_menu_cancel(void* user_data) {
    destroy_action_menu_internal(false);
}

static void handle_action_menu_ok(void* user_data) {
    ESP_LOGI(TAG, "Delete action confirmed for: %s", selected_item_path);
    sd_manager_delete_item(selected_item_path);
    destroy_action_menu_internal(true);
}

static void create_action_menu(const char* path) {
    if (action_menu_container) return;
    ESP_LOGI(TAG, "Creating action menu for: %s", path);
    strncpy(selected_item_path, path, sizeof(selected_item_path) - 1);
    
    file_explorer_set_input_active(false);

    lv_style_init(&style_action_menu_focused);
    lv_style_set_bg_color(&style_action_menu_focused, lv_palette_main(LV_PALETTE_BLUE));

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

    lv_obj_t* btn = lv_list_add_button(list, LV_SYMBOL_TRASH, "Eliminar");
    action_menu_group = lv_group_create();
    lv_obj_add_style(btn, &style_action_menu_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(action_menu_group, btn);
    lv_group_set_default(action_menu_group);

    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_action_menu_ok, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_action_menu_cancel, true, nullptr);
}

static void destroy_action_menu_internal(bool refresh_explorer) {
    if (action_menu_container) {
        if(action_menu_group) { lv_group_del(action_menu_group); action_menu_group = NULL; }
        lv_obj_del(action_menu_container);
        action_menu_container = NULL;
        file_explorer_set_input_active(true);
        if (refresh_explorer) file_explorer_refresh();
    }
}

// --- Callbacks de los Componentes ---
static void on_player_exit() {
    // Cuando el usuario sale del reproductor, volvemos al explorador de archivos.
    show_file_explorer();
}

static void on_audio_file_selected(const char *path) {
    // Cuando se selecciona un archivo, destruimos el explorador y creamos el reproductor.
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