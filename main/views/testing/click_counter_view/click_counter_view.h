#ifndef CLICK_COUNTER_VIEW_H
#define CLICK_COUNTER_VIEW_H

#include "views/view.h"
#include "controllers/data_manager/data_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "esp_log.h"

// Declare the LVGL image descriptor from the C file.
extern "C" const lv_image_dsc_t coin_pile;

/**
 * @brief Represents the Click Counter view.
 *
 * This view allows the user to increment a counter. The count is saved
 * persistently to NVS. Every 10 clicks, a sound is played and an
 * animation is shown.
 */
class ClickCounterView : public View {
public:
    /**
     * @brief Constructs the ClickCounterView.
     * It loads the previous count from NVS upon creation.
     */
    ClickCounterView();

    /**
     * @brief Destroys the ClickCounterView.
     * Ensures that any running animations or sounds are stopped.
     */
    ~ClickCounterView() override;

    /**
     * @brief Creates the UI elements for the view.
     * @param parent The parent LVGL object to create the view on.
     */
    void create(lv_obj_t* parent) override;

private:
    // --- Constants ---
    static const char* CLICK_COUNT_KEY;

    // --- UI Widgets ---
    lv_obj_t* count_label = nullptr;
    lv_obj_t* coin_image = nullptr;

    // --- State ---
    uint32_t click_count = 0;

    // --- UI Setup & Logic ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();
    void update_counter_label();
    void start_fade_out_animation();

    // --- Instance Methods for Button Actions ---
    void on_ok_press();
    void on_cancel_press();

    // --- Static Callbacks for Button Manager (Bridge to instance methods) ---
    static void ok_press_cb(void* user_data);
    static void cancel_press_cb(void* user_data);

    // --- Static Callbacks for LVGL Animations ---
    static void anim_set_opacity_cb(void* var, int32_t v);
    static void anim_ready_cb(lv_anim_t* anim);
};

#endif // CLICK_COUNTER_VIEW_H