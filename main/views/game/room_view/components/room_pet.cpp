#include "room_pet.h"
#include "isometric_renderer.h"
#include "config/app_config.h"
#include "models/asset_config.h"
#include "controllers/pet_manager/pet_manager.h"
#include "controllers/sprite_cache_manager/sprite_cache_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "models/pet_asset_data.h" // For PET_DATA_REGISTRY
#include "esp_log.h"
#include "esp_random.h"
#include <cmath>

static const char* TAG = "RoomPet";

static constexpr int PET_MOVE_INTERVAL_MS = 1000;
static constexpr int PET_MOVE_CHANCE_PERCENT = 70;
static constexpr int PET_ANIMATION_DURATION_MS = 1200;
static constexpr int PET_ANIMATION_FRAME_INTERVAL_MS = 250;
static constexpr int PET_Y_OFFSET = 10;

RoomPet::RoomPet(int room_width, int room_depth)
    : ROOM_WIDTH(room_width), ROOM_DEPTH(room_depth) {}

RoomPet::~RoomPet() {
    remove();
}

bool RoomPet::is_animating() const { return animating; }
bool RoomPet::is_spawned() const { return spawned; }
int RoomPet::get_grid_x() const { return grid_x; }
int RoomPet::get_grid_y() const { return grid_y; }
int RoomPet::get_target_grid_x() const { return target_grid_x; }
int RoomPet::get_target_grid_y() const { return target_grid_y; }

void RoomPet::get_interpolated_grid_pos(float& x, float& y) const {
    if (animating && target_grid_x != -1) {
        uint32_t elapsed = lv_tick_elaps(anim_start_tick);
        float normalized_time = (float)elapsed / PET_ANIMATION_DURATION_MS;

        if (normalized_time > 1.0f) {
            normalized_time = 1.0f;
        } else if (normalized_time < 0.0f) {
            normalized_time = 0.0f;
        }

        float eased_progress = 0.5f - 0.5f * cosf(normalized_time * (float)M_PI);
        
        x = grid_x + (target_grid_x - grid_x) * eased_progress;
        y = grid_y + (target_grid_y - grid_y) * eased_progress;
    } else {
        x = static_cast<float>(grid_x);
        y = static_cast<float>(grid_y);
    }
}

const lv_image_dsc_t* RoomPet::get_current_sprite() const {
    if (!spawned || animation_frames.empty()) {
        return nullptr;
    }
    return animation_frames[current_animation_frame];
}

bool RoomPet::spawn() {
    if (is_spawned()) return true;
    if (!sd_manager_check_ready()) { ESP_LOGE(TAG, "Cannot spawn, SD card not ready."); return false; }

    auto& pet_manager = PetManager::get_instance();
    auto collection = pet_manager.get_collection();
    
    // Build a list of all spawnable pet stages based on collection progress.
    std::vector<PetId> spawnable_pet_ids;
    for (const auto& entry : collection) {
        if (!entry.discovered) {
            continue; // Skip evolution lines the player hasn't started.
        }

        PetId current_id = entry.base_id;
        while (current_id != PetId::NONE) {
            spawnable_pet_ids.push_back(current_id);

            // If the line is not fully collected, stop before the final evolution.
            const auto& data = PET_DATA_REGISTRY.at(current_id);
            if (!entry.collected && data.evolves_to != PetId::NONE && PET_DATA_REGISTRY.at(data.evolves_to).evolves_to == PetId::NONE) {
                break;
            }
            current_id = data.evolves_to;
        }
    }

    if (spawnable_pet_ids.empty()) { 
        ESP_LOGW(TAG, "No discovered pets to spawn in the room."); 
        return false; 
    }

    // Pick a random pet from all available stages.
    id = spawnable_pet_ids[esp_random() % spawnable_pet_ids.size()];
    
    auto& sprite_cache = SpriteCacheManager::get_instance();
    const char* sprite_names[] = {PET_SPRITE_DEFAULT, PET_SPRITE_IDLE_01};
    for (const char* sprite_name : sprite_names) {
        std::string path = build_pet_sprite_path(id, sprite_name);
        const lv_image_dsc_t* sprite_dsc = sprite_cache.get_sprite(path);
        if (sprite_dsc) {
            sprite_paths.push_back(path);
            animation_frames.push_back(sprite_dsc);
        } else {
            ESP_LOGW(TAG, "Failed to load sprite frame '%s' for pet ID %d", sprite_name, (int)id);
        }
    }

    if (animation_frames.empty()) {
        ESP_LOGE(TAG, "Failed to load any sprites for pet ID %d. Aborting spawn.", (int)id);
        sprite_paths.clear();
        id = PetId::NONE;
        return false;
    }
    
    grid_x = esp_random() % ROOM_WIDTH;
    grid_y = esp_random() % ROOM_DEPTH;
    spawned = true;

    ESP_LOGI(TAG, "Spawning pet '%s' (ID: %d) at (%d, %d) with %zu frames", 
             pet_manager.get_pet_name(id).c_str(), (int)id, grid_x, grid_y, animation_frames.size());

    movement_timer = lv_timer_create(movement_timer_cb, PET_MOVE_INTERVAL_MS, this);
    return true;
}

void RoomPet::remove() {
    if (!is_spawned()) return;

    if (movement_timer) { lv_timer_delete(movement_timer); movement_timer = nullptr; }
    if (animation_timer) { lv_timer_delete(animation_timer); animation_timer = nullptr; }
    
    if (!sprite_paths.empty()) {
        SpriteCacheManager::get_instance().release_sprite_group(sprite_paths);
        sprite_paths.clear();
        animation_frames.clear();
    }
    id = PetId::NONE;
    animating = false;
    spawned = false;
    target_grid_x = -1;
    target_grid_y = -1;
}

void RoomPet::move_to_random_tile() {
    if (animating || !is_spawned()) return;

    const int moves[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
    std::vector<std::pair<int, int>> valid_moves;
    for (const auto& move : moves) {
        int new_x = grid_x + move[0];
        int new_y = grid_y + move[1];
        if (new_x >= 0 && new_x < ROOM_WIDTH && new_y >= 0 && new_y < ROOM_DEPTH) {
            valid_moves.push_back({new_x, new_y});
        }
    }
    if (valid_moves.empty()) return;

    auto target_pos = valid_moves[esp_random() % valid_moves.size()];
    target_grid_x = target_pos.first;
    target_grid_y = target_pos.second;
    
    animating = true;
    anim_start_tick = lv_tick_get();

    if (animation_frames.size() > 1) {
        animation_timer = lv_timer_create(animation_timer_cb, PET_ANIMATION_FRAME_INTERVAL_MS, this);
    }

    ESP_LOGD(TAG, "Move requested from (%d, %d) to (%d, %d)", grid_x, grid_y, target_grid_x, target_grid_y);
}

void RoomPet::update_state() {
    if (!spawned || !animating) {
        return;
    }

    uint32_t elapsed = lv_tick_elaps(anim_start_tick);
    if (elapsed >= PET_ANIMATION_DURATION_MS) {
        animating = false;
        grid_x = target_grid_x;
        grid_y = target_grid_y;
        target_grid_x = -1;
        target_grid_y = -1;
        
        if (animation_timer) {
            lv_timer_delete(animation_timer);
            animation_timer = nullptr;
        }
        current_animation_frame = 0;
        
        ESP_LOGD(TAG, "Movement animation finished. New position: (%d, %d)", grid_x, grid_y);
    }
}

void RoomPet::draw(lv_layer_t* layer, const lv_point_t& camera_offset) {
    if (!is_spawned()) return;

    const lv_image_dsc_t* sprite_dsc = get_current_sprite();
    if (!sprite_dsc) return;

    float interp_x, interp_y;
    get_interpolated_grid_pos(interp_x, interp_y);

    lv_point_t world_origin = { (lv_coord_t)(SCREEN_WIDTH / 2 - camera_offset.x), (lv_coord_t)(SCREEN_HEIGHT / 2 - camera_offset.y) };
    
    lv_point_t tile_center;
    IsometricRenderer::grid_to_screen_center_float(interp_x, interp_y, world_origin, &tile_center);
    
    lv_coord_t final_x = tile_center.x - (sprite_dsc->header.w / 2);
    lv_coord_t final_y = tile_center.y - sprite_dsc->header.h + PET_Y_OFFSET;

    lv_area_t draw_area;
    draw_area.x1 = final_x;
    draw_area.y1 = final_y;
    draw_area.x2 = final_x + sprite_dsc->header.w - 1;
    draw_area.y2 = final_y + sprite_dsc->header.h - 1;
    
    lv_draw_image_dsc_t img_dsc;
    lv_draw_image_dsc_init(&img_dsc);
    img_dsc.src = sprite_dsc;

    lv_draw_image(layer, &img_dsc, &draw_area);
}

std::string RoomPet::build_pet_sprite_path(PetId pet_id, const char* sprite_name) {
    char path_buffer[256];
    snprintf(path_buffer, sizeof(path_buffer), "%s%s%s%s%04d/%s",
             sd_manager_get_mount_point(),
             ASSETS_BASE_SUBPATH, ASSETS_SPRITES_SUBPATH, SPRITES_PETS_SUBPATH, (int)pet_id, sprite_name);
    return std::string(path_buffer);
}

void RoomPet::movement_timer_cb(lv_timer_t* timer) {
    auto* pet = static_cast<RoomPet*>(lv_timer_get_user_data(timer));
    
    uint32_t roll = esp_random() % 100;
    if (roll < PET_MOVE_CHANCE_PERCENT) {
        pet->move_to_random_tile();
    } else {
        ESP_LOGD(TAG, "Pet decided to stay still.");
    }
}

void RoomPet::animation_timer_cb(lv_timer_t* timer) {
    auto* pet = static_cast<RoomPet*>(lv_timer_get_user_data(timer));
    if (!pet->is_spawned() || pet->animation_frames.size() <= 1) {
        return;
    }

    pet->current_animation_frame = (pet->current_animation_frame + 1) % pet->animation_frames.size();
}