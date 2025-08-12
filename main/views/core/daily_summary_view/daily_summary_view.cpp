#include "daily_summary_view.h"
#include "views/view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/daily_summary_manager/daily_summary_manager.h"
#include "controllers/habit_data_manager/habit_data_manager.h"
#include "esp_log.h"
#include <cstring>
#include <algorithm>
#include <vector>

static const char *TAG = "DAILY_SUMMARY_VIEW";

DailySummaryView::DailySummaryView() {
    ESP_LOGI(TAG, "Constructed");
    DailySummaryManager::set_on_data_changed_callback([this](time_t changed_date) {
        this->reload_data_if_needed(changed_date);
    });
}

DailySummaryView::~DailySummaryView() {
    ESP_LOGI(TAG, "Destructed");
    if (audio_manager_is_playing()) {
        audio_manager_stop();
    }
    if (m_content_group) {
        if (lv_group_get_default() == m_content_group) {
            lv_group_set_default(nullptr);
        }
        lv_group_del(m_content_group);
        m_content_group = nullptr;
    }
    reset_styles();
    DailySummaryManager::set_on_data_changed_callback(nullptr);
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

    load_available_dates();
    set_nav_mode(NavMode::DATE);
}

void DailySummaryView::load_available_dates() {
    m_available_dates = DailySummaryManager::get_all_summary_dates();
    
    time_t today = get_start_of_day(time(NULL));
    m_available_dates.push_back(today);
    
    std::sort(m_available_dates.begin(), m_available_dates.end());
    m_available_dates.erase(std::unique(m_available_dates.begin(), m_available_dates.end()), m_available_dates.end());

    auto today_it = std::find(m_available_dates.begin(), m_available_dates.end(), today);
    int initial_index = std::distance(m_available_dates.begin(), today_it);

    if (m_available_dates.empty()) {
        m_current_date_index = -1;
        update_ui();
    } else {
        load_data_for_date_by_index(initial_index);
    }
}

bool DailySummaryView::load_data_for_date_by_index(int index) {
    if (index < 0 || (size_t)index >= m_available_dates.size()) {
        return false;
    }
    m_current_date_index = index;
    m_current_date = m_available_dates[index];
    m_current_summary = DailySummaryManager::get_summary_for_date(m_current_date);
    ESP_LOGI(TAG, "Loading data for date: %lld (index %d)", (long long)m_current_date, m_current_date_index);
    update_ui();
    return true;
}

void DailySummaryView::reload_data_if_needed(time_t changed_date) {
    if (get_start_of_day(changed_date) == m_current_date) {
        ESP_LOGI(TAG, "Summary data changed for current view, reloading.");
        load_available_dates();
    }
}

void DailySummaryView::init_styles() {
    if (m_styles_initialized) return;
    lv_style_init(&m_style_focused_content);
    lv_style_set_bg_color(&m_style_focused_content, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_color(&m_style_focused_content, lv_color_white());
    m_styles_initialized = true;
}

void DailySummaryView::reset_styles() {
    if (!m_styles_initialized) return;
    lv_style_reset(&m_style_focused_content);
    m_styles_initialized = false;
}

void DailySummaryView::set_nav_mode(NavMode mode) {
    m_nav_mode = mode;
    lv_obj_t* date_cont = lv_obj_get_child(container, 0);
    lv_obj_t* left_arrow = lv_obj_get_child(date_cont, 0);
    lv_obj_t* right_arrow = lv_obj_get_child(date_cont, 2);

    if (m_nav_mode == NavMode::CONTENT) {
        // Content is active: Date navigator is inactive (dimmed)
        lv_obj_set_style_bg_color(date_cont, lv_palette_main(LV_PALETTE_BLUE_GREY), 0);
        lv_obj_set_style_text_color(left_arrow, lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_set_style_text_color(right_arrow, lv_palette_main(LV_PALETTE_GREY), 0);

        lv_group_focus_freeze(m_content_group, false);
        lv_group_set_default(m_content_group);
        if (lv_group_get_obj_count(m_content_group) > 0) {
            lv_group_focus_obj(lv_group_get_obj_by_index(m_content_group, 0));
        }
    } else { // DATE mode
        // Date is active: Date navigator is highlighted
        lv_obj_set_style_bg_color(date_cont, lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_obj_set_style_text_color(left_arrow, lv_color_white(), 0);
        lv_obj_set_style_text_color(right_arrow, lv_color_white(), 0);

        lv_group_focus_freeze(m_content_group, true);
        lv_group_set_default(nullptr);
    }
}

void DailySummaryView::setup_ui(lv_obj_t* parent) {
    // --- Date Header ---
    lv_obj_t* date_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(date_cont);
    lv_obj_set_style_bg_opa(date_cont, LV_OPA_COVER, 0); // Ensure background color is visible
    lv_obj_set_size(date_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(date_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(date_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(date_cont, 10, 0);
    lv_obj_set_style_pad_ver(date_cont, 5, 0);

    lv_obj_t* left_arrow = lv_label_create(date_cont);
    lv_label_set_text(left_arrow, LV_SYMBOL_LEFT);
    m_date_label = lv_label_create(date_cont);
    lv_obj_set_style_text_font(m_date_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(m_date_label, lv_color_white(), 0); // Date text is always white on dark bg
    lv_obj_t* right_arrow = lv_label_create(date_cont);
    lv_label_set_text(right_arrow, LV_SYMBOL_RIGHT);
    
    // --- Main list for content ---
    m_list = lv_list_create(parent);
    lv_obj_set_width(m_list, LV_PCT(100));
    lv_obj_set_flex_grow(m_list, 1);
    lv_obj_center(m_list);

    m_content_group = lv_group_create();
    lv_group_set_wrap(m_content_group, true);
}

lv_obj_t* DailySummaryView::add_list_item(ContentItem item_id, const char* title) {
    lv_obj_t* btn = lv_list_add_button(m_list, NULL, title);
    lv_obj_add_style(btn, &m_style_focused_content, LV_STATE_FOCUSED);
    lv_obj_set_user_data(btn, (void*)item_id);
    lv_group_add_obj(m_content_group, btn);

    lv_obj_t* value_label = lv_label_create(btn);
    lv_label_set_text(value_label, ""); // Will be populated in update_ui
    lv_obj_set_style_text_color(value_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_style(value_label, &m_style_focused_content, LV_STATE_FOCUSED);
    lv_obj_align(value_label, LV_ALIGN_RIGHT_MID, -10, 0);
    return btn;
}

void DailySummaryView::update_ui() {
    lv_obj_clean(m_list);
    lv_group_remove_all_objs(m_content_group);

    if (m_current_date_index == -1) {
        lv_label_set_text(m_date_label, "No Data");
        lv_list_add_text(m_list, "No data available.");
        return;
    }

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

    // --- Update Journal Item ---
    lv_obj_t* journal_btn = add_list_item(ContentItem::JOURNAL, "Journal Entry");
    lv_obj_t* journal_value_label = lv_obj_get_child(journal_btn, 1);
    if (!m_current_summary.journal_entry_path.empty()) {
        lv_obj_clear_state(journal_btn, LV_STATE_DISABLED);
        if (audio_manager_is_playing() && m_current_summary.journal_entry_path == audio_manager_get_current_file()) {
             lv_label_set_text(journal_value_label, "Playing...");
        } else {
             lv_label_set_text(journal_value_label, "Play " LV_SYMBOL_PLAY);
        }
    } else {
        lv_obj_add_state(journal_btn, LV_STATE_DISABLED);
        lv_label_set_text(journal_value_label, "None");
    }

    // --- Update Habits Item ---
    lv_obj_t* habits_btn = add_list_item(ContentItem::HABITS, "Completed Habits");
    lv_obj_t* habits_value_label = lv_obj_get_child(habits_btn, 1);
    size_t completed_count = m_current_summary.completed_habit_ids.size();
    size_t total_count = HabitDataManager::get_all_active_habits().size();
    lv_label_set_text_fmt(habits_value_label, "%d / %d", (int)completed_count, (int)total_count);
    total_count == 0 ? lv_obj_add_state(habits_btn, LV_STATE_DISABLED) : lv_obj_clear_state(habits_btn, LV_STATE_DISABLED);

    // --- Update Notes Item ---
    lv_obj_t* notes_btn = add_list_item(ContentItem::NOTES, "Voice Notes");
    lv_obj_t* notes_value_label = lv_obj_get_child(notes_btn, 1);
    int note_count = m_current_summary.voice_note_paths.size();
    lv_label_set_text_fmt(notes_value_label, "%d recorded", note_count);
    note_count == 0 ? lv_obj_add_state(notes_btn, LV_STATE_DISABLED) : lv_obj_clear_state(notes_btn, LV_STATE_DISABLED);
}

void DailySummaryView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_left_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_right_press_cb, true, this);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press_cb, true, this);
}

void DailySummaryView::on_left_press() {
    if (m_nav_mode == NavMode::DATE) {
        if (m_current_date_index > 0) {
            load_data_for_date_by_index(m_current_date_index - 1);
        }
    } else { // CONTENT mode
        lv_group_focus_prev(m_content_group);
        lv_obj_t* focused_obj = lv_group_get_focused(m_content_group);
        if (focused_obj) {
            lv_obj_scroll_to_view(focused_obj, LV_ANIM_ON);
        }
    }
}

void DailySummaryView::on_right_press() {
    if (m_nav_mode == NavMode::DATE) {
        if (m_current_date_index < (int)m_available_dates.size() - 1) {
            load_data_for_date_by_index(m_current_date_index + 1);
        }
    } else { // CONTENT mode
        lv_group_focus_next(m_content_group);
        lv_obj_t* focused_obj = lv_group_get_focused(m_content_group);
        if (focused_obj) {
            lv_obj_scroll_to_view(focused_obj, LV_ANIM_ON);
        }
    }
}

void DailySummaryView::on_ok_press() {
    if (m_nav_mode == NavMode::DATE) {
        set_nav_mode(NavMode::CONTENT);
    } else { // CONTENT mode
        on_item_action();
    }
}

void DailySummaryView::on_cancel_press() {
    if (audio_manager_is_playing()) {
        audio_manager_stop();
        update_ui();
        return;
    }
    
    if (m_nav_mode == NavMode::CONTENT) {
        set_nav_mode(NavMode::DATE);
    } else { // DATE mode
        view_manager_load_view(VIEW_ID_MENU);
    }
}

void DailySummaryView::on_item_action() {
    lv_obj_t* focused_item = lv_group_get_focused(m_content_group);
    if (!focused_item) return;

    ContentItem item_id = (ContentItem)(uintptr_t)lv_obj_get_user_data(focused_item);

    switch (item_id) {
        case ContentItem::JOURNAL:
            if (audio_manager_is_playing()) {
                audio_manager_stop();
            } else if (!m_current_summary.journal_entry_path.empty()) {
                ESP_LOGI(TAG, "Playing journal: %s", m_current_summary.journal_entry_path.c_str());
                audio_manager_play(m_current_summary.journal_entry_path.c_str());
            }
            update_ui();
            break;

        case ContentItem::NOTES:
            ESP_LOGI(TAG, "Navigating to voice note player");
            view_manager_load_view(VIEW_ID_VOICE_NOTE_PLAYER);
            break;

        case ContentItem::HABITS:
            ESP_LOGI(TAG, "Navigating to habit tracker");
            view_manager_load_view(VIEW_ID_TRACK_HABITS);
            break;
    }
}

time_t DailySummaryView::get_start_of_day(time_t timestamp) {
    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = -1; // Let mktime determine DST
    return mktime(&timeinfo);
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