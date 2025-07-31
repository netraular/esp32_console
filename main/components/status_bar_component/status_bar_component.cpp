#include "status_bar_component.h"
#include "config.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "esp_log.h"
#include <time.h>
#include <cstring>

static const char *TAG = "STATUS_BAR";

// UI elements managed by the component
typedef struct {
    lv_obj_t* wifi_icon_label;
    lv_obj_t* volume_icon_label;
    lv_obj_t* volume_text_label;
    lv_timer_t* update_timer;
} status_bar_ui_t;

// Static pointer to access the UI from the public function
static status_bar_ui_t* g_ui = NULL;

// Implementation of the volume update function
void status_bar_update_volume_display(void) {
    if (g_ui == NULL) { // Safety check
        return;
    }
    
    uint8_t physical_volume = audio_manager_get_volume();
    // Map the physical volume (e.g., 0-25) to a 0-100% UI scale
    uint8_t ui_volume = (physical_volume * 100) / MAX_VOLUME_PERCENTAGE;
    lv_label_set_text_fmt(g_ui->volume_text_label, "%d%%", ui_volume);
    
    // Update the volume icon based on the UI volume level
    if (ui_volume == 0) lv_label_set_text(g_ui->volume_icon_label, LV_SYMBOL_MUTE);
    else if (ui_volume < 50) lv_label_set_text(g_ui->volume_icon_label, LV_SYMBOL_VOLUME_MID);
    else lv_label_set_text(g_ui->volume_icon_label, LV_SYMBOL_VOLUME_MAX);
}

static void update_task(lv_timer_t *timer) {
    status_bar_ui_t* ui = (status_bar_ui_t*)lv_timer_get_user_data(timer);
    if (!ui) return;

    EventBits_t wifi_bits = xEventGroupGetBits(wifi_manager_get_event_group());

    // Update WiFi icon based on connection and time sync status
    if ((wifi_bits & WIFI_CONNECTED_BIT) != 0) {
        // Use a solid WIFI symbol only if time is also synced, otherwise a checkmark.
        // This provides more detailed status feedback.
        if ((wifi_bits & TIME_SYNC_BIT) != 0) {
            lv_label_set_text(ui->wifi_icon_label, LV_SYMBOL_WIFI);
            lv_obj_set_style_text_color(ui->wifi_icon_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        } else {
            lv_label_set_text(ui->wifi_icon_label, LV_SYMBOL_OK); // Connected, but no time yet
            lv_obj_set_style_text_color(ui->wifi_icon_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
        }
    } else {
        lv_label_set_text(ui->wifi_icon_label, LV_SYMBOL_CLOSE); // Not connected
        lv_obj_set_style_text_color(ui->wifi_icon_label, lv_palette_main(LV_PALETTE_GREY), 0);
    }

    // The timer also updates the volume to maintain consistency
    status_bar_update_volume_display();
}

static void cleanup_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        ESP_LOGI(TAG, "Status bar is being deleted, cleaning up resources.");
        status_bar_ui_t* ui = (status_bar_ui_t*)lv_event_get_user_data(e);
        if (ui) {
            if (ui->update_timer) lv_timer_del(ui->update_timer);
            free(ui);
            g_ui = NULL; // Clear the global pointer
        }
    }
}

lv_obj_t* status_bar_create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Status Bar component");

    g_ui = (status_bar_ui_t*)malloc(sizeof(status_bar_ui_t));
    if (!g_ui) {
        ESP_LOGE(TAG, "Failed to allocate memory for status bar UI");
        return NULL;
    }
    // Ensure internal pointers are initialized to NULL
    memset(g_ui, 0, sizeof(status_bar_ui_t));

    // Main container for the status icons
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    
    // Style the container as a small, rounded-corner panel
    lv_obj_set_style_bg_color(container, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_ver(container, 2, 0);
    lv_obj_set_style_pad_hor(container, 5, 0);
    lv_obj_set_style_radius(container, 3, 0);
    
    // Use a flex layout to arrange icons horizontally
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(container, 8, 0);

    // --- Create UI Widgets ---
    g_ui->wifi_icon_label = lv_label_create(container);
    lv_obj_set_style_text_font(g_ui->wifi_icon_label, &lv_font_montserrat_14, 0);

    g_ui->volume_icon_label = lv_label_create(container);
    lv_obj_set_style_text_font(g_ui->volume_icon_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_ui->volume_icon_label, lv_color_black(), 0);

    g_ui->volume_text_label = lv_label_create(container);
    lv_obj_set_style_text_font(g_ui->volume_text_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_ui->volume_text_label, lv_color_black(), 0);
    
    // Align the entire component to the top right of its parent with padding
    lv_obj_align(container, LV_ALIGN_TOP_RIGHT, 0, 0);
    
    // --- Setup Logic ---
    lv_obj_add_event_cb(container, cleanup_event_cb, LV_EVENT_DELETE, g_ui);
    g_ui->update_timer = lv_timer_create(update_task, 1000, g_ui);
    
    // Run the update task once immediately to set the initial state
    update_task(g_ui->update_timer);

    return container;
}