#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"
#include <cassert>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static const char *TAG = "BTN_MGR";

// --- State Variables ---
static button_handle_t buttons[BUTTON_COUNT];
static button_handlers_t button_handlers[BUTTON_COUNT] = { {nullptr, nullptr} };

// --- New state for dispatch mode ---
static input_dispatch_mode_t s_current_dispatch_mode = INPUT_DISPATCH_MODE_QUEUED;
static QueueHandle_t s_input_event_queue = NULL;
static lv_timer_t* s_input_processor_timer = NULL;

// Default handlers
static void default_button_left_handler() { ESP_LOGI(TAG, "Button pressed: LEFT"); }
static void default_button_cancel_handler() { ESP_LOGI(TAG, "Button pressed: CANCEL"); }
static void default_button_ok_handler() { ESP_LOGI(TAG, "Button pressed: OK"); }
static void default_button_right_handler() { ESP_LOGI(TAG, "Button pressed: RIGHT"); }
static void default_button_on_off_handler() { ESP_LOGI(TAG, "Button pressed: ON/OFF"); }

// --- Private Functions ---

// The actual execution logic, callable from either mode
static void execute_handler(button_id_t button_id) {
    if (button_id < BUTTON_COUNT) {
        if (button_handlers[button_id].view_handler) {
            button_handlers[button_id].view_handler();
        } else if (button_handlers[button_id].default_handler) {
            button_handlers[button_id].default_handler();
        }
    }
}

// Timer callback for QUEUED mode
static void process_queued_input_cb(lv_timer_t *timer) {
    button_id_t button_id;
    if (xQueueReceive(s_input_event_queue, &button_id, 0) == pdPASS) {
        ESP_LOGD(TAG, "[Queued] Executing handler for button %d", button_id);
        execute_handler(button_id);
    }
}

// Generic callback registered for all buttons.
// This is the CRITICAL part. It decides HOW to execute the action.
static void button_event_cb(void *arg, void *usr_data) {
    button_id_t button_id = (button_id_t)(uintptr_t)usr_data;

    if (s_current_dispatch_mode == INPUT_DISPATCH_MODE_IMMEDIATE) {
        // --- Immediate Mode ---
        ESP_LOGD(TAG, "[Immediate] Executing handler for button %d", button_id);
        execute_handler(button_id);
    } else {
        // --- Queued Mode ---
        if (s_input_event_queue) {
            ESP_LOGD(TAG, "[Queued] Sending button %d to queue", button_id);
            xQueueSend(s_input_event_queue, &button_id, 0);
        }
    }
}

// --- Public API Functions ---

void button_manager_init() {
    ESP_LOGI(TAG, "Initializing button manager...");

    // Create the queue and LVGL timer for QUEUED mode
    s_input_event_queue = xQueueCreate(10, sizeof(button_id_t));
    assert(s_input_event_queue);
    s_input_processor_timer = lv_timer_create(process_queued_input_cb, 20, NULL);
    assert(s_input_processor_timer);

    // General button config
    button_config_t btn_config = {
        .long_press_time = 0,
        .short_press_time = 0,
    };
    const button_gpio_config_t gpio_configs[BUTTON_COUNT] = {
        [BUTTON_LEFT]   = {BUTTON_LEFT_PIN, 0, false, false},
        [BUTTON_CANCEL] = {BUTTON_CANCEL_PIN, 0, false, false},
        [BUTTON_OK]     = {BUTTON_OK_PIN, 0, false, false},
        [BUTTON_RIGHT]  = {BUTTON_RIGHT_PIN, 0, false, false},
        [BUTTON_ON_OFF] = {BUTTON_ON_OFF_PIN, 0, false, false}
    };

    // Create each button and register its single callback
    for (int i = 0; i < BUTTON_COUNT; i++) {
        ESP_ERROR_CHECK(iot_button_new_gpio_device(&btn_config, &gpio_configs[i], &buttons[i]));
        assert(buttons[i]);
        iot_button_register_cb(buttons[i], BUTTON_SINGLE_CLICK, NULL, button_event_cb, (void*)(uintptr_t)i);
    }
    
    // Assign default handlers
    button_manager_register_default_handler(BUTTON_LEFT, default_button_left_handler);
    button_manager_register_default_handler(BUTTON_CANCEL, default_button_cancel_handler);
    button_manager_register_default_handler(BUTTON_OK, default_button_ok_handler);
    button_manager_register_default_handler(BUTTON_RIGHT, default_button_right_handler);
    button_manager_register_default_handler(BUTTON_ON_OFF, default_button_on_off_handler);

    ESP_LOGI(TAG, "Button manager initialized successfully in QUEUED mode.");
}

void button_manager_set_dispatch_mode(input_dispatch_mode_t mode) {
    if (s_current_dispatch_mode != mode) {
        s_current_dispatch_mode = mode;
        // Clear the queue when changing modes to prevent processing old events
        if(s_input_event_queue) {
            xQueueReset(s_input_event_queue);
        }
        ESP_LOGI(TAG, "Input dispatch mode changed to %s", (mode == INPUT_DISPATCH_MODE_QUEUED) ? "QUEUED" : "IMMEDIATE");
    }
}

void button_manager_register_default_handler(button_id_t button, button_handler_t handler) {
    if (button < BUTTON_COUNT) {
        button_handlers[button].default_handler = handler;
    }
}

void button_manager_register_view_handler(button_id_t button, button_handler_t handler) {
    if (button < BUTTON_COUNT) {
        button_handlers[button].view_handler = handler;
    }
}

void button_manager_unregister_view_handlers() {
    ESP_LOGI(TAG, "Unregistering all view-specific handlers.");
    for (int i = 0; i < BUTTON_COUNT; i++) {
        button_handlers[i].view_handler = nullptr;
    }
    ESP_LOGI(TAG, "Restored default button handlers.");
}