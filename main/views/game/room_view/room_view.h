#ifndef ROOM_VIEW_H
#define ROOM_VIEW_H

#include "views/view.h"
#include "lvgl.h"
#include "components/isometric_renderer.h"
#include "components/room_camera.h"
#include "components/room_pet.h"
#include "components/room_mode_selector.h"
#include "components/room_object_manager.h"
#include "controllers/furniture_data_manager/furniture_data_manager.h"
#include <memory>
#include <vector>
#include <string>
#include <map>

class RoomView : public View {
public:
    RoomView();
    ~RoomView() override;
    void create(lv_obj_t* parent) override;

private:
    static constexpr int ROOM_WIDTH = 10;
    static constexpr int ROOM_DEPTH = 10;
    static constexpr int WALL_HEIGHT_UNITS = 4;

    // UI and component members
    lv_obj_t* room_canvas = nullptr;
    std::unique_ptr<IsometricRenderer> renderer;
    std::unique_ptr<RoomCamera> camera;
    std::unique_ptr<RoomPet> pet;
    std::unique_ptr<RoomModeSelector> mode_selector;
    std::unique_ptr<RoomObjectManager> object_manager;

    // State members
    int cursor_grid_x;
    int cursor_grid_y;
    RoomMode current_mode;
    lv_timer_t* update_timer = nullptr;
    
    // --- FIX: Map to store pre-loaded sprite descriptors for fast drawing ---
    std::map<std::string, const lv_image_dsc_t*> m_cached_sprites;
    
    // --- Setup Functions ---
    void setup_ui(lv_obj_t* parent);
    void setup_view_button_handlers();
    void load_all_furniture_sprites();
    void release_all_furniture_sprites();

    // --- Core Logic ---
    void set_mode(RoomMode new_mode);
    void open_mode_selector();
    void on_mode_selector_cancel();
    void periodic_update();

    // --- Action Handlers ---
    void on_grid_move(int dx, int dy);
    void on_back_to_menu();
    void on_place_object();

    // --- Static Callbacks ---
    static void draw_event_cb(lv_event_t* e);
    static void timer_cb(lv_timer_t* timer);
    static void handle_move_northeast_cb(void* user_data);
    static void handle_move_northwest_cb(void* user_data);
    static void handle_move_southeast_cb(void* user_data);
    static void handle_move_southwest_cb(void* user_data);
    static void handle_back_long_press_cb(void* user_data);
    static void handle_open_mode_selector_cb(void* user_data);
    static void handle_place_object_cb(void* user_data);
};

#endif // ROOM_VIEW_H