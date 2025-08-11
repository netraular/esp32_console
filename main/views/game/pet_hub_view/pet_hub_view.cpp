#include "pet_hub_view.h"
#include "controllers/pet_manager/pet_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/sprite_cache_manager/sprite_cache_manager.h"
#include "views/view_manager.h"
#include "models/asset_config.h"
#include "components/memory_monitor_component/memory_monitor_component.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string>
#include <vector>
#include <random>
#include <algorithm>

static const char* TAG = "PET_HUB_VIEW";

// --- CONSTRUCTOR & DESTRUCTOR ---
PetHubView::PetHubView() {
    ESP_LOGI(TAG, "PetHubView constructed");
}

PetHubView::~PetHubView() {
    ESP_LOGI(TAG, "PetHubView destructed. Releasing all view-specific sprites...");
    if (movement_timer) lv_timer_delete(movement_timer);
    if (animation_timer) lv_timer_delete(animation_timer);
    
    auto& cache_manager = SpriteCacheManager::get_instance();
    
    // Release all sprites for pets currently in the hub
    for (const auto& pet : s_pets) {
        ESP_LOGD(TAG, "Releasing sprites for pet ID %d", (int)pet.id);
        cache_manager.release_sprite_group(pet.sprite_paths);
    }
    s_pets.clear();
    
    // Release all tile sprites
    ESP_LOGD(TAG, "Releasing tile sprites");
    cache_manager.release_sprite_group(loaded_tile_sprite_paths);
    loaded_tile_sprite_paths.clear();
    tile_sprites.clear();

    ESP_LOGI(TAG, "PetHubView cleanup complete.");
}

// --- VIEW CREATION ---
void PetHubView::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
    
    setup_ui(container);

    if (sd_manager_check_ready()) {
        if (load_tile_sprites()) {
            setup_grid(hub_container);
            place_initial_pets();
        } else {
            ESP_LOGE(TAG, "Failed to load tile sprites. Hub cannot be displayed.");
            lv_obj_t* err_label = lv_label_create(hub_container);
            lv_label_set_text(err_label, "Error: Failed to load\nbackground assets.");
            lv_obj_set_style_text_color(err_label, lv_color_white(), 0);
            lv_obj_center(err_label);
        }
    } else {
        lv_obj_t* err_label = lv_label_create(hub_container);
        lv_label_set_text(err_label, LV_SYMBOL_SD_CARD " SD Card not found.");
        lv_obj_set_style_text_color(err_label, lv_color_white(), 0);
        lv_obj_center(err_label);
    }
    
    setup_button_handlers();

    movement_timer = lv_timer_create(movement_timer_cb, 3000, this);
    animation_timer = lv_timer_create(animation_timer_cb, 300, this);
}

// --- RESOURCE MANAGEMENT ---

bool PetHubView::load_tile_sprites() {
    ESP_LOGI(TAG, "Loading tile sprites ONCE for the entire view");
    loaded_tile_sprite_paths.clear();
    tile_sprites.clear();

    auto& cache_manager = SpriteCacheManager::get_instance();
    
    const char* tile_names[] = {HUB_TILE_GROUND_01, HUB_TILE_GROUND_02};
    for (const char* tile_name : tile_names) {
        char tile_path_buffer[256];
        // Build the standard C-style path (e.g., "/sdcard/assets/...")
        snprintf(tile_path_buffer, sizeof(tile_path_buffer), "%s%s%s%s%s",
                 sd_manager_get_mount_point(),
                 ASSETS_BASE_SUBPATH, ASSETS_SPRITES_SUBPATH, SPRITES_HUB_SUBPATH, tile_name);
        
        std::string tile_path_str(tile_path_buffer);
        const lv_image_dsc_t* sprite_dsc = cache_manager.get_sprite(tile_path_str);

        if (sprite_dsc) {
            loaded_tile_sprite_paths.push_back(tile_path_str);
            tile_sprites.push_back(sprite_dsc);
            ESP_LOGD(TAG, "Loaded tile sprite: %s (%dx%d)", tile_name, (int)sprite_dsc->header.w, (int)sprite_dsc->header.h);
        } else {
            ESP_LOGE(TAG, "Failed to load tile sprite: %s", tile_name);
            cache_manager.release_sprite_group(loaded_tile_sprite_paths);
            loaded_tile_sprite_paths.clear();
            tile_sprites.clear();
            return false;
        }
    }
    ESP_LOGI(TAG, "Successfully loaded %zu tile sprites", tile_sprites.size());
    return true; 
}

void PetHubView::setup_grid(lv_obj_t* parent) {
    if (tile_sprites.empty()) {
        ESP_LOGE(TAG, "Cannot setup grid, no tile sprites loaded.");
        return;
    }
    
    ESP_LOGI(TAG, "Setting up %dx%d grid with %zu tile sprites", GRID_SIZE, GRID_SIZE, tile_sprites.size());
    
    for (int row = 0; row < GRID_SIZE; ++row) {
        for (int col = 0; col < GRID_SIZE; ++col) {
            const lv_image_dsc_t* tile_sprite_dsc = tile_sprites[esp_random() % tile_sprites.size()];
            
            lv_obj_t* tile_img = lv_image_create(parent);
            lv_image_set_src(tile_img, tile_sprite_dsc);
            lv_image_set_antialias(tile_img, false);
            lv_obj_set_pos(tile_img, col * TILE_SIZE, row * TILE_SIZE);
            
            // Set explicit size to ensure proper grid alignment
            lv_obj_set_size(tile_img, TILE_SIZE, TILE_SIZE);
            
            ESP_LOGD(TAG, "Placed tile at grid[%d][%d] -> pos(%d,%d)", row, col, col * TILE_SIZE, row * TILE_SIZE);
        }
    }
}

std::string PetHubView::build_pet_sprite_path(PetId pet_id, const char* sprite_name) {
    char path_buffer[256];
    // Build the standard C-style path (e.g., "/sdcard/assets/...")
    snprintf(path_buffer, sizeof(path_buffer), "%s%s%s%s%04d/%s",
             sd_manager_get_mount_point(),
             ASSETS_BASE_SUBPATH, ASSETS_SPRITES_SUBPATH, SPRITES_PETS_SUBPATH, (int)pet_id, sprite_name);
    return std::string(path_buffer);
}

void PetHubView::setup_ui(lv_obj_t* parent) {
    hub_container = lv_obj_create(parent);
    lv_obj_remove_style_all(hub_container);
    lv_obj_set_size(hub_container, HUB_AREA_SIZE, HUB_AREA_SIZE);
    lv_obj_center(hub_container);
    
    // Add the flag that allows children to be drawn outside the container's boundaries.
    lv_obj_add_flag(hub_container, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    // Set a visible background for debugging
    lv_obj_set_style_bg_color(hub_container, lv_color_make(32, 32, 32), 0);
    lv_obj_set_style_bg_opa(hub_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(hub_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(hub_container, 1, 0);

    lv_obj_t* mem_monitor = memory_monitor_create(parent);
    lv_obj_align(mem_monitor, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
    
    ESP_LOGI(TAG, "Hub container created: %dx%d", HUB_AREA_SIZE, HUB_AREA_SIZE);
}

void PetHubView::place_initial_pets() {
    add_new_pet();
}

void PetHubView::add_new_pet() {
    if (s_pets.size() >= MAX_PETS_IN_HUB) {
        ESP_LOGI(TAG, "Hub is full. Cannot add more pets.");
        return;
    }

    auto& pet_manager = PetManager::get_instance();
    auto& cache_manager = SpriteCacheManager::get_instance();
    auto collection = pet_manager.get_collection();

    std::vector<PetId> available_pet_ids;
    for (const auto& entry : collection) {
        if (entry.collected) {
            PetId final_id = pet_manager.get_final_evolution(entry.base_id);
            auto it = std::find_if(s_pets.begin(), s_pets.end(), [final_id](const HubPet& p) { return p.id == final_id; });
            if (it == s_pets.end()) {
                available_pet_ids.push_back(final_id);
            }
        }
    }

    if (available_pet_ids.empty()) {
        ESP_LOGI(TAG, "No more available pets to add to the hub.");
        return;
    }

    PetId pet_to_add_id = available_pet_ids[esp_random() % available_pet_ids.size()];

    int row, col;
    if (!get_random_unoccupied_position(row, col)) {
        ESP_LOGW(TAG, "No unoccupied positions left to add a pet.");
        return;
    }

    ESP_LOGI(TAG, "Adding pet ID %d to hub at (%d, %d)", (int)pet_to_add_id, row, col);

    const char* sprite_names[] = {PET_SPRITE_DEFAULT, PET_SPRITE_IDLE_01, PET_SPRITE_IDLE_02};
    HubPet new_pet;
    new_pet.id = pet_to_add_id;
    new_pet.row = row;
    new_pet.col = col;
    new_pet.animation_frame = 0;
    bool all_loaded = true;

    for (const char* name : sprite_names) {
        std::string path = build_pet_sprite_path(pet_to_add_id, name);
        const lv_image_dsc_t* sprite_dsc = cache_manager.get_sprite(path);
        
        if (sprite_dsc) {
            new_pet.sprite_paths.push_back(path);
            new_pet.animation_frames.push_back(sprite_dsc);
            ESP_LOGD(TAG, "Loaded pet sprite: %s (%dx%d)", name, (int)sprite_dsc->header.w, (int)sprite_dsc->header.h);
        } else {
            ESP_LOGE(TAG, "Failed to load required sprite '%s'. Aborting add.", path.c_str());
            all_loaded = false;
            break;
        }
    }

    if (!all_loaded) {
        cache_manager.release_sprite_group(new_pet.sprite_paths);
        grid_occupied[row][col] = false;
        return;
    }

    // Create the LVGL image object for the pet
    new_pet.img_obj = lv_image_create(hub_container);
    if (!new_pet.img_obj) {
        ESP_LOGE(TAG, "Failed to create image object for pet");
        cache_manager.release_sprite_group(new_pet.sprite_paths);
        grid_occupied[row][col] = false;
        return;
    }
    
    // Set the initial sprite (first animation frame)
    lv_image_set_src(new_pet.img_obj, new_pet.animation_frames[0]);
    lv_image_set_antialias(new_pet.img_obj, false);
    
    // The image object's size will now automatically match the source sprite.
    // We do not set an explicit size, to allow sprites to be larger than a tile.

    // Position the pet on the grid
    set_pet_position(new_pet, row, col, false);
    
    s_pets.push_back(new_pet);
    ESP_LOGI(TAG, "Successfully added pet %d at grid[%d][%d]. Total pets: %zu", (int)new_pet.id, row, col, s_pets.size());
}

void PetHubView::remove_last_pet() {
    if (s_pets.empty()) {
        ESP_LOGI(TAG, "Hub is empty. Nothing to remove.");
        return;
    }

    HubPet pet_to_remove = s_pets.back();
    s_pets.pop_back();

    ESP_LOGI(TAG, "Removing pet ID %d from hub at (%d, %d)", (int)pet_to_remove.id, pet_to_remove.row, pet_to_remove.col);

    grid_occupied[pet_to_remove.row][pet_to_remove.col] = false;
    lv_obj_delete(pet_to_remove.img_obj);

    SpriteCacheManager::get_instance().release_sprite_group(pet_to_remove.sprite_paths);
    ESP_LOGI(TAG, "Released %zu sprite paths for the removed pet.", pet_to_remove.sprite_paths.size());
}

void PetHubView::animate_pet_sprites() {
    if (s_pets.empty()) return;

    for (auto& pet : s_pets) {
        pet.animation_frame = (pet.animation_frame + 1) % 4;
        
        int sprite_idx;
        switch (pet.animation_frame) {
            case 1:  sprite_idx = 1; break;
            case 3:  sprite_idx = 2; break;
            default: sprite_idx = 0; break;
        }
        
        if (sprite_idx < pet.animation_frames.size()) {
            const lv_image_dsc_t* frame_dsc = pet.animation_frames[sprite_idx];
            if (frame_dsc && pet.img_obj) {
                lv_image_set_src(pet.img_obj, frame_dsc);
            }
        }
    }
}

void PetHubView::set_pet_position(HubPet& pet, int row, int col, bool animate) {
    pet.row = row;
    pet.col = col;

    // Get the actual dimensions of the sprite currently being displayed
    const lv_image_dsc_t* current_sprite = (const lv_image_dsc_t*)lv_image_get_src(pet.img_obj);
    if (!current_sprite) {
        ESP_LOGE(TAG, "Cannot set pet position, sprite source is null!");
        return;
    }
    lv_coord_t sprite_width = current_sprite->header.w;
    lv_coord_t sprite_height = current_sprite->header.h;

    // Calculate the anchor points based on the target grid cell
    lv_coord_t target_x_center = col * TILE_SIZE + (TILE_SIZE / 2);
    lv_coord_t target_y_bottom = (row + 1) * TILE_SIZE;
    
    // Calculate the final top-left coordinate to align the sprite.
    // It should be horizontally centered and its bottom edge should align with the cell's bottom.
    lv_coord_t final_x = target_x_center - (sprite_width / 2);
    lv_coord_t final_y = target_y_bottom - sprite_height;

    ESP_LOGD(TAG, "Setting pet pos: grid[%d][%d], sprite[%dx%d] -> screen(%d,%d)", row, col, (int)sprite_width, (int)sprite_height, (int)final_x, (int)final_y);

    if (animate) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, pet.img_obj);
        lv_anim_set_duration(&a, 750);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
        lv_anim_set_values(&a, lv_obj_get_x(pet.img_obj), final_x);
        lv_anim_start(&a);
        
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_set_values(&a, lv_obj_get_y(pet.img_obj), final_y);
        lv_anim_start(&a);
    } else {
        lv_obj_set_pos(pet.img_obj, final_x, final_y);
    }
}

void PetHubView::move_random_pet() {
    if (s_pets.size() <= 1) return;

    uint32_t pet_idx = esp_random() % s_pets.size();
    HubPet& pet_to_move = s_pets[pet_idx];

    const int moves[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
    std::vector<std::pair<int, int>> valid_moves;

    for (const auto& move : moves) {
        int new_row = pet_to_move.row + move[0];
        int new_col = pet_to_move.col + move[1];

        if (new_row >= 0 && new_row < GRID_SIZE && new_col >= 0 && new_col < GRID_SIZE) {
            if (!grid_occupied[new_row][new_col]) {
                valid_moves.push_back({new_row, new_col});
            }
        }
    }

    if (!valid_moves.empty()) {
        uint32_t move_idx = esp_random() % valid_moves.size();
        auto target_pos = valid_moves[move_idx];
        
        grid_occupied[pet_to_move.row][pet_to_move.col] = false;
        grid_occupied[target_pos.first][target_pos.second] = true;

        ESP_LOGD(TAG, "Moving pet %d from (%d,%d) to (%d,%d)", (int)pet_to_move.id, pet_to_move.row, pet_to_move.col, target_pos.first, target_pos.second);
        set_pet_position(pet_to_move, target_pos.first, target_pos.second, true);
    }
}

bool PetHubView::get_random_unoccupied_position(int& row, int& col) {
    const int max_attempts = GRID_SIZE * GRID_SIZE * 2;
    for (int i = 0; i < max_attempts; ++i) {
        int r = esp_random() % GRID_SIZE;
        int c = esp_random() % GRID_SIZE;
        if (!grid_occupied[r][c]) {
            grid_occupied[r][c] = true;
            row = r;
            col = c;
            return true;
        }
    }
    return false;
}

void PetHubView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, back_button_cb, true, this);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, add_button_cb, true, this);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, remove_button_cb, true, this);
}

void PetHubView::go_back_to_menu() {
    view_manager_load_view(VIEW_ID_MENU);
}

void PetHubView::back_button_cb(void* user_data) {
    static_cast<PetHubView*>(user_data)->go_back_to_menu();
}

void PetHubView::add_button_cb(void* user_data) {
    static_cast<PetHubView*>(user_data)->add_new_pet();
}

void PetHubView::remove_button_cb(void* user_data) {
    static_cast<PetHubView*>(user_data)->remove_last_pet();
}

void PetHubView::movement_timer_cb(lv_timer_t* timer) {
    auto* view = static_cast<PetHubView*>(lv_timer_get_user_data(timer));
    view->move_random_pet();
}

void PetHubView::animation_timer_cb(lv_timer_t* timer) {
    auto* view = static_cast<PetHubView*>(lv_timer_get_user_data(timer));
    view->animate_pet_sprites();
}