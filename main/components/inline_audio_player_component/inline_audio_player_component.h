#ifndef INLINE_AUDIO_PLAYER_COMPONENT_H
#define INLINE_AUDIO_PLAYER_COMPONENT_H

#include "lvgl.h"
#include <string>
#include <functional>

/**
 * @class InlineAudioPlayerComponent
 * @brief A self-contained, inline LVGL component for playing a single audio file.
 *
 * This class encapsulates the UI (progress slider, time labels, volume) for audio playback.
 * It is "headless", meaning it has no internal buttons and relies on the parent view
 * for playback control (play/pause, stop). The playback state is visually indicated
 * by changing the shape of the slider's knob.
 */
class InlineAudioPlayerComponent {
public:
    /**
     * @brief Constructs the inline audio player and starts playback.
     * @param parent The parent LVGL object to create the player on.
     * @param file_path The path to the audio file to play.
     */
    InlineAudioPlayerComponent(lv_obj_t* parent, const std::string& file_path);

    /**
     * @brief Destructor. Cleans up resources, including LVGL styles, the timer, and stops audio.
     */
    ~InlineAudioPlayerComponent();

    /**
     * @brief Sets a callback to be invoked when playback finishes or is cancelled.
     * @param cb The callback function.
     */
    void set_on_close_callback(std::function<void()> cb);

    /**
     * @brief Toggles the playback state between playing and paused.
     */
    void toggle_play_pause();

    /**
     * @brief Updates the volume display label within the component.
     */
    void update_volume_display();

    /**
     * @brief Gets the main container object for this component.
     * @return A pointer to the LVGL container object.
     */
    lv_obj_t* get_container();

private:
    std::string m_file_path;
    std::function<void()> m_on_close_callback;
    bool m_styles_initialized = false;

    // LVGL objects
    lv_obj_t* m_container = nullptr;
    lv_obj_t* m_slider = nullptr;
    lv_obj_t* m_time_current_label = nullptr;
    lv_obj_t* m_time_total_label = nullptr;
    lv_obj_t* m_volume_label = nullptr;
    lv_timer_t* m_update_timer = nullptr;

    // LVGL Styles
    lv_style_t m_style_slider_main;
    lv_style_t m_style_knob_playing;
    lv_style_t m_style_knob_paused;

    void init_styles();
    void reset_styles();
    void setup_ui(lv_obj_t* parent);
    void update_ui_timer_cb();
    void update_playback_state_indicator();
    
    static void static_update_ui_timer_cb(lv_timer_t* timer);
};

#endif // INLINE_AUDIO_PLAYER_COMPONENT_H