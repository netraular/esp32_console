#ifndef DAILY_SUMMARY_VIEW_H
#define DAILY_SUMMARY_VIEW_H

#include "views/view.h"
#include "controllers/audio_manager/audio_manager.h"
#include "models/daily_summary_model.h"
#include "lvgl.h"
#include <time.h>

/**
 * @brief A view to display a summary of activities for a specific day.
 *
 * This view allows the user to navigate through different days and see
 * what habits were completed, listen to the daily journal entry, and see
 * other tracked data for that day. It features a two-mode navigation system:
 * DATE mode for changing days and CONTENT mode for scrolling the day's details.
 */
class DailySummaryView : public View {
public:
    DailySummaryView();
    ~DailySummaryView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- Navigation state ---
    enum class NavMode { DATE, CONTENT };
    NavMode m_nav_mode = NavMode::DATE;

    // --- UI and State Members ---
    time_t m_current_date;
    DailySummaryData m_current_summary;
    lv_obj_t* m_date_label = nullptr;
    lv_obj_t* m_journal_button = nullptr;
    lv_obj_t* m_journal_label = nullptr;
    lv_obj_t* m_habits_label = nullptr;
    lv_obj_t* m_notes_label = nullptr;
    lv_obj_t* m_content_container = nullptr;

    lv_style_t m_style_focused_content;
    bool m_styles_initialized = false;

    // --- Private Methods ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();
    void load_data_for_date(time_t date);
    void update_ui();
    time_t get_start_of_day(time_t timestamp);

    void init_styles();
    void reset_styles();
    void set_nav_mode(NavMode mode);
    void reload_data_if_needed(time_t changed_date);

    // --- Instance Methods for Actions ---
    void on_left_press();
    void on_right_press();
    void on_ok_press();
    void on_cancel_press();
    void on_play_journal_press();

    // --- Static Callbacks ---
    static void handle_left_press_cb(void* user_data);
    static void handle_right_press_cb(void* user_data);
    static void handle_ok_press_cb(void* user_data);
    static void handle_cancel_press_cb(void* user_data);
    static void play_journal_event_cb(lv_event_t* e);
};

#endif // DAILY_SUMMARY_VIEW_H