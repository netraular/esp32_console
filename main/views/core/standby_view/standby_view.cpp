#include "standby_view.h"
#include "views/view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "controllers/power_manager/power_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "components/status_bar_component/status_bar_component.h"
#include "esp_log.h"
#include <time.h>
#include <sys/stat.h>
#include <string>

static const char *TAG = "STANDBY_VIEW";
const char* NOTIFICATION_SOUND_PATH = "/sdcard/sounds/notification.wav";

// To see the borders of all UI elements for layout debugging, set this to true.
constexpr bool DEBUG_LAYOUT = true;

// Helper function to add a debug border to an object
static void add_debug_border(lv_obj_t* obj) {
    if (DEBUG_LAYOUT) {
        lv_obj_set_style_border_width(obj, 1, 0);
        lv_obj_set_style_border_color(obj, lv_palette_main(LV_PALETTE_RED), 0);
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
    
    status_bar_create(parent);

    // Top section: Image placeholder
    // This container's height is implicitly set by its content (the 64px tall image placeholder).
    lv_obj_t* image_placeholder = lv_obj_create(parent);
    lv_obj_set_size(image_placeholder, 128, 64);
    lv_obj_align(image_placeholder, LV_ALIGN_TOP_MID, 0, 5); // Align to top, with small padding
    lv_obj_set_style_bg_color(image_placeholder, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_set_style_radius(image_placeholder, 8, 0);
    lv_obj_set_style_border_width(image_placeholder, 0, 0);
    lv_obj_t* img_label = lv_label_create(image_placeholder);
    lv_label_set_text(img_label, "IMG");
    lv_obj_center(img_label);
    add_debug_border(image_placeholder);

    // Middle section: Clock and Date container
    // Positioned relative to the image, and enlarged to allow its content to be moved up.
    lv_obj_t* clock_container = lv_obj_create(parent);
    lv_obj_remove_style_all(clock_container);
    lv_obj_set_width(clock_container, LV_PCT(100)); // Full width for horizontal centering of content
    lv_obj_set_height(clock_container, 130);       // Enlarged height to provide layout space
    lv_obj_align_to(clock_container, image_placeholder, LV_ALIGN_OUT_BOTTOM_MID, 0, 0); // Position below image
    lv_obj_set_layout(clock_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(clock_container, LV_FLEX_FLOW_COLUMN);
    // Align content at the top of this container, making it appear higher on screen
    lv_obj_set_flex_align(clock_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(clock_container, 12, 0); // Spacing between time and date
    add_debug_border(clock_container);

    center_time_label = lv_label_create(clock_container);
    lv_obj_set_style_text_font(center_time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(center_time_label, lv_color_white(), 0);

    lv_label_set_text(center_time_label, "00:00");
    lv_obj_update_layout(center_time_label);

    lv_obj_set_style_transform_pivot_x(center_time_label, lv_obj_get_width(center_time_label) / 2, 0);
    lv_obj_set_style_transform_pivot_y(center_time_label, lv_obj_get_height(center_time_label) / 2, 0);
    
    lv_obj_set_style_transform_zoom(center_time_label, 384, 0); // 1.5x zoom
    add_debug_border(center_time_label);

    center_date_label = lv_label_create(clock_container);
    lv_obj_set_style_text_font(center_date_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(center_date_label, lv_color_white(), 0);
    lv_obj_set_style_translate_y(center_date_label, 0, 0);
    add_debug_border(center_date_label);
    
    // Bottom section: Weather forecast placeholders, pinned to the bottom of the screen
    lv_obj_t* forecast_container = lv_obj_create(parent);
    lv_obj_remove_style_all(forecast_container);
    lv_obj_set_size(forecast_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(forecast_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(forecast_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(forecast_container, 5, 0); // 5px spacing between widgets
    lv_obj_align(forecast_container, LV_ALIGN_BOTTOM_MID, 0, 0); //touching the bottom border
    add_debug_border(forecast_container);

    create_forecast_widget(forecast_container, "11 AM");
    create_forecast_widget(forecast_container, "1 PM");
    create_forecast_widget(forecast_container, "3 PM");
    
    // Syncing label (overlaid on the screen)
    loading_label = lv_label_create(parent);
    lv_obj_align(loading_label, LV_ALIGN_BOTTOM_MID, 0, -5); // Align with forecast widgets
    lv_obj_set_style_text_color(loading_label, lv_color_white(), 0);
    lv_label_set_text(loading_label, "Syncing time...");

    update_clock(); // Initial update
    update_timer = lv_timer_create(update_clock_cb, 500, this);
}

void StandbyView::create_forecast_widget(lv_obj_t* parent, const char* time_text) {
    lv_obj_t* widget_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(widget_cont);
    lv_obj_set_size(widget_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(widget_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(widget_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(widget_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(widget_cont, 2, 0);
    add_debug_border(widget_cont);

    lv_obj_t* icon_placeholder = lv_obj_create(widget_cont);
    lv_obj_set_size(icon_placeholder, 50, 50);
    lv_obj_set_style_radius(icon_placeholder, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(icon_placeholder, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(icon_placeholder, 0, 0);
    add_debug_border(icon_placeholder);

    lv_obj_t* time_label = lv_label_create(widget_cont);
    lv_label_set_text(time_label, time_text);
    lv_obj_set_style_text_color(time_label, lv_color_white(), 0);
    add_debug_border(time_label);
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
    if (now > 1672531200) {
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
    } else {
        lv_label_set_text(center_time_label, "--:--");
        lv_label_set_text(center_date_label, "No Time Sync");
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