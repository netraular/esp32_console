#ifndef VOICE_NOTE_VIEW_H
#define VOICE_NOTE_VIEW_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Crea la interfaz de usuario para la vista de grabación de notas de voz.
 * @param parent El objeto padre sobre el que se creará la UI.
 */
void voice_note_view_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif // VOICE_NOTE_VIEW_H