#ifndef PET_HUB_VIEW_H
#define PET_HUB_VIEW_H

#include "views/view.h"
#include "models/pet_data_model.h"
#include <vector>
#include <string>

/**
 * @brief A view that displays a small, tile-based "world" for collected pets.
 *
 * This view uses the SpriteCacheManager to load and display sprites for tiles
 * and pets dynamically ("just-in-time"). It is responsible for requesting sprites
 * when a pet is added and releasing them upon removal or view destruction to
 * ensure optimal memory usage.
 */
class PetHubView : public View {
public:
    PetHubView();
    ~PetHubView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- Grid and Gameplay Constants ---
    static constexpr int GRID_SIZE = 5;
    static constexpr int TILE_SIZE = 48;
    static constexpr int HUB_AREA_SIZE = GRID_SIZE * TILE_SIZE; // 240
    static constexpr int MAX_PETS_IN_HUB = 6;

    // --- State Management ---
    struct HubPet {
        lv_obj_t* img_obj;
        int row;
        int col;
        PetId id;
        int animation_frame;
        // Stores the full paths to this specific pet's animation sprites
        std::vector<std::string> sprite_paths;
    };
    std::vector<HubPet> s_pets;
    bool grid_occupied[GRID_SIZE][GRID_SIZE] = {{false}};
    
    // --- Resource Management ---
    // A list of all sprite paths for the background tiles.
    std::vector<std::string> loaded_tile_sprite_paths;

    // --- UI Widgets and Timers ---
    lv_obj_t* hub_container = nullptr;
    lv_timer_t* movement_timer = nullptr;
    lv_timer_t* animation_timer = nullptr;

    // --- Sprite Management ---
    bool load_tile_sprites();
    const lv_image_dsc_t* get_sprite_from_cache(const std::string& path);
    const lv_image_dsc_t* get_random_tile_sprite();
    std::string build_pet_sprite_path(PetId pet_id, const char* sprite_name);

    // --- UI Setup ---
    void setup_ui(lv_obj_t* parent);
    void setup_grid(lv_obj_t* parent);
    void setup_button_handlers();

    // --- Logic ---
    void place_initial_pets();
    bool get_random_unoccupied_position(int& row, int& col);
    void set_pet_position(HubPet& pet, int row, int col, bool animate);
    void move_random_pet();
    void animate_pet_sprites();
    
    // --- Actions ---
    void add_new_pet();
    void remove_last_pet();
    void go_back_to_menu();

    // --- Static Callbacks ---
    static void back_button_cb(void* user_data);
    static void add_button_cb(void* user_data);
    static void remove_button_cb(void* user_data);
    static void movement_timer_cb(lv_timer_t* timer);
    static void animation_timer_cb(lv_timer_t* timer);
};

#endif // PET_HUB_VIEW_H