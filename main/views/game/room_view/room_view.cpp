#include "room_view.h"
#include "controllers/button_manager/button_manager.h"
#include "views/view_manager.h"
#include "esp_log.h"

static const char* TAG = "RoomView";

RoomView::RoomView() 
    : cursor_grid_x(ROOM_WIDTH / 2), 
      cursor_grid_y(ROOM_DEPTH / 2),
      current_mode(RoomMode::CURSOR) { // Start in cursor mode
    ESP_LOGI(TAG, "RoomView constructed");
}

RoomView::~RoomView() {
    ESP_LOGI(TAG, "RoomView destructed");
}

void RoomView::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);

    setup_ui(container);
    setup_view_button_handlers();
}

void RoomView::setup_ui(lv_obj_t* parent) {
    room_canvas = lv_obj_create(parent);
    lv_obj_remove_style_all(room_canvas);
    lv_obj_set_size(room_canvas, LV_PCT(100), LV_PCT(100));
    lv_obj_center(room_canvas);
    lv_obj_add_event_cb(room_canvas, draw_event_cb, LV_EVENT_DRAW_MAIN, this);
    
    renderer = std::make_unique<IsometricRenderer>(ROOM_WIDTH, ROOM_DEPTH, WALL_HEIGHT_UNITS);
    camera = std::make_unique<RoomCamera>(room_canvas);
    pet = std::make_unique<RoomPet>(ROOM_WIDTH, ROOM_DEPTH, room_canvas);

    // Create the mode selector. It will be hidden by default.
    // Provide both a selection and a cancellation callback.
    mode_selector = std::make_unique<RoomModeSelector>(
        container, 
        [this](RoomMode mode) { this->set_mode(mode); },
        [this]() { this->on_mode_selector_cancel(); }
    );

    // Set initial state
    set_mode(RoomMode::CURSOR);
    
    lv_timer_create(timer_cb, 33, this); // ~30 FPS update timer
}

void RoomView::setup_view_button_handlers() {
    ESP_LOGD(TAG, "Setting up view button handlers");
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_move_southwest_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_move_northwest_cb, true, this);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_move_northeast_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_move_southeast_cb, true, this);
    
    // Long presses
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_LONG_PRESS_START, handle_back_long_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_LONG_PRESS_START, handle_open_mode_selector_cb, true, this);
}


void RoomView::set_mode(RoomMode new_mode) {
    ESP_LOGI(TAG, "Switching mode from %d to %d", (int)current_mode, (int)new_mode);

    if (mode_selector->is_visible()) {
        mode_selector->hide();
    }
    
    // Unregister old handlers before setting the mode and registering new ones.
    button_manager_unregister_view_handlers();

    current_mode = new_mode;

    switch (current_mode) {
        case RoomMode::CURSOR:
            if (pet->is_spawned()) {
                pet->remove();
            }
            camera->move_to(cursor_grid_x, cursor_grid_y, true);
            break;

        case RoomMode::PET:
            if (!pet->is_spawned()) {
                if (!pet->spawn()) {
                    ESP_LOGE(TAG, "Failed to spawn pet, reverting to cursor mode.");
                    current_mode = RoomMode::CURSOR; // Revert on failure
                }
            }
            // The periodic_update will handle camera following the pet.
            break;

        case RoomMode::DECORATE:
            ESP_LOGW(TAG, "Decorate mode is not yet implemented.");
            if (pet->is_spawned()) {
                pet->remove();
            }
            // Here you would load furniture, etc.
            // For now, just stay in this mode.
            break;
    }
    
    // Re-register the main view's button handlers after the mode has been set.
    setup_view_button_handlers();
    lv_obj_invalidate(room_canvas);
}


void RoomView::open_mode_selector() {
    if (camera->is_animating() || pet->is_animating()) return;
    
    ESP_LOGD(TAG, "Opening mode selector");
    // Unregister view-specific handlers to give control to the menu
    button_manager_unregister_view_handlers();
    mode_selector->show();
}

void RoomView::on_mode_selector_cancel() {
    ESP_LOGD(TAG, "Mode selector cancelled, restoring view button handlers.");
    // The selector's hide() method already called unregister_view_handlers.
    // We just need to put our view's handlers back.
    setup_view_button_handlers();
}

void RoomView::on_grid_move(int dx, int dy) {
    if (current_mode != RoomMode::CURSOR || camera->is_animating()) return;
    int new_x = cursor_grid_x + dx;
    int new_y = cursor_grid_y + dy;
    if (new_x >= 0 && new_x < ROOM_WIDTH && new_y >= 0 && new_y < ROOM_DEPTH) {
        cursor_grid_x = new_x;
        cursor_grid_y = new_y;
        camera->move_to(cursor_grid_x, cursor_grid_y, true);
    }
}

void RoomView::on_back_to_menu() {
    view_manager_load_view(VIEW_ID_MENU);
}

void RoomView::periodic_update() {
    if (current_mode == RoomMode::PET && pet->is_spawned()) {
        // If the pet is moving, smoothly follow its interpolated position.
        // If it's static, the camera will also remain static, centered on the pet's tile.
        float pet_interp_x, pet_interp_y;
        pet->get_interpolated_grid_pos(pet_interp_x, pet_interp_y);
        camera->center_on(pet_interp_x, pet_interp_y);
    }
    
    // The pet's screen position depends on the camera, so update it after the camera.
    if (pet->is_spawned()) {
        pet->update_screen_position(camera->get_offset());
    }
}

void RoomView::timer_cb(lv_timer_t* timer) {
    auto* view = static_cast<RoomView*>(lv_timer_get_user_data(timer));
    view->periodic_update();
}

void RoomView::draw_event_cb(lv_event_t* e) {
    RoomView* view = (RoomView*)lv_event_get_user_data(e);
    lv_layer_t* layer = lv_event_get_layer(e);
    
    view->renderer->draw_world(layer, view->camera->get_offset());
    if (view->current_mode == RoomMode::CURSOR) {
        view->renderer->draw_cursor(layer, view->camera->get_offset(), view->cursor_grid_x, view->cursor_grid_y);
    } 
    else if (view->current_mode == RoomMode::PET && view->pet->is_animating()) {
        int target_x = view->pet->get_target_grid_x();
        int target_y = view->pet->get_target_grid_y();
        if(target_x != -1 && target_y != -1) {
            view->renderer->draw_target_tile(layer, view->camera->get_offset(), target_x, target_y);
            view->renderer->draw_target_point(layer, view->camera->get_offset(), target_x, target_y);
        }
    }
    // NOTE: Furniture drawing would be added here in the future
}

// --- Static Callbacks ---
void RoomView::handle_move_northeast_cb(void* user_data) { static_cast<RoomView*>(user_data)->on_grid_move(1, 0); }
void RoomView::handle_move_northwest_cb(void* user_data) { static_cast<RoomView*>(user_data)->on_grid_move(0, -1); }
void RoomView::handle_move_southeast_cb(void* user_data) { static_cast<RoomView*>(user_data)->on_grid_move(0, 1); }
void RoomView::handle_move_southwest_cb(void* user_data) { static_cast<RoomView*>(user_data)->on_grid_move(-1, 0); }
void RoomView::handle_back_long_press_cb(void* user_data) { static_cast<RoomView*>(user_data)->on_back_to_menu(); }
void RoomView::handle_open_mode_selector_cb(void* user_data) { static_cast<RoomView*>(user_data)->open_mode_selector(); }