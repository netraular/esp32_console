#ifndef ROOM_VIEW_H
#define ROOM_VIEW_H

#include "views/view.h"
#include "lvgl.h"

/**
 * @brief A view for displaying and interacting with an isometric room.
 *
 * This view uses LVGL's core drawing engine to render an isometric grid.
 * The user can move a cursor tile-by-tile with smooth animated transitions,
 * and the camera will automatically center on the selected tile.
 */
class RoomView : public View {
public:
    RoomView();
    ~RoomView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- Constants ---
    static constexpr int ROOM_WIDTH = 10;           // Room width in tiles
    static constexpr int ROOM_DEPTH = 10;           // Room depth in tiles
    static constexpr int WALL_HEIGHT_UNITS = 4;     // Height of the walls in tile-height units
    static constexpr int TILE_WIDTH = 64;           // Isometric tile width in pixels
    static constexpr int TILE_HEIGHT = 32;          // Isometric tile height in pixels
    static constexpr int ANIMATION_DURATION_MS = 100; // Duration of the camera pan animation

    // --- LVGL Objects ---
    lv_obj_t* room_canvas = nullptr; // A basic object that acts as our drawing surface

    // --- State ---
    int selected_grid_x; // The X coordinate of the selected tile on the grid
    int selected_grid_y; // The Y coordinate of the selected tile on the grid
    
    // --- Animation State ---
    bool is_animating;           // Flag to prevent input during animation
    lv_point_t camera_offset;      // The current, animated camera offset
    lv_point_t anim_start_offset;  // Camera offset at the start of an animation
    lv_point_t anim_end_offset;    // Target camera offset for the end of an animation
    
    // --- Drawing & Logic ---
    void grid_to_screen(int grid_x, int grid_y, const lv_point_t& origin, lv_point_t* p_out);
    void calculate_camera_offset(int grid_x, int grid_y, lv_point_t* out_offset);

    // --- UI & Button Handling ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();

    // --- Button Actions ---
    void on_grid_move(int dx, int dy);
    void on_back_to_menu();

    // --- Static Callbacks ---
    static void draw_event_cb(lv_event_t* e);
    // Button callbacks
    static void handle_move_northeast_cb(void* user_data); // Top-Right
    static void handle_move_northwest_cb(void* user_data); // Top-Left
    static void handle_move_southeast_cb(void* user_data); // Bottom-Right
    static void handle_move_southwest_cb(void* user_data); // Bottom-Left
    static void handle_back_long_press_cb(void* user_data);
    // Animation callbacks
    static void anim_exec_cb(void* var, int32_t v);
    static void anim_ready_cb(lv_anim_t* a);
};

#endif // ROOM_VIEW_H