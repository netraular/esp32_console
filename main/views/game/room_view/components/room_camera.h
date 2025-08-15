#ifndef ROOM_CAMERA_H
#define ROOM_CAMERA_H

#include "lvgl.h"

class RoomCamera {
public:
    RoomCamera(lv_obj_t* view_canvas);
    ~RoomCamera();

    void move_to(int grid_x, int grid_y, bool animate);
    bool is_animating() const;
    const lv_point_t& get_offset() const;

private:
    static void calculate_target_offset(int grid_x, int grid_y, lv_point_t* out_offset);
    static void anim_exec_cb(void* var, int32_t v);
    static void anim_ready_cb(lv_anim_t* a);

    lv_obj_t* canvas;
    lv_point_t current_offset;
    lv_point_t anim_start_offset;
    lv_point_t anim_end_offset;
    bool animating;
};

#endif // ROOM_CAMERA_H