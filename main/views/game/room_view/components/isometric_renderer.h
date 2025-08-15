#ifndef ISOMETRIC_RENDERER_H
#define ISOMETRIC_RENDERER_H

#include "lvgl.h"

class IsometricRenderer {
public:
    IsometricRenderer(int room_width, int room_depth, int wall_height_units);
    void draw_world(lv_layer_t* layer, const lv_point_t& camera_offset);
    void draw_cursor(lv_layer_t* layer, const lv_point_t& camera_offset, int grid_x, int grid_y);
    void draw_target_tile(lv_layer_t* layer, const lv_point_t& camera_offset, int grid_x, int grid_y);
    void draw_target_point(lv_layer_t* layer, const lv_point_t& camera_offset, int grid_x, int grid_y);
    void draw_placeholder_object(lv_layer_t* layer, const lv_point_t& camera_offset, int grid_x, int grid_y, int width, int depth, float height);

    static void grid_to_screen(int grid_x, int grid_y, const lv_point_t& origin, lv_point_t* p_out);
    static void grid_to_screen_center(int grid_x, int grid_y, const lv_point_t& origin, lv_point_t* p_out);

private:
    // --- Constants ---
    static constexpr int TILE_WIDTH = 64;
    static constexpr int TILE_HEIGHT = 32;

    const int ROOM_WIDTH;
    const int ROOM_DEPTH;
    const int WALL_HEIGHT_UNITS;
};

#endif // ISOMETRIC_RENDERER_H