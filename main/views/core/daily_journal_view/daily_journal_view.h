#ifndef DAILY_JOURNAL_VIEW_H
#define DAILY_JOURNAL_VIEW_H

#include "views/view.h"
#include "controllers/audio_recorder/audio_recorder.h"
#include "components/popup_manager/popup_manager.h"
#include "lvgl.h"
#include <string>

/**
 * @brief View for recording a daily voice journal entry.
 *
 * This class provides a UI to record a single voice entry per day.
 * It handles checking for existing entries, prompting for overwrite,
 * and managing the recording process.
 */
class DailyJournalView : public View {
public:
    DailyJournalView();
    ~DailyJournalView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- UI Widgets ---
    lv_obj_t* status_label = nullptr;
    lv_obj_t* time_label = nullptr;
    lv_obj_t* icon_label = nullptr;
    lv_timer_t* ui_update_timer = nullptr;

    // --- State ---
    std::string current_filepath;
    audio_recorder_state_t last_known_state;

    // --- Private Methods ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();
    void format_time(char* buf, size_t buf_size, uint32_t time_s);
    void update_ui_for_state(audio_recorder_state_t state);
    void update_ui();
    void get_todays_filepath(std::string& path);
    void start_recording();

    // --- Instance Methods for Actions ---
    void on_ok_press();
    void on_cancel_press();
    void handle_overwrite_confirmation(popup_result_t result);

    // --- Static Callbacks (Bridge to C-style APIs) ---
    static void ok_press_cb(void* user_data);
    static void cancel_press_cb(void* user_data);
    static void ui_update_timer_cb(lv_timer_t* timer);
    static void overwrite_popup_cb(popup_result_t result, void* user_data);
};

#endif // DAILY_JOURNAL_VIEW_H