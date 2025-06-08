#include "speaker_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "../file_explorer/file_explorer.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SPEAKER_TEST_VIEW";

// --- Variables de estado de la vista ---
static lv_obj_t *view_parent = NULL; // Contenedor principal de la vista, para limpiarlo
static bool is_playing = false;      // Estado de reproducción (play/pause)
static lv_obj_t *play_pause_btn_label = NULL; // Puntero al label del botón para cambiar el icono

// --- Prototipos de las funciones que crean cada "pantalla" ---
static void create_initial_speaker_view();
static void show_file_explorer();
static void create_now_playing_view(const char *file_path);

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
 * @param path Ruta completa del archivo (p.ej. "/sdcard/music/song.wav").
 * @return Puntero al inicio del nombre del archivo ("song.wav").
 */
static const char* get_filename_from_path(const char* path) {
    const char* filename = strrchr(path, '/');
    if (filename) {
        return filename + 1;
    }
    return path;
}


// --- Callbacks y Handlers para la pantalla "Now Playing" ---

static void handle_ok_press_playing() {
    is_playing = !is_playing; // Invertir estado
    if (is_playing) {
        lv_label_set_text(play_pause_btn_label, LV_SYMBOL_PAUSE);
        ESP_LOGI(TAG, "Audio: Play");
        // Aquí iría la lógica para iniciar/reanudar la reproducción
    } else {
        lv_label_set_text(play_pause_btn_label, LV_SYMBOL_PLAY);
        ESP_LOGI(TAG, "Audio: Pause");
        // Aquí iría la lógica para pausar la reproducción
    }
}

static void handle_cancel_press_playing() {
    ESP_LOGI(TAG, "Volviendo al explorador de archivos");
    // Limpiar la pantalla "Now Playing" y volver a mostrar el explorador
    is_playing = false; // Resetear estado
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
        // Opcional: mostrar un popup de error
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

    // Creamos un contenedor que ocupe toda la pantalla para el explorador
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
    is_playing = false; // Estado inicial es pausado

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
    lv_label_set_text(vis_label, LV_SYMBOL_AUDIO " Visualizer");
    lv_obj_set_style_text_color(vis_label, lv_color_hex(0x888888), 0);
    lv_obj_center(vis_label);

    // --- 3. Barra de progreso y tiempos ---
    lv_obj_t *progress_cont = lv_obj_create(main_cont);
    lv_obj_remove_style_all(progress_cont);
    lv_obj_set_width(progress_cont, lv_pct(95));
    lv_obj_set_height(progress_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(progress_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(progress_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *time_current_label = lv_label_create(progress_cont);
    lv_label_set_text(time_current_label, "00:00");
    
    lv_obj_t *slider = lv_slider_create(progress_cont);
    // *** ESTA ES LA LÍNEA CORREGIDA ***
    lv_obj_set_style_flex_grow(slider, 1, 0); // Para que ocupe el espacio disponible
    lv_obj_remove_flag(slider, LV_OBJ_FLAG_CLICKABLE); // No se puede mover con el dedo/ratón
    lv_slider_set_value(slider, 0, LV_ANIM_OFF);
    lv_obj_set_style_margin_hor(slider, 10, 0);

    lv_obj_t *time_total_label = lv_label_create(progress_cont);
    lv_label_set_text(time_total_label, "03:45"); // Placeholder

    // --- 4. Botón de Play/Pause ---
    lv_obj_t *play_pause_btn = lv_button_create(main_cont);
    play_pause_btn_label = lv_label_create(play_pause_btn);
    lv_label_set_text(play_pause_btn_label, LV_SYMBOL_PLAY); // Estado inicial
    lv_obj_set_style_text_font(play_pause_btn_label, &lv_font_montserrat_28, 0);
    
    // --- Registrar Handlers para esta pantalla ---
    button_manager_register_view_handler(BUTTON_OK, handle_ok_press_playing);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press_playing);
}


// --- Creación de la vista inicial ---

static void handle_ok_press_initial() {
    show_file_explorer();
}

static void handle_cancel_press_initial() {
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

void speaker_test_view_create(lv_obj_t *parent) {
    view_parent = parent;
    create_initial_speaker_view();
}