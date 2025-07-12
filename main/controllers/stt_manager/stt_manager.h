#ifndef STT_MANAGER_H
#define STT_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback para notificar el resultado de la transcripción.
 *
 * @param success true si la transcripción fue exitosa.
 * @param result Puntero al texto transcrito (debe ser liberado por el receptor con free()) o a un mensaje de error.
 */
typedef void (*stt_result_callback_t)(bool success, char* result);

/**
 * @brief Inicializa el manager de STT.
 */
void stt_manager_init(void);

/**
 * @brief Inicia la transcripción de un archivo de audio en una tarea de fondo.
 *
 * @param file_path Ruta completa del archivo .wav a transcribir.
 * @param cb El callback que se ejecutará al finalizar la transcripción.
 * @return true si la tarea de transcripción se inició correctamente, false en caso contrario.
 */
bool stt_manager_transcribe(const char* file_path, stt_result_callback_t cb);

#ifdef __cplusplus
}
#endif

#endif // STT_MANAGER_H