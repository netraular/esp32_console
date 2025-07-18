#ifndef VOICE_NOTE_VIEW_H
#define VOICE_NOTE_VIEW_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates the user interface for the voice note recording view.
 * @param parent The parent object on which the UI will be created.
 */
void voice_note_view_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif // VOICE_NOTE_VIEW_H