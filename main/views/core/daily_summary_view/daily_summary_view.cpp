#include "daily_summary_view.h"
#include "views/view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/daily_summary_manager/daily_summary_manager.h"
#include "controllers/habit_data_manager/habit_data_manager.h"
#include "esp_log.h"
#include <cstring>

static const char *TAG = "DAILY_SUMMARY_VIEW";
constexpr int32_t SCROLL_AMOUNT = 40;

DailySummaryView::DailySummaryView() {
    ESP_LOGI(TAG, "Constructed");
}

DailySummaryView::~DailySummaryView() {
    ESP_LOGI(TAG, "Destructed");
    if (audio_manager_is_playing()) {
        audio_manager_stop();
    }
    reset_styles();
}

void DailySummaryView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating UI");
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    
    init_styles();
    setup_ui(container);
    setup_button_handlers();

    time_t latest_date = DailySummaryManager::get_latest_summary_date();
    if (latest_date == 0) {
        latest_date = time(NULL); // Default to today if no summaries exist
    }
    load_data_for_date(latest_date);
    set_nav_mode(NavMode::DATE);
}

void DailySummaryView::init_styles() {
    if (m_styles_initialized) return;
    lv_style_init(&m_style_focused_content);
    lv_style_set_border_width(&m_style_focused_content, 2);
    lv_style_set_border_color(&m_style_focused_content, lv_palette_main(LV_PALETTE_BLUE));
    m_styles_initialized = true;
}

void DailySummaryView::reset_styles() {
    if (!m_styles_initialized) return;
    lv_style_reset(&m_style_focused_content);
    m_styles_initialized = false;
}

void DailySummaryView::set_nav_mode(NavMode mode) {
    m_nav_mode = mode;
    if (m_nav_mode == NavMode::CONTENT) {
        lv_obj_add_style(m_content_container, &m_style_focused_content, 0);
    } else {
        lv_obj_remove_style(m_content_container, &m_style_focused_content, 0);
    }
}

time_t DailySummaryView::get_start_of_day(time_t timestamp) {
    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    return mktime(&timeinfo);
}

void DailySummaryView::setup_ui(lv_obj_t* parent) {
    // --- Date Header ---
    lv_obj_t* date_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(date_cont);
    lv_obj_set_size(date_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(date_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(date_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(date_cont, 10, 0);

    lv_obj_t* left_arrow = lv_label_create(date_cont);
    lv_label_set_text(left_arrow, LV_SYMBOL_LEFT);
    m_date_label = lv_label_create(date_cont);
    lv_obj_set_style_text_font(m_date_label, &lv_font_montserrat_20, 0);
    lv_obj_t* right_arrow = lv_label_create(date_cont);
    lv_label_set_text(right_arrow, LV_SYMBOL_RIGHT);
    
    // --- Main scrollable content area ---
    m_content_container = lv_obj_create(parent);
    lv_obj_set_width(m_content_container, LV_PCT(95));
    lv_obj_set_flex_grow(m_content_container, 1);
    lv_obj_set_flex_flow(m_content_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_content_container, 10, 0);
    lv_obj_set_style_pad_row(m_content_container, 15, 0);
    lv_obj_set_scroll_dir(m_content_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(m_content_container, LV_SCROLLBAR_MODE_AUTO);

    // --- Journal Section ---
    lv_obj_t* journal_title = lv_label_create(m_content_container);
    lv_label_set_text(journal_title, "Daily Journal");
    lv_obj_set_style_text_font(journal_title, &lv_font_montserrat_16, 0);

    m_journal_button = lv_button_create(m_content_container);
    lv_obj_set_size(m_journal_button, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_add_event_cb(m_journal_button, play_journal_event_cb, LV_EVENT_CLICKED, this);
    
    m_journal_label = lv_label_create(m_journal_button);
    lv_obj_center(m_journal_label);
    
    // --- Habits Section ---
    lv_obj_t* habits_title = lv_label_create(m_content_container);
    lv_label_set_text(habits_title, "Completed Habits");
    lv_obj_set_style_text_font(habits_title, &lv_font_montserrat_16, 0);
    
    m_habits_label = lv_label_create(m_content_container);
    lv_label_set_long_mode(m_habits_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(m_habits_label, LV_PCT(100));

    // --- Notes Section ---
    lv_obj_t* notes_title = lv_label_create(m_content_container);
    lv_label_set_text(notes_title, "Voice Notes");
    lv_obj_set_style_text_font(notes_title, &lv_font_montserrat_16, 0);

    m_notes_label = lv_label_create(m_content_container);
    lv_label_set_long_mode(m_notes_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(m_notes_label, LV_PCT(100));
}

void DailySummaryView::load_data_for_date(time_t date) {
    m_current_date = get_start_of_day(date);
    m_current_summary = DailySummaryManager::get_summary_for_date(m_current_date);
    ESP_LOGI(TAG, "Loading data for date: %lld", (long long)m_current_date);
    update_ui();
}

void DailySummaryView::update_ui() {
    // --- Update Date Label ---
    char date_buf[32];
    time_t today = get_start_of_day(time(NULL));
    if (m_current_date == today) {
        strcpy(date_buf, "Today");
    } else {
        struct tm timeinfo;
        localtime_r(&m_current_date, &timeinfo);
        strftime(date_buf, sizeof(date_buf), "%b %d, %Y", &timeinfo);
    }
    lv_label_set_text(m_date_label, date_buf);

    // --- Update Journal Button ---
    if (!m_current_summary.journal_entry_path.empty()) {
        lv_obj_clear_state(m_journal_button, LV_STATE_DISABLED);
        lv_label_set_text(m_journal_label, LV_SYMBOL_PLAY "  Play Entry");
    } else {
        lv_obj_add_state(m_journal_button, LV_STATE_DISABLED);
        lv_label_set_text(m_journal_label, "No Entry Recorded");
    }

    // --- Update Habits List ---
    if (!m_current_summary.completed_habit_ids.empty()) {
        std::string habits_text;
        for (uint32_t id : m_current_summary.completed_habit_ids) {
            Habit* habit = HabitDataManager::get_habit_by_id(id);
            if (habit) {
                habits_text += "- " + habit->name + "\n";
            }
        }
        lv_label_set_text(m_habits_label, habits_text.c_str());
    } else {
        lv_label_set_text(m_habits_label, "No habits completed.");
    }
    
    // --- Update Notes Count ---
    int note_count = m_current_summary.voice_note_paths.size();
    if (note_count > 0) {
        lv_label_set_text_fmt(m_notes_label, "%d note(s) recorded.", note_count);
    } else {
        lv_label_set_text(m_notes_label, "No notes recorded.");
    }
    
    // Reset scroll position when data changes
    lv_obj_scroll_to(m_content_container, 0, 0, LV_ANIM_OFF);
}

void DailySummaryView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_left_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_right_press_cb, true, this);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press_cb, true, this);
}

void DailySummaryView::on_left_press() {
    if (m_nav_mode == NavMode::DATE) {
        m_current_date -= 24 * 60 * 60; // Subtract one day
        load_data_for_date(m_current_date);
    } else { // NavMode::CONTENT
        lv_obj_scroll_by(m_content_container, 0, -SCROLL_AMOUNT, LV_ANIM_ON);
    }
}

void DailySummaryView::on_right_press() {
    if (m_nav_mode == NavMode::DATE) {
        time_t today = get_start_of_day(time(NULL));
        if (m_current_date >= today) return; // Don't go into the future
        m_current_date += 24 * 60 * 60; // Add one day
        load_data_for_date(m_current_date);
    } else { // NavMode::CONTENT
        lv_obj_scroll_by(m_content_container, 0, SCROLL_AMOUNT, LV_ANIM_ON);
    }
}

void DailySummaryView::on_ok_press() {
    if (m_nav_mode == NavMode::DATE) {
        set_nav_mode(NavMode::CONTENT);
    } else { // NavMode::CONTENT
        on_play_journal_press();
    }
}

void DailySummaryView::on_cancel_press() {
    if (audio_manager_is_playing()) {
        audio_manager_stop();
        return;
    }
    
    if (m_nav_mode == NavMode::CONTENT) {
        set_nav_mode(NavMode::DATE);
    } else { // NavMode::DATE
        view_manager_load_view(VIEW_ID_MENU);
    }
}

void DailySummaryView::on_play_journal_press() {
    if (audio_manager_is_playing()) {
        audio_manager_stop();
        lv_label_set_text(m_journal_label, LV_SYMBOL_PLAY "  Play Entry");
        return;
    }

    if (!m_current_summary.journal_entry_path.empty()) {
        ESP_LOGI(TAG, "Playing journal: %s", m_current_summary.journal_entry_path.c_str());
        if(audio_manager_play(m_current_summary.journal_entry_path.c_str())) {
            lv_label_set_text(m_journal_label, LV_SYMBOL_STOP "  Stop");
        }
    }
}

// --- Static Callbacks ---
void DailySummaryView::handle_left_press_cb(void* user_data) {
    static_cast<DailySummaryView*>(user_data)->on_left_press();
}

void DailySummaryView::handle_right_press_cb(void* user_data) {
    static_cast<DailySummaryView*>(user_data)->on_right_press();
}

void DailySummaryView::handle_ok_press_cb(void* user_data) {
    static_cast<DailySummaryView*>(user_data)->on_ok_press();
}

void DailySummaryView::handle_cancel_press_cb(void* user_data) {
    static_cast<DailySummaryView*>(user_data)->on_cancel_press();
}

void DailySummaryView::play_journal_event_cb(lv_event_t* e) {
    auto* view = static_cast<DailySummaryView*>(lv_event_get_user_data(e));
    if (view && view->m_nav_mode == NavMode::CONTENT) {
        view->on_play_journal_press();
    }
}