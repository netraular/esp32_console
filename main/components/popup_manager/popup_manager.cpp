#include "popup_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "POPUP_MANAGER";

// --- Module State ---
static lv_obj_t* s_popup_overlay = nullptr;
static lv_obj_t* s_loading_container = nullptr;
static lv_group_t* s_popup_group = nullptr;
static popup_callback_t s_current_callback = nullptr;
static void* s_current_user_data = nullptr;

// --- Forward Declarations ---
static void destroy_popup_with_result(popup_result_t result);
static void handle_ok_press(void* user_data);
static void handle_cancel_press(void* user_data);
static void handle_nav_press(void* user_data);
static lv_obj_t* create_popup_container(void);
static void create_overlay(void);
static void init_styles(void);

// --- Styles for pop-up elements ---
static lv_style_t style_btn_default;
static lv_style_t style_btn_focused;
static bool style_initialized = false;

/**
 * @brief Initializes the styles used by the popup manager.
 * This is done once to avoid re-creating styles on every popup.
 */
static void init_styles() {
    if (style_initialized) return;

    // --- Transition properties for smooth animations ---
    static const lv_style_prop_t props[] = {
        LV_STYLE_BG_COLOR,
        LV_STYLE_PAD_LEFT,
        LV_STYLE_PAD_RIGHT,
        LV_STYLE_PAD_TOP,
        LV_STYLE_PAD_BOTTOM,
        LV_STYLE_PROP_INV // Mark end of array
    };
    static lv_style_transition_dsc_t trans_dsc;
    lv_style_transition_dsc_init(&trans_dsc, props, lv_anim_path_ease_out, 150, 0, NULL);

    // --- Default button style (white with blue border) ---
    lv_style_init(&style_btn_default);
    lv_style_set_radius(&style_btn_default, 6);
    lv_style_set_bg_color(&style_btn_default, lv_color_white());
    lv_style_set_bg_opa(&style_btn_default, LV_OPA_100);
    lv_style_set_border_color(&style_btn_default, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_border_width(&style_btn_default, 2);
    lv_style_set_text_color(&style_btn_default, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_pad_hor(&style_btn_default, 15);
    lv_style_set_pad_ver(&style_btn_default, 8);
    lv_style_set_transition(&style_btn_default, &trans_dsc);

    // --- Focused button style (blue with white text, slightly larger via padding) ---
    lv_style_init(&style_btn_focused);
    lv_style_set_bg_color(&style_btn_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_color(&style_btn_focused, lv_color_white());
    lv_style_set_pad_hor(&style_btn_focused, 18);
    lv_style_set_pad_ver(&style_btn_focused, 11);

    style_initialized = true;
}

// --- Private Functions ---

static void create_overlay() {
    if (s_popup_overlay) return;
    button_manager_unregister_view_handlers();
    s_popup_overlay = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(s_popup_overlay);
    lv_obj_set_size(s_popup_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_popup_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_popup_overlay, LV_OPA_70, 0);
    lv_obj_clear_flag(s_popup_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(s_popup_overlay);
}

static void setup_popup_input_handlers() {
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_press, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_nav_press, true, (void*)LV_KEY_LEFT);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_nav_press, true, (void*)LV_KEY_RIGHT);
}

// --- Button Handlers ---

static void handle_ok_press(void* user_data) {
    if (!s_popup_group) return;
    lv_obj_t* focused_btn = lv_group_get_focused(s_popup_group);
    if (!focused_btn) return;
    lv_obj_send_event(focused_btn, LV_EVENT_CLICKED, NULL);
}

static void handle_cancel_press(void* user_data) {
    destroy_popup_with_result(POPUP_RESULT_DISMISSED);
}

static void handle_nav_press(void* user_data) {
    if (s_popup_group) {
        uint32_t key = (uint32_t)(uintptr_t)user_data;
        if (key == LV_KEY_LEFT) {
            lv_group_focus_prev(s_popup_group);
        } else if (key == LV_KEY_RIGHT) {
            lv_group_focus_next(s_popup_group);
        }
    }
}

// --- Event Callbacks for Popup Buttons ---

static void primary_btn_event_cb(lv_event_t * e) {
    destroy_popup_with_result(POPUP_RESULT_PRIMARY);
}

static void secondary_btn_event_cb(lv_event_t * e) {
    destroy_popup_with_result(POPUP_RESULT_SECONDARY);
}

// --- Cleanup ---
static void destroy_popup_with_result(popup_result_t result) {
    if (!s_popup_overlay) return;
    button_manager_unregister_view_handlers();
    if (s_popup_group) {
        lv_group_del(s_popup_group);
        s_popup_group = nullptr;
    }
    lv_obj_del(s_popup_overlay);
    s_popup_overlay = nullptr;
    if (s_current_callback) {
        s_current_callback(result, s_current_user_data);
    }
    s_current_callback = nullptr;
    s_current_user_data = nullptr;
    ESP_LOGD(TAG, "Popup destroyed.");
}

static lv_obj_t* create_footer_button(lv_obj_t* parent, const char* text) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_style(btn, &style_btn_default, LV_STATE_DEFAULT);
    lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}

static lv_obj_t* create_popup_container() {
    lv_obj_t* cont = lv_obj_create(s_popup_overlay);
    lv_obj_set_width(cont, 220);
    lv_obj_set_height(cont, LV_SIZE_CONTENT);
    lv_obj_center(cont);
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(cont, 10, 0);
    lv_obj_set_style_pad_row(cont, 15, 0);
    lv_obj_set_style_radius(cont, 8, 0);
    return cont;
}

// --- Public API Implementation ---

bool popup_manager_is_active(void) {
    // A popup is active if either the standard overlay or the loading container exists.
    return s_popup_overlay != nullptr || s_loading_container != nullptr;
}

void popup_manager_show_alert(const char* title, const char* message, popup_callback_t cb, void* user_data) {
    if (s_popup_overlay) {
        ESP_LOGW(TAG, "Cannot show alert, a popup is already active.");
        return;
    }
    ESP_LOGD(TAG, "Showing alert: '%s'", title);
    s_current_callback = cb;
    s_current_user_data = user_data;
    create_overlay();
    init_styles();
    lv_obj_t* popup_cont = create_popup_container();
    lv_obj_t* title_label = lv_label_create(popup_cont);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, lv_theme_get_font_large(popup_cont), 0);
    lv_obj_t* msg_label = lv_label_create(popup_cont);
    lv_label_set_text(msg_label, message);
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg_label, LV_PCT(100));
    lv_obj_t* footer = lv_obj_create(popup_cont);
    lv_obj_remove_style_all(footer);
    lv_obj_set_width(footer, LV_PCT(100));
    lv_obj_set_height(footer, LV_SIZE_CONTENT);
    lv_obj_set_layout(footer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* btn_ok = create_footer_button(footer, "OK");
    lv_obj_add_event_cb(btn_ok, primary_btn_event_cb, LV_EVENT_CLICKED, NULL);
    s_popup_group = lv_group_create();
    lv_group_add_obj(s_popup_group, btn_ok);
    lv_group_focus_obj(btn_ok);
    setup_popup_input_handlers();
}

void popup_manager_show_confirmation(const char* title, const char* message, const char* primary_btn_text, const char* secondary_btn_text, popup_callback_t cb, void* user_data) {
    if (s_popup_overlay) {
        ESP_LOGW(TAG, "Cannot show confirmation, a popup is already active.");
        if (cb) cb(POPUP_RESULT_DISMISSED, user_data);
        return;
    }
    ESP_LOGD(TAG, "Showing confirmation: '%s'", title);
    s_current_callback = cb;
    s_current_user_data = user_data;
    create_overlay();
    init_styles();
    lv_obj_t* popup_cont = create_popup_container();
    lv_obj_t* title_label = lv_label_create(popup_cont);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, lv_theme_get_font_large(popup_cont), 0);
    lv_obj_t* msg_label = lv_label_create(popup_cont);
    lv_label_set_text(msg_label, message);
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg_label, LV_PCT(100));
    lv_obj_t* footer = lv_obj_create(popup_cont);
    lv_obj_remove_style_all(footer);
    lv_obj_set_width(footer, LV_PCT(100));
    lv_obj_set_height(footer, LV_SIZE_CONTENT);
    lv_obj_set_layout(footer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(footer, 10, 0);
    lv_obj_t* btn_secondary = create_footer_button(footer, secondary_btn_text);
    lv_obj_add_event_cb(btn_secondary, secondary_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* btn_primary = create_footer_button(footer, primary_btn_text);
    lv_obj_add_event_cb(btn_primary, primary_btn_event_cb, LV_EVENT_CLICKED, NULL);
    s_popup_group = lv_group_create();
    lv_group_set_wrap(s_popup_group, true);
    lv_group_add_obj(s_popup_group, btn_secondary);
    lv_group_add_obj(s_popup_group, btn_primary);

    lv_group_focus_obj(btn_primary); 
    setup_popup_input_handlers();
}

void popup_manager_show_loading(const char* message) {
    if (s_loading_container) return;
    ESP_LOGD(TAG, "Showing loading screen: '%s'", message);
    if (s_popup_overlay) {
        ESP_LOGW(TAG, "Cannot show loading screen, a standard popup is already active.");
        return;
    }
    button_manager_unregister_view_handlers();
    s_loading_container = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(s_loading_container);
    lv_obj_set_size(s_loading_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_loading_container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_loading_container, LV_OPA_70, 0);
    lv_obj_center(s_loading_container);
    lv_obj_clear_flag(s_loading_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* cont = lv_obj_create(s_loading_container);
    lv_obj_set_size(cont, 150, 100);
    lv_obj_center(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* spinner = lv_spinner_create(cont);
    lv_obj_set_size(spinner, 50, 50);
    lv_obj_t* label = lv_label_create(cont);
    lv_label_set_text(label, message);
}

void popup_manager_hide_loading() {
    if (!s_loading_container) return;
    ESP_LOGD(TAG, "Hiding loading screen.");
    lv_obj_del(s_loading_container);
    s_loading_container = nullptr;
}