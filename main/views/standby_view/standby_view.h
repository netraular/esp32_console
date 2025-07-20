#ifndef STANDBY_VIEW_H
#define STANDBY_VIEW_H

#include "../view.h"

class StandbyView : public View {
public:
    void create(lv_obj_t* parent) override;
    ~StandbyView() override;

private:
    // UI Widgets
    lv_obj_t* center_time_label = nullptr;
    lv_obj_t* center_date_label = nullptr;
    lv_obj_t* loading_label = nullptr;

    // Timers
    lv_timer_t* update_timer = nullptr;
    lv_timer_t* volume_up_timer = nullptr;
    lv_timer_t* volume_down_timer = nullptr;

    // State
    bool is_time_synced = false;

    // Shutdown Popup
    lv_obj_t* shutdown_popup_container = nullptr;
    lv_group_t* shutdown_popup_group = nullptr;

    // Styles
    lv_style_t style_popup_focused;
    lv_style_t style_popup_normal;
    bool popup_styles_initialized = false;

    // Private methods
    void update_center_clock_task();
    void register_main_view_handlers();
    void create_shutdown_popup();
    void destroy_shutdown_popup();
    void init_popup_styles();
    void reset_popup_styles();
};

#endif // STANDBY_VIEW_H