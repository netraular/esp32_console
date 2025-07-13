#include "status_bar_component.h"
#include "config.h" // Para MAX_VOLUME_PERCENTAGE
#include "controllers/wifi_manager/wifi_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "esp_log.h"
#include <time.h>
#include <cstring> // <-- CORRECCIÓN: Añadir este include para memset

static const char *TAG = "STATUS_BAR";

typedef struct {
    lv_obj_t* wifi_icon_label;
    lv_obj_t* volume_icon_label;
    lv_obj_t* volume_text_label;
    lv_obj_t* datetime_label;
    lv_timer_t* update_timer;
} status_bar_ui_t;

// Puntero estático para acceder a la UI desde la función pública
static status_bar_ui_t* g_ui = NULL;

// Implementación de la función de actualización de volumen
void status_bar_update_volume_display(void) {
    if (g_ui == NULL) { // Comprobación de seguridad
        return;
    }
    
    uint8_t physical_volume = audio_manager_get_volume();
    uint8_t ui_volume = (physical_volume * 100) / MAX_VOLUME_PERCENTAGE;
    lv_label_set_text_fmt(g_ui->volume_text_label, "%d%%", ui_volume);
    
    if (ui_volume == 0) lv_label_set_text(g_ui->volume_icon_label, LV_SYMBOL_MUTE);
    else if (ui_volume < 50) lv_label_set_text(g_ui->volume_icon_label, LV_SYMBOL_VOLUME_MID);
    else lv_label_set_text(g_ui->volume_icon_label, LV_SYMBOL_VOLUME_MAX);
}

static void update_task(lv_timer_t *timer) {
    status_bar_ui_t* ui = (status_bar_ui_t*)lv_timer_get_user_data(timer);
    if (!ui) return;

    EventBits_t wifi_bits = xEventGroupGetBits(wifi_manager_get_event_group());

    if ((wifi_bits & TIME_SYNC_BIT) != 0) {
        lv_label_set_text(ui->wifi_icon_label, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(ui->wifi_icon_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        char datetime_str[20];
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        strftime(datetime_str, sizeof(datetime_str), "%H:%M  %d/%m/%y", &timeinfo);
        lv_label_set_text(ui->datetime_label, datetime_str);
    } else {
        lv_label_set_text(ui->datetime_label, "--:--  --/--/--");
        if ((wifi_bits & WIFI_CONNECTED_BIT) != 0) {
            lv_label_set_text(ui->wifi_icon_label, LV_SYMBOL_OK);
            lv_obj_set_style_text_color(ui->wifi_icon_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
        } else {
            lv_label_set_text(ui->wifi_icon_label, LV_SYMBOL_CLOSE);
            lv_obj_set_style_text_color(ui->wifi_icon_label, lv_palette_main(LV_PALETTE_GREY), 0);
        }
    }

    // El timer también actualiza el volumen para mantener la consistencia en caso de que cambie por otra vía.
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
            g_ui = NULL; // Limpiar el puntero global
        }
    }
}

lv_obj_t* status_bar_create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Status Bar component (Hybrid Layout)");

    g_ui = (status_bar_ui_t*)malloc(sizeof(status_bar_ui_t));
    if (!g_ui) {
        ESP_LOGE(TAG, "Failed to allocate memory for status bar UI");
        return NULL;
    }
    // Asegurarse de que los punteros internos estén inicializados a NULL
    memset(g_ui, 0, sizeof(status_bar_ui_t));

    lv_obj_t* background_bar = lv_obj_create(parent);
    lv_obj_remove_style_all(background_bar);
    lv_obj_set_size(background_bar, LV_PCT(100), 20);
    lv_obj_set_style_bg_color(background_bar, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_bg_opa(background_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(background_bar, 0, 0);
    lv_obj_set_style_border_width(background_bar, 0, 0);
    lv_obj_align(background_bar, LV_ALIGN_TOP_MID, 0, 0);
    
    // --- SECCIÓN IZQUIERDA (FECHA Y HORA) ---
    g_ui->datetime_label = lv_label_create(background_bar);
    lv_obj_set_style_text_color(g_ui->datetime_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(g_ui->datetime_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(g_ui->datetime_label, lv_palette_lighten(LV_PALETTE_GREEN, 3), 0);
    lv_obj_set_style_bg_opa(g_ui->datetime_label, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_ver(g_ui->datetime_label, 2, 0);
    lv_obj_set_style_pad_hor(g_ui->datetime_label, 5, 0);
    lv_obj_set_style_radius(g_ui->datetime_label, 3, 0);
    lv_obj_align(g_ui->datetime_label, LV_ALIGN_LEFT_MID, 5, 0);

    // --- SECCIÓN DERECHA (WIFI Y VOLUMEN) ---
    lv_obj_t* right_panel = lv_obj_create(background_bar);
    lv_obj_remove_style_all(right_panel);
    lv_obj_set_style_bg_color(right_panel, lv_palette_lighten(LV_PALETTE_ORANGE, 2), 0);
    lv_obj_set_style_bg_opa(right_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(right_panel, 2, 0);
    lv_obj_set_style_radius(right_panel, 3, 0);
    
    lv_obj_set_size(right_panel, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(right_panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(right_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(right_panel, 8, 0);

    g_ui->wifi_icon_label = lv_label_create(right_panel);
    lv_obj_set_style_text_font(g_ui->wifi_icon_label, &lv_font_montserrat_14, 0);

    g_ui->volume_icon_label = lv_label_create(right_panel);
    lv_obj_set_style_text_font(g_ui->volume_icon_label, &lv_font_montserrat_14, 0);

    g_ui->volume_text_label = lv_label_create(right_panel);
    lv_obj_set_style_text_font(g_ui->volume_text_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_ui->volume_text_label, lv_color_black(), 0);
    
    lv_obj_align(right_panel, LV_ALIGN_RIGHT_MID, -5, 0);
    
    lv_obj_add_event_cb(background_bar, cleanup_event_cb, LV_EVENT_DELETE, g_ui);
    g_ui->update_timer = lv_timer_create(update_task, 1000, g_ui);
    
    update_task(g_ui->update_timer);

    return background_bar;
}