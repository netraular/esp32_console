#ifndef VOLUME_TESTER_VIEW_H
#define VOLUME_TESTER_VIEW_H

#include "views/view.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h" // <<< ADDED
#include "esp_log.h"
#include "lvgl.h"

/**
 * @brief View for testing speaker volume levels.
 *
 * This view allows the user to adjust the physical volume and play a test
 * sound on a loop. It's designed to help find a safe maximum volume for the
 * device's speaker to prevent damage. When exiting, it resets the volume
 * to a safe default.
 */
class VolumeTesterView : public View {
public:
    VolumeTesterView();
    ~VolumeTesterView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- View State ---
    enum class ViewState {
        CHECKING_SD,
        SD_ERROR,
        READY
    };
    ViewState current_state;

    // --- Constants ---
    static const char* TEST_SOUND_PATH;
    static const uint8_t SAFE_DEFAULT_VOLUME = 15;

    // --- UI Widgets ---
    lv_obj_t* volume_label = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_timer_t* audio_check_timer = nullptr;

    // --- State ---
    bool is_playing = false;

    // --- Private Methods ---
    void setup_ui();
    void show_ready_ui();
    void show_error_ui();
    void update_volume_label();

    // --- Instance Methods for Actions ---
    void on_play_toggle();
    void on_retry_check();
    void on_exit_press();
    void on_volume_up();
    void on_volume_down();

    // --- Static Callbacks (Bridge to C-style APIs) ---
    static void audio_check_timer_cb(lv_timer_t* timer);
    static void ok_press_cb(void* user_data);
    static void exit_press_cb(void* user_data);
    static void volume_up_cb(void* user_data);
    static void volume_down_cb(void* user_data);
};

#endif // VOLUME_TESTER_VIEW_H