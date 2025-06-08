#include "speaker_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "../file_explorer/file_explorer.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SPEAKER_TEST_VIEW";

// --- Variables de estado de la vista ---
static lv_obj_t *view_parent = NULL; // Contenedor principal de la vista
static lv_obj_t *play_pause_btn_label = NULL; // Puntero al label del botón para cambiar el icono
static char current_song_path[256]; // Guardar la ruta del archivo seleccionado

// --- Widgets de la UI de reproducción que necesitan ser actualizados ---
static lv_obj_t *slider_widget = NULL;
static lv_obj_t *time_current_label_widget = NULL;
static lv_obj_t *time_total_label_widget = NULL;
static lv_timer_t *ui_update_timer = NULL;

// --- Prototipos de las funciones ---
static void create_initial_speaker_view();
static void show_file_explorer();
static void create_now_playing_view(const char *file_path);
static void handle_ok_press_playing();
static void handle_cancel_press_playing();
static void on_audio_file_selected(const char *path);
static void on_explorer_exit_speaker();
static void handle_ok_press_initial();
static void handle_cancel_press_initial();
static void ui_update_timer_cb(lv_timer_t *timer);

// --- Funciones de utilidad ---

/**
 * @brief Comprueba si un archivo tiene la extensión .wav (insensible a mayúsculas/minúsculas).
 */
static bool is_wav_file(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) {
        return false;
    }
    return (strcasecmp(ext, ".wav") == 0);
}

/**
 * @brief Extrae el nombre del archivo de una ruta completa.
 */
static const char* get_filename_from_path(const char* path) {
    const char* filename = strrchr(path, '/');
    if (filename) {
        return filename + 1;
    }
    return path;
}

/**
 * @brief Formatea un tiempo en segundos a un string MM:SS.
 */
static void format_time(char *buf, size_t buf_size, uint32_t time_s) {
    snprintf(buf, buf_size, "%02lu:%02lu", time_s / 60, time_s % 60);
}


// --- Timer para actualizar la UI ---

/**
 * @brief Callback del timer que actualiza la barra de progreso y los tiempos.
 */
static void ui_update_timer_cb(lv_timer_t *timer) {
    audio_player_state_t state = audio_manager_get_state();
    
    // Si la canción terminó o fue detenida por otra razón
    if (state == AUDIO_STATE_STOPPED) {
        if (play_pause_btn_label) lv_label_set_text(play_pause_btn_label, LV_SYMBOL_PLAY);
        if (slider_widget) lv_slider_set_value(slider_widget, 0, LV_ANIM_OFF);
        if (time_current_label_widget) lv_label_set_text(time_current_label_widget, "00:00");
        
        // Detenemos este timer para no consumir recursos
        lv_timer_delete(ui_update_timer);
        ui_update_timer = NULL;
        return;
    }

    uint32_t duration = audio_manager_get_duration_s();
    uint32_t progress = audio_manager_get_progress_s();

    // Actualizar la duración total (solo se hace una vez cuando cambia de 0)
    if (duration > 0 && lv_slider_get_max_value(slider_widget) != duration) {
        char time_buf[8];
        format_time(time_buf, sizeof(time_buf), duration);
        lv_label_set_text(time_total_label_widget, time_buf);
        lv_slider_set_range(slider_widget, 0, duration);
    }

    // Actualizar el progreso actual
    char time_buf[8];
    format_time(time_buf, sizeof(time_buf), progress);
    lv_label_set_text(time_current_label_widget, time_buf);
    lv_slider_set_value(slider_widget, progress, LV_ANIM_OFF);
}


// --- Callbacks y Handlers para la pantalla "Now Playing" ---

static void handle_ok_press_playing() {
    audio_player_state_t state = audio_manager_get_state();

    if (state == AUDIO_STATE_STOPPED) {
        ESP_LOGI(TAG, "Comando: Play");
        if (audio_manager_play(current_song_path)) {
            lv_label_set_text(play_pause_btn_label, LV_SYMBOL_PAUSE);
            // Iniciar el timer si no está corriendo
            if (ui_update_timer == NULL) {
                ui_update_timer = lv_timer_create(ui_update_timer_cb, 500, NULL);
            }
        }
    } else if (state == AUDIO_STATE_PLAYING) {
        ESP_LOGI(TAG, "Comando: Pause");
        audio_manager_pause();
        lv_label_set_text(play_pause_btn_label, LV_SYMBOL_PLAY);
    } else if (state == AUDIO_STATE_PAUSED) {
        ESP_LOGI(TAG, "Comando: Resume");
        audio_manager_resume();
        lv_label_set_text(play_pause_btn_label, LV_SYMBOL_PAUSE);
    }
}

static void handle_cancel_press_playing() {
    ESP_LOGI(TAG, "Volviendo al explorador de archivos");
    if (ui_update_timer) { // Detener y limpiar el timer de UI
        lv_timer_delete(ui_update_timer);
        ui_update_timer = NULL;
    }
    audio_manager_stop(); // Detener la música
    play_pause_btn_label = NULL;
    show_file_explorer();
}


// --- Callbacks y Handlers para la pantalla del Explorador de Archivos ---

static void on_audio_file_selected(const char *path) {
    ESP_LOGI(TAG, "Entrada seleccionada: %s", path);

    if (is_wav_file(path)) {
        ESP_LOGI(TAG, "Es un archivo WAV. Creando vista de reproducción.");
        file_explorer_destroy(); // Muy importante: limpiar recursos del explorador
        create_now_playing_view(path);
    } else {
        ESP_LOGW(TAG, "No es un archivo WAV. Selección ignorada.");
    }
}

static void on_explorer_exit_speaker() {
    file_explorer_destroy(); // Limpiar recursos del explorador
    lv_obj_clean(view_parent);
    create_initial_speaker_view(); // Volver a la pantalla inicial de la vista
}


// --- Lógica para mostrar las diferentes pantallas ---

static void show_file_explorer() {
    if (!sd_manager_is_mounted()) {
        ESP_LOGE(TAG, "No se puede abrir el explorador, la tarjeta SD no está montada.");
        // Podríamos mostrar un mensaje de error aquí
        return;
    }

    ESP_LOGI(TAG, "Abriendo explorador de archivos...");
    lv_obj_clean(view_parent);

    lv_obj_t * explorer_container = lv_obj_create(view_parent);
    lv_obj_remove_style_all(explorer_container);
    lv_obj_set_size(explorer_container, lv_pct(100), lv_pct(100));
    lv_obj_center(explorer_container);

    file_explorer_create(
        explorer_container,
        sd_manager_get_mount_point(),
        on_audio_file_selected,
        on_explorer_exit_speaker
    );
}

static void create_now_playing_view(const char *file_path) {
    lv_obj_clean(view_parent); // Limpiar la pantalla anterior
    strncpy(current_song_path, file_path, sizeof(current_song_path) - 1);

    // --- Contenedor principal con layout de columna ---
    lv_obj_t *main_cont = lv_obj_create(view_parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // --- 1. Título de la canción ---
    lv_obj_t *title_label = lv_label_create(main_cont);
    const char* filename = get_filename_from_path(file_path);
    lv_label_set_text(title_label, filename);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(title_label, lv_pct(90));
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);

    // --- 2. Espacio para el visualizador ---
    lv_obj_t *visualizer_area = lv_obj_create(main_cont);
    lv_obj_set_size(visualizer_area, lv_pct(80), lv_pct(40));
    lv_obj_set_style_bg_color(visualizer_area, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_color(visualizer_area, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(visualizer_area, 1, 0);
    lv_obj_set_style_radius(visualizer_area, 8, 0);
    
    lv_obj_t *vis_label = lv_label_create(visualizer_area);
    lv_label_set_text(vis_label, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(vis_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(vis_label, lv_color_hex(0x888888), 0);
    lv_obj_center(vis_label);

    // --- 3. Barra de progreso y tiempos ---
    lv_obj_t *progress_cont = lv_obj_create(main_cont);
    lv_obj_remove_style_all(progress_cont);
    lv_obj_set_width(progress_cont, lv_pct(95));
    lv_obj_set_height(progress_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(progress_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(progress_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    time_current_label_widget = lv_label_create(progress_cont);
    lv_label_set_text(time_current_label_widget, "00:00");
    
    slider_widget = lv_slider_create(progress_cont);
    lv_obj_set_style_flex_grow(slider_widget, 1, 0);
    lv_obj_remove_flag(slider_widget, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_margin_hor(slider_widget, 10, 0);

    time_total_label_widget = lv_label_create(progress_cont);
    lv_label_set_text(time_total_label_widget, "??:??"); // Placeholder

    // --- 4. Botón de Play/Pause ---
    lv_obj_t *play_pause_btn = lv_button_create(main_cont);
    play_pause_btn_label = lv_label_create(play_pause_btn);
    lv_label_set_text(play_pause_btn_label, LV_SYMBOL_PLAY); // Estado inicial
    lv_obj_set_style_text_font(play_pause_btn_label, &lv_font_montserrat_28, 0);
    
    // --- Registrar Handlers para esta pantalla ---
    button_manager_register_view_handler(BUTTON_OK, handle_ok_press_playing);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press_playing);

    // --- INICIAR REPRODUCCIÓN AUTOMÁTICAMENTE ---
    handle_ok_press_playing();
}


// --- Creación de la vista inicial ---

static void handle_ok_press_initial() {
    show_file_explorer();
}

static void handle_cancel_press_initial() {
    // Asegurarse de que no quede nada corriendo al salir de la vista
    if (ui_update_timer) {
        lv_timer_delete(ui_update_timer);
        ui_update_timer = NULL;
    }
    audio_manager_stop(); 
    view_manager_load_view(VIEW_ID_MENU);
}

static void create_initial_speaker_view() {
    lv_obj_t *label = lv_label_create(view_parent);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_label_set_text(label, "Test Speaker");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *info_label = lv_label_create(view_parent);
    lv_label_set_text(info_label, "Presiona OK para\nseleccionar un archivo de audio");
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label);

    button_manager_register_view_handler(BUTTON_OK, handle_ok_press_initial);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press_initial);
}

// --- Función pública de entrada ---

void speaker_test_view_create(lv_obj_t *parent) {
    view_parent = parent;
    create_initial_speaker_view();
}