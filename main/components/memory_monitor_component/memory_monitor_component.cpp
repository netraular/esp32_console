#include "memory_monitor_component.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char* TAG = "MEM_MONITOR";
static const uint32_t UPDATE_INTERVAL_MS = 1000; // Update every second

// Structure to hold component-specific data
typedef struct {
    lv_obj_t* label;
    lv_timer_t* timer;
} memory_monitor_t;

// Timer callback to update the memory usage display
static void update_timer_cb(lv_timer_t* timer) {
    lv_obj_t* monitor_cont = static_cast<lv_obj_t*>(lv_timer_get_user_data(timer));
    if (!monitor_cont) return;

    memory_monitor_t* monitor = static_cast<memory_monitor_t*>(lv_obj_get_user_data(monitor_cont));
    if (!monitor || !monitor->label) return;

    // Get internal RAM info
    multi_heap_info_t internal_info;
    heap_caps_get_info(&internal_info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t internal_used_kb = internal_info.total_allocated_bytes / 1024;
    size_t internal_total_kb = (internal_info.total_free_bytes + internal_info.total_allocated_bytes) / 1024;

    // Get PSRAM info
    multi_heap_info_t psram_info;
    heap_caps_get_info(&psram_info, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    size_t psram_used_kb = psram_info.total_allocated_bytes / 1024;
    size_t psram_total_kb = (psram_info.total_free_bytes + psram_info.total_allocated_bytes) / 1024;

    // Update the label text to show USED/TOTAL memory
    lv_label_set_text_fmt(monitor->label, "RAM: %zu/%zu KB\nPSRAM: %zu/%zu KB",
                          internal_used_kb, internal_total_kb,
                          psram_used_kb, psram_total_kb);
}

// Cleanup event callback to delete the timer and allocated data
static void cleanup_event_cb(lv_event_t * e) {
    lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    memory_monitor_t* monitor = static_cast<memory_monitor_t*>(lv_obj_get_user_data(obj));

    if (monitor) {
        if (monitor->timer) {
            lv_timer_delete(monitor->timer);
            monitor->timer = NULL;
        }
        // The label is a child of the container, so it gets deleted automatically.
        // We just need to free the struct itself.
        lv_free(monitor);
        lv_obj_set_user_data(obj, NULL);
        ESP_LOGD(TAG, "Memory monitor cleaned up.");
    }
}

lv_obj_t* memory_monitor_create(lv_obj_t* parent) {
    // Create a container for the widget
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(cont, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_60, 0);
    lv_obj_set_style_pad_all(cont, 4, 0);
    lv_obj_set_style_radius(cont, 4, 0); // Use a specific radius value
    lv_obj_set_style_border_width(cont, 1, 0);
    lv_obj_set_style_border_color(cont, lv_color_hex(0x555555), 0);

    // Allocate memory for the component's state
    memory_monitor_t* monitor = static_cast<memory_monitor_t*>(lv_malloc(sizeof(memory_monitor_t)));
    if (!monitor) {
        ESP_LOGE(TAG, "Failed to allocate memory for monitor struct");
        lv_obj_del(cont);
        return NULL;
    }

    // Create the label inside the container
    monitor->label = lv_label_create(cont);
    lv_obj_set_style_text_color(monitor->label, lv_color_white(), 0);
    lv_obj_set_style_text_font(monitor->label, &lv_font_montserrat_12, 0);
    lv_label_set_text(monitor->label, "RAM: -/-\nPSRAM: -/-"); // Initial placeholder text

    // Store the monitor struct in the container's user data
    lv_obj_set_user_data(cont, monitor);

    // Create the timer, passing the container as user data
    monitor->timer = lv_timer_create(update_timer_cb, UPDATE_INTERVAL_MS, cont);
    
    // Add an event to clean up resources when the object is deleted
    lv_obj_add_event_cb(cont, cleanup_event_cb, LV_EVENT_DELETE, NULL);

    // Run the timer once immediately to populate the data
    update_timer_cb(monitor->timer);

    ESP_LOGI(TAG, "Memory monitor component created.");
    return cont;
}