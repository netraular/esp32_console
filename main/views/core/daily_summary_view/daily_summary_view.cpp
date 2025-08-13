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

// --- Lifecycle Methods ---

DailySummaryView::DailySummaryView() {
    ESP_LOGI(TAG, "Constructed");
    // Register a callback to be notified of data changes for the currently viewed day.
    DailySummaryManager::set_on_data_changed_callback([this](time_t changed_date) {
        this->reload_data_if_needed(changed_date);
    });
}

DailySummaryView::~DailySummaryView() {
    ESP_LOGI(TAG, "Destructed");
    if (audio_manager_is_playing()) {
        audio_manager_stop();
    }
    // Clean up the LVGL group
    if (m_content_group) {
        if (lv_group_get_default() == m_content_group) {
            lv_group_set_default(nullptr);
        }
        lv_group_del(m_content_group);
        m_content_group = nullptr;
    }
    reset_styles(); // Free style resources
    // Unregister the callback
    DailySummaryManager::set_on_data_changed_callback(nullptr);
}

void DailySummaryView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating UI");
    
    // Initialize styles first
    init_styles();

    // Main container for the view
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container, 5, 0);
    lv_obj_set_style_pad_gap(container, 5, 0);
    lv_obj_set_style_bg_color(container, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
    
    // Build the UI elements
    setup_ui(container);
    setup_button_handlers();

    // Load data and set initial state
    load_available_dates();
    set_nav_mode(NavMode::DATE);
}

// --- Data Handling ---

void DailySummaryView::load_available_dates() {
    m_available_dates = DailySummaryManager::get_all_summary_dates();
    
    // Ensure "Today" is always an option, even if it has no data yet.
    time_t today = get_start_of_day(time(NULL));
    m_available_dates.push_back(today);
    
    // Sort and remove duplicates
    std::sort(m_available_dates.begin(), m_available_dates.end());
    m_available_dates.erase(std::unique(m_available_dates.begin(), m_available_dates.end()), m_available_dates.end());

    // Find today's index to start on the current day.
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
    // If the data for the currently displayed day has changed, reload it and redraw the UI.
    if (get_start_of_day(changed_date) == m_current_date) {
        ESP_LOGI(TAG, "Summary data changed for current view, reloading.");
        m_current_summary = DailySummaryManager::get_summary_for_date(m_current_date);
        update_ui();
    }
}

// --- Style and UI Setup ---

void DailySummaryView::init_styles() {
    if (m_styles_initialized) return;

    // --- Transition for smooth focus effects ---
    static const lv_style_prop_t props[] = {LV_STYLE_BORDER_WIDTH, LV_STYLE_BORDER_COLOR, LV_STYLE_SHADOW_WIDTH, LV_STYLE_PROP_INV};
    static lv_style_transition_dsc_t trans_dsc;
    lv_style_transition_dsc_init(&trans_dsc, props, lv_anim_path_ease_out, 150, 0, NULL);

    // --- Date Header Style (inactive) ---
    lv_style_init(&m_style_date_header);
    lv_style_set_bg_color(&m_style_date_header, lv_color_white());
    lv_style_set_radius(&m_style_date_header, 8);
    lv_style_set_shadow_width(&m_style_date_header, 10);
    lv_style_set_shadow_opa(&m_style_date_header, LV_OPA_10);
    lv_style_set_shadow_ofs_y(&m_style_date_header, 2);
    lv_style_set_border_width(&m_style_date_header, 0); // No border by default
    lv_style_set_transition(&m_style_date_header, &trans_dsc);

    // --- Date Header Style (active/focused) ---
    lv_style_init(&m_style_date_header_active);
    lv_style_set_border_width(&m_style_date_header_active, 2);
    lv_style_set_border_color(&m_style_date_header_active, lv_palette_main(LV_PALETTE_CYAN));
    lv_style_set_shadow_width(&m_style_date_header_active, 15);

    // --- Card Style (inactive) ---
    lv_style_init(&m_style_card);
    lv_style_set_bg_color(&m_style_card, lv_color_white());
    lv_style_set_radius(&m_style_card, 8);
    lv_style_set_shadow_width(&m_style_card, 10);
    lv_style_set_shadow_opa(&m_style_card, LV_OPA_10);
    lv_style_set_shadow_ofs_y(&m_style_card, 2);
    lv_style_set_border_width(&m_style_card, 0); // No border by default
    lv_style_set_transition(&m_style_card, &trans_dsc);

    // --- Card Style (focused) ---
    lv_style_init(&m_style_card_focused);
    lv_style_set_border_width(&m_style_card_focused, 2);
    lv_style_set_border_color(&m_style_card_focused, lv_palette_main(LV_PALETTE_BLUE));

    // --- Card Icon Style ---
    lv_style_init(&m_style_card_icon);
    lv_style_set_text_color(&m_style_card_icon, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_text_font(&m_style_card_icon, &lv_font_montserrat_22);

    // --- Card Title Style ---
    lv_style_init(&m_style_card_title);
    lv_style_set_text_color(&m_style_card_title, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_text_font(&m_style_card_title, &lv_font_montserrat_16);

    m_styles_initialized = true;
}

void DailySummaryView::reset_styles() {
    if (!m_styles_initialized) return;
    lv_style_reset(&m_style_date_header);
    lv_style_reset(&m_style_date_header_active);
    lv_style_reset(&m_style_card);
    lv_style_reset(&m_style_card_focused);
    lv_style_reset(&m_style_card_icon);
    lv_style_reset(&m_style_card_title);
    m_styles_initialized = false;
}

void DailySummaryView::setup_ui(lv_obj_t* parent) {
    // --- Date Header ---
    m_date_header = lv_obj_create(parent);
    lv_obj_remove_style_all(m_date_header);
    lv_obj_add_style(m_date_header, &m_style_date_header, 0);
    lv_obj_set_width(m_date_header, LV_PCT(100));
    lv_obj_set_height(m_date_header, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(m_date_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m_date_header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(m_date_header, 10, 0);
    lv_obj_set_style_pad_ver(m_date_header, 8, 0);

    lv_obj_t* left_arrow = lv_label_create(m_date_header);
    lv_label_set_text(left_arrow, LV_SYMBOL_LEFT);

    m_date_label = lv_label_create(m_date_header);
    lv_obj_set_style_text_font(m_date_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(m_date_label, lv_palette_main(LV_PALETTE_GREY), 0);
    
    lv_obj_t* right_arrow = lv_label_create(m_date_header);
    lv_label_set_text(right_arrow, LV_SYMBOL_RIGHT);
    
    // --- Main scrollable area for content cards ---
    m_content_area = lv_obj_create(parent);
    lv_obj_remove_style_all(m_content_area);
    lv_obj_set_width(m_content_area, LV_PCT(100));
    lv_obj_set_flex_grow(m_content_area, 1);
    lv_obj_set_flex_flow(m_content_area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_content_area, 0, 0);
    lv_obj_set_style_pad_gap(m_content_area, 5, 0);
    lv_obj_set_scrollbar_mode(m_content_area, LV_SCROLLBAR_MODE_AUTO);

    m_content_group = lv_group_create();
    lv_group_set_wrap(m_content_group, true);
}

lv_obj_t* DailySummaryView::create_content_card(lv_obj_t* parent, ContentItem item_id, const char* icon, const char* title) {
    // Card is a button for focus handling and click events.
    lv_obj_t* card = lv_btn_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, &m_style_card, LV_STATE_DEFAULT);
    lv_obj_add_style(card, &m_style_card_focused, LV_STATE_FOCUSED);
    lv_obj_set_user_data(card, (void*)item_id);
    lv_group_add_obj(m_content_group, card);
    
    // Card layout
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_pad_column(card, 10, 0);
    
    // Icon on the left
    lv_obj_t* icon_label = lv_label_create(card);
    lv_obj_add_style(icon_label, &m_style_card_icon, 0);
    lv_label_set_text(icon_label, icon);
    
    // Text container for Title and Value
    lv_obj_t* text_cont = lv_obj_create(card);
    lv_obj_remove_style_all(text_cont);
    lv_obj_set_flex_grow(text_cont, 1);
    lv_obj_set_height(text_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(text_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Title Label
    lv_obj_t* title_label = lv_label_create(text_cont);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);

    // Value Label (its text will be set in update_ui)
    lv_obj_t* value_label = lv_label_create(text_cont);
    lv_label_set_text(value_label, "");
    lv_obj_add_style(value_label, &m_style_card_title, 0);
    
    return card;
}

void DailySummaryView::update_ui() {
    lv_obj_clean(m_content_area);
    lv_group_remove_all_objs(m_content_group);

    if (m_current_date_index == -1) {
        lv_label_set_text(m_date_label, "No Data");
        lv_obj_t* no_data_label = lv_label_create(m_content_area);
        lv_label_set_text(no_data_label, "No data available for any day.");
        lv_obj_center(no_data_label);
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

    // --- Journal Card ---
    lv_obj_t* journal_card = create_content_card(m_content_area, ContentItem::JOURNAL, LV_SYMBOL_AUDIO, "Daily Journal");
    lv_obj_t* journal_value_label = lv_obj_get_child(lv_obj_get_child(journal_card, 1), 1);
    if (!m_current_summary.journal_entry_path.empty()) {
        if (audio_manager_is_playing() && m_current_summary.journal_entry_path == audio_manager_get_current_file()) {
             lv_label_set_text(journal_value_label, "Playing...");
        } else {
             lv_label_set_text(journal_value_label, "Play " LV_SYMBOL_PLAY);
        }
    } else {
        lv_label_set_text(journal_value_label, "None recorded");
    }

    // --- Habits Card ---
    lv_obj_t* habits_card = create_content_card(m_content_area, ContentItem::HABITS, LV_SYMBOL_LIST, "Completed Habits");
    lv_obj_t* habits_value_label = lv_obj_get_child(lv_obj_get_child(habits_card, 1), 1);
    size_t completed_count = m_current_summary.completed_habit_ids.size();
    size_t total_count = HabitDataManager::get_all_active_habits().size();
    lv_label_set_text_fmt(habits_value_label, "%d of %d completed", (int)completed_count, (int)total_count);

    // --- Notes Card ---
    lv_obj_t* notes_card = create_content_card(m_content_area, ContentItem::NOTES, LV_SYMBOL_FILE, "Voice Notes");
    lv_obj_t* notes_value_label = lv_obj_get_child(lv_obj_get_child(notes_card, 1), 1);
    int note_count = m_current_summary.voice_note_paths.size();
    lv_label_set_text_fmt(notes_value_label, "%d saved notes", note_count);

    // --- Pomodoro Card ---
    lv_obj_t* pomodoro_card = create_content_card(m_content_area, ContentItem::POMODORO, LV_SYMBOL_REFRESH, "Focus Time");
    lv_obj_t* pomodoro_value_label = lv_obj_get_child(lv_obj_get_child(pomodoro_card, 1), 1);
    uint32_t seconds = m_current_summary.pomodoro_work_seconds;
    if (seconds == 0) {
        lv_label_set_text(pomodoro_value_label, "None tracked");
    } else {
        uint32_t hours = seconds / 3600;
        uint32_t minutes = (seconds % 3600) / 60;
        if (hours > 0 && minutes > 0) {
            lv_label_set_text_fmt(pomodoro_value_label, "%" LV_PRIu32 "h %" LV_PRIu32 "m", hours, minutes);
        } else if (hours > 0) {
            lv_label_set_text_fmt(pomodoro_value_label, "%" LV_PRIu32 "h", hours);
        } else {
            lv_label_set_text_fmt(pomodoro_value_label, "%" LV_PRIu32 "m", minutes);
        }
    }

    // If we are in date navigation mode, ensure no content card is visually focused,
    // even though the group might have an internally focused object.
    if (m_nav_mode == NavMode::DATE) {
        lv_obj_t* focused_obj = lv_group_get_focused(m_content_group);
        if (focused_obj) {
            lv_obj_clear_state(focused_obj, LV_STATE_FOCUSED);
        }
    }
}

void DailySummaryView::set_nav_mode(NavMode mode) {
    m_nav_mode = mode;
    lv_obj_t* left_arrow = lv_obj_get_child(m_date_header, 0);
    lv_obj_t* right_arrow = lv_obj_get_child(m_date_header, 2);

    if (m_nav_mode == NavMode::CONTENT) {
        // Content is active: De-emphasize date navigator
        lv_obj_remove_style(m_date_header, &m_style_date_header_active, 0);
        lv_obj_set_style_text_color(left_arrow, lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_set_style_text_color(right_arrow, lv_palette_main(LV_PALETTE_GREY), 0);

        // Activate the content group
        lv_group_set_default(m_content_group);
        lv_group_focus_freeze(m_content_group, false);

        // Explicitly focus the first available object in the group and scroll to it
        if (lv_group_get_obj_count(m_content_group) > 0) {
            lv_obj_t* first_obj = lv_group_get_obj_by_index(m_content_group, 0);
            lv_group_focus_obj(first_obj);
            lv_obj_scroll_to_view(first_obj, LV_ANIM_ON);
        }
    } else { // DATE mode
        // Date is active: Emphasize date navigator
        lv_obj_add_style(m_date_header, &m_style_date_header_active, 0);
        lv_obj_set_style_text_color(left_arrow, lv_palette_main(LV_PALETTE_CYAN), 0);
        lv_obj_set_style_text_color(right_arrow, lv_palette_main(LV_PALETTE_CYAN), 0);

        // Deactivate the content group and remove visual focus from its items
        lv_group_set_default(nullptr);
        lv_group_focus_freeze(m_content_group, true);

        lv_obj_t* focused_obj = lv_group_get_focused(m_content_group);
        if (focused_obj) {
            lv_obj_clear_state(focused_obj, LV_STATE_FOCUSED);
        }
    }
}


// --- Button Handlers and Actions ---

void DailySummaryView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_left_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_right_press_cb, true, this);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press_cb, true, this);
}

void DailySummaryView::navigate_content(bool is_next) {
    if (is_next) {
        lv_group_focus_next(m_content_group);
    } else {
        lv_group_focus_prev(m_content_group);
    }

    lv_obj_t* focused_obj = lv_group_get_focused(m_content_group);
    if (focused_obj) {
        lv_obj_scroll_to_view(focused_obj, LV_ANIM_ON);
    }
}

void DailySummaryView::on_left_press() {
    if (m_nav_mode == NavMode::DATE) {
        if (m_current_date_index > 0) {
            load_data_for_date_by_index(m_current_date_index - 1);
        }
    } else { // CONTENT mode
        navigate_content(false);
    }
}

void DailySummaryView::on_right_press() {
    if (m_nav_mode == NavMode::DATE) {
        if (m_current_date_index < (int)m_available_dates.size() - 1) {
            load_data_for_date_by_index(m_current_date_index + 1);
        }
    } else { // CONTENT mode
        navigate_content(true);
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
        update_ui(); // Redraw to update "Playing..." status
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
    bool is_today = (m_current_date == get_start_of_day(time(NULL)));

    switch (item_id) {
        case ContentItem::JOURNAL:
            if (!m_current_summary.journal_entry_path.empty()) {
                if (audio_manager_is_playing()) {
                    audio_manager_stop();
                } else {
                    ESP_LOGI(TAG, "Playing journal: %s", m_current_summary.journal_entry_path.c_str());
                    audio_manager_play(m_current_summary.journal_entry_path.c_str());
                }
                update_ui(); // Redraw to update "Playing..." status
            } else if (is_today) {
                ESP_LOGI(TAG, "No journal for today, navigating to journal creation view.");
                view_manager_load_view(VIEW_ID_DAILY_JOURNAL);
            }
            break;

        case ContentItem::NOTES:
            if (!m_current_summary.voice_note_paths.empty()) {
                ESP_LOGI(TAG, "Notes exist, navigating to voice note player.");
                view_manager_load_view(VIEW_ID_VOICE_NOTE_PLAYER);
            } else if (is_today) {
                ESP_LOGI(TAG, "No notes for today, navigating to voice note creation view.");
                view_manager_load_view(VIEW_ID_VOICE_NOTE);
            }
            break;

        case ContentItem::HABITS:
            // This action is independent of the day and depends only on habits being defined.
            if (HabitDataManager::get_all_active_habits().size() > 0) {
                ESP_LOGI(TAG, "Navigating to habit tracker");
                view_manager_load_view(VIEW_ID_TRACK_HABITS);
            }
            break;
        
        case ContentItem::POMODORO:
            ESP_LOGI(TAG, "Navigating to Pomodoro view.");
            view_manager_load_view(VIEW_ID_POMODORO);
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

// --- Static Callbacks to Bridge to Instance Methods ---

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