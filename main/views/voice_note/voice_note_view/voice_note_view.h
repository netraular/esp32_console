#ifndef VOICE_NOTE_VIEW_H
#define VOICE_NOTE_VIEW_H

#include "views/view.h"
#include "controllers/audio_recorder/audio_recorder.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"

/**
 * @brief View for recording voice notes.
 *
 * This class provides a UI to start, stop, and cancel audio recordings.
 * It displays the recording state, elapsed time, and handles file creation.
 * It also provides navigation to the player view.
 */
class VoiceNoteView : public View {
public:
    VoiceNoteView();
    ~VoiceNoteView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- UI Widgets ---
    lv_obj_t* status_label = nullptr;
    lv_obj_t* time_label = nullptr;
    lv_obj_t* icon_label = nullptr;
    lv_timer_t* ui_update_timer = nullptr;

    // --- State ---
    char current_filepath[256] = {0};
    audio_recorder_state_t last_known_state;

    // --- Private Methods ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();
    void format_time(char* buf, size_t buf_size, uint32_t time_s);
    void update_ui_for_state(audio_recorder_state_t state);
    void update_ui();

    // --- Instance Methods for Actions ---
    void on_ok_press();
    void on_right_press();
    void on_cancel_press();

    // --- Static Callbacks (Bridge to C-style APIs) ---
    static void ok_press_cb(void* user_data);
    static void right_press_cb(void* user_data);
    static void cancel_press_cb(void* user_data);
    static void ui_update_timer_cb(lv_timer_t* timer);
};

#endif // VOICE_NOTE_VIEW_H