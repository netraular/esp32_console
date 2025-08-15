#include "room_pet.h"
#include "isometric_renderer.h" // For grid_to_screen and constants
#include "config/app_config.h"
#include "models/asset_config.h"
#include "controllers/pet_manager/pet_manager.h"
#include "controllers/sprite_cache_manager/sprite_cache_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "esp_log.h"
#include "esp_random.h"

static const char* TAG = "RoomPet";

static constexpr int PET_MOVE_INTERVAL_MS = 2000;
static constexpr int PET_ANIMATION_DURATION_MS = 750;

RoomPet::RoomPet(int room_width, int room_depth, lv_obj_t* parent_canvas)
    : parent_canvas(parent_canvas), ROOM_WIDTH(room_width), ROOM_DEPTH(room_depth) {}

RoomPet::~RoomPet() {
    remove();
}

bool RoomPet::is_animating() const { return animating; }
bool RoomPet::is_spawned() const { return img_obj != nullptr; }
int RoomPet::get_grid_x() const { return grid_x; }
int RoomPet::get_grid_y() const { return grid_y; }
int RoomPet::get_target_grid_x() const { return target_grid_x; }
int RoomPet::get_target_grid_y() const { return target_grid_y; }

bool RoomPet::spawn() {
    if (is_spawned()) return true;
    if (!parent_canvas) { ESP_LOGE(TAG, "Cannot spawn, parent canvas is null."); return false; }

    if (!sd_manager_check_ready()) { ESP_LOGE(TAG, "Cannot spawn, SD card not ready."); return false; }

    auto& pet_manager = PetManager::get_instance();
    auto collection = pet_manager.get_collection();
    std::vector<PetId> discovered_pet_ids;
    for (const auto& entry : collection) { if (entry.discovered) { discovered_pet_ids.push_back(entry.base_id); } }
    if (discovered_pet_ids.empty()) { ESP_LOGE(TAG, "No discovered pets to spawn."); return false; }

    id = discovered_pet_ids[esp_random() % discovered_pet_ids.size()];
    sprite_path = build_pet_sprite_path(id, PET_SPRITE_DEFAULT);
    const lv_image_dsc_t* sprite_dsc = SpriteCacheManager::get_instance().get_sprite(sprite_path);
    
    if (!sprite_dsc) {
        ESP_LOGE(TAG, "Failed to load pet sprite: %s", sprite_path.c_str());
        sprite_path.clear();
        id = PetId::NONE;
        return false;
    }
    
    grid_x = esp_random() % ROOM_WIDTH;
    grid_y = esp_random() % ROOM_DEPTH;
    
    img_obj = lv_image_create(parent_canvas);
    lv_image_set_src(img_obj, sprite_dsc);
    lv_image_set_antialias(img_obj, false);

    ESP_LOGI(TAG, "Spawning pet '%s' at (%d, %d)", pet_manager.get_pet_name(id).c_str(), grid_x, grid_y);

    movement_timer = lv_timer_create(movement_timer_cb, PET_MOVE_INTERVAL_MS, this);
    return true;
}

void RoomPet::remove() {
    if (!is_spawned()) return;

    if (movement_timer) { lv_timer_delete(movement_timer); movement_timer = nullptr; }
    
    lv_obj_delete(img_obj);
    img_obj = nullptr;
    
    if (!sprite_path.empty()) {
        SpriteCacheManager::get_instance().release_sprite(sprite_path);
        sprite_path.clear();
    }
    id = PetId::NONE;
    animating = false;
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
    
    // Start the animation process on the next update loop
    animating = true;
    anim_start_tick = lv_tick_get();

    // Invalidate the canvas to ensure the target marker is drawn correctly from the first frame.
    lv_obj_invalidate(parent_canvas);

    ESP_LOGD(TAG, "Move requested from (%d, %d) to (%d, %d)", grid_x, grid_y, target_grid_x, target_grid_y);
}

void RoomPet::update_screen_position(const lv_point_t& camera_offset) {
    if (!is_spawned()) return;
    
    const lv_image_dsc_t* sprite_dsc = static_cast<const lv_image_dsc_t*>(lv_image_get_src(img_obj));
    if (!sprite_dsc) return;

    lv_point_t world_origin = { (lv_coord_t)(SCREEN_WIDTH / 2 - camera_offset.x), (lv_coord_t)(SCREEN_HEIGHT / 2 - camera_offset.y) };

    if (animating) {
        uint32_t elapsed = lv_tick_elaps(anim_start_tick);
        
        lv_point_t start_pos, end_pos;
        IsometricRenderer::grid_to_screen_center(grid_x, grid_y, world_origin, &start_pos);
        IsometricRenderer::grid_to_screen_center(target_grid_x, target_grid_y, world_origin, &end_pos);
        
        if (elapsed >= PET_ANIMATION_DURATION_MS) {
            // Animation finished
            animating = false;
            grid_x = target_grid_x;
            grid_y = target_grid_y;
            target_grid_x = -1;
            target_grid_y = -1;
            
            // Set final position accurately
            lv_coord_t final_x = end_pos.x - (sprite_dsc->header.w / 2);
            lv_coord_t final_y = end_pos.y - sprite_dsc->header.h;
            lv_obj_set_pos(img_obj, final_x, final_y);
            
            // Invalidate to remove the target marker
            lv_obj_invalidate(parent_canvas);
            ESP_LOGD(TAG, "Movement animation finished. New position: (%d, %d)", grid_x, grid_y);
        } else {
            // Animation in progress, calculate interpolated position using lv_map
            // The range 0-256 is a common resolution for interpolation in LVGL
            int32_t progress = lv_map(elapsed, 0, PET_ANIMATION_DURATION_MS, 0, 256);
            
            lv_coord_t current_x = start_pos.x + ((end_pos.x - start_pos.x) * progress / 256);
            lv_coord_t current_y = start_pos.y + ((end_pos.y - start_pos.y) * progress / 256);

            lv_obj_set_pos(img_obj, current_x - (sprite_dsc->header.w / 2), current_y - sprite_dsc->header.h);
        }
    } else {
        // Static position, e.g., when camera is panning
        lv_point_t tile_center;
        IsometricRenderer::grid_to_screen_center(grid_x, grid_y, world_origin, &tile_center);
        lv_coord_t final_x = tile_center.x - (sprite_dsc->header.w / 2);
        lv_coord_t final_y = tile_center.y - sprite_dsc->header.h;
        lv_obj_set_pos(img_obj, final_x, final_y);
    }
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
    pet->move_to_random_tile();
}