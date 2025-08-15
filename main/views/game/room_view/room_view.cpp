#include "room_view.h"
#include "controllers/button_manager/button_manager.h"
#include "views/view_manager.h"
#include "config/app_config.h" // For SCREEN_WIDTH/HEIGHT
#include "esp_log.h"
#include "lvgl.h" // Main LVGL header - includes everything needed

static const char* TAG = "ROOM_VIEW";

RoomView::RoomView() 
    : selected_grid_x(ROOM_WIDTH / 2), 
      selected_grid_y(ROOM_DEPTH / 2),
      is_animating(false) {
    ESP_LOGI(TAG, "RoomView constructed, cursor at (%d, %d)", selected_grid_x, selected_grid_y);
    // Set the initial camera position without animation
    calculate_camera_offset(selected_grid_x, selected_grid_y, &camera_offset);
}

RoomView::~RoomView() {
    // Ensure any running animations are stopped to prevent callbacks on a deleted object
    lv_anim_delete(this, anim_exec_cb);
    ESP_LOGI(TAG, "RoomView destructed");
}

void RoomView::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);

    setup_ui(container);
    setup_button_handlers();
}

void RoomView::setup_ui(lv_obj_t* parent) {
    room_canvas = lv_obj_create(parent);
    lv_obj_remove_style_all(room_canvas);
    lv_obj_set_size(room_canvas, LV_PCT(100), LV_PCT(100));
    lv_obj_center(room_canvas);
    lv_obj_add_event_cb(room_canvas, draw_event_cb, LV_EVENT_DRAW_POST_END, this);
    lv_obj_invalidate(room_canvas);
}

void RoomView::setup_button_handlers() {
    // Button mapping for isometric diagonal movement
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_move_southwest_cb, true, this);   // Bottom-Left (-X)
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_move_northwest_cb, true, this);  // Top-Left (-Y)
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_move_northeast_cb, true, this);     // Top-Right (+X)
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_move_southeast_cb, true, this);  // Bottom-Right (+Y)

    // A long press on the Cancel button still exits the view
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_LONG_PRESS_START, handle_back_long_press_cb, true, this);
}

/**
 * @brief Converts isometric grid coordinates to screen coordinates.
 */
void RoomView::grid_to_screen(int grid_x, int grid_y, const lv_point_t& origin, lv_point_t* p_out) {
    p_out->x = origin.x + (grid_x - grid_y) * (TILE_WIDTH / 2);
    p_out->y = origin.y + (grid_x + grid_y) * (TILE_HEIGHT / 2);
}

/**
 * @brief Calculates the camera offset required to center the view on a specific grid tile.
 */
void RoomView::calculate_camera_offset(int grid_x, int grid_y, lv_point_t* out_offset) {
    // This calculates the screen-space position of the tile's center point, relative to a (0,0) origin
    out_offset->x = (grid_x - grid_y) * (TILE_WIDTH / 2);
    out_offset->y = (grid_x + grid_y) * (TILE_HEIGHT / 2) + (TILE_HEIGHT / 2);
}

void RoomView::draw_event_cb(lv_event_t* e) {
    RoomView* view = (RoomView*)lv_event_get_user_data(e);
    lv_layer_t * layer = lv_event_get_layer(e);
    
    // Line descriptors
    lv_draw_line_dsc_t grid_line_dsc, wall_line_dsc, highlight_dsc, floor_fill_dsc;
    
    // Grid lines (light gray)
    lv_draw_line_dsc_init(&grid_line_dsc);
    grid_line_dsc.color = lv_color_hex(0xC0C0C0); // Light gray for grid outlines
    grid_line_dsc.width = 1;

    // Wall lines (light gray outlines)
    lv_draw_line_dsc_init(&wall_line_dsc);
    wall_line_dsc.color = lv_color_hex(0xC0C0C0); // Light gray for wall outlines  
    wall_line_dsc.width = 1;

    // Floor fill lines (light brown, thicker to simulate fill)
    lv_draw_line_dsc_init(&floor_fill_dsc);
    floor_fill_dsc.color = lv_color_hex(0xD2B48C); // Light brown for floor fill
    floor_fill_dsc.width = 3; // Thicker lines to create fill effect

    // Highlight lines
    lv_draw_line_dsc_init(&highlight_dsc);
    highlight_dsc.color = lv_palette_main(LV_PALETTE_YELLOW);
    highlight_dsc.width = 2;

    // Rectangle descriptor for wall fills
    lv_draw_rect_dsc_t wall_rect_dsc;
    lv_draw_rect_dsc_init(&wall_rect_dsc);
    wall_rect_dsc.bg_color = lv_color_hex(0xFF8C00); // Dark orange for wall fill
    wall_rect_dsc.bg_opa = LV_OPA_COVER;
    wall_rect_dsc.border_width = 0; // No border for fill rectangles

    // The origin is calculated to center the view based on the current animated camera offset
    lv_point_t origin;
    origin.x = (SCREEN_WIDTH / 2) - view->camera_offset.x;
    origin.y = (SCREEN_HEIGHT / 2) - view->camera_offset.y;

    // Draw Floor Fill (multiple parallel lines following isometric shape)
    for (int y = 0; y < ROOM_DEPTH; ++y) {
        for (int x = 0; x < ROOM_WIDTH; ++x) {
            lv_point_t corners[4];
            view->grid_to_screen(x, y, origin, &corners[0]);         // Top
            view->grid_to_screen(x + 1, y, origin, &corners[1]);     // Right
            view->grid_to_screen(x + 1, y + 1, origin, &corners[2]); // Bottom
            view->grid_to_screen(x, y + 1, origin, &corners[3]);     // Left
            
            // Draw horizontal fill lines within the diamond shape
            int top_y = corners[0].y;
            int bottom_y = corners[2].y;
            
            // Fill with horizontal lines from top to bottom
            for (int fill_y = top_y; fill_y <= bottom_y; fill_y += 2) {
                // Calculate left and right boundaries for this y-level
                int line_left_x, line_right_x;
                
                if (fill_y <= (top_y + bottom_y) / 2) {
                    // Upper half of diamond
                    float ratio = (float)(fill_y - top_y) / ((top_y + bottom_y) / 2 - top_y);
                    line_left_x = corners[0].x + ratio * (corners[3].x - corners[0].x);
                    line_right_x = corners[0].x + ratio * (corners[1].x - corners[0].x);
                } else {
                    // Lower half of diamond
                    float ratio = (float)(fill_y - (top_y + bottom_y) / 2) / (bottom_y - (top_y + bottom_y) / 2);
                    line_left_x = corners[3].x + ratio * (corners[2].x - corners[3].x);
                    line_right_x = corners[1].x + ratio * (corners[2].x - corners[1].x);
                }
                
                floor_fill_dsc.p1 = {(lv_coord_t)line_left_x, (lv_coord_t)fill_y};
                floor_fill_dsc.p2 = {(lv_coord_t)line_right_x, (lv_coord_t)fill_y};
                lv_draw_line(layer, &floor_fill_dsc);
            }
        }
    }

    // Draw Floor Grid Outlines
    for (int y = 0; y <= ROOM_DEPTH; ++y) {
        for (int x = 0; x <= ROOM_WIDTH; ++x) {
            lv_point_t p1;
            view->grid_to_screen(x, y, origin, &p1);
            if (x < ROOM_WIDTH) {
                lv_point_t p2; view->grid_to_screen(x + 1, y, origin, &p2);
                grid_line_dsc.p1 = {p1.x, p1.y}; grid_line_dsc.p2 = {p2.x, p2.y};
                lv_draw_line(layer, &grid_line_dsc);
            }
            if (y < ROOM_DEPTH) {
                lv_point_t p3; view->grid_to_screen(x, y + 1, origin, &p3);
                grid_line_dsc.p1 = {p1.x, p1.y}; grid_line_dsc.p2 = {p3.x, p3.y};
                lv_draw_line(layer, &grid_line_dsc);
            }
        }
    }

    // Draw Floor Grid Outlines
    for (int y = 0; y <= ROOM_DEPTH; ++y) {
        for (int x = 0; x <= ROOM_WIDTH; ++x) {
            lv_point_t p1;
            view->grid_to_screen(x, y, origin, &p1);
            if (x < ROOM_WIDTH) {
                lv_point_t p2; view->grid_to_screen(x + 1, y, origin, &p2);
                grid_line_dsc.p1 = {p1.x, p1.y}; grid_line_dsc.p2 = {p2.x, p2.y};
                lv_draw_line(layer, &grid_line_dsc);
            }
            if (y < ROOM_DEPTH) {
                lv_point_t p3; view->grid_to_screen(x, y + 1, origin, &p3);
                grid_line_dsc.p1 = {p1.x, p1.y}; grid_line_dsc.p2 = {p3.x, p3.y};
                lv_draw_line(layer, &grid_line_dsc);
            }
        }
    }
    
    // Draw Highlighted Tile (always highlights the destination tile)
    lv_point_t highlight_corners[4];
    view->grid_to_screen(view->selected_grid_x, view->selected_grid_y, origin, &highlight_corners[0]);
    view->grid_to_screen(view->selected_grid_x + 1, view->selected_grid_y, origin, &highlight_corners[1]);
    view->grid_to_screen(view->selected_grid_x + 1, view->selected_grid_y + 1, origin, &highlight_corners[2]);
    view->grid_to_screen(view->selected_grid_x, view->selected_grid_y + 1, origin, &highlight_corners[3]);
    highlight_dsc.p1 = {highlight_corners[0].x, highlight_corners[0].y}; highlight_dsc.p2 = {highlight_corners[1].x, highlight_corners[1].y}; lv_draw_line(layer, &highlight_dsc);
    highlight_dsc.p1 = {highlight_corners[1].x, highlight_corners[1].y}; highlight_dsc.p2 = {highlight_corners[2].x, highlight_corners[2].y}; lv_draw_line(layer, &highlight_dsc);
    highlight_dsc.p1 = {highlight_corners[2].x, highlight_corners[2].y}; highlight_dsc.p2 = {highlight_corners[3].x, highlight_corners[3].y}; lv_draw_line(layer, &highlight_dsc);
    highlight_dsc.p1 = {highlight_corners[3].x, highlight_corners[3].y}; highlight_dsc.p2 = {highlight_corners[0].x, highlight_corners[0].y}; lv_draw_line(layer, &highlight_dsc);
    
    // Draw Highlighted Tile (always highlights the destination tile)
    lv_point_t p[4];
    view->grid_to_screen(view->selected_grid_x, view->selected_grid_y, origin, &p[0]);
    view->grid_to_screen(view->selected_grid_x + 1, view->selected_grid_y, origin, &p[1]);
    view->grid_to_screen(view->selected_grid_x + 1, view->selected_grid_y + 1, origin, &p[2]);
    view->grid_to_screen(view->selected_grid_x, view->selected_grid_y + 1, origin, &p[3]);
    highlight_dsc.p1 = {p[0].x, p[0].y}; highlight_dsc.p2 = {p[1].x, p[1].y}; lv_draw_line(layer, &highlight_dsc);
    highlight_dsc.p1 = {p[1].x, p[1].y}; highlight_dsc.p2 = {p[2].x, p[2].y}; lv_draw_line(layer, &highlight_dsc);
    highlight_dsc.p1 = {p[2].x, p[2].y}; highlight_dsc.p2 = {p[3].x, p[3].y}; lv_draw_line(layer, &highlight_dsc);
    highlight_dsc.p1 = {p[3].x, p[3].y}; highlight_dsc.p2 = {p[0].x, p[0].y}; lv_draw_line(layer, &highlight_dsc);
    
    // Draw Wall Fills (parallelograms following isometric perspective)
    const int wall_pixel_height = WALL_HEIGHT_UNITS * TILE_HEIGHT;
    
    // Back-right wall (each segment is a parallelogram)
    for (int i = 0; i < ROOM_WIDTH; ++i) {
        lv_point_t bottom_left, bottom_right, top_left, top_right;
        view->grid_to_screen(i, 0, origin, &bottom_left);
        view->grid_to_screen(i + 1, 0, origin, &bottom_right);
        top_left = {bottom_left.x, (lv_coord_t)(bottom_left.y - wall_pixel_height)};
        top_right = {bottom_right.x, (lv_coord_t)(bottom_right.y - wall_pixel_height)};
        
        // Fill the parallelogram wall segment with horizontal lines
        int min_y = LV_MIN(top_left.y, top_right.y);
        int max_y = LV_MAX(bottom_left.y, bottom_right.y);
        
        for (int fill_y = min_y; fill_y <= max_y; fill_y += 1) {
            // Calculate the left and right x positions for this y level
            // For the left edge: interpolate between top_left and bottom_left
            // For the right edge: interpolate between top_right and bottom_right
            float left_ratio = (float)(fill_y - top_left.y) / (bottom_left.y - top_left.y);
            float right_ratio = (float)(fill_y - top_right.y) / (bottom_right.y - top_right.y);
            
            // Clamp ratios to valid range
            left_ratio = LV_MAX(0.0f, LV_MIN(1.0f, left_ratio));
            right_ratio = LV_MAX(0.0f, LV_MIN(1.0f, right_ratio));
            
            int line_left_x = top_left.x + left_ratio * (bottom_left.x - top_left.x);
            int line_right_x = top_right.x + right_ratio * (bottom_right.x - top_right.x);
            
            // Draw the horizontal fill line
            lv_draw_line_dsc_t wall_fill_dsc;
            lv_draw_line_dsc_init(&wall_fill_dsc);
            wall_fill_dsc.color = lv_color_hex(0xFF8C00); // Dark orange
            wall_fill_dsc.width = 1;
            wall_fill_dsc.p1 = {(lv_coord_t)line_left_x, (lv_coord_t)fill_y};
            wall_fill_dsc.p2 = {(lv_coord_t)line_right_x, (lv_coord_t)fill_y};
            lv_draw_line(layer, &wall_fill_dsc);
        }
    }
    
    // Back-left wall (each segment is a parallelogram)
    for (int i = 0; i < ROOM_DEPTH; ++i) {
        lv_point_t bottom_left, bottom_right, top_left, top_right;
        view->grid_to_screen(0, i, origin, &bottom_left);
        view->grid_to_screen(0, i + 1, origin, &bottom_right);
        top_left = {bottom_left.x, (lv_coord_t)(bottom_left.y - wall_pixel_height)};
        top_right = {bottom_right.x, (lv_coord_t)(bottom_right.y - wall_pixel_height)};
        
        // Fill the parallelogram wall segment with horizontal lines
        int min_y = LV_MIN(top_left.y, top_right.y);
        int max_y = LV_MAX(bottom_left.y, bottom_right.y);
        
        for (int fill_y = min_y; fill_y <= max_y; fill_y += 1) {
            // Calculate the left and right x positions for this y level
            float left_ratio = (float)(fill_y - top_left.y) / (bottom_left.y - top_left.y);
            float right_ratio = (float)(fill_y - top_right.y) / (bottom_right.y - top_right.y);
            
            // Clamp ratios to valid range
            left_ratio = LV_MAX(0.0f, LV_MIN(1.0f, left_ratio));
            right_ratio = LV_MAX(0.0f, LV_MIN(1.0f, right_ratio));
            
            int line_left_x = top_left.x + left_ratio * (bottom_left.x - top_left.x);
            int line_right_x = top_right.x + right_ratio * (bottom_right.x - top_right.x);
            
            // Draw the horizontal fill line
            lv_draw_line_dsc_t wall_fill_dsc;
            lv_draw_line_dsc_init(&wall_fill_dsc);
            wall_fill_dsc.color = lv_color_hex(0xFF8C00); // Dark orange
            wall_fill_dsc.width = 1;
            wall_fill_dsc.p1 = {(lv_coord_t)line_left_x, (lv_coord_t)fill_y};
            wall_fill_dsc.p2 = {(lv_coord_t)line_right_x, (lv_coord_t)fill_y};
            lv_draw_line(layer, &wall_fill_dsc);
        }
    }
    
    // Draw Wall Outlines (light gray lines over the fills)
    for (int i = 0; i <= ROOM_WIDTH; ++i) { // Back-right wall outlines
        lv_point_t p_floor, p_top; 
        view->grid_to_screen(i, 0, origin, &p_floor);
        p_top = {p_floor.x, (lv_coord_t)(p_floor.y - wall_pixel_height)};
        wall_line_dsc.p1 = {p_floor.x, p_floor.y}; 
        wall_line_dsc.p2 = {p_top.x, p_top.y}; 
        lv_draw_line(layer, &wall_line_dsc);
    }
    
    for (int i = 0; i <= ROOM_DEPTH; ++i) { // Back-left wall outlines
        lv_point_t p_floor, p_top; 
        view->grid_to_screen(0, i, origin, &p_floor);
        p_top = {p_floor.x, (lv_coord_t)(p_floor.y - wall_pixel_height)};
        wall_line_dsc.p1 = {p_floor.x, p_floor.y}; 
        wall_line_dsc.p2 = {p_top.x, p_top.y}; 
        lv_draw_line(layer, &wall_line_dsc);
    }
    
    // Draw wall top edges
    lv_point_t c_floor, c_wall, r_floor, r_wall, l_floor, l_wall;
    view->grid_to_screen(0, 0, origin, &c_floor); c_wall = {c_floor.x, (lv_coord_t)(c_floor.y - wall_pixel_height)};
    view->grid_to_screen(ROOM_WIDTH, 0, origin, &r_floor); r_wall = {r_floor.x, (lv_coord_t)(r_floor.y - wall_pixel_height)};
    wall_line_dsc.p1 = {c_wall.x, c_wall.y}; wall_line_dsc.p2 = {r_wall.x, r_wall.y}; lv_draw_line(layer, &wall_line_dsc);
    view->grid_to_screen(0, ROOM_DEPTH, origin, &l_floor); l_wall = {l_floor.x, (lv_coord_t)(l_floor.y - wall_pixel_height)};
    wall_line_dsc.p1 = {c_wall.x, c_wall.y}; wall_line_dsc.p2 = {l_wall.x, l_wall.y}; lv_draw_line(layer, &wall_line_dsc);
}

void RoomView::on_grid_move(int dx, int dy) {
    if (is_animating) return; // Ignore input if already moving

    int new_x = selected_grid_x + dx;
    int new_y = selected_grid_y + dy;

    if (new_x >= 0 && new_x < ROOM_WIDTH && new_y >= 0 && new_y < ROOM_DEPTH) {
        is_animating = true;
        anim_start_offset = camera_offset;
        calculate_camera_offset(new_x, new_y, &anim_end_offset);
        
        selected_grid_x = new_x;
        selected_grid_y = new_y;

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, this);
        lv_anim_set_values(&a, 0, 256); // Use 256 for smoother mapping
        lv_anim_set_duration(&a, ANIMATION_DURATION_MS);
        lv_anim_set_exec_cb(&a, anim_exec_cb);
        lv_anim_set_ready_cb(&a, anim_ready_cb);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
    }
}

void RoomView::on_back_to_menu() {
    view_manager_load_view(VIEW_ID_MENU);
}

// Animation callback to interpolate the camera position
void RoomView::anim_exec_cb(void* var, int32_t v) {
    RoomView* view = static_cast<RoomView*>(var);
    view->camera_offset.x = lv_map(v, 0, 256, view->anim_start_offset.x, view->anim_end_offset.x);
    view->camera_offset.y = lv_map(v, 0, 256, view->anim_start_offset.y, view->anim_end_offset.y);
    lv_obj_invalidate(view->room_canvas);
}

// Animation callback for when the movement is complete
void RoomView::anim_ready_cb(lv_anim_t* a) {
    RoomView* view = static_cast<RoomView*>(a->var);
    view->is_animating = false;
}

// --- Static Button Callbacks ---
void RoomView::handle_move_northeast_cb(void* user_data) { // Mapped to OK button, moves +X
    static_cast<RoomView*>(user_data)->on_grid_move(1, 0);
}

void RoomView::handle_move_northwest_cb(void* user_data) { // Mapped to RIGHT button, moves -Y
    static_cast<RoomView*>(user_data)->on_grid_move(0, -1);
}

void RoomView::handle_move_southeast_cb(void* user_data) { // Mapped to CANCEL button, moves +Y
    static_cast<RoomView*>(user_data)->on_grid_move(0, 1);
}

void RoomView::handle_move_southwest_cb(void* user_data) { // Mapped to LEFT button, moves -X
    static_cast<RoomView*>(user_data)->on_grid_move(-1, 0);
}

void RoomView::handle_back_long_press_cb(void* user_data) {
    static_cast<RoomView*>(user_data)->on_back_to_menu();
}