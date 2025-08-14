#ifndef ROOM_VIEW_H
#define ROOM_VIEW_H

#include "views/view.h"
#include "lvgl.h"

/**
 * @brief A view for displaying and interacting with an isometric room.
 *
 * This view uses LVGL's core drawing engine for custom rendering of an
 * isometric grid, providing high performance and control over the visuals.
 * The user can pan the camera across the room using the hardware buttons.
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
    static constexpr int WALL_HEIGHT_UNITS = 5;     // Height of the walls in tile-height units
    static constexpr int TILE_WIDTH = 48;           // Isometric tile width in pixels
    static constexpr int TILE_HEIGHT = 24;          // Isometric tile height in pixels
    static constexpr int MOVE_SPEED = 12;           // Pixels to move the camera per button press

    // --- LVGL Objects ---
    lv_obj_t* room_canvas = nullptr; // A basic object that acts as our drawing surface

    // --- State ---
    lv_point_t view_offset = {0, 0}; // Camera offset from the origin
    
    // --- Drawing ---
    void grid_to_screen(int grid_x, int grid_y, const lv_point_t& origin, lv_point_t* p_out);

    // --- UI & Button Handling ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();

    // --- Button Actions ---
    void on_move(int dx, int dy);
    void on_back_to_menu();

    // --- Static Callbacks ---
    static void draw_event_cb(lv_event_t* e);
    static void handle_up_left_cb(void* user_data);
    static void handle_down_left_cb(void* user_data);
    static void handle_up_right_cb(void* user_data);
    static void handle_down_right_cb(void* user_data);
    static void handle_back_long_press_cb(void* user_data);
};

#endif // ROOM_VIEW_H