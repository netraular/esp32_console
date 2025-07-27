#ifndef SPEAKER_TEST_VIEW_H
#define SPEAKER_TEST_VIEW_H

#include "views/view.h"
#include "components/file_explorer/file_explorer.h"
#include "controllers/button_manager/button_manager.h"
#include "lvgl.h"

class SpeakerTestView : public View {
public:
    SpeakerTestView();
    ~SpeakerTestView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- UI Widgets ---
    lv_obj_t* info_label = nullptr;
    lv_obj_t* file_explorer_host_container = nullptr;

    // --- UI Setup & Logic ---
    void create_initial_view();
    void show_file_explorer();
    void setup_initial_button_handlers();

    // --- Instance Methods for Actions ---
    void on_initial_ok_press();
    void on_initial_cancel_press();
    void on_audio_file_selected(const char* path);
    void on_explorer_exit_from_root();
    void on_player_exit();
    
    // --- Static Callbacks for Button Manager (Bridge to instance methods) ---
    static void initial_ok_press_cb(void* user_data);
    static void initial_cancel_press_cb(void* user_data);

    // --- Static Callbacks for C Components (Bridge to instance methods) ---
    static void audio_file_selected_cb_c(const char* path, void* user_data);
    static void explorer_exit_cb_c(void* user_data);
    static void player_exit_cb_c(void* user_data);
    static void explorer_cleanup_event_cb(lv_event_t * e);
};

#endif // SPEAKER_TEST_VIEW_H