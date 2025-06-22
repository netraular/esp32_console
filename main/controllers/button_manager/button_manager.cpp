#include "controllers/button_manager/button_manager.h"
#include "config.h" // --> AÑADIDO: para obtener los tiempos de los botones
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
static TickType_t last_press_ticks[BUTTON_COUNT] = {0};
static bool is_long_press_active[BUTTON_COUNT] = {false}; // --> AÑADIDO: para suprimir TAP después de LONG_PRESS

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

static void execute_handler(button_id_t button_id, button_event_type_t event_type) {
    if (button_id >= BUTTON_COUNT || event_type >= BUTTON_EVENT_COUNT) return;
    if (button_handlers[button_id].view_handlers.handlers[event_type]) {
        button_handlers[button_id].view_handlers.handlers[event_type]();
    } else if (button_handlers[button_id].default_handlers.handlers[event_type]) {
        button_handlers[button_id].default_handlers.handlers[event_type]();
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

    // --- LOGIC FOR TAP, SINGLE/DOUBLE CLICK, and LONG PRESS SUPPRESSION ---

    if (raw_event == BUTTON_PRESS_DOWN) {
        // Reset long press flag and dispatch the event
        is_long_press_active[button_id] = false;
        dispatch_event(button_id, BUTTON_EVENT_PRESS_DOWN);
        return;
    }

    if (raw_event == BUTTON_PRESS_UP) {
        dispatch_event(button_id, BUTTON_EVENT_PRESS_UP);
        // Do NOT generate TAP or SINGLE_CLICK if a long press was just released.
        if (is_long_press_active[button_id]) {
            is_long_press_active[button_id] = false; // Reset for next press
            return;
        }

        TickType_t current_ticks = xTaskGetTickCount();
        bool is_potential_double_click = (current_ticks - last_press_ticks[button_id] < pdMS_TO_TICKS(BUTTON_DOUBLE_CLICK_MS));
        last_press_ticks[button_id] = current_ticks;

        if (!is_potential_double_click) {
             dispatch_event(button_id, BUTTON_EVENT_TAP);
        }
        return;
    }

    if (raw_event == BUTTON_SINGLE_CLICK) {
        // Suppress single click if it was part of a long press
        if (!is_long_press_active[button_id]) {
            dispatch_event(button_id, BUTTON_EVENT_SINGLE_CLICK);
        }
        return;
    }
    
    if (raw_event == BUTTON_DOUBLE_CLICK) {
        dispatch_event(button_id, BUTTON_EVENT_TAP); // Fire TAP for the second click
        dispatch_event(button_id, BUTTON_EVENT_DOUBLE_CLICK);
        return;
    }

    if (raw_event == BUTTON_LONG_PRESS_START || raw_event == BUTTON_LONG_PRESS_HOLD) {
        is_long_press_active[button_id] = true;
        // Dispatch the corresponding long press event
        auto it = event_map.find(raw_event);
        if (it != event_map.end()) {
            dispatch_event(button_id, it->second);
        }
        return;
    }
}

// --- Public API Functions ---

void button_manager_init() {
    ESP_LOGI(TAG, "Initializing button manager...");
    
    memset(button_handlers, 0, sizeof(button_handlers));
    memset(last_press_ticks, 0, sizeof(last_press_ticks));
    memset(is_long_press_active, 0, sizeof(is_long_press_active));

    s_input_event_queue = xQueueCreate(10, sizeof(button_event_data_t));
    assert(s_input_event_queue);
    s_input_processor_timer = lv_timer_create(process_queued_input_cb, 20, NULL);
    assert(s_input_processor_timer);

    // Use timings from config.h
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

        // Register for all raw events from the underlying library
        for (auto const& [raw_event, mapped_event] : event_map) {
            button_cb_user_data_t* cb_data = new button_cb_user_data_t{ (button_id_t)i, raw_event };
            iot_button_register_cb(buttons[i], raw_event, NULL, generic_button_event_cb, cb_data);
        }
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

void button_manager_register_handler(button_id_t button, button_event_type_t event, button_handler_t handler, bool is_view_handler) {
    if (button >= BUTTON_COUNT || event >= BUTTON_EVENT_COUNT) return;
    
    if (is_view_handler) {
        button_handlers[button].view_handlers.handlers[event] = handler;
    } else {
        button_handlers[button].default_handlers.handlers[event] = handler;
    }
}

void button_manager_unregister_view_handlers() {
    ESP_LOGI(TAG, "Unregistering all view-specific handlers.");
    for (int i = 0; i < BUTTON_COUNT; i++) {
        for (int j = 0; j < BUTTON_EVENT_COUNT; j++) {
            button_handlers[i].view_handlers.handlers[j] = nullptr;
        }
    }
    ESP_LOGI(TAG, "Restored default button handlers.");
}