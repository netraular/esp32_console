#ifndef DAILY_SUMMARY_VIEW_H
#define DAILY_SUMMARY_VIEW_H

#include "views/view.h"
#include "controllers/audio_manager/audio_manager.h"
#include "models/daily_summary_model.h"
#include "lvgl.h"
#include <time.h>
#include <vector>
#include <string>

/**
 * @brief A view to display a summary of activities for a specific day.
 *
 * This view uses a modern, card-based layout to present daily data. It allows
 * the user to navigate through different days and see what habits were completed,
 * listen to the daily journal entry, and view other tracked activities.
 * It features a two-mode navigation system: DATE mode for changing days and
 * CONTENT mode for interacting with the cards.
 */
class DailySummaryView : public View {
public:
    DailySummaryView();
    ~DailySummaryView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- Internal Enums for State Management ---
    enum class NavMode { DATE, CONTENT };
    enum class ContentItem { JOURNAL, HABITS, NOTES };

    // --- UI and State Members ---
    NavMode m_nav_mode = NavMode::DATE;
    time_t m_current_date;
    DailySummaryData m_current_summary;
    std::vector<time_t> m_available_dates;
    int m_current_date_index = -1;

    // --- LVGL Objects ---
    lv_obj_t* m_date_header = nullptr;
    lv_obj_t* m_date_label = nullptr;
    lv_obj_t* m_content_area = nullptr;
    lv_group_t* m_content_group = nullptr;
    
    // --- LVGL Styles ---
    lv_style_t m_style_date_header;
    lv_style_t m_style_date_header_active;
    lv_style_t m_style_card;
    lv_style_t m_style_card_focused;
    lv_style_t m_style_card_icon;
    lv_style_t m_style_card_title;
    bool m_styles_initialized = false;

    // --- Private Methods ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();
    void load_available_dates();
    bool load_data_for_date_by_index(int index);
    void update_ui();
    time_t get_start_of_day(time_t timestamp);

    void init_styles();
    void reset_styles();
    void set_nav_mode(NavMode mode);
    void reload_data_if_needed(time_t changed_date);
    lv_obj_t* create_content_card(lv_obj_t* parent, ContentItem item_id, const char* icon, const char* title);
    
    // --- Instance Methods for Actions ---
    void on_left_press();
    void on_right_press();
    void on_ok_press();
    void on_cancel_press();
    void on_item_action();

    // --- Static Callbacks (Bridge to C-style APIs) ---
    static void handle_left_press_cb(void* user_data);
    static void handle_right_press_cb(void* user_data);
    static void handle_ok_press_cb(void* user_data);
    static void handle_cancel_press_cb(void* user_data);
};

#endif // DAILY_SUMMARY_VIEW_H