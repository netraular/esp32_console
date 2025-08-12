#include "standby_view.h"
#include "views/view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "controllers/power_manager/power_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "components/status_bar_component/status_bar_component.h"
#include "controllers/weather_manager/weather_manager.h"
#include "models/asset_config.h"
#include "esp_log.h"
#include <time.h>
#include <sys/stat.h>
#include <string>
#include <array>

static const char *TAG = "STANDBY_VIEW";

// To see the borders of all UI elements for layout debugging, set this to true.
constexpr bool DEBUG_LAYOUT = false;

// Helper function to add a debug border to an object
static void add_debug_border(lv_obj_t* obj) {
    if (DEBUG_LAYOUT) {
        lv_obj_set_style_outline_width(obj, 1, 0);
        lv_obj_set_style_outline_color(obj, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_outline_pad(obj, 0, 0);
    }
}

// Initialize static member
StandbyView* StandbyView::s_instance = nullptr;

// --- Lifecycle Methods ---
StandbyView::StandbyView() {
    s_instance = this;
    is_time_synced = false;
    update_timer = nullptr;
    ESP_LOGI(TAG, "StandbyView constructed");
}

StandbyView::~StandbyView() {
    if (update_timer) {
        lv_timer_delete(update_timer);
        update_timer = nullptr;
    }
    s_instance = nullptr;
    ESP_LOGI(TAG, "StandbyView destructed");
}

void StandbyView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Standby View");

    // Create a main container for this view
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container); // Clean slate for custom styling
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);

    // All UI elements will be children of 'container'
    setup_ui(container); 
    setup_main_button_handlers();
}

// --- Public Static Method for Notifications ---
void StandbyView::show_notification_popup(const Notification& notif) {
    if (!s_instance) {
        ESP_LOGE(TAG, "Cannot show notification popup, StandbyView instance is null.");
        return;
    }

    ESP_LOGI(TAG, "Showing notification popup: %s", notif.title.c_str());

    // The notification sound is played by the NotificationManager *before* calling this
    // function. This avoids duplicate sound playback and centralizes the logic.
    
    popup_manager_show_alert(
        notif.title.c_str(),
        notif.message.c_str(),
        notification_popup_closed_cb,
        s_instance
    );
}

// --- UI & Handler Setup ---
void StandbyView::setup_ui(lv_obj_t* parent) {
    add_debug_border(parent);

    // Set a dark, gradient, and fully opaque background for this view's container
    lv_obj_set_style_bg_color(parent, lv_palette_darken(LV_PALETTE_BLUE, 4), 0);
    lv_obj_set_style_bg_grad_color(parent, lv_palette_darken(LV_PALETTE_BLUE, 2), 0);
    lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    
    // We will position 3 main blocks with fixed sizes and positions.

    // Create the status bar on the screen to act as an overlay
    status_bar_create(lv_screen_active());

    // --- Top Block: Image ---
    // Total height 105px (5px pad top + 96px content + 4px pad bottom)
    lv_obj_t* top_container = lv_obj_create(parent);
    lv_obj_remove_style_all(top_container);
    lv_obj_set_size(top_container, LV_PCT(100), 105);
    lv_obj_align(top_container, LV_ALIGN_TOP_MID, 0, 0); // Anchor top
    lv_obj_set_style_pad_top(top_container, 5, 0);
    lv_obj_set_style_pad_bottom(top_container, 4, 0);
    add_debug_border(top_container);

    lv_obj_t* image_placeholder = lv_obj_create(top_container);
    lv_obj_set_size(image_placeholder, 224, 96);
    lv_obj_align(image_placeholder, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(image_placeholder, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_set_style_radius(image_placeholder, 8, 0);
    lv_obj_set_style_border_width(image_placeholder, 0, 0);
    
    lv_obj_t* img_label = lv_label_create(image_placeholder);
    lv_label_set_text(img_label, "IMG");
    lv_obj_center(img_label);
    add_debug_border(image_placeholder);

    // --- Middle Block: Clock and Date ---
    // Fixed height of 70px
    lv_obj_t* middle_container = lv_obj_create(parent);
    lv_obj_remove_style_all(middle_container);
    lv_obj_set_size(middle_container, LV_PCT(100), 70);
    lv_obj_align_to(middle_container, top_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 0); // Position below top block
    add_debug_border(middle_container);

    // Clock - centered in the middle of the central block
    center_time_label = lv_label_create(middle_container);
    lv_obj_set_style_text_font(center_time_label, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(center_time_label, lv_color_white(), 0);
    lv_label_set_text(center_time_label, "00:00");
    lv_obj_update_layout(center_time_label); // Update layout to get dimensions for transform pivot
    lv_obj_set_style_transform_pivot_x(center_time_label, lv_obj_get_width(center_time_label) / 2, 0);
    lv_obj_set_style_transform_pivot_y(center_time_label, lv_obj_get_height(center_time_label) / 2, 0);
    lv_obj_set_style_transform_zoom(center_time_label, 768, 0); // 3x zoom
    lv_obj_align(center_time_label, LV_ALIGN_CENTER, 0, -10);
    add_debug_border(center_time_label);

    // Date - centered horizontally and 20px below the vertical center
    center_date_label = lv_label_create(middle_container);
    lv_obj_set_style_text_font(center_date_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(center_date_label, lv_color_white(), 0);
    lv_obj_align(center_date_label, LV_ALIGN_CENTER, 0, 25); // Center and move 20px down
    add_debug_border(center_date_label);
    
    // --- Bottom Block: Weather ---
    // Fixed height of 65px
    lv_obj_t* bottom_container = lv_obj_create(parent);
    lv_obj_remove_style_all(bottom_container);
    lv_obj_set_size(bottom_container, LV_PCT(100), 65);
    lv_obj_align(bottom_container, LV_ALIGN_BOTTOM_MID, 0, 0); // Anchor bottom
    lv_obj_set_style_pad_bottom(bottom_container, 2, 0); // 2px bottom padding
    add_debug_border(bottom_container);
    
    // Container for the weather widgets (using Flexbox to align them in a row)
    lv_obj_t* forecast_container = lv_obj_create(bottom_container);
    lv_obj_remove_style_all(forecast_container);
    lv_obj_set_width(forecast_container, LV_SIZE_CONTENT); // Width adapts to content
    lv_obj_set_height(forecast_container, LV_PCT(100));     // Height fills available space
    lv_obj_set_layout(forecast_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(forecast_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(forecast_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(forecast_container, 15, 0);
    // Center this container at the bottom of the block
    lv_obj_align(forecast_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    add_debug_border(forecast_container);

    // Create the 3 forecast widgets and store their UI elements
    for (int i = 0; i < 3; ++i) {
        forecast_widgets[i] = create_forecast_widget(forecast_container);
    }
    
    show_weather_placeholders();

    update_clock();
    update_timer = lv_timer_create(update_clock_cb, 1000, this);
}

StandbyView::ForecastWidgetUI StandbyView::create_forecast_widget(lv_obj_t* parent) {
    ForecastWidgetUI ui;

    lv_obj_t* widget_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(widget_cont);
    lv_obj_set_size(widget_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(widget_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(widget_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(widget_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_row(widget_cont, 2, 0);
    lv_obj_set_style_pad_ver(widget_cont, 2, 0);
    add_debug_border(widget_cont);

    // The icon is a label that will hold a symbol font character
    ui.icon_label = lv_label_create(widget_cont);
    lv_obj_set_style_text_font(ui.icon_label, &lv_font_montserrat_24, 0); // Use a larger font for the icon
    lv_obj_set_style_text_color(ui.icon_label, lv_color_white(), 0);
    add_debug_border(ui.icon_label);

    // The time for the forecast point
    ui.time_label = lv_label_create(widget_cont);
    lv_obj_set_style_text_color(ui.time_label, lv_color_white(), 0);
    add_debug_border(ui.time_label);
    
    return ui;
}

void StandbyView::setup_main_button_handlers() {
    button_manager_unregister_view_handlers();
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, menu_press_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, habit_tracker_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, voice_notes_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, settings_press_cb, true, this);
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_TAP, sleep_press_cb, true, this);
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_LONG_PRESS_START, shutdown_long_press_cb, true, this);
}

// --- UI Logic ---
void StandbyView::update_clock() {
    time_t now = time(NULL);
    if (now > 1672531200) { // A reasonable epoch time check (start of 2023)
        if (!is_time_synced) {
            is_time_synced = true;
            ESP_LOGI(TAG, "Time has been synchronized.");
        }
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        lv_label_set_text_fmt(center_time_label, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        lv_label_set_text_fmt(center_date_label, "%s, %s %02d", 
            (const char*[]){"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}[timeinfo.tm_wday],
            (const char*[]){"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"}[timeinfo.tm_mon],
            timeinfo.tm_mday
        );
        // Update weather information along with the clock
        update_weather();
    } else {
        lv_label_set_text(center_time_label, "--:--");
        lv_label_set_text(center_date_label, "No Time Sync");
    }
}

void StandbyView::show_weather_placeholders() {
    for (auto& widget : forecast_widgets) {
        lv_label_set_text(widget.icon_label, LV_SYMBOL_REFRESH);
        lv_label_set_text(widget.time_label, "--:--");
    }
}

void StandbyView::update_weather() {
    // Only update weather if time is synced, otherwise the forecast is meaningless
    if (!is_time_synced) {
        return;
    }
    
    auto forecast_data = WeatherManager::get_forecast();
    
    if (forecast_data.empty()) {
        ESP_LOGD(TAG, "Weather data not yet available from manager. Placeholders will be shown.");
        return; // Do nothing, keep the placeholders visible
    }

    if (forecast_data.size() < forecast_widgets.size()) {
        ESP_LOGW(TAG, "Mismatch between forecast data (%zu) and UI widgets (%zu)", forecast_data.size(), forecast_widgets.size());
    }

    for (size_t i = 0; i < forecast_widgets.size() && i < forecast_data.size(); ++i) {
        const auto& data = forecast_data[i];
        auto& ui = forecast_widgets[i];

        // Update the weather icon using the helper function
        lv_label_set_text(ui.icon_label, WeatherManager::wmo_code_to_lvgl_symbol(data.weather_code));

        // Update the time label for the forecast point
        struct tm timeinfo;
        localtime_r(&data.timestamp, &timeinfo);
        lv_label_set_text_fmt(ui.time_label, "%d:00", timeinfo.tm_hour);
    }
}

// --- Instance Methods for Actions ---
void StandbyView::on_menu_press() {
    view_manager_load_view(VIEW_ID_MENU);
}

void StandbyView::on_settings_press() {
    view_manager_load_view(VIEW_ID_SETTINGS);
}

void StandbyView::on_habit_tracker_press() {
    view_manager_load_view(VIEW_ID_TRACK_HABITS);
}

void StandbyView::on_voice_notes_press() {
    view_manager_load_view(VIEW_ID_VOICE_NOTE);
}

void StandbyView::on_sleep_press() {
    power_manager_enter_light_sleep();
}

void StandbyView::on_shutdown_long_press() {
    popup_manager_show_confirmation(
        "Shutdown", "Turn off the device?", "Shutdown", "Cancel",
        shutdown_popup_cb, this
    );
}

void StandbyView::handle_shutdown_result(popup_result_t result) {
    if (result == POPUP_RESULT_PRIMARY) {
        power_manager_enter_deep_sleep();
    } else {
        setup_main_button_handlers();
    }
}

// --- Static Callbacks (Bridge to C-style APIs and instance methods) ---
void StandbyView::update_clock_cb(lv_timer_t* timer) {
    auto* instance = static_cast<StandbyView*>(lv_timer_get_user_data(timer));
    if (instance) {
        instance->update_clock();
    }
}

void StandbyView::menu_press_cb(void* user_data) {
    static_cast<StandbyView*>(user_data)->on_menu_press();
}

void StandbyView::settings_press_cb(void* user_data) {
    static_cast<StandbyView*>(user_data)->on_settings_press();
}

void StandbyView::habit_tracker_press_cb(void* user_data) {
    static_cast<StandbyView*>(user_data)->on_habit_tracker_press();
}

void StandbyView::voice_notes_press_cb(void* user_data) {
    static_cast<StandbyView*>(user_data)->on_voice_notes_press();
}

void StandbyView::sleep_press_cb(void* user_data) {
    static_cast<StandbyView*>(user_data)->on_sleep_press();
}

void StandbyView::shutdown_long_press_cb(void* user_data) {
    static_cast<StandbyView*>(user_data)->on_shutdown_long_press();
}

void StandbyView::shutdown_popup_cb(popup_result_t result, void* user_data) {
    static_cast<StandbyView*>(user_data)->handle_shutdown_result(result);
}

void StandbyView::notification_popup_closed_cb(popup_result_t result, void* user_data) {
    auto* instance = static_cast<StandbyView*>(user_data);
    if (instance) {
        ESP_LOGD(TAG, "Notification popup closed, re-registering standby view handlers.");
        instance->setup_main_button_handlers();
    }
}