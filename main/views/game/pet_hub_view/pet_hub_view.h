#ifndef PET_HUB_VIEW_H
#define PET_HUB_VIEW_H

#include "views/view.h"
#include "models/pet_data_model.h"
#include <vector>
#include <string>

class PetHubView : public View {
public:
    PetHubView();
    ~PetHubView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- Configuration ---
    // Set to 'true' to only show the final evolution of collected pets.
    // Set to 'false' to show any discovered stage of any pet.
    static constexpr bool SHOW_ADULTS_ONLY = false;

    // --- Constants ---
    static constexpr int GRID_SIZE = 5;
    static constexpr int TILE_SIZE = 48;
    static constexpr int HUB_AREA_SIZE = GRID_SIZE * TILE_SIZE; // 240
    static constexpr int MAX_PETS_IN_HUB = 24;
    
    struct HubPet {
        lv_obj_t* img_obj;
        int row;
        int col;
        PetId id;
        int animation_frame;
        // Store the full paths for releasing them later
        std::vector<std::string> sprite_paths;
        // Store pointers to the cached image descriptors
        std::vector<const lv_image_dsc_t*> animation_frames;
    };
    std::vector<HubPet> s_pets;
    bool grid_occupied[GRID_SIZE][GRID_SIZE] = {{false}};
    
    // --- Resource Management ---
    // Store paths for releasing, and descriptor pointers for using
    std::vector<std::string> loaded_tile_sprite_paths;
    // Store pointers to the cached image descriptors
    std::vector<const lv_image_dsc_t*> tile_sprites;

    lv_obj_t* hub_container = nullptr;
    lv_timer_t* movement_timer = nullptr;
    lv_timer_t* animation_timer = nullptr;

    // --- Sprite Management ---
    bool load_tile_sprites();
    std::string build_pet_sprite_path(PetId pet_id, const char* sprite_name);

    void setup_ui(lv_obj_t* parent);
    void setup_grid(lv_obj_t* parent);
    void setup_button_handlers();
    void place_initial_pets();
    bool get_random_unoccupied_position(int& row, int& col);
    void set_pet_position(HubPet& pet, int row, int col, bool animate);
    void move_random_pet();
    void animate_pet_sprites();
    void add_new_pet();
    void remove_last_pet();
    void go_back_to_menu();

    static void back_button_cb(void* user_data);
    static void add_button_cb(void* user_data);
    static void remove_button_cb(void* user_data);
    static void movement_timer_cb(lv_timer_t* timer);
    static void animation_timer_cb(lv_timer_t* timer);
};

#endif // PET_HUB_VIEW_H