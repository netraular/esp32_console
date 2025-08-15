#include "room_view.h"
#include "controllers/button_manager/button_manager.h"
#include "views/view_manager.h"
#include "config/app_config.h" // For SCREEN_WIDTH/HEIGHT
#include "esp_log.h"
#include "lvgl.h" // Main LVGL header - includes everything needed

static const char* TAG = "ROOM_VIEW";

RoomView::RoomView() {
    ESP_LOGI(TAG, "RoomView constructed");
}

RoomView::~RoomView() {
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
    // Create a basic object to serve as our "canvas". We will do custom drawing on it.
    room_canvas = lv_obj_create(parent);
    lv_obj_remove_style_all(room_canvas);
    lv_obj_set_size(room_canvas, LV_PCT(100), LV_PCT(100));
    lv_obj_center(room_canvas);

    // Add the core drawing event callback. This is where all rendering happens.
    lv_obj_add_event_cb(room_canvas, draw_event_cb, LV_EVENT_DRAW_POST_END, this);

    // Invalidate the object to trigger the first draw
    lv_obj_invalidate(room_canvas);
}

void RoomView::setup_button_handlers() {
    // --- Movement Handlers (on single tap) ---
    // Left Button: Move Top-Left
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_up_left_cb, true, this);
    // Right Button: Move Top-Right
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_up_right_cb, true, this);
    // Cancel Button: Move Bottom-Left
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_down_left_cb, true, this);
    // OK Button: Move Bottom-Right
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_down_right_cb, true, this);
    
    // --- Action Handler (triggers once after hold time) ---
    // Cancel Button (Long Press): Go back to menu
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
 * @brief The main drawing callback, called by LVGL when the canvas needs to be redrawn.
 * This function is compatible with the LVGL v9 drawing engine.
 */
void RoomView::draw_event_cb(lv_event_t* e) {
    RoomView* view = (RoomView*)lv_event_get_user_data(e);
    
    // In LVGL v9.x, we get the layer directly from the event
    lv_layer_t * layer = lv_event_get_layer(e);
    
    // Setup for lines
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_palette_main(LV_PALETTE_GREY);
    line_dsc.width = 1;

    // The origin is the screen position of grid point (0,0).
    // We center it on the screen and apply the camera offset.
    lv_point_t origin = {
        (lv_coord_t)(SCREEN_WIDTH / 2 - view->view_offset.x),
        (lv_coord_t)(WALL_HEIGHT_UNITS * TILE_HEIGHT - view->view_offset.y + 40) // Add top margin
    };

    // --- Draw Floor Grid ---
    for (int y = 0; y <= ROOM_DEPTH; ++y) {
        for (int x = 0; x <= ROOM_WIDTH; ++x) {
            lv_point_t p1;
            view->grid_to_screen(x, y, origin, &p1);

            if (x < ROOM_WIDTH) {
                lv_point_t p2;
                view->grid_to_screen(x + 1, y, origin, &p2);
                line_dsc.p1.x = p1.x;
                line_dsc.p1.y = p1.y;
                line_dsc.p2.x = p2.x;
                line_dsc.p2.y = p2.y;
                lv_draw_line(layer, &line_dsc);
            }
            if (y < ROOM_DEPTH) {
                lv_point_t p3;
                view->grid_to_screen(x, y + 1, origin, &p3);
                line_dsc.p1.x = p1.x;
                line_dsc.p1.y = p1.y;
                line_dsc.p2.x = p3.x;
                line_dsc.p2.y = p3.y;
                lv_draw_line(layer, &line_dsc);
            }
        }
    }
    
    // --- Draw Walls ---
    const int wall_pixel_height = WALL_HEIGHT_UNITS * TILE_HEIGHT;
    line_dsc.color = lv_palette_main(LV_PALETTE_BLUE_GREY);

    for (int i = 0; i <= ROOM_WIDTH; ++i) { // Back-right wall
        lv_point_t p_floor, p_top;
        view->grid_to_screen(i, 0, origin, &p_floor);
        p_top = {p_floor.x, (lv_coord_t)(p_floor.y - wall_pixel_height)};
        line_dsc.p1.x = p_floor.x;
        line_dsc.p1.y = p_floor.y;
        line_dsc.p2.x = p_top.x;
        line_dsc.p2.y = p_top.y;
        lv_draw_line(layer, &line_dsc); // Vertical post
    }
    for (int i = 0; i <= ROOM_DEPTH; ++i) { // Back-left wall
        lv_point_t p_floor, p_top;
        view->grid_to_screen(0, i, origin, &p_floor);
        p_top = {p_floor.x, (lv_coord_t)(p_floor.y - wall_pixel_height)};
        line_dsc.p1.x = p_floor.x;
        line_dsc.p1.y = p_floor.y;
        line_dsc.p2.x = p_top.x;
        line_dsc.p2.y = p_top.y;
        lv_draw_line(layer, &line_dsc); // Vertical post
    }

    // Top edges of walls
    lv_point_t corner_top_floor, corner_top_wall;
    view->grid_to_screen(0, 0, origin, &corner_top_floor);
    corner_top_wall = {corner_top_floor.x, (lv_coord_t)(corner_top_floor.y - wall_pixel_height)};

    lv_point_t right_top_floor, right_top_wall;
    view->grid_to_screen(ROOM_WIDTH, 0, origin, &right_top_floor);
    right_top_wall = {right_top_floor.x, (lv_coord_t)(right_top_floor.y - wall_pixel_height)};
    line_dsc.p1.x = corner_top_wall.x;
    line_dsc.p1.y = corner_top_wall.y;
    line_dsc.p2.x = right_top_wall.x;
    line_dsc.p2.y = right_top_wall.y;
    lv_draw_line(layer, &line_dsc);

    lv_point_t left_top_floor, left_top_wall;
    view->grid_to_screen(0, ROOM_DEPTH, origin, &left_top_floor);
    left_top_wall = {left_top_floor.x, (lv_coord_t)(left_top_floor.y - wall_pixel_height)};
    line_dsc.p1.x = corner_top_wall.x;
    line_dsc.p1.y = corner_top_wall.y;
    line_dsc.p2.x = left_top_wall.x;
    line_dsc.p2.y = left_top_wall.y;
    lv_draw_line(layer, &line_dsc);
}

// --- Button Actions ---

void RoomView::on_move(int dx, int dy) {
    view_offset.x += dx;
    view_offset.y += dy;
    lv_obj_invalidate(room_canvas); // Trigger a redraw
}

void RoomView::on_back_to_menu() {
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Static Callbacks ---

void RoomView::handle_up_left_cb(void* user_data) {
    static_cast<RoomView*>(user_data)->on_move(-MOVE_SPEED, -MOVE_SPEED);
}

void RoomView::handle_down_left_cb(void* user_data) {
    static_cast<RoomView*>(user_data)->on_move(-MOVE_SPEED, MOVE_SPEED);
}

void RoomView::handle_up_right_cb(void* user_data) {
    static_cast<RoomView*>(user_data)->on_move(MOVE_SPEED, -MOVE_SPEED);
}

void RoomView::handle_down_right_cb(void* user_data) {
    static_cast<RoomView*>(user_data)->on_move(MOVE_SPEED, MOVE_SPEED);
}

void RoomView::handle_back_long_press_cb(void* user_data) {
    static_cast<RoomView*>(user_data)->on_back_to_menu();
}