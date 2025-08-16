#ifndef ROOM_PET_H
#define ROOM_PET_H

#include "lvgl.h"
#include "models/pet_data_model.h"
#include <string>
#include <vector>

class RoomPet {
public:
    RoomPet(int room_width, int room_depth);
    ~RoomPet();

    bool spawn();
    void remove();
    void update_state();
    void draw(lv_layer_t* layer, const lv_point_t& camera_offset);
    
    int get_grid_x() const;
    int get_grid_y() const;
    int get_target_grid_x() const;
    int get_target_grid_y() const;
    void get_interpolated_grid_pos(float& x, float& y) const;
    const lv_image_dsc_t* get_current_sprite() const;

    bool is_animating() const;
    bool is_spawned() const;

private:
    void move_to_random_tile();
    std::string build_pet_sprite_path(PetId pet_id, const char* sprite_name);

    static void movement_timer_cb(lv_timer_t* timer);
    static void animation_timer_cb(lv_timer_t* timer);

    PetId id = PetId::NONE;
    
    // Current logical position
    int grid_x = 0;
    int grid_y = 0;
    
    // Target position for animation
    int target_grid_x = -1;
    int target_grid_y = -1;

    // Sprite resource management
    std::vector<std::string> sprite_paths;
    std::vector<const lv_image_dsc_t*> animation_frames;
    int current_animation_frame = 0;

    // Movement state
    bool animating = false;
    uint32_t anim_start_tick = 0;
    bool spawned = false;

    const int ROOM_WIDTH;
    const int ROOM_DEPTH;

    lv_timer_t* movement_timer = nullptr;
    lv_timer_t* animation_timer = nullptr;
};

#endif // ROOM_PET_H