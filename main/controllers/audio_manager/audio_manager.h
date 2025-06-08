#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <stdbool.h>
#include <stdint.h> // Necesario para uint32_t

// Estados del reproductor de audio
typedef enum {
    AUDIO_STATE_STOPPED,
    AUDIO_STATE_PLAYING,
    AUDIO_STATE_PAUSED
} audio_player_state_t;

/**
 * @brief Inicializa el gestor de audio.
 */
void audio_manager_init(void);

/**
 * @brief Inicia la reproducción de un archivo WAV en una nueva tarea.
 * @param filepath Ruta completa al archivo .wav a reproducir.
 * @return true si la tarea de reproducción se inició, false si hubo un error.
 */
bool audio_manager_play(const char *filepath);

/**
 * @brief Pausa la reproducción de audio actual.
 */
void audio_manager_pause(void);

/**
 * @brief Reanuda la reproducción de audio si estaba en pausa.
 */
void audio_manager_resume(void);

/**
 * @brief Detiene la reproducción de audio y libera los recursos.
 */
void audio_manager_stop(void);

/**
 * @brief Obtiene el estado actual del reproductor.
 * @return El estado actual.
 */
audio_player_state_t audio_manager_get_state(void);

/**
 * @brief Obtiene la duración total de la canción actual en segundos.
 * @return Duración en segundos, o 0 si no se está reproduciendo nada.
 */
uint32_t audio_manager_get_duration_s(void);

/**
 * @brief Obtiene el progreso actual de la reproducción en segundos.
 * @return Progreso en segundos.
 */
uint32_t audio_manager_get_progress_s(void);


#endif // AUDIO_MANAGER_H