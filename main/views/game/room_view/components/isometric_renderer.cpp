#include "isometric_renderer.h"
#include "config/app_config.h" // For SCREEN_WIDTH/HEIGHT
#include "esp_log.h"

static const char* TAG = "IsometricRenderer";

IsometricRenderer::IsometricRenderer(int room_width, int room_depth, int wall_height_units)
    : ROOM_WIDTH(room_width), ROOM_DEPTH(room_depth), WALL_HEIGHT_UNITS(wall_height_units) {}

void IsometricRenderer::grid_to_screen(int grid_x, int grid_y, const lv_point_t& origin, lv_point_t* p_out) {
    p_out->x = origin.x + (grid_x - grid_y) * (TILE_WIDTH / 2);
    p_out->y = origin.y + (grid_x + grid_y) * (TILE_HEIGHT / 2);
}

void IsometricRenderer::grid_to_screen_center(int grid_x, int grid_y, const lv_point_t& origin, lv_point_t* p_out) {
    // First, get the top corner of the tile
    grid_to_screen(grid_x, grid_y, origin, p_out);
    // The visual center is halfway down the tile's height from the top corner, and at the same x.
    p_out->y += (TILE_HEIGHT / 2);
}

void IsometricRenderer::draw_world(lv_layer_t* layer, const lv_point_t& camera_offset) {
    lv_draw_line_dsc_t grid_line_dsc, wall_line_dsc;
    lv_draw_triangle_dsc_t floor_fill_dsc, wall_fill_dsc;

    lv_draw_line_dsc_init(&grid_line_dsc);
    grid_line_dsc.color = lv_color_hex(0xC0C0C0);
    grid_line_dsc.width = 1;
    lv_draw_line_dsc_init(&wall_line_dsc);
    wall_line_dsc.color = lv_color_hex(0xC0C0C0);
    wall_line_dsc.width = 1;
    lv_draw_triangle_dsc_init(&floor_fill_dsc);
    floor_fill_dsc.opa = LV_OPA_COVER;
    floor_fill_dsc.color = lv_color_hex(0xD2B48C);
    lv_draw_triangle_dsc_init(&wall_fill_dsc);
    wall_fill_dsc.opa = LV_OPA_COVER;
    wall_fill_dsc.color = lv_color_hex(0xFF8C00);

    lv_point_t origin;
    origin.x = (SCREEN_WIDTH / 2) - camera_offset.x;
    origin.y = (SCREEN_HEIGHT / 2) - camera_offset.y;

    for (int y = 0; y < ROOM_DEPTH; ++y) {
        for (int x = 0; x < ROOM_WIDTH; ++x) {
            lv_point_t c[4];
            grid_to_screen(x, y, origin, &c[0]); grid_to_screen(x + 1, y, origin, &c[1]);
            grid_to_screen(x + 1, y + 1, origin, &c[2]); grid_to_screen(x, y + 1, origin, &c[3]);
            floor_fill_dsc.p[0] = { c[0].x, c[0].y }; floor_fill_dsc.p[1] = { c[1].x, c[1].y }; floor_fill_dsc.p[2] = { c[2].x, c[2].y };
            lv_draw_triangle(layer, &floor_fill_dsc);
            floor_fill_dsc.p[1] = { c[2].x, c[2].y }; floor_fill_dsc.p[2] = { c[3].x, c[3].y };
            lv_draw_triangle(layer, &floor_fill_dsc);
        }
    }
    for (int y = 0; y <= ROOM_DEPTH; ++y) {
        for (int x = 0; x <= ROOM_WIDTH; ++x) {
            lv_point_t p1; grid_to_screen(x, y, origin, &p1);
            if (x < ROOM_WIDTH) { lv_point_t p2; grid_to_screen(x + 1, y, origin, &p2); grid_line_dsc.p1 = {p1.x, p1.y}; grid_line_dsc.p2 = {p2.x, p2.y}; lv_draw_line(layer, &grid_line_dsc); }
            if (y < ROOM_DEPTH) { lv_point_t p3; grid_to_screen(x, y + 1, origin, &p3); grid_line_dsc.p1 = {p1.x, p1.y}; grid_line_dsc.p2 = {p3.x, p3.y}; lv_draw_line(layer, &grid_line_dsc); }
        }
    }
    const int wall_pixel_height = WALL_HEIGHT_UNITS * TILE_HEIGHT;
    for (int i = 0; i < ROOM_WIDTH; ++i) {
        lv_point_t p[4]; grid_to_screen(i, 0, origin, &p[0]); grid_to_screen(i + 1, 0, origin, &p[1]);
        p[2] = {p[1].x, (lv_coord_t)(p[1].y - wall_pixel_height)}; p[3] = {p[0].x, (lv_coord_t)(p[0].y - wall_pixel_height)};
        wall_fill_dsc.p[0] = {p[0].x, p[0].y}; wall_fill_dsc.p[1] = {p[1].x, p[1].y}; wall_fill_dsc.p[2] = {p[2].x, p[2].y}; lv_draw_triangle(layer, &wall_fill_dsc);
        wall_fill_dsc.p[1] = {p[2].x, p[2].y}; wall_fill_dsc.p[2] = {p[3].x, p[3].y}; lv_draw_triangle(layer, &wall_fill_dsc);
    }
    for (int i = 0; i < ROOM_DEPTH; ++i) {
        lv_point_t p[4]; grid_to_screen(0, i, origin, &p[0]); grid_to_screen(0, i + 1, origin, &p[1]);
        p[2] = {p[1].x, (lv_coord_t)(p[1].y - wall_pixel_height)}; p[3] = {p[0].x, (lv_coord_t)(p[0].y - wall_pixel_height)};
        wall_fill_dsc.p[0] = {p[0].x, p[0].y}; wall_fill_dsc.p[1] = {p[1].x, p[1].y}; wall_fill_dsc.p[2] = {p[2].x, p[2].y}; lv_draw_triangle(layer, &wall_fill_dsc);
        wall_fill_dsc.p[1] = {p[2].x, p[2].y}; wall_fill_dsc.p[2] = {p[3].x, p[3].y}; lv_draw_triangle(layer, &wall_fill_dsc);
    }
    for (int i = 0; i <= ROOM_WIDTH; ++i) { lv_point_t pf, pt; grid_to_screen(i, 0, origin, &pf); pt = {pf.x, (lv_coord_t)(pf.y - wall_pixel_height)}; wall_line_dsc.p1 = {pf.x, pf.y}; wall_line_dsc.p2 = {pt.x, pt.y}; lv_draw_line(layer, &wall_line_dsc); }
    for (int i = 0; i <= ROOM_DEPTH; ++i) { lv_point_t pf, pt; grid_to_screen(0, i, origin, &pf); pt = {pf.x, (lv_coord_t)(pf.y - wall_pixel_height)}; wall_line_dsc.p1 = {pf.x, pf.y}; wall_line_dsc.p2 = {pt.x, pt.y}; lv_draw_line(layer, &wall_line_dsc); }
    lv_point_t c_f, c_w, r_f, r_w, l_f, l_w;
    grid_to_screen(0, 0, origin, &c_f); c_w = {c_f.x, (lv_coord_t)(c_f.y - wall_pixel_height)};
    grid_to_screen(ROOM_WIDTH, 0, origin, &r_f); r_w = {r_f.x, (lv_coord_t)(r_f.y - wall_pixel_height)};
    wall_line_dsc.p1 = {c_w.x, c_w.y}; wall_line_dsc.p2 = {r_w.x, r_w.y}; lv_draw_line(layer, &wall_line_dsc);
    grid_to_screen(0, ROOM_DEPTH, origin, &l_f); l_w = {l_f.x, (lv_coord_t)(l_f.y - wall_pixel_height)};
    wall_line_dsc.p1 = {c_w.x, c_w.y}; wall_line_dsc.p2 = {l_w.x, l_w.y}; lv_draw_line(layer, &wall_line_dsc);
}

void IsometricRenderer::draw_cursor(lv_layer_t* layer, const lv_point_t& camera_offset, int grid_x, int grid_y) {
    lv_draw_line_dsc_t highlight_dsc;
    lv_draw_line_dsc_init(&highlight_dsc);
    highlight_dsc.color = lv_palette_main(LV_PALETTE_YELLOW);
    highlight_dsc.width = 2;

    lv_point_t origin;
    origin.x = (SCREEN_WIDTH / 2) - camera_offset.x;
    origin.y = (SCREEN_HEIGHT / 2) - camera_offset.y;

    lv_point_t p[4];
    grid_to_screen(grid_x, grid_y, origin, &p[0]);
    grid_to_screen(grid_x + 1, grid_y, origin, &p[1]);
    grid_to_screen(grid_x + 1, grid_y + 1, origin, &p[2]);
    grid_to_screen(grid_x, grid_y + 1, origin, &p[3]);
    highlight_dsc.p1 = {p[0].x, p[0].y}; highlight_dsc.p2 = {p[1].x, p[1].y}; lv_draw_line(layer, &highlight_dsc);
    highlight_dsc.p1 = {p[1].x, p[1].y}; highlight_dsc.p2 = {p[2].x, p[2].y}; lv_draw_line(layer, &highlight_dsc);
    highlight_dsc.p1 = {p[2].x, p[2].y}; highlight_dsc.p2 = {p[3].x, p[3].y}; lv_draw_line(layer, &highlight_dsc);
    highlight_dsc.p1 = {p[3].x, p[3].y}; highlight_dsc.p2 = {p[0].x, p[0].y}; lv_draw_line(layer, &highlight_dsc);
}

void IsometricRenderer::draw_sprite(lv_layer_t* layer, const lv_point_t& camera_offset, const PlacedFurniture& furni,
                                    const lv_image_dsc_t* sprite_dsc, int offset_x, int offset_y, bool flip_h) {
    if (!sprite_dsc) return;

    lv_point_t origin;
    origin.x = (SCREEN_WIDTH / 2) - camera_offset.x;
    origin.y = (SCREEN_HEIGHT / 2) - camera_offset.y;

    // Calculate the top-left corner of the tile the furniture sits on.
    lv_point_t tile_origin;
    grid_to_screen(furni.grid_x, furni.grid_y, origin, &tile_origin);
    
    // Adjust for Z-height (elevation)
    tile_origin.y -= (int)(furni.grid_z * TILE_HEIGHT);

    // Calculate final screen coordinates including sprite offsets
    lv_point_t final_pos;
    final_pos.x = tile_origin.x - offset_x;
    final_pos.y = tile_origin.y - offset_y;

    // --- FIX START ---
    // In LVGL v9, the drawing area is passed as a separate parameter, not as part of the descriptor.
    lv_area_t draw_area;
    draw_area.x1 = final_pos.x;
    draw_area.y1 = final_pos.y;
    draw_area.x2 = final_pos.x + sprite_dsc->header.w - 1;
    draw_area.y2 = final_pos.y + sprite_dsc->header.h - 1;

    lv_draw_image_dsc_t img_dsc;
    lv_draw_image_dsc_init(&img_dsc);
    img_dsc.src = sprite_dsc;
    
    // NOTE: Direct horizontal flipping is not supported by lv_draw_image.
    // This would require more complex matrix transformations. For now, we assume
    // flip_h is false for the assets we need to render.
    if (flip_h) {
        ESP_LOGW(TAG, "Horizontal flipping is not yet implemented in the renderer.");
    }
    
    // The lv_draw_image function now requires the layer, descriptor, and drawing area.
    lv_draw_image(layer, &img_dsc, &draw_area);
    // --- FIX END ---
}


void IsometricRenderer::draw_target_tile(lv_layer_t* layer, const lv_point_t& camera_offset, int grid_x, int grid_y) {
    lv_draw_line_dsc_t highlight_dsc;
    lv_draw_line_dsc_init(&highlight_dsc);
    highlight_dsc.color = lv_palette_main(LV_PALETTE_LIGHT_BLUE);
    highlight_dsc.width = 2;

    lv_point_t origin;
    origin.x = (SCREEN_WIDTH / 2) - camera_offset.x;
    origin.y = (SCREEN_HEIGHT / 2) - camera_offset.y;

    lv_point_t p[4];
    grid_to_screen(grid_x, grid_y, origin, &p[0]);
    grid_to_screen(grid_x + 1, grid_y, origin, &p[1]);
    grid_to_screen(grid_x + 1, grid_y + 1, origin, &p[2]);
    grid_to_screen(grid_x, grid_y + 1, origin, &p[3]);
    highlight_dsc.p1 = {p[0].x, p[0].y}; highlight_dsc.p2 = {p[1].x, p[1].y}; lv_draw_line(layer, &highlight_dsc);
    highlight_dsc.p1 = {p[1].x, p[1].y}; highlight_dsc.p2 = {p[2].x, p[2].y}; lv_draw_line(layer, &highlight_dsc);
    highlight_dsc.p1 = {p[2].x, p[2].y}; highlight_dsc.p2 = {p[3].x, p[3].y}; lv_draw_line(layer, &highlight_dsc);
    highlight_dsc.p1 = {p[3].x, p[3].y}; highlight_dsc.p2 = {p[0].x, p[0].y}; lv_draw_line(layer, &highlight_dsc);
}

void IsometricRenderer::draw_target_point(lv_layer_t* layer, const lv_point_t& camera_offset, int grid_x, int grid_y) {
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = lv_palette_main(LV_PALETTE_RED);
    arc_dsc.width = 2;

    lv_point_t origin;
    origin.x = (SCREEN_WIDTH / 2) - camera_offset.x;
    origin.y = (SCREEN_HEIGHT / 2) - camera_offset.y;

    lv_point_t center;
    grid_to_screen_center(grid_x, grid_y, origin, &center);
    
    arc_dsc.center.x = center.x;
    arc_dsc.center.y = center.y;
    arc_dsc.radius = 3;
    arc_dsc.start_angle = 0;
    arc_dsc.end_angle = 360;

    lv_draw_arc(layer, &arc_dsc);
}