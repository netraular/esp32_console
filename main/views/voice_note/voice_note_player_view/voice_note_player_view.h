#ifndef VOICE_NOTE_PLAYER_VIEW_H
#define VOICE_NOTE_PLAYER_VIEW_H

#include "views/view.h"
#include "lvgl.h"

/**
 * @brief A view for browsing, playing, and managing voice notes.
 *
 * This view uses the file_explorer component to list .wav files from the
 * user's notes directory. It allows playback, deletion, and transcription
 * of selected notes.
 */
class VoiceNotePlayerView : public View {
public:
    VoiceNotePlayerView();
    ~VoiceNotePlayerView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- UI Widgets and State ---
    lv_obj_t* loading_indicator = nullptr;
    lv_obj_t* action_menu_container = nullptr;
    lv_obj_t* file_explorer_host_container = nullptr;
    lv_group_t* action_menu_group = nullptr;
    char selected_item_path[256] = {0};
    lv_style_t style_action_menu_focused;
    bool styles_initialized = false;

    // --- UI Setup & Logic ---
    void show_file_explorer();
    void create_action_menu(const char* path);
    void destroy_action_menu(bool refresh_explorer);
    void show_loading_indicator(const char* text);
    void hide_loading_indicator();
    void init_action_menu_styles();
    void reset_action_menu_styles();

    // --- Instance Methods for Actions (called from static callbacks) ---
    void on_audio_file_selected(const char* path);
    void on_file_long_pressed(const char* path);
    void on_explorer_exit();
    void on_player_exit();
    void on_viewer_exit();
    
    // --- Action Menu Handlers ---
    void on_action_menu_ok();
    void on_action_menu_cancel();
    void on_action_menu_nav(bool is_next);
    
    // --- Static Callbacks for C Components (Bridge to instance methods) ---
    static void audio_file_selected_cb_c(const char* path, void* user_data);
    static void file_long_pressed_cb_c(const char* path, void* user_data);
    static void explorer_exit_cb_c(void* user_data);
    static void player_exit_cb_c(void* user_data);
    static void viewer_exit_cb_c(void* user_data);

    // --- Static Callbacks for Button Manager ---
    static void action_menu_ok_cb(void* user_data);
    static void action_menu_cancel_cb(void* user_data);
    static void action_menu_left_cb(void* user_data);
    static void action_menu_right_cb(void* user_data);

    // --- Static LVGL Callbacks ---
    static void explorer_cleanup_cb(lv_event_t * e);
    static void on_transcription_complete_ui_thread(void *user_data);
};

#endif // VOICE_NOTE_PLAYER_VIEW_H