#include "controllers/button_manager/button_manager.h"
#include "config.h"
#include "esp_log.h"
#include <cassert>
#include <map>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

static const char *TAG = "BTN_MGR";

// --- State Variables ---
static button_handle_t buttons[BUTTON_COUNT];
static button_handlers_t button_handlers[BUTTON_COUNT];

static input_dispatch_mode_t s_current_dispatch_mode = INPUT_DISPATCH_MODE_QUEUED;
static QueueHandle_t s_input_event_queue = NULL;
static lv_timer_t* s_input_processor_timer = NULL;

// --- State for Advanced Click Logic ---
// Track long press state to suppress TAP and SINGLE_CLICK events correctly.
static bool is_long_press_active[BUTTON_COUNT] = {false};

// Maps the underlying library event to our custom, simplified enum
static const std::map<button_event_t, button_event_type_t> event_map = {
    {BUTTON_PRESS_DOWN,       BUTTON_EVENT_PRESS_DOWN},
    {BUTTON_PRESS_UP,         BUTTON_EVENT_PRESS_UP},
    {BUTTON_SINGLE_CLICK,     BUTTON_EVENT_SINGLE_CLICK},
    {BUTTON_DOUBLE_CLICK,     BUTTON_EVENT_DOUBLE_CLICK},
    {BUTTON_LONG_PRESS_START, BUTTON_EVENT_LONG_PRESS_START},
    {BUTTON_LONG_PRESS_HOLD,  BUTTON_EVENT_LONG_PRESS_HOLD},
};

// Data for the event queue
typedef struct {
    button_id_t button_id;
    button_event_type_t event_type;
} button_event_data_t;

// User data passed to the generic esp-iot-button callback
typedef struct {
    button_id_t button_id;
    button_event_t raw_event_type;
} button_cb_user_data_t;

// --- Private Functions ---

// MODIFIED: Now invokes the handler with its associated user_data
static void execute_handler(button_id_t button_id, button_event_type_t event_type) {
    if (button_id >= BUTTON_COUNT || event_type >= BUTTON_EVENT_COUNT) return;

    handler_entry_t* view_entry = &button_handlers[button_id].view_handlers.handlers[event_type];
    handler_entry_t* default_entry = &button_handlers[button_id].default_handlers.handlers[event_type];

    if (view_entry->handler) {
        view_entry->handler(view_entry->user_data);
    } else if (default_entry->handler) {
        default_entry->handler(default_entry->user_data);
    }
}

static void dispatch_event(button_id_t button_id, button_event_type_t event_type) {
    if (s_current_dispatch_mode == INPUT_DISPATCH_MODE_IMMEDIATE) {
        execute_handler(button_id, event_type);
    } else {
        if (s_input_event_queue) {
            button_event_data_t event_to_queue = {button_id, event_type};
            xQueueSend(s_input_event_queue, &event_to_queue, 0);
        }
    }
}

static void process_queued_input_cb(lv_timer_t *timer) {
    button_event_data_t event_data;
    if (xQueueReceive(s_input_event_queue, &event_data, 0) == pdPASS) {
        execute_handler(event_data.button_id, event_data.event_type);
    }
}

// Generic callback registered for all buttons and events.
static void generic_button_event_cb(void *arg, void *usr_data) {
    button_cb_user_data_t* cb_data = static_cast<button_cb_user_data_t*>(usr_data);
    button_id_t button_id = cb_data->button_id;
    button_event_t raw_event = cb_data->raw_event_type;

    // --- REFINED LOGIC FOR TAP, SINGLE/DOUBLE CLICK, and LONG PRESS SUPPRESSION ---

    if (raw_event == BUTTON_PRESS_DOWN) {
        is_long_press_active[button_id] = false;
        dispatch_event(button_id, BUTTON_EVENT_PRESS_DOWN);
        return;
    }

    if (raw_event == BUTTON_PRESS_UP) {
        // Always dispatch PRESS_UP.
        dispatch_event(button_id, BUTTON_EVENT_PRESS_UP);

        // Dispatch TAP unless a long press was just active.
        if (!is_long_press_active[button_id]) {
            dispatch_event(button_id, BUTTON_EVENT_TAP);
        } else {
            // Reset the flag now that the button is up.
            is_long_press_active[button_id] = false;
        }
        return;
    }

    if (raw_event == BUTTON_LONG_PRESS_START || raw_event == BUTTON_LONG_PRESS_HOLD) {
        is_long_press_active[button_id] = true;
        auto it = event_map.find(raw_event);
        if (it != event_map.end()) {
            dispatch_event(button_id, it->second);
        }
        return;
    }

    // For SINGLE and DOUBLE click, let the library tell us when they happen.
    // Suppress SINGLE_CLICK if a long press occurred.
    if (raw_event == BUTTON_SINGLE_CLICK) {
        if (!is_long_press_active[button_id]) {
            dispatch_event(button_id, BUTTON_EVENT_SINGLE_CLICK);
        }
        return;
    }

    if (raw_event == BUTTON_DOUBLE_CLICK) {
        dispatch_event(button_id, BUTTON_EVENT_DOUBLE_CLICK);
        return;
    }
}


// --- Public API Functions ---

void button_manager_init() {
    ESP_LOGI(TAG, "Initializing button manager...");
    
    // MODIFIED: memset works the same because nullptr is 0.
    memset(button_handlers, 0, sizeof(button_handlers));
    memset(is_long_press_active, 0, sizeof(is_long_press_active));

    s_input_event_queue = xQueueCreate(10, sizeof(button_event_data_t));
    assert(s_input_event_queue);
    s_input_processor_timer = lv_timer_create(process_queued_input_cb, 20, NULL);
    assert(s_input_processor_timer);

    button_config_t btn_config = {
        .long_press_time = BUTTON_LONG_PRESS_MS,
        .short_press_time = BUTTON_DOUBLE_CLICK_MS,
    };
    const button_gpio_config_t gpio_configs[BUTTON_COUNT] = {
        [BUTTON_LEFT]   = {BUTTON_LEFT_PIN, 0, false, false},
        [BUTTON_CANCEL] = {BUTTON_CANCEL_PIN, 0, false, false},
        [BUTTON_OK]     = {BUTTON_OK_PIN, 0, false, false},
        [BUTTON_RIGHT]  = {BUTTON_RIGHT_PIN, 0, false, false},
        [BUTTON_ON_OFF] = {BUTTON_ON_OFF_PIN, 0, false, false}
    };

    for (int i = 0; i < BUTTON_COUNT; i++) {
        ESP_ERROR_CHECK(iot_button_new_gpio_device(&btn_config, &gpio_configs[i], &buttons[i]));
        assert(buttons[i]);
        
        // Register for all raw events needed for our custom logic.
        // We now need to register for every event from the library.
        iot_button_register_cb(buttons[i], BUTTON_PRESS_DOWN, NULL, generic_button_event_cb, new button_cb_user_data_t{ (button_id_t)i, BUTTON_PRESS_DOWN });
        iot_button_register_cb(buttons[i], BUTTON_PRESS_UP, NULL, generic_button_event_cb, new button_cb_user_data_t{ (button_id_t)i, BUTTON_PRESS_UP });
        iot_button_register_cb(buttons[i], BUTTON_SINGLE_CLICK, NULL, generic_button_event_cb, new button_cb_user_data_t{ (button_id_t)i, BUTTON_SINGLE_CLICK });
        iot_button_register_cb(buttons[i], BUTTON_DOUBLE_CLICK, NULL, generic_button_event_cb, new button_cb_user_data_t{ (button_id_t)i, BUTTON_DOUBLE_CLICK });
        iot_button_register_cb(buttons[i], BUTTON_LONG_PRESS_START, NULL, generic_button_event_cb, new button_cb_user_data_t{ (button_id_t)i, BUTTON_LONG_PRESS_START });
        iot_button_register_cb(buttons[i], BUTTON_LONG_PRESS_HOLD, NULL, generic_button_event_cb, new button_cb_user_data_t{ (button_id_t)i, BUTTON_LONG_PRESS_HOLD });
    }

    ESP_LOGI(TAG, "Button manager initialized with: Double Click=%dms, Long Press=%dms", BUTTON_DOUBLE_CLICK_MS, BUTTON_LONG_PRESS_MS);
}

void button_manager_set_dispatch_mode(input_dispatch_mode_t mode) {
    if (s_current_dispatch_mode != mode) {
        s_current_dispatch_mode = mode;
        if(s_input_event_queue) {
            xQueueReset(s_input_event_queue);
        }
        ESP_LOGI(TAG, "Input dispatch mode changed to %s", (mode == INPUT_DISPATCH_MODE_QUEUED) ? "QUEUED" : "IMMEDIATE");
    }
}

// MODIFIED: Accepts and stores the user_data
void button_manager_register_handler(button_id_t button, button_event_type_t event, button_handler_t handler, bool is_view_handler, void* user_data) {
    if (button >= BUTTON_COUNT || event >= BUTTON_EVENT_COUNT) return;
    
    handler_entry_t* entry;
    if (is_view_handler) {
        entry = &button_handlers[button].view_handlers.handlers[event];
    } else {
        entry = &button_handlers[button].default_handlers.handlers[event];
    }

    entry->handler = handler;
    entry->user_data = user_data;
}

void button_manager_unregister_view_handlers() {
    ESP_LOGI(TAG, "Unregistering view-handlers and clearing event queue.");
    
    if (s_input_event_queue) {
        xQueueReset(s_input_event_queue);
    }
    
    // MODIFIED: Clears the complete structure (handler and user_data)
    for (int i = 0; i < BUTTON_COUNT; i++) {
        memset(&button_handlers[i].view_handlers, 0, sizeof(button_event_handlers_t));
    }
    ESP_LOGI(TAG, "Event queue cleared and default button handlers restored.");
}