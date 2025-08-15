#include "room_camera.h"

static constexpr int TILE_WIDTH = 64;
static constexpr int TILE_HEIGHT = 32;
static constexpr int CAMERA_ANIMATION_DURATION_MS = 250;

RoomCamera::RoomCamera(lv_obj_t* view_canvas) : canvas(view_canvas), current_offset({0, 0}), animating(false) {}

RoomCamera::~RoomCamera() {
    lv_anim_delete(this, anim_exec_cb);
}

bool RoomCamera::is_animating() const {
    return animating;
}

const lv_point_t& RoomCamera::get_offset() const {
    return current_offset;
}

void RoomCamera::move_to(int grid_x, int grid_y, bool animate) {
    if (animating && animate) return;

    if (animate) {
        animating = true;
        anim_start_offset = current_offset;
        calculate_target_offset(grid_x, grid_y, &anim_end_offset);

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, this);
        lv_anim_set_values(&a, 0, 256);
        lv_anim_set_duration(&a, CAMERA_ANIMATION_DURATION_MS);
        lv_anim_set_exec_cb(&a, anim_exec_cb);
        lv_anim_set_ready_cb(&a, anim_ready_cb);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
    } else {
        calculate_target_offset(grid_x, grid_y, &current_offset);
        lv_obj_invalidate(canvas);
    }
}

void RoomCamera::calculate_target_offset(int grid_x, int grid_y, lv_point_t* out_offset) {
    out_offset->x = (grid_x - grid_y) * (TILE_WIDTH / 2);
    out_offset->y = (grid_x + grid_y) * (TILE_HEIGHT / 2) + (TILE_HEIGHT / 2);
}

void RoomCamera::anim_exec_cb(void* var, int32_t v) {
    RoomCamera* cam = static_cast<RoomCamera*>(var);
    cam->current_offset.x = lv_map(v, 0, 256, cam->anim_start_offset.x, cam->anim_end_offset.x);
    cam->current_offset.y = lv_map(v, 0, 256, cam->anim_start_offset.y, cam->anim_end_offset.y);
    lv_obj_invalidate(cam->canvas);
}

void RoomCamera::anim_ready_cb(lv_anim_t* a) {
    RoomCamera* cam = static_cast<RoomCamera*>(a->var);
    cam->animating = false;
}