#include "room_view.h"
#include "controllers/button_manager/button_manager.h"
#include "views/view_manager.h"
#include "components/memory_monitor_component/memory_monitor_component.h"
#include "esp_log.h"
#include <algorithm> // For std::sort
#include <vector>    // For std::vector copy

static const char* TAG = "RoomView";

RoomView::RoomView() 
    : cursor_grid_x(ROOM_WIDTH / 2), 
      cursor_grid_y(ROOM_DEPTH / 2),
      current_mode(RoomMode::CURSOR) { // Start in cursor mode
    ESP_LOGI(TAG, "RoomView constructed");
}

RoomView::~RoomView() {
    // We must delete the timer when the view is destroyed to prevent use-after-free errors.
    if (update_timer) {
        lv_timer_delete(update_timer);
        update_timer = nullptr;
        ESP_LOGI(TAG, "Periodic update timer deleted.");
    }
    ESP_LOGI(TAG, "RoomView destructed");
}

void RoomView::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);

    setup_ui(container);
    // Button handlers are set by set_mode()
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
    object_manager = std::make_unique<RoomObjectManager>();

    mode_selector = std::make_unique<RoomModeSelector>(
        container, 
        [this](RoomMode mode) { this->set_mode(mode); },
        [this]() { this->on_mode_selector_cancel(); }
    );

    // Set initial state
    set_mode(RoomMode::DECORATE);
    
    // Create the timer and store its handle in our member variable
    update_timer = lv_timer_create(timer_cb, 33, this); // ~30 FPS update timer

    // Add the memory monitor component to the view's main container.
    lv_obj_t* mem_monitor = memory_monitor_create(parent);
    lv_obj_align(mem_monitor, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
}

void RoomView::setup_view_button_handlers() {
    ESP_LOGD(TAG, "Setting up button handlers for mode %d", (int)current_mode);
    button_manager_unregister_view_handlers();

    // These long-press handlers are common to all modes
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_LONG_PRESS_START, handle_back_long_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_LONG_PRESS_START, handle_open_mode_selector_cb, true, this);

    // Mode-specific tap handlers
    switch(current_mode) {
        case RoomMode::CURSOR:
        case RoomMode::DECORATE:
            button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_move_southwest_cb, true, this);
            button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_move_northwest_cb, true, this);
            button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_move_northeast_cb, true, this);
            button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_move_southeast_cb, true, this);
            if (current_mode == RoomMode::DECORATE) {
                // Add the long press for placing objects, it can co-exist with tap.
                button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_LONG_PRESS_START, handle_place_object_cb, true, this);
            }
            break;

        case RoomMode::PET:
            // Pet mode might have different controls, e.g., interact, feed. For now, none.
            break;
    }
}


void RoomView::set_mode(RoomMode new_mode) {
    ESP_LOGI(TAG, "Switching mode from %d to %d", (int)current_mode, (int)new_mode);

    if (mode_selector->is_visible()) {
        mode_selector->hide();
    }
    
    current_mode = new_mode;

    switch (current_mode) {
        case RoomMode::CURSOR:
        case RoomMode::DECORATE:
            if (pet->is_spawned()) {
                pet->remove();
            }
            camera->move_to(cursor_grid_x, cursor_grid_y, true);
            break;

        case RoomMode::PET:
            if (!pet->is_spawned()) {
                if (!pet->spawn()) {
                    ESP_LOGE(TAG, "Failed to spawn pet, reverting to DECORATE mode.");
                    current_mode = RoomMode::DECORATE; // Revert on failure
                }
            }
            break;
    }
    
    setup_view_button_handlers();
    lv_obj_invalidate(room_canvas);
}


void RoomView::open_mode_selector() {
    if (camera->is_animating() || (pet->is_spawned() && pet->is_animating())) return;
    
    ESP_LOGD(TAG, "Opening mode selector");
    button_manager_unregister_view_handlers();
    mode_selector->show();
}

void RoomView::on_mode_selector_cancel() {
    ESP_LOGD(TAG, "Mode selector cancelled, restoring view button handlers.");
    setup_view_button_handlers();
}

void RoomView::on_grid_move(int dx, int dy) {
    if ((current_mode != RoomMode::CURSOR && current_mode != RoomMode::DECORATE) || camera->is_animating()) return;

    int new_x = cursor_grid_x + dx;
    int new_y = cursor_grid_y + dy;
    if (new_x >= 0 && new_x < ROOM_WIDTH && new_y >= 0 && new_y < ROOM_DEPTH) {
        cursor_grid_x = new_x;
        cursor_grid_y = new_y;
        camera->move_to(cursor_grid_x, cursor_grid_y, true);
    }
}

void RoomView::on_place_object() {
    if (current_mode != RoomMode::DECORATE) return;

    if (object_manager->remove_object_at(cursor_grid_x, cursor_grid_y)) {
        ESP_LOGI(TAG, "Object removed at (%d, %d)", cursor_grid_x, cursor_grid_y);
    } else {
        PlacedFurniture new_obj;
        new_obj.type_name = "ads_gsArcade_2"; // Hardcoded for testing
        new_obj.grid_x = cursor_grid_x;
        new_obj.grid_y = cursor_grid_y;
        new_obj.direction = 90;

        // Ensure we have the definition before placing
        const auto* def = FurnitureDataManager::get_instance().get_definition(new_obj.type_name);
        if (def) {
             if (object_manager->add_object(new_obj)) {
                ESP_LOGI(TAG, "Object '%s' placed at (%d, %d)", new_obj.type_name.c_str(), cursor_grid_x, cursor_grid_y);
            }
        } else {
            ESP_LOGE(TAG, "Cannot place object, definition for '%s' not found!", new_obj.type_name.c_str());
        }
    }
    object_manager->save_layout();
    lv_obj_invalidate(room_canvas);
}


void RoomView::on_back_to_menu() {
    view_manager_load_view(VIEW_ID_MENU);
}

void RoomView::periodic_update() {
    if (current_mode == RoomMode::PET && pet->is_spawned()) {
        float pet_interp_x, pet_interp_y;
        pet->get_interpolated_grid_pos(pet_interp_x, pet_interp_y);
        camera->center_on(pet_interp_x, pet_interp_y);
    }
    
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

    // Create a mutable copy of the objects to sort for rendering.
    std::vector<PlacedFurniture> sorted_objects = view->object_manager->get_all_objects();

    // Sort the objects using the painter's algorithm for this isometric projection.
    // Objects are drawn from back to front (increasing Y) and left to right (increasing X).
    std::sort(sorted_objects.begin(), sorted_objects.end(),
        [](const PlacedFurniture& a, const PlacedFurniture& b) {
            if (a.grid_y != b.grid_y) {
                return a.grid_y < b.grid_y;
            }
            return a.grid_x < b.grid_x;
        }
    );

    // Draw all placed objects in the correct z-order.
    auto& furni_manager = FurnitureDataManager::get_instance();
    for(const auto& obj : sorted_objects) {
        const auto* def = furni_manager.get_definition(obj.type_name);
        if (def) {
            view->renderer->draw_placeholder_object(layer, view->camera->get_offset(), obj.grid_x, obj.grid_y, def->dimensions.x, def->dimensions.y, def->dimensions.z);
        }
    }

    if (view->current_mode == RoomMode::CURSOR || view->current_mode == RoomMode::DECORATE) {
        view->renderer->draw_cursor(layer, view->camera->get_offset(), view->cursor_grid_x, view->cursor_grid_y);
    } 
    else if (view->current_mode == RoomMode::PET && view->pet->is_animating()) {
        int target_x = view->pet->get_target_grid_x();
        int target_y = view->pet->get_target_grid_y();
        if(target_x != -1 && target_y != -1) {
            view->renderer->draw_target_tile(layer, view->camera->get_offset(), target_x, target_y);
        }
    }
}

// --- Static Callbacks ---
void RoomView::handle_move_northeast_cb(void* user_data) { static_cast<RoomView*>(user_data)->on_grid_move(1, 0); }
void RoomView::handle_move_northwest_cb(void* user_data) { static_cast<RoomView*>(user_data)->on_grid_move(0, -1); }
void RoomView::handle_move_southeast_cb(void* user_data) { static_cast<RoomView*>(user_data)->on_grid_move(0, 1); }
void RoomView::handle_move_southwest_cb(void* user_data) { static_cast<RoomView*>(user_data)->on_grid_move(-1, 0); }
void RoomView::handle_back_long_press_cb(void* user_data) { static_cast<RoomView*>(user_data)->on_back_to_menu(); }
void RoomView::handle_open_mode_selector_cb(void* user_data) { static_cast<RoomView*>(user_data)->open_mode_selector(); }
void RoomView::handle_place_object_cb(void* user_data) { static_cast<RoomView*>(user_data)->on_place_object(); }