#ifndef MULTI_CLICK_TEST_VIEW_H
#define MULTI_CLICK_TEST_VIEW_H

#include "views/view.h"
#include "views/view_manager.h"
#include "controllers/button_manager/button_manager.h" // For BUTTON_COUNT
#include "lvgl.h"

/**
 * @brief View to demonstrate and test all available button events.
 *
 * This class encapsulates the UI and logic for a screen that shows the last
 * detected event for each physical button, including tap, single/double click,
 * and long press states.
 */
class MultiClickTestView : public View {
public:
    MultiClickTestView();
    ~MultiClickTestView() override;

    void create(lv_obj_t* parent) override;

private:
    // --- UI and State Members ---
    lv_obj_t* event_labels[BUTTON_COUNT] = {nullptr};
    static const char* button_names[BUTTON_COUNT];

    // --- Private Methods ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();
    void handle_event(button_id_t button, const char* event_name);

    // --- Instance Methods for Button Actions ---
    void on_event(button_id_t button, button_event_type_t event_type);
    void on_cancel_press();

    // --- Static Callback for Button Manager ---
    // A single static callback can handle all events by passing a context struct.
    // However, for clarity, we use one per event type.
    static void generic_event_cb(void* user_data);
    static void cancel_press_cb(void* user_data);

    // Helper struct for passing context to the generic callback
    struct event_context {
        MultiClickTestView* view_instance;
        button_id_t button_id;
        button_event_type_t event_type;
    };
    // We need to store these contexts statically as they are passed by pointer
    event_context contexts[BUTTON_COUNT][BUTTON_EVENT_COUNT];
};

#endif // MULTI_CLICK_TEST_VIEW_H