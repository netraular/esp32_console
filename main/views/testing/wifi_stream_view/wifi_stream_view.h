#ifndef WIFI_STREAM_VIEW_H
#define WIFI_STREAM_VIEW_H

#include "views/view.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/wifi_streamer/wifi_streamer.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "esp_log.h"
#include "lvgl.h"

class WifiStreamView : public View {
public:
    WifiStreamView();
    ~WifiStreamView() override;
    void create(lv_obj_t* parent) override;

private:
    lv_obj_t* status_label = nullptr;
    lv_obj_t* ip_label = nullptr;
    lv_obj_t* icon_label = nullptr;
    lv_timer_t* ui_update_timer = nullptr;

    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();
    void update_ui();

    void on_ok_press();
    void on_cancel_press();

    static void ok_press_cb(void* user_data);
    static void cancel_press_cb(void* user_data);
    static void ui_update_timer_cb(lv_timer_t* timer);
};

#endif // WIFI_STREAM_VIEW_H