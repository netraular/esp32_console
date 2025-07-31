#include "standby_view.h"
#include "views/view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "controllers/power_manager/power_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "components/status_bar_component/status_bar_component.h"
#include "controllers/weather_manager/weather_manager.h"
#include "esp_log.h"
#include <time.h>
#include <sys/stat.h>
#include <string>
#include <array>

static const char *TAG = "STANDBY_VIEW";
const char* NOTIFICATION_SOUND_PATH = "/sdcard/sounds/notification.wav";

// To see the borders of all UI elements for layout debugging, set this to true.
constexpr bool DEBUG_LAYOUT = false;

// Helper function to add a debug border to an object
static void add_debug_border(lv_obj_t* obj) {
    if (DEBUG_LAYOUT) {
        lv_obj_set_style_border_width(obj, 1, 0);
        lv_obj_set_style_border_color(obj, lv_palette_main(LV_PALETTE_RED), 0);
        // Draw the border inside the widget's boundaries so it doesn't affect the layout size
        lv_obj_set_style_border_post(obj, true, 0);
    }
}

// Initialize static member
StandbyView* StandbyView::s_instance = nullptr;

// --- Lifecycle Methods ---
StandbyView::StandbyView() {
    s_instance = this;
    is_time_synced = false;
    update_timer = nullptr;
    volume_up_timer = nullptr;
    volume_down_timer = nullptr;
    ESP_LOGI(TAG, "StandbyView constructed");
}

StandbyView::~StandbyView() {
    if (update_timer) {
        lv_timer_delete(update_timer);
        update_timer = nullptr;
    }
    stop_volume_repeat_timer(&volume_up_timer);
    stop_volume_repeat_timer(&volume_down_timer);
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

// --- UI & Handler Setup ---
void StandbyView::setup_ui(lv_obj_t* parent) {
    // 'parent' is now the view's own container, not the screen.
    add_debug_border(parent);

    // Set a dark, gradient, and fully opaque background for this view's container
    lv_obj_set_style_bg_color(parent, lv_palette_darken(LV_PALETTE_BLUE, 4), 0);
    lv_obj_set_style_bg_grad_color(parent, lv_palette_darken(LV_PALETTE_BLUE, 2), 0);
    lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    
    // Use a flexbox layout for the main container to manage the three vertical sections
    lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    // Create the status bar on the screen to act as an overlay, independent of the flex layout.
    status_bar_create(lv_screen_active());

    // Top section: Image placeholder container (Flex item 1)
    lv_obj_t* image_placeholder = lv_obj_create(parent);
    lv_obj_set_size(image_placeholder, 128, 96);
    lv_obj_set_style_margin_top(image_placeholder, 5, 0);
    lv_obj_set_style_bg_color(image_placeholder, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_set_style_radius(image_placeholder, 8, 0);
    lv_obj_set_style_border_width(image_placeholder, 0, 0);
    lv_obj_t* img_label = lv_label_create(image_placeholder);
    lv_label_set_text(img_label, "IMG");
    lv_obj_center(img_label);
    add_debug_border(image_placeholder);

    // Middle section: Clock and Date container (Flex item 2)
    lv_obj_t* clock_container = lv_obj_create(parent);
    lv_obj_remove_style_all(clock_container);
    lv_obj_set_width(clock_container, LV_PCT(100));
    lv_obj_set_flex_grow(clock_container, 1); // Make this container fill the available vertical space
    // NO LAYOUT on this container. We will use absolute alignment for its children.
    add_debug_border(clock_container);

    center_time_label = lv_label_create(clock_container);
    lv_obj_set_style_text_font(center_time_label, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(center_time_label, lv_color_white(), 0);
    // Align to the center of its parent and move it 15px up.
    lv_obj_align(center_time_label, LV_ALIGN_CENTER, 0, -15);

    lv_label_set_text(center_time_label, "00:00");
    lv_obj_update_layout(center_time_label);

    lv_obj_set_style_transform_pivot_x(center_time_label, lv_obj_get_width(center_time_label) / 2, 0);
    lv_obj_set_style_transform_pivot_y(center_time_label, lv_obj_get_height(center_time_label) / 2, 0);
    
    lv_obj_set_style_transform_zoom(center_time_label, 768, 0); // 3x zoom
    add_debug_border(center_time_label);

    center_date_label = lv_label_create(clock_container);
    lv_obj_set_style_text_font(center_date_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(center_date_label, lv_color_white(), 0);
    // Align to the bottom-middle of its parent and move it 10px up.
    lv_obj_align(center_date_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    add_debug_border(center_date_label);
    
    // Bottom section: Weather forecast container (Flex item 3)
    lv_obj_t* forecast_container = lv_obj_create(parent);
    lv_obj_remove_style_all(forecast_container);
    lv_obj_set_size(forecast_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(forecast_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(forecast_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(forecast_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(forecast_container, 20, 0); // 10px spacing between widgets
    lv_obj_set_style_pad_ver(forecast_container, 5, 0); // Add 5px top/bottom padding to increase height
    add_debug_border(forecast_container);

    // Create the 3 forecast widgets and store their UI elements
    for (int i = 0; i < 3; ++i) {
        forecast_widgets[i] = create_forecast_widget(forecast_container);
    }
    
    show_weather_placeholders(); // Set the initial "loading" state

    // Syncing label (overlaid on the screen, not part of the flex layout)
    loading_label = lv_label_create(lv_screen_active()); // Create on screen to overlay everything
    lv_obj_align(loading_label, LV_ALIGN_BOTTOM_MID, 0, -5); 
    lv_obj_set_style_text_color(loading_label, lv_color_white(), 0);
    lv_label_set_text(loading_label, "Syncing time...");

    update_clock(); // Initial update (which will also call update_weather)
    update_timer = lv_timer_create(update_clock_cb, 1000, this); // Check every second
}

StandbyView::ForecastWidgetUI StandbyView::create_forecast_widget(lv_obj_t* parent) {
    ForecastWidgetUI ui;

    lv_obj_t* widget_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(widget_cont);
    lv_obj_set_size(widget_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(widget_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(widget_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(widget_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
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
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, settings_press_cb, true, this);
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_TAP, sleep_press_cb, true, this);
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_LONG_PRESS_START, shutdown_long_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_LONG_PRESS_START, volume_up_long_press_start_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_PRESS_UP, volume_up_press_up_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_LONG_PRESS_START, volume_down_long_press_start_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_PRESS_UP, volume_down_press_up_cb, true, this);
}

// --- UI Logic ---
void StandbyView::update_clock() {
    time_t now = time(NULL);
    if (now > 1672531200) { // A reasonable epoch time check (start of 2023)
        if (!is_time_synced) {
            is_time_synced = true;
            lv_obj_add_flag(loading_label, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(TAG, "Time has been synchronized.");
        }
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        lv_label_set_text_fmt(center_time_label, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        lv_label_set_text_fmt(center_date_label, "%s, %s %02d", 
            (const char*[]){"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"}[timeinfo.tm_wday],
            (const char*[]){"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"}[timeinfo.tm_mon],
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

    if (forecast_data.size() != forecast_widgets.size()) {
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

void StandbyView::on_volume_up_long_press_start() {
    stop_volume_repeat_timer(&volume_up_timer);
    audio_manager_volume_up();
    status_bar_update_volume_display();
    volume_up_timer = lv_timer_create([](lv_timer_t* timer){
        audio_manager_volume_up();
        status_bar_update_volume_display();
    }, 150, nullptr);
}

void StandbyView::on_volume_down_long_press_start() {
    stop_volume_repeat_timer(&volume_down_timer);
    audio_manager_volume_down();
    status_bar_update_volume_display();
    volume_down_timer = lv_timer_create([](lv_timer_t* timer){
        audio_manager_volume_down();
        status_bar_update_volume_display();
    }, 150, nullptr);
}

void StandbyView::stop_volume_repeat_timer(lv_timer_t** timer_ptr) {
    if (timer_ptr && *timer_ptr) {
        lv_timer_delete(*timer_ptr);
        *timer_ptr = nullptr;
    }
}

// --- Public Static Method for Notifications ---
void StandbyView::show_notification_popup(const Notification& notif) {
    if (!s_instance) {
        ESP_LOGE(TAG, "Cannot show notification popup, StandbyView instance is null.");
        return;
    }

    ESP_LOGI(TAG, "Showing notification popup: %s", notif.title.c_str());

    if (sd_manager_is_mounted()) {
        struct stat st;
        if (stat(NOTIFICATION_SOUND_PATH, &st) == 0) {
            audio_manager_play(NOTIFICATION_SOUND_PATH);
        } else {
            ESP_LOGW(TAG, "Notification sound file not found at %s", NOTIFICATION_SOUND_PATH);
        }
    } else {
        ESP_LOGW(TAG, "SD card not mounted, cannot play notification sound.");
    }
    
    popup_manager_show_alert(
        notif.title.c_str(),
        notif.message.c_str(),
        notification_popup_closed_cb,
        s_instance
    );
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

void StandbyView::sleep_press_cb(void* user_data) {
    static_cast<StandbyView*>(user_data)->on_sleep_press();
}

void StandbyView::shutdown_long_press_cb(void* user_data) {
    static_cast<StandbyView*>(user_data)->on_shutdown_long_press();
}

void StandbyView::volume_up_long_press_start_cb(void* user_data) {
    static_cast<StandbyView*>(user_data)->on_volume_up_long_press_start();
}

void StandbyView::volume_down_long_press_start_cb(void* user_data) {
    static_cast<StandbyView*>(user_data)->on_volume_down_long_press_start();
}

void StandbyView::volume_up_press_up_cb(void* user_data) {
    auto* instance = static_cast<StandbyView*>(user_data);
    if(instance) {
        instance->stop_volume_repeat_timer(&instance->volume_up_timer);
    }
}

void StandbyView::volume_down_press_up_cb(void* user_data) {
    auto* instance = static_cast<StandbyView*>(user_data);
    if(instance) {
        instance->stop_volume_repeat_timer(&instance->volume_down_timer);
    }
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