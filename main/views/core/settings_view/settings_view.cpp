#include "settings_view.h"
#include "views/view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "config/secrets.h"
#include "esp_log.h"
#include <string>
#include <algorithm> // Required for std::transform
#include <cctype>    // Required for ::toupper

static const char *TAG = "SETTINGS_VIEW";

// --- Lifecycle Methods ---
SettingsView::SettingsView() {
    ESP_LOGI(TAG, "SettingsView constructed");
}

SettingsView::~SettingsView() {
    if (m_group) {
        lv_group_del(m_group);
        m_group = nullptr;
    }
    reset_styles();
    ESP_LOGI(TAG, "SettingsView destructed");
}

void SettingsView::create(lv_obj_t* parent) {
    init_styles();

    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container, 5, 0);
    lv_obj_set_style_pad_gap(container, 5, 0);
    lv_obj_set_style_bg_color(container, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
    
    setup_ui(container);
    setup_button_handlers();
}

// --- Style Management ---
void SettingsView::init_styles() {
    if (m_styles_initialized) return;
    
    // --- Transition for smooth focus effects ---
    static const lv_style_prop_t props[] = {LV_STYLE_BORDER_WIDTH, LV_STYLE_BORDER_COLOR, LV_STYLE_SHADOW_WIDTH, LV_STYLE_PROP_INV};
    static lv_style_transition_dsc_t trans_dsc;
    lv_style_transition_dsc_init(&trans_dsc, props, lv_anim_path_ease_out, 150, 0, NULL);

    // --- Card Style (default state) ---
    lv_style_init(&m_style_card);
    lv_style_set_bg_color(&m_style_card, lv_color_white());
    lv_style_set_radius(&m_style_card, 8);
    lv_style_set_shadow_width(&m_style_card, 10);
    lv_style_set_shadow_opa(&m_style_card, LV_OPA_10);
    lv_style_set_shadow_ofs_y(&m_style_card, 2);
    lv_style_set_border_width(&m_style_card, 0);
    lv_style_set_pad_all(&m_style_card, 12);
    lv_style_set_transition(&m_style_card, &trans_dsc);

    // --- Card Style (focused state) ---
    lv_style_init(&m_style_card_focused);
    lv_style_set_border_width(&m_style_card_focused, 2);
    lv_style_set_border_color(&m_style_card_focused, lv_palette_main(LV_PALETTE_BLUE));

    // --- Section Title Style ---
    lv_style_init(&m_style_section_title);
    lv_style_set_text_color(&m_style_section_title, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_font(&m_style_section_title, &lv_font_montserrat_14);
    // Note: Automatic text transform is handled in C++, not by a style property.
    lv_style_set_pad_top(&m_style_section_title, 10);
    lv_style_set_pad_hor(&m_style_section_title, 5);

    // --- Divider Line Style ---
    lv_style_init(&m_style_divider);
    lv_style_set_line_color(&m_style_divider, lv_palette_lighten(LV_PALETTE_GREY, 2));
    lv_style_set_line_width(&m_style_divider, 1);

    m_styles_initialized = true;
}

void SettingsView::reset_styles() {
    if (!m_styles_initialized) return;
    lv_style_reset(&m_style_card);
    lv_style_reset(&m_style_card_focused);
    lv_style_reset(&m_style_section_title);
    lv_style_reset(&m_style_divider);
    m_styles_initialized = false;
}

// --- UI & Handler Setup ---
void SettingsView::setup_ui(lv_obj_t* parent) {
    // --- Main Header ---
    lv_obj_t* header_label = lv_label_create(parent);
    lv_label_set_text(header_label, "Settings");
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(header_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_pad_hor(header_label, 5, 0);
    lv_obj_set_style_pad_bottom(header_label, 5, 0);
    
    // --- Scrollable content area ---
    m_content_area = lv_obj_create(parent);
    lv_obj_remove_style_all(m_content_area);
    lv_obj_set_width(m_content_area, LV_PCT(100));
    lv_obj_set_flex_grow(m_content_area, 1);
    lv_obj_set_flex_flow(m_content_area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_content_area, 0, 0);
    lv_obj_set_style_pad_gap(m_content_area, 5, 0);
    
    // Enable scrolling for the content area
    lv_obj_set_scrollbar_mode(m_content_area, LV_SCROLLBAR_MODE_AUTO);

    m_group = lv_group_create();
    lv_group_set_wrap(m_group, true);

    populate_settings_list();
}

void SettingsView::populate_settings_list() {
    // --- General Section ---
    create_section_header("General");
    create_setting_card("WiFi SSID", WIFI_SSID);

    char ip_buffer[16] = "Not Connected";
    wifi_manager_get_ip_address(ip_buffer, sizeof(ip_buffer));
    create_setting_card("IP Address", ip_buffer);

    // --- User Profile Section ---
    create_section_header("User Profile");
    create_setting_card("Bedtime", "22:00");
    create_setting_card("Birthday", "January 1st");
    create_setting_card("Age", "30");
}

void SettingsView::create_section_header(const char* title) {
    // Container for title and line to group them
    lv_obj_t* cont = lv_obj_create(m_content_area);
    lv_obj_remove_style_all(cont);
    lv_obj_set_width(cont, LV_PCT(100));
    lv_obj_set_height(cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(cont, 3, 0);

    // Convert title to uppercase in C++ before giving it to LVGL
    std::string upper_title_str(title);
    std::transform(upper_title_str.begin(), upper_title_str.end(), upper_title_str.begin(),
                   [](unsigned char c){ return std::toupper(c); });

    lv_obj_t* label = lv_label_create(cont);
    lv_label_set_text(label, upper_title_str.c_str());
    lv_obj_add_style(label, &m_style_section_title, 0);

    lv_obj_t* line = lv_line_create(cont);
    // Corrected: Use lv_point_precise_t for LVGL v9+
    static const lv_point_precise_t line_points[] = {{0, 0}, {220, 0}};
    lv_line_set_points(line, line_points, 2);
    lv_obj_add_style(line, &m_style_divider, 0);
    lv_obj_set_align(line, LV_ALIGN_CENTER);
}

void SettingsView::create_setting_card(const char* name, const std::string& value) {
    lv_obj_t* card = lv_obj_create(m_content_area);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, &m_style_card, LV_STATE_DEFAULT);
    lv_obj_add_style(card, &m_style_card_focused, LV_STATE_FOCUSED);

    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_group_add_obj(m_group, card);

    // If this is the first interactive item, store a pointer to it.
    if (!m_first_interactive_item) {
        m_first_interactive_item = card;
    }
    
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* name_label = lv_label_create(card);
    lv_label_set_text(name_label, name);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(name_label, lv_palette_main(LV_PALETTE_GREY), 0);

    lv_obj_t* value_label = lv_label_create(card);
    lv_label_set_text(value_label, value.c_str());
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(value_label, lv_palette_darken(LV_PALETTE_GREY, 2), 0);
}

void SettingsView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, cancel_press_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, left_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, right_press_cb, true, this);
}

// --- Instance Methods for Actions ---
void SettingsView::on_cancel_press() {
    view_manager_load_view(VIEW_ID_STANDBY);
}

void SettingsView::on_nav_press(bool is_next) {
    if (!m_group) return;

    if (is_next) {
        lv_group_focus_next(m_group);
    } else {
        lv_group_focus_prev(m_group);
    }
    
    lv_obj_t* focused_obj = lv_group_get_focused(m_group);
    if (focused_obj) {
        // Check if the focused object is the first one we added.
        if (focused_obj == m_first_interactive_item) {
            // If it is, scroll the entire container to the top.
            lv_obj_scroll_to(m_content_area, 0, 0, LV_ANIM_ON);
        } else {
            // Otherwise, just ensure the focused item is visible.
            lv_obj_scroll_to_view(focused_obj, LV_ANIM_ON);
        }
    }
}

// --- Static Callbacks ---
void SettingsView::cancel_press_cb(void* user_data) {
    static_cast<SettingsView*>(user_data)->on_cancel_press();
}

void SettingsView::left_press_cb(void* user_data) {
    static_cast<SettingsView*>(user_data)->on_nav_press(false); // Previous item
}

void SettingsView::right_press_cb(void* user_data) {
    static_cast<SettingsView*>(user_data)->on_nav_press(true); // Next item
}