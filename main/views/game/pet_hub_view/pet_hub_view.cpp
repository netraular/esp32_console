#include "pet_hub_view.h"
#include "controllers/pet_manager/pet_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "views/view_manager.h"
#include "models/asset_config.h"
#include "components/memory_monitor_component/memory_monitor_component.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>

#include "lvgl.h"

static const char* TAG = "PET_HUB_VIEW";

PetHubView::PetHubView() {
    ESP_LOGI(TAG, "PetHubView constructed");
}

PetHubView::~PetHubView() {
    if (movement_timer) {
        lv_timer_delete(movement_timer);
    }
    if (animation_timer) {
        lv_timer_delete(animation_timer);
    }
    ESP_LOGI(TAG, "PetHubView destructed");
}

void PetHubView::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
    
    setup_ui(container);

    if (sd_manager_check_ready()) {
        setup_grid(hub_container);
        place_initial_pets(); // This function now places repeated pets
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

void PetHubView::setup_ui(lv_obj_t* parent) {
    hub_container = lv_obj_create(parent);
    lv_obj_remove_style_all(hub_container);
    lv_obj_set_size(hub_container, HUB_AREA_SIZE, HUB_AREA_SIZE);
    lv_obj_center(hub_container);

    // Create the memory monitor widget on the main view container
    lv_obj_t* mem_monitor = memory_monitor_create(parent);
    lv_obj_align(mem_monitor, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
}

void PetHubView::setup_grid(lv_obj_t* parent) {
    const char* tile_options[] = {HUB_TILE_GROUND_01, HUB_TILE_GROUND_02};
    char tile_path[256];

    for (int row = 0; row < GRID_SIZE; ++row) {
        for (int col = 0; col < GRID_SIZE; ++col) {
            const char* chosen_tile = tile_options[esp_random() % 2];
            snprintf(tile_path, sizeof(tile_path), "%s%s%s%s%s%s",
                     LVGL_VFS_SD_CARD_PREFIX, SD_CARD_ROOT_PATH, ASSETS_BASE_SUBPATH,
                     ASSETS_SPRITES_SUBPATH, SPRITES_HUB_SUBPATH, chosen_tile);
            
            lv_obj_t* tile_img = lv_image_create(parent);
            lv_image_set_src(tile_img, tile_path);
            lv_image_set_antialias(tile_img, false);
            lv_obj_set_pos(tile_img, col * TILE_SIZE, row * TILE_SIZE);
        }
    }
}

void PetHubView::place_initial_pets() {
    // Hardcode the pet ID to be displayed repeatedly
    const PetId pet_to_repeat = PetId::PET_0001; // Example: Bulbasaur
    const int num_repetitions = 10;

    int pets_placed = 0;
    for (int i = 0; i < num_repetitions; ++i) {
        int row, col;
        if (get_random_unoccupied_position(row, col)) {
            HubPet new_pet;
            new_pet.id = pet_to_repeat;
            new_pet.animation_frame = 0;
            
            char sprite_path[256];
            snprintf(sprite_path, sizeof(sprite_path), "%s%s%s%s%s%04d/%s",
                LVGL_VFS_SD_CARD_PREFIX, SD_CARD_ROOT_PATH, ASSETS_BASE_SUBPATH,
                ASSETS_SPRITES_SUBPATH, SPRITES_PETS_SUBPATH, (int)new_pet.id, PET_SPRITE_DEFAULT);

            new_pet.img_obj = lv_image_create(hub_container);
            lv_image_set_src(new_pet.img_obj, sprite_path);
            lv_image_set_antialias(new_pet.img_obj, false);
            
            set_pet_position(new_pet, row, col, false);
            s_pets.push_back(new_pet);
            pets_placed++;
        } else {
            ESP_LOGW(TAG, "Could not find an unoccupied position for pet %d. Max pets reached or grid full.", i + 1);
            break; // Stop trying to place if no more space
        }
    }

    if (pets_placed == 0) {
        lv_obj_t* no_pets_label = lv_label_create(hub_container);
        lv_label_set_text(no_pets_label, "No pets could be placed.\nCheck SD card & sprite files.");
        lv_obj_set_style_text_align(no_pets_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(no_pets_label, lv_color_white(), 0);
        lv_obj_set_style_bg_color(no_pets_label, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(no_pets_label, LV_OPA_70, 0);
        lv_obj_center(no_pets_label);
    }
}

void PetHubView::set_pet_position(HubPet& pet, int row, int col, bool animate) {
    pet.row = row;
    pet.col = col;

    lv_coord_t target_x_center = col * TILE_SIZE + (TILE_SIZE / 2);
    lv_coord_t target_y_bottom = row * TILE_SIZE + TILE_SIZE;
    
    // We assume the sprite is TILE_SIZE x TILE_SIZE (48x48)
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
        // Increment frame counter for a 4-step animation
        pet.animation_frame = (pet.animation_frame + 1) % 4;
        
        const char* frame_name;
        // Select sprite based on the new 4-step sequence
        switch (pet.animation_frame) {
            case 0:  frame_name = PET_SPRITE_DEFAULT;   break; // Frame 1
            case 1:  frame_name = PET_SPRITE_IDLE_01;   break; // Frame 2
            case 2:  frame_name = PET_SPRITE_DEFAULT;   break; // Frame 1
            case 3:  frame_name = PET_SPRITE_IDLE_02;   break; // Frame 3
            default: frame_name = PET_SPRITE_DEFAULT;   break;
        }

        char sprite_path[256];
        snprintf(sprite_path, sizeof(sprite_path), "%s%s%s%s%s%04d/%s",
            LVGL_VFS_SD_CARD_PREFIX, SD_CARD_ROOT_PATH, ASSETS_BASE_SUBPATH,
            ASSETS_SPRITES_SUBPATH, SPRITES_PETS_SUBPATH, (int)pet.id, frame_name);

        lv_image_set_src(pet.img_obj, sprite_path);
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