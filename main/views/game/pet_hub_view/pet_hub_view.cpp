#include "pet_hub_view.h"
#include "controllers/pet_manager/pet_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/sprite_cache_manager/sprite_cache_manager.h" // Include the new manager
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
    ESP_LOGI(TAG, "PetHubView destructed. Releasing sprites...");
    if (movement_timer) {
        lv_timer_delete(movement_timer);
    }
    if (animation_timer) {
        lv_timer_delete(animation_timer);
    }
    
    // CRITICAL: Release all sprites this view instance was using.
    SpriteCacheManager::get_instance().release_sprite_group(loaded_sprite_paths);

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
        // Load all required sprites via the cache manager
        if (load_and_cache_all_sprites()) {
            ESP_LOGI(TAG, "All sprites loaded via cache. Setting up hub...");
            setup_grid(hub_container);
            place_initial_pets();
        } else {
            ESP_LOGE(TAG, "Failed to load sprites. Showing error message.");
            lv_obj_t* err_label = lv_label_create(hub_container);
            lv_label_set_text(err_label, "Failed to load sprites.\nCheck SD card.");
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

bool PetHubView::load_and_cache_all_sprites() {
    ESP_LOGI(TAG, "Requesting all hub sprites from SpriteCacheManager...");
    loaded_sprite_paths.clear();
    auto& cache_manager = SpriteCacheManager::get_instance();
    
    int total_sprites = 0;
    int successful_loads = 0;
    
    // --- Load tile sprites ---
    const char* tile_options[] = {HUB_TILE_GROUND_01, HUB_TILE_GROUND_02};
    for (const char* tile_name : tile_options) {
        total_sprites++;
        char tile_path[256];
        snprintf(tile_path, sizeof(tile_path), "%s%s%s%s%s",
                 sd_manager_get_mount_point(), ASSETS_BASE_SUBPATH,
                 ASSETS_SPRITES_SUBPATH, SPRITES_HUB_SUBPATH, tile_name);
        
        if (cache_manager.get_sprite(tile_path)) {
            loaded_sprite_paths.push_back(tile_path);
            successful_loads++;
        }
    }

    // --- Load pet sprites for collected pets ---
    auto& pet_manager = PetManager::get_instance();
    auto collection = pet_manager.get_collection();
    
    const char* pet_sprite_names[] = {PET_SPRITE_DEFAULT, PET_SPRITE_IDLE_01, PET_SPRITE_IDLE_02};

    for (const auto& entry : collection) {
        if (entry.collected) {
            PetId final_id = pet_manager.get_final_evolution(entry.base_id);
            for (const char* sprite_name : pet_sprite_names) {
                total_sprites++;
                std::string pet_sprite_path = build_pet_sprite_path(final_id, sprite_name);
                if (cache_manager.get_sprite(pet_sprite_path)) {
                    loaded_sprite_paths.push_back(pet_sprite_path);
                    successful_loads++;
                }
            }
        }
    }
    
    ESP_LOGI(TAG, "Sprite loading complete: %d/%d sprites available in cache for this view", 
             successful_loads, total_sprites);
    
    // Succeed if at least the tile sprites are loaded
    return (successful_loads >= 2); 
}

const lv_image_dsc_t* PetHubView::get_pet_sprite(PetId pet_id, const char* sprite_name) {
    std::string path = build_pet_sprite_path(pet_id, sprite_name);
    return SpriteCacheManager::get_instance().get_sprite(path);
}

const lv_image_dsc_t* PetHubView::get_random_tile_sprite() {
    char tile_path[256];
    const char* tile_name = (esp_random() % 2 == 0) ? HUB_TILE_GROUND_01 : HUB_TILE_GROUND_02;

    snprintf(tile_path, sizeof(tile_path), "%s%s%s%s%s",
             sd_manager_get_mount_point(), ASSETS_BASE_SUBPATH,
             ASSETS_SPRITES_SUBPATH, SPRITES_HUB_SUBPATH, tile_name);

    return SpriteCacheManager::get_instance().get_sprite(tile_path);
}

// (El resto de PetHubView.cpp, desde setup_ui hasta el final, no necesita cambios)
// ...
// ... Copia el resto del código de PetHubView.cpp aquí, pero asegúrate de que
// ... la función animate_pet_sprites() llame a get_pet_sprite()
// ... como se muestra a continuación.

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

void PetHubView::place_initial_pets() {
    auto& pet_manager = PetManager::get_instance();
    auto collection = pet_manager.get_collection();
    std::vector<PetId> available_pets;

    for (const auto& entry : collection) {
        if (entry.collected) {
            available_pets.push_back(pet_manager.get_final_evolution(entry.base_id));
        }
    }

    if (available_pets.empty()) {
        lv_obj_t* no_pets_label = lv_label_create(hub_container);
        lv_label_set_text(no_pets_label, "Collect a pet\nto see it here!");
        lv_obj_set_style_text_align(no_pets_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(no_pets_label, lv_color_white(), 0);
        lv_obj_set_style_bg_color(no_pets_label, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(no_pets_label, LV_OPA_70, 0);
        lv_obj_center(no_pets_label);
        return;
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(available_pets.begin(), available_pets.end(), g);

    int num_to_place = std::min((size_t)MAX_PETS_IN_HUB, available_pets.size());
    for (int i = 0; i < num_to_place; ++i) {
        int row, col;
        if (get_random_unoccupied_position(row, col)) {
            HubPet new_pet;
            new_pet.id = available_pets[i];
            new_pet.animation_frame = 0;
            
            const lv_image_dsc_t* initial_sprite = get_pet_sprite(new_pet.id, PET_SPRITE_DEFAULT);
            if (!initial_sprite) {
                ESP_LOGW(TAG, "No cached sprite available for pet %04d, skipping", (int)new_pet.id);
                continue;
            }

            new_pet.img_obj = lv_image_create(hub_container);
            lv_image_set_src(new_pet.img_obj, initial_sprite);
            lv_image_set_antialias(new_pet.img_obj, false);
            
            set_pet_position(new_pet, row, col, false);
            s_pets.push_back(new_pet);
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
    if (s_pets.empty()) return;

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

void PetHubView::animate_pet_sprites() {
    if (s_pets.empty()) return;

    for (auto& pet : s_pets) {
        pet.animation_frame = (pet.animation_frame + 1) % 4;
        
        const char* frame_name;
        switch (pet.animation_frame) {
            case 0:  frame_name = PET_SPRITE_DEFAULT;   break;
            case 1:  frame_name = PET_SPRITE_IDLE_01;   break;
            case 2:  frame_name = PET_SPRITE_DEFAULT;   break;
            case 3:  frame_name = PET_SPRITE_IDLE_02;   break;
            default: frame_name = PET_SPRITE_DEFAULT;   break;
        }

        const lv_image_dsc_t* cached_sprite = get_pet_sprite(pet.id, frame_name);
        if (cached_sprite) {
            lv_image_set_src(pet.img_obj, cached_sprite);
        }
    }
}

bool PetHubView::get_random_unoccupied_position(int& row, int& col) {
    const int max_attempts = GRID_SIZE * GRID_SIZE;
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
}

void PetHubView::go_back_to_menu() {
    view_manager_load_view(VIEW_ID_MENU);
}

void PetHubView::back_button_cb(void* user_data) {
    static_cast<PetHubView*>(user_data)->go_back_to_menu();
}

void PetHubView::movement_timer_cb(lv_timer_t* timer) {
    auto* view = static_cast<PetHubView*>(lv_timer_get_user_data(timer));
    view->move_random_pet();
}

void PetHubView::animation_timer_cb(lv_timer_t* timer) {
    auto* view = static_cast<PetHubView*>(lv_timer_get_user_data(timer));
    view->animate_pet_sprites();
}