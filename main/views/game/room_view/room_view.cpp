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
    lv_draw_line_dsc_t grid_line_dsc, wall_line_dsc, highlight_dsc;
    lv_draw_line_dsc_init(&grid_line_dsc); grid_line_dsc.color = lv_palette_main(LV_PALETTE_GREY); grid_line_dsc.width = 1;
    lv_draw_line_dsc_init(&wall_line_dsc); wall_line_dsc.color = lv_palette_main(LV_PALETTE_BLUE_GREY); wall_line_dsc.width = 1;
    lv_draw_line_dsc_init(&highlight_dsc); highlight_dsc.color = lv_palette_main(LV_PALETTE_YELLOW); highlight_dsc.width = 2;

    // The origin is calculated to center the view based on the current animated camera offset
    lv_point_t origin;
    origin.x = (SCREEN_WIDTH / 2) - view->camera_offset.x;
    origin.y = (SCREEN_HEIGHT / 2) - view->camera_offset.y;

    // Draw Floor Grid
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
    lv_point_t p[4];
    view->grid_to_screen(view->selected_grid_x, view->selected_grid_y, origin, &p[0]);
    view->grid_to_screen(view->selected_grid_x + 1, view->selected_grid_y, origin, &p[1]);
    view->grid_to_screen(view->selected_grid_x + 1, view->selected_grid_y + 1, origin, &p[2]);
    view->grid_to_screen(view->selected_grid_x, view->selected_grid_y + 1, origin, &p[3]);
    highlight_dsc.p1 = {p[0].x, p[0].y}; highlight_dsc.p2 = {p[1].x, p[1].y}; lv_draw_line(layer, &highlight_dsc);
    highlight_dsc.p1 = {p[1].x, p[1].y}; highlight_dsc.p2 = {p[2].x, p[2].y}; lv_draw_line(layer, &highlight_dsc);
    highlight_dsc.p1 = {p[2].x, p[2].y}; highlight_dsc.p2 = {p[3].x, p[3].y}; lv_draw_line(layer, &highlight_dsc);
    highlight_dsc.p1 = {p[3].x, p[3].y}; highlight_dsc.p2 = {p[0].x, p[0].y}; lv_draw_line(layer, &highlight_dsc);
    
    // Draw Walls (code unchanged, but position will move with camera)
    const int wall_pixel_height = WALL_HEIGHT_UNITS * TILE_HEIGHT;
    for (int i = 0; i <= ROOM_WIDTH; ++i) { // Back-right wall
        lv_point_t p_floor, p_top; view->grid_to_screen(i, 0, origin, &p_floor);
        p_top = {p_floor.x, (lv_coord_t)(p_floor.y - wall_pixel_height)};
        wall_line_dsc.p1 = {p_floor.x, p_floor.y}; wall_line_dsc.p2 = {p_top.x, p_top.y}; lv_draw_line(layer, &wall_line_dsc);
    }
    for (int i = 0; i <= ROOM_DEPTH; ++i) { // Back-left wall
        lv_point_t p_floor, p_top; view->grid_to_screen(0, i, origin, &p_floor);
        p_top = {p_floor.x, (lv_coord_t)(p_floor.y - wall_pixel_height)};
        wall_line_dsc.p1 = {p_floor.x, p_floor.y}; wall_line_dsc.p2 = {p_top.x, p_top.y}; lv_draw_line(layer, &wall_line_dsc);
    }
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