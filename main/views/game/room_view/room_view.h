#ifndef ROOM_VIEW_H
#define ROOM_VIEW_H

#include "views/view.h"
#include "lvgl.h"
#include "components/isometric_renderer.h"
#include "components/room_camera.h"
#include "components/room_pet.h"
#include <memory>

class RoomView : public View {
public:
    RoomView();
    ~RoomView() override;
    void create(lv_obj_t* parent) override;

private:
    static constexpr int ROOM_WIDTH = 10;
    static constexpr int ROOM_DEPTH = 10;
    static constexpr int WALL_HEIGHT_UNITS = 4;

    enum class ControlMode { CURSOR, PET };

    lv_obj_t* room_canvas = nullptr;

    std::unique_ptr<IsometricRenderer> renderer;
    std::unique_ptr<RoomCamera> camera;
    std::unique_ptr<RoomPet> pet;

    int cursor_grid_x;
    int cursor_grid_y;
    int last_pet_grid_x;
    int last_pet_grid_y;
    ControlMode control_mode;
    
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();

    void on_grid_move(int dx, int dy);
    void on_back_to_menu();
    void toggle_pet_mode();
    void periodic_update();

    static void draw_event_cb(lv_event_t* e);
    static void timer_cb(lv_timer_t* timer);
    static void handle_move_northeast_cb(void* user_data);
    static void handle_move_northwest_cb(void* user_data);
    static void handle_move_southeast_cb(void* user_data);
    static void handle_move_southwest_cb(void* user_data);
    static void handle_back_long_press_cb(void* user_data);
    static void handle_pet_mode_toggle_cb(void* user_data);
};

#endif // ROOM_VIEW_H