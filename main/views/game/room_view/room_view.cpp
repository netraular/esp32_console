#include "room_view.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/sprite_cache_manager/sprite_cache_manager.h"
#include "models/asset_config.h"
#include "views/view_manager.h"
#include "components/memory_monitor_component/memory_monitor_component.h"
#include "esp_log.h"
#include <algorithm>
#include <vector>
#include <set>

static const char* TAG = "RoomView";

RoomView::RoomView() 
    : cursor_grid_x(ROOM_WIDTH / 2), 
      cursor_grid_y(ROOM_DEPTH / 2),
      current_mode(RoomMode::CURSOR) {
    ESP_LOGI(TAG, "RoomView constructed");
}

RoomView::~RoomView() {
    if (update_timer) {
        lv_timer_delete(update_timer);
        update_timer = nullptr;
    }
    release_all_furniture_sprites();
    ESP_LOGI(TAG, "RoomView destructed");
}

void RoomView::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);

    setup_ui(container);
}

void RoomView::setup_ui(lv_obj_t* parent) {
    room_canvas = lv_obj_create(parent);
    lv_obj_remove_style_all(room_canvas);
    lv_obj_set_size(room_canvas, LV_PCT(100), LV_PCT(100));
    lv_obj_center(room_canvas);
    lv_obj_add_event_cb(room_canvas, draw_event_cb, LV_EVENT_DRAW_MAIN, this);
    
    renderer = std::make_unique<IsometricRenderer>(ROOM_WIDTH, ROOM_DEPTH, WALL_HEIGHT_UNITS);
    camera = std::make_unique<RoomCamera>(room_canvas);
    pet = std::make_unique<RoomPet>(ROOM_WIDTH, ROOM_DEPTH);
    object_manager = std::make_unique<RoomObjectManager>();

    mode_selector = std::make_unique<RoomModeSelector>(
        container, 
        [this](RoomMode mode) { this->set_mode(mode); },
        [this]() { this->on_mode_selector_cancel(); }
    );
    
    load_all_furniture_sprites();
    set_mode(RoomMode::PET);
    
    update_timer = lv_timer_create(timer_cb, 33, this); // ~30 FPS

    lv_obj_t* mem_monitor = memory_monitor_create(parent);
    lv_obj_align(mem_monitor, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
}

void RoomView::release_all_furniture_sprites() {
    if (m_cached_sprites.empty()) return;

    auto& sprite_cache = SpriteCacheManager::get_instance();
    for (const auto& pair : m_cached_sprites) {
        sprite_cache.release_sprite(pair.first);
    }
    m_cached_sprites.clear();
    ESP_LOGI(TAG, "Released all cached furniture sprites.");
}

void RoomView::load_all_furniture_sprites() {
    release_all_furniture_sprites();

    ESP_LOGI(TAG, "Pre-loading all required furniture sprites...");
    std::set<std::string> unique_paths;

    auto& furni_manager = FurnitureDataManager::get_instance();
    for (const auto& obj : object_manager->get_all_objects()) {
        const auto* def = furni_manager.get_definition(obj.type_name);
        if (!def) continue;

        int habbo_dir = (obj.direction == 90) ? 2 : 4;

        for (int i = 0; i < def->layer_count; ++i) {
            char layer_char = 'a' + i;
            char asset_key_buf[128];
            snprintf(asset_key_buf, sizeof(asset_key_buf), "%s_64_%c_%d_0", obj.type_name.c_str(), layer_char, habbo_dir);
            std::string asset_key(asset_key_buf);

            auto it = def->assets.find(asset_key);
            if (it == def->assets.end()) continue;

            const FurnitureAsset& asset = it->second;
            const std::string& asset_name_to_load = asset.source.empty() ? asset.name : asset.source;

            char path_buf[256];
            snprintf(path_buf, sizeof(path_buf), "%s%s%s%s/%s.png",
                     sd_manager_get_mount_point(), ASSETS_BASE_SUBPATH, ASSETS_FURNITURE_SUBPATH,
                     obj.type_name.c_str(), asset_name_to_load.c_str());
            
            unique_paths.insert(path_buf);
        }
    }

    auto& sprite_cache = SpriteCacheManager::get_instance();
    for (const auto& path : unique_paths) {
        const lv_image_dsc_t* dsc = sprite_cache.get_sprite(path);
        if (dsc) {
            m_cached_sprites[path] = dsc;
        }
    }
    ESP_LOGI(TAG, "Finished pre-loading %zu unique sprites.", m_cached_sprites.size());
}

void RoomView::setup_view_button_handlers() {
    ESP_LOGD(TAG, "Setting up button handlers for mode %d", (int)current_mode);
    button_manager_unregister_view_handlers();

    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_LONG_PRESS_START, handle_back_long_press_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_LONG_PRESS_START, handle_open_mode_selector_cb, true, this);

    switch(current_mode) {
        case RoomMode::CURSOR:
        case RoomMode::DECORATE:
            button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_move_southwest_cb, true, this);
            button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_move_northwest_cb, true, this);
            button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_move_northeast_cb, true, this);
            button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_move_southeast_cb, true, this);
            if (current_mode == RoomMode::DECORATE) {
                button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_LONG_PRESS_START, handle_place_object_cb, true, this);
            }
            break;
        case RoomMode::PET:
            break;
    }
}

void RoomView::set_mode(RoomMode new_mode) {
    ESP_LOGI(TAG, "Switching mode from %d to %d", (int)current_mode, (int)new_mode);
    if (mode_selector->is_visible()) mode_selector->hide();
    current_mode = new_mode;
    switch (current_mode) {
        case RoomMode::CURSOR:
        case RoomMode::DECORATE:
            if (pet->is_spawned()) pet->remove();
            camera->move_to(cursor_grid_x, cursor_grid_y, true);
            break;
        case RoomMode::PET:
            if (!pet->is_spawned() && !pet->spawn()) {
                ESP_LOGE(TAG, "Failed to spawn pet, reverting to DECORATE mode.");
                current_mode = RoomMode::DECORATE;
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

    bool changed = false;
    if (object_manager->remove_object_at(cursor_grid_x, cursor_grid_y)) {
        ESP_LOGI(TAG, "Object removed at (%d, %d)", cursor_grid_x, cursor_grid_y);
        changed = true;
    } else {
        PlacedFurniture new_obj;
        new_obj.type_name = "ads_gsArcade_2";
        new_obj.grid_x = cursor_grid_x;
        new_obj.grid_y = cursor_grid_y;
        new_obj.direction = 90;
        const auto* def = FurnitureDataManager::get_instance().get_definition(new_obj.type_name);
        if (def && object_manager->add_object(new_obj)) {
            ESP_LOGI(TAG, "Object '%s' placed at (%d, %d)", new_obj.type_name.c_str(), cursor_grid_x, cursor_grid_y);
            changed = true;
        } else {
            ESP_LOGE(TAG, "Cannot place object, definition for '%s' not found or tile occupied!", new_obj.type_name.c_str());
        }
    }
    if (changed) {
        object_manager->save_layout();
        load_all_furniture_sprites();
        lv_obj_invalidate(room_canvas);
    }
}

void RoomView::on_back_to_menu() {
    view_manager_load_view(VIEW_ID_MENU);
}

void RoomView::periodic_update() {
    bool needs_redraw = false;

    if (pet->is_spawned()) {
        if (pet->is_animating()) {
            needs_redraw = true;
        }
        pet->update_state();
        
        if (current_mode == RoomMode::PET) {
            float pet_interp_x, pet_interp_y;
            pet->get_interpolated_grid_pos(pet_interp_x, pet_interp_y);
            camera->center_on(pet_interp_x, pet_interp_y);
        }
    }
    
    if (needs_redraw) {
        lv_obj_invalidate(room_canvas);
    }
}

void RoomView::timer_cb(lv_timer_t* timer) {
    static_cast<RoomView*>(lv_timer_get_user_data(timer))->periodic_update();
}

void RoomView::draw_event_cb(lv_event_t* e) {
    RoomView* view = (RoomView*)lv_event_get_user_data(e);
    lv_layer_t* layer = lv_event_get_layer(e);
    
    view->renderer->draw_world(layer, view->camera->get_offset());

    std::vector<DrawableObject> drawables;

    for (const auto& obj : view->object_manager->get_all_objects()) {
        drawables.push_back({DrawableObject::FURNITURE, &obj, (float)obj.grid_y, (float)obj.grid_x});
    }

    if (view->pet->is_spawned()) {
        float pet_x, pet_y;
        view->pet->get_interpolated_grid_pos(pet_x, pet_y);
        drawables.push_back({DrawableObject::PET, view->pet.get(), pet_y, pet_x});
    }

    std::sort(drawables.begin(), drawables.end());

    auto& furni_manager = FurnitureDataManager::get_instance();
    const lv_point_t& camera_offset = view->camera->get_offset();

    for(const auto& drawable : drawables) {
        if (drawable.type == DrawableObject::FURNITURE) {
            const PlacedFurniture* obj = static_cast<const PlacedFurniture*>(drawable.data);
            const auto* def = furni_manager.get_definition(obj->type_name);
            if (!def) continue;

            int habbo_dir = (obj->direction == 90) ? 2 : 4;

            for (int i = 0; i < def->layer_count; ++i) {
                char layer_char = 'a' + i;
                char asset_key_buf[128];
                snprintf(asset_key_buf, sizeof(asset_key_buf), "%s_64_%c_%d_0", obj->type_name.c_str(), layer_char, habbo_dir);
                std::string asset_key(asset_key_buf);

                auto asset_it = def->assets.find(asset_key);
                if (asset_it == def->assets.end()) continue;
                
                const FurnitureAsset& asset = asset_it->second;
                const FurnitureAsset* final_asset = &asset;
                bool flip_h = asset.flip_h;

                if (!asset.source.empty()) {
                    auto source_it = def->assets.find(asset.source);
                    if (source_it != def->assets.end()) final_asset = &source_it->second;
                    else continue;
                }
                
                char path_buf[256];
                snprintf(path_buf, sizeof(path_buf), "%s%s%s%s/%s.png",
                         sd_manager_get_mount_point(), ASSETS_BASE_SUBPATH, ASSETS_FURNITURE_SUBPATH,
                         obj->type_name.c_str(), final_asset->name.c_str());

                auto sprite_it = view->m_cached_sprites.find(path_buf);
                if(sprite_it != view->m_cached_sprites.end()) {
                    view->renderer->draw_sprite(layer, camera_offset, *obj, sprite_it->second, final_asset->x_offset, final_asset->y_offset, flip_h);
                }
            }
        } else if (drawable.type == DrawableObject::PET) {
            view->pet->draw(layer, camera_offset);
        }
    }

    if (view->current_mode == RoomMode::CURSOR || view->current_mode == RoomMode::DECORATE) {
        view->renderer->draw_cursor(layer, camera_offset, view->cursor_grid_x, view->cursor_grid_y);
    } 
    else if (view->current_mode == RoomMode::PET && view->pet->is_animating()) {
        int target_x = view->pet->get_target_grid_x();
        int target_y = view->pet->get_target_grid_y();
        if(target_x != -1 && target_y != -1) {
            view->renderer->draw_target_tile(layer, camera_offset, target_x, target_y);
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