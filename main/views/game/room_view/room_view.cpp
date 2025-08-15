#include "room_view.h"
#include "controllers/button_manager/button_manager.h"
#include "views/view_manager.h"
#include "esp_log.h"

static const char* TAG = "RoomView";

RoomView::RoomView() 
    : cursor_grid_x(ROOM_WIDTH / 2), 
      cursor_grid_y(ROOM_DEPTH / 2),
      last_pet_grid_x(-1),
      last_pet_grid_y(-1),
      control_mode(ControlMode::CURSOR) {
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
    setup_button_handlers();
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

    camera->move_to(cursor_grid_x, cursor_grid_y, false);
    lv_timer_create(timer_cb, 33, this); // ~30 FPS update timer
}

void RoomView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_move_southwest_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_move_northwest_cb, true, this);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_move_northeast_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_move_southeast_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_LONG_PRESS_START, handle_back_long_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_LONG_PRESS_START, handle_pet_mode_toggle_cb, true, this);
}

void RoomView::on_grid_move(int dx, int dy) {
    if (control_mode != ControlMode::CURSOR || camera->is_animating()) return;
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

void RoomView::toggle_pet_mode() {
    if (camera->is_animating() || pet->is_animating()) return;

    if (control_mode == ControlMode::CURSOR) {
        if (pet->spawn()) {
            ESP_LOGI(TAG, "Entering Pet Mode");
            control_mode = ControlMode::PET;
            last_pet_grid_x = pet->get_grid_x();
            last_pet_grid_y = pet->get_grid_y();
            camera->move_to(last_pet_grid_x, last_pet_grid_y, true);
        } else {
            ESP_LOGE(TAG, "Failed to spawn pet, staying in cursor mode.");
        }
    } else {
        ESP_LOGI(TAG, "Exiting Pet Mode");
        control_mode = ControlMode::CURSOR;
        pet->remove();
        last_pet_grid_x = -1;
        last_pet_grid_y = -1;
        camera->move_to(cursor_grid_x, cursor_grid_y, true);
    }
}

void RoomView::periodic_update() {
    if (control_mode == ControlMode::PET && pet->is_spawned()) {
        // If the pet is moving, smoothly follow its interpolated position.
        // If it's static, the camera will also remain static, centered on the pet's tile.
        float pet_interp_x, pet_interp_y;
        pet->get_interpolated_grid_pos(pet_interp_x, pet_interp_y);
        camera->center_on(pet_interp_x, pet_interp_y);
    }
    
    // The pet's screen position depends on the camera, so update it after the camera.
    // This needs to run even if not in PET mode, in case a pet was moving when we switched modes.
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
    if (view->control_mode == ControlMode::CURSOR) {
        view->renderer->draw_cursor(layer, view->camera->get_offset(), view->cursor_grid_x, view->cursor_grid_y);
    } 
    else if (view->control_mode == ControlMode::PET && view->pet->is_animating()) {
        int target_x = view->pet->get_target_grid_x();
        int target_y = view->pet->get_target_grid_y();
        if(target_x != -1 && target_y != -1) {
            view->renderer->draw_target_tile(layer, view->camera->get_offset(), target_x, target_y);
            view->renderer->draw_target_point(layer, view->camera->get_offset(), target_x, target_y);
        }
    }
}

void RoomView::handle_move_northeast_cb(void* user_data) { static_cast<RoomView*>(user_data)->on_grid_move(1, 0); }
void RoomView::handle_move_northwest_cb(void* user_data) { static_cast<RoomView*>(user_data)->on_grid_move(0, -1); }
void RoomView::handle_move_southeast_cb(void* user_data) { static_cast<RoomView*>(user_data)->on_grid_move(0, 1); }
void RoomView::handle_move_southwest_cb(void* user_data) { static_cast<RoomView*>(user_data)->on_grid_move(-1, 0); }
void RoomView::handle_back_long_press_cb(void* user_data) { static_cast<RoomView*>(user_data)->on_back_to_menu(); }
void RoomView::handle_pet_mode_toggle_cb(void* user_data) { static_cast<RoomView*>(user_data)->toggle_pet_mode(); }