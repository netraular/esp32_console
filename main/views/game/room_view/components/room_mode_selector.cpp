#include "room_mode_selector.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"
#include <cstdint> // Required for intptr_t

static const char* TAG = "RoomModeSelector";

RoomModeSelector::RoomModeSelector(lv_obj_t* parent, std::function<void(RoomMode)> on_mode_selected_cb, std::function<void()> on_cancel_cb)
    : on_mode_selected_callback(on_mode_selected_cb), on_cancel_callback(on_cancel_cb) {
    init_styles();
    create_ui(parent);
}

RoomModeSelector::~RoomModeSelector() {
    ESP_LOGD(TAG, "Destructing RoomModeSelector");
    remove_button_handlers();
    if (input_group) {
        if(lv_group_get_default() == input_group) {
            lv_group_set_default(NULL);
        }
        lv_group_del(input_group);
        input_group = nullptr;
    }
    // The container is a child of the parent and will be deleted with it,
    // but we can delete it explicitly if needed.
    if (container) {
        lv_obj_del(container);
        container = nullptr;
    }
    reset_styles();
}

void RoomModeSelector::create_ui(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_70, 0);
    lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN); // Start hidden
    lv_obj_center(container);

    lv_obj_t* list = lv_list_create(container);
    lv_obj_set_size(list, 180, LV_SIZE_CONTENT);
    lv_obj_center(list);

    input_group = lv_group_create();

    // Add options to the list
    lv_obj_t* btn;
    btn = lv_list_add_button(list, LV_SYMBOL_EDIT, "Cursor Mode");
    lv_obj_set_user_data(btn, (void*)RoomMode::CURSOR);
    lv_obj_add_style(btn, &style_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(input_group, btn);

    btn = lv_list_add_button(list, LV_SYMBOL_IMAGE, "Pet Mode");
    lv_obj_set_user_data(btn, (void*)RoomMode::PET);
    lv_obj_add_style(btn, &style_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(input_group, btn);
    
    btn = lv_list_add_button(list, LV_SYMBOL_PLUS, "Decorate Mode");
    lv_obj_set_user_data(btn, (void*)RoomMode::DECORATE);
    lv_obj_add_style(btn, &style_focused, LV_STATE_FOCUSED);
    lv_group_add_obj(input_group, btn);
}

bool RoomModeSelector::is_visible() const {
    return container && !lv_obj_has_flag(container, LV_OBJ_FLAG_HIDDEN);
}

void RoomModeSelector::show() {
    if (!container) return;
    ESP_LOGI(TAG, "Showing mode selector");
    lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(container);
    
    // Set this group as the one LVGL should use for focus drawing
    lv_group_set_default(input_group);

    if (lv_group_get_obj_count(input_group) > 0) {
        lv_group_focus_obj(lv_obj_get_child(lv_obj_get_child(container, 0), 0));
    }
    setup_button_handlers();
}

void RoomModeSelector::hide() {
    if (!container) return;
    ESP_LOGD(TAG, "Hiding mode selector UI");
    
    // Release the default group so it doesn't interfere with other views
    if(lv_group_get_default() == input_group) {
        lv_group_set_default(NULL);
    }

    lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
    remove_button_handlers();
}

void RoomModeSelector::setup_button_handlers() {
    // Register these handlers as view-specific (true) so they can be cleared easily.
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_nav_up_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_nav_down_cb, true, this);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_select_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_cb, true, this);
}

void RoomModeSelector::remove_button_handlers() {
    // Use the provided API to clear all view-specific handlers.
    button_manager_unregister_view_handlers();
}

void RoomModeSelector::init_styles() {
    if (styles_initialized) return;
    lv_style_init(&style_focused);
    lv_style_set_bg_color(&style_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_color(&style_focused, lv_color_white());
    styles_initialized = true;
}

void RoomModeSelector::reset_styles() {
    if (!styles_initialized) return;
    lv_style_reset(&style_focused);
    styles_initialized = false;
}

void RoomModeSelector::on_nav_up() {
    if (input_group) lv_group_focus_prev(input_group);
}

void RoomModeSelector::on_nav_down() {
    if (input_group) lv_group_focus_next(input_group);
}

void RoomModeSelector::on_select() {
    if (!input_group) return;
    lv_obj_t* focused_obj = lv_group_get_focused(input_group);
    if (focused_obj) {
        // First cast the void* to an integer of the same size (intptr_t),
        // then cast that integer to the RoomMode enum.
        RoomMode selected_mode = static_cast<RoomMode>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(focused_obj)));
        
        if (on_mode_selected_callback) {
            on_mode_selected_callback(selected_mode);
        }
    }
}

void RoomModeSelector::on_cancel() {
    hide();
    if (on_cancel_callback) {
        on_cancel_callback();
    }
}

// --- Static Callbacks ---
void RoomModeSelector::handle_nav_up_cb(void* user_data) { static_cast<RoomModeSelector*>(user_data)->on_nav_up(); }
void RoomModeSelector::handle_nav_down_cb(void* user_data) { static_cast<RoomModeSelector*>(user_data)->on_nav_down(); }
void RoomModeSelector::handle_select_cb(void* user_data) { static_cast<RoomModeSelector*>(user_data)->on_select(); }
void RoomModeSelector::handle_cancel_cb(void* user_data) { static_cast<RoomModeSelector*>(user_data)->on_cancel(); }