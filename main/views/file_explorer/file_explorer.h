#ifndef FILE_EXPLORER_H
#define FILE_EXPLORER_H

#include "lvgl.h"

// --- Tipos de Callback ---

/**
 * @brief Callback que se ejecuta cuando el usuario selecciona un archivo.
 * @param file_path La ruta completa del archivo seleccionado.
 */
typedef void (*file_select_callback_t)(const char *file_path);

/**
 * @brief Callback que se ejecuta cuando el usuario sale del explorador 
 * (p.ej., presionando "Cancelar" en el directorio raíz).
 */
typedef void (*file_explorer_exit_callback_t)(void);


// --- Funciones Públicas del Componente ---

/**
 * @brief Crea la interfaz del explorador de archivos.
 * 
 * @param parent El objeto LVGL padre donde se creará el explorador.
 * @param initial_path La ruta inicial desde donde empezar a explorar (p.ej. "/sdcard").
 * @param on_select Callback para cuando se selecciona un archivo.
 * @param on_exit Callback para cuando se sale del explorador.
 */
void file_explorer_create(
    lv_obj_t *parent, 
    const char *initial_path, 
    file_select_callback_t on_select, 
    file_explorer_exit_callback_t on_exit
);

/**
 * @brief Destruye el explorador de archivos y libera todos sus recursos.
 * Es crucial llamar a esta función para evitar fugas de memoria.
 */
void file_explorer_destroy(void);

#endif // FILE_EXPLORER_H