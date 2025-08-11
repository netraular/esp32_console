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

PetHubView::PetHubView() {
    ESP_LOGI(TAG, "PetHubView constructed");
}

PetHubView::~PetHubView() {
    ESP_LOGI(TAG, "PetHubView destructed. Releasing all view-specific sprites...");
    if (movement_timer) {
        lv_timer_delete(movement_timer);
    }
    if (animation_timer) {
        lv_timer_delete(animation_timer);
    }
    
    auto& cache_manager = SpriteCacheManager::get_instance();
    
    // Release all sprites for pets currently in the hub
    for (const auto& pet : s_pets) {
        cache_manager.release_sprite_group(pet.sprite_paths);
    }
    s_pets.clear();
    
    // Release all tile sprites
    cache_manager.release_sprite_group(loaded_tile_sprite_paths);
    loaded_tile_sprite_paths.clear();

    ESP_LOGI(TAG, "PetHubView cleanup complete.");
}

void PetHubView::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
    
    setup_ui(container);

    if (sd_manager_check_ready()) {
        // Load only the essential background sprites
        if (load_tile_sprites()) {
            ESP_LOGI(TAG, "Tile sprites loaded. Setting up hub...");
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
        lv_label_set_text(err_label, LV_SYMBOL_SD_CARD " SD Card not found.\nCannot load hub.");
        lv_obj_set_style_text_color(err_label, lv_color_white(), 0);
        lv_obj_center(err_label);
    }
    
    setup_button_handlers();

    movement_timer = lv_timer_create(movement_timer_cb, 3000, this);
    animation_timer = lv_timer_create(animation_timer_cb, 300, this);
}

std::string PetHubView::build_pet_sprite_path(PetId pet_id, const char* sprite_name) {
    char path_buffer[256];
    snprintf(path_buffer, sizeof(path_buffer), "%s%s%s%s%04d/%s",
             sd_manager_get_mount_point(), ASSETS_BASE_SUBPATH,
             ASSETS_SPRITES_SUBPATH, SPRITES_PETS_SUBPATH, (int)pet_id, sprite_name);
    return std::string(path_buffer);
}

bool PetHubView::load_tile_sprites() {
    ESP_LOGI(TAG, "Requesting tile sprites from SpriteCacheManager...");
    loaded_tile_sprite_paths.clear();
    auto& cache_manager = SpriteCacheManager::get_instance();
    
    int successful_loads = 0;
    const char* tile_options[] = {HUB_TILE_GROUND_01, HUB_TILE_GROUND_02};
    for (const char* tile_name : tile_options) {
        char tile_path[256];
        snprintf(tile_path, sizeof(tile_path), "%s%s%s%s%s",
                 sd_manager_get_mount_point(), ASSETS_BASE_SUBPATH,
                 ASSETS_SPRITES_SUBPATH, SPRITES_HUB_SUBPATH, tile_name);
        
        if (cache_manager.get_sprite(tile_path)) {
            loaded_tile_sprite_paths.push_back(tile_path);
            successful_loads++;
        }
    }
    
    return (successful_loads == 2); 
}

const lv_image_dsc_t* PetHubView::get_sprite_from_cache(const std::string& path) {
    // This just gets the pointer, it doesn't increment the ref count again.
    // The manager handles the state internally.
    return SpriteCacheManager::get_instance().get_sprite(path);
}

const lv_image_dsc_t* PetHubView::get_random_tile_sprite() {
    if (loaded_tile_sprite_paths.empty()) return nullptr;
    int idx = esp_random() % loaded_tile_sprite_paths.size();
    // We get the sprite again here, which increments the ref count. This is
    // technically not needed if we assume the tiles are always loaded, but it's safer.
    // The release will happen in the destructor.
    return get_sprite_from_cache(loaded_tile_sprite_paths[idx]);
}

void PetHubView::add_new_pet() {
    if (s_pets.size() >= MAX_PETS_IN_HUB) {
        ESP_LOGI(TAG, "Hub is full. Cannot add more pets.");
        return;
    }

    auto& pet_manager = PetManager::get_instance();
    auto& cache_manager = SpriteCacheManager::get_instance();
    auto collection = pet_manager.get_collection();

    // Find a collected pet that is not already in the hub
    std::vector<PetId> available_pet_ids;
    for (const auto& entry : collection) {
        if (entry.collected) {
            PetId final_id = pet_manager.get_final_evolution(entry.base_id);
            auto it = std::find_if(s_pets.begin(), s_pets.end(), [final_id](const HubPet& p) {
                return p.id == final_id;
            });
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

    ESP_LOGI(TAG, "Attempting to add pet ID %d to hub at (%d, %d)", (int)pet_to_add_id, row, col);

    // DYNAMIC LOADING: Load sprites just for this pet.
    const char* sprite_names[] = {PET_SPRITE_DEFAULT, PET_SPRITE_IDLE_01, PET_SPRITE_IDLE_02};
    std::vector<std::string> new_pet_paths;
    bool all_loaded = true;

    for (const char* name : sprite_names) {
        std::string path = build_pet_sprite_path(pet_to_add_id, name);
        // get_sprite loads if not present and increments ref count.
        if (cache_manager.get_sprite(path)) {
            new_pet_paths.push_back(path);
        } else {
            ESP_LOGE(TAG, "Failed to load required sprite '%s'. Aborting add.", path.c_str());
            all_loaded = false;
            break;
        }
    }

    if (!all_loaded) {
        // Cleanup any sprites that were successfully loaded for this attempt
        cache_manager.release_sprite_group(new_pet_paths);
        grid_occupied[row][col] = false; // Free the spot back up
        return;
    }

    HubPet new_pet;
    new_pet.id = pet_to_add_id;
    new_pet.row = row;
    new_pet.col = col;
    new_pet.animation_frame = 0;
    new_pet.sprite_paths = new_pet_paths;

    // Use the first sprite for the initial image
    const lv_image_dsc_t* initial_sprite = get_sprite_from_cache(new_pet.sprite_paths[0]);
    new_pet.img_obj = lv_image_create(hub_container);
    lv_image_set_src(new_pet.img_obj, initial_sprite);
    lv_image_set_antialias(new_pet.img_obj, false);

    set_pet_position(new_pet, row, col, false);
    s_pets.push_back(new_pet);
    ESP_LOGI(TAG, "Successfully added pet %d. Total pets: %d", (int)new_pet.id, s_pets.size());
}

void PetHubView::animate_pet_sprites() {
    if (s_pets.empty()) return;

    for (auto& pet : s_pets) {
        pet.animation_frame = (pet.animation_frame + 1) % 4;
        
        // Map animation frame to sprite path index
        int sprite_idx;
        switch (pet.animation_frame) {
            case 0:  sprite_idx = 0; break; // PET_SPRITE_DEFAULT
            case 1:  sprite_idx = 1; break; // PET_SPRITE_IDLE_01
            case 2:  sprite_idx = 0; break; // PET_SPRITE_DEFAULT
            case 3:  sprite_idx = 2; break; // PET_SPRITE_IDLE_02
            default: sprite_idx = 0; break;
        }
        
        if (sprite_idx < pet.sprite_paths.size()) {
            const lv_image_dsc_t* cached_sprite = get_sprite_from_cache(pet.sprite_paths[sprite_idx]);
            if (cached_sprite) {
                lv_image_set_src(pet.img_obj, cached_sprite);
            }
        }
    }
}


// --- El resto del código no necesita cambios ---
// (place_initial_pets, remove_last_pet, set_pet_position, move_random_pet, etc.)
// ...copia el resto de las funciones desde la respuesta anterior...
void PetHubView::place_initial_pets() {
    // Start with one pet for demonstration
    add_new_pet();
}

void PetHubView::remove_last_pet() {
    if (s_pets.empty()) {
        ESP_LOGI(TAG, "Hub is empty. Nothing to remove.");
        return;
    }

    // Get the last pet added
    HubPet pet_to_remove = s_pets.back();
    s_pets.pop_back();

    ESP_LOGI(TAG, "Removing pet ID %d from hub at (%d, %d)", (int)pet_to_remove.id, pet_to_remove.row, pet_to_remove.col);

    // Mark its grid position as free
    grid_occupied[pet_to_remove.row][pet_to_remove.col] = false;

    // Delete the LVGL object
    lv_obj_delete(pet_to_remove.img_obj);

    // Release its sprites from the cache manager
    SpriteCacheManager::get_instance().release_sprite_group(pet_to_remove.sprite_paths);
    ESP_LOGI(TAG, "Released %zu sprites for the removed pet.", pet_to_remove.sprite_paths.size());
}


void PetHubView::setup_ui(lv_obj_t* parent) {
    hub_container = lv_obj_create(parent);
    lv_obj_remove_style_all(hub_container);
    lv_obj_set_size(hub_container, HUB_AREA_SIZE, HUB_AREA_SIZE);
    lv_obj_center(hub_container);

    lv_obj_t* mem_monitor = memory_monitor_create(parent);
    lv_obj_align(mem_monitor, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
}

void PetHubView::setup_grid(lv_obj_t* parent) {
    for (int row = 0; row < GRID_SIZE; ++row) {
        for (int col = 0; col < GRID_SIZE; ++col) {
            const lv_image_dsc_t* tile_sprite = get_random_tile_sprite();
            if (!tile_sprite) {
                ESP_LOGW(TAG, "No tile sprite available for grid position (%d, %d)", row, col);
                continue;
            }
            
            lv_obj_t* tile_img = lv_image_create(parent);
            lv_image_set_src(tile_img, tile_sprite);
            lv_image_set_antialias(tile_img, false);
            lv_obj_set_pos(tile_img, col * TILE_SIZE, row * TILE_SIZE);
        }
    }
}

void PetHubView::set_pet_position(HubPet& pet, int row, int col, bool animate) {
    pet.row = row;
    pet.col = col;

    lv_coord_t target_x_center = col * TILE_SIZE + (TILE_SIZE / 2);
    lv_coord_t target_y_bottom = row * TILE_SIZE + TILE_SIZE;
    
    constexpr lv_coord_t sprite_size = TILE_SIZE;
    lv_coord_t final_x = target_x_center - (sprite_size / 2);
    lv_coord_t final_y = target_y_bottom - sprite_size;

    if (animate) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, pet.img_obj);
        lv_anim_set_duration(&a, 750);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);

        lv_anim_set_values(&a, lv_obj_get_x(pet.img_obj), final_x);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
        lv_anim_start(&a);
        
        lv_anim_set_values(&a, lv_obj_get_y(pet.img_obj), final_y);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_start(&a);
    } else {
        lv_obj_set_pos(pet.img_obj, final_x, final_y);
    }
}

void PetHubView::move_random_pet() {
    if (s_pets.size() <= 1) return; // No need to move if there's only one or zero pets

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