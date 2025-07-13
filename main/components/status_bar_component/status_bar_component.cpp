#include "status_bar_component.h"
#include "config.h" // Para MAX_VOLUME_PERCENTAGE
#include "controllers/wifi_manager/wifi_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "STATUS_BAR";

// La estructura de datos y las funciones de actualización y limpieza no cambian.
typedef struct {
    lv_obj_t* wifi_icon_label;
    lv_obj_t* volume_icon_label;
    lv_obj_t* volume_text_label;
    lv_obj_t* datetime_label;
    lv_timer_t* update_timer;
} status_bar_ui_t;

static void update_task(lv_timer_t *timer) {
    status_bar_ui_t* ui = (status_bar_ui_t*)lv_timer_get_user_data(timer);
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

    uint8_t physical_volume = audio_manager_get_volume();
    uint8_t ui_volume = (physical_volume * 100) / MAX_VOLUME_PERCENTAGE;
    lv_label_set_text_fmt(ui->volume_text_label, "%d%%", ui_volume);
    
    if (ui_volume == 0) lv_label_set_text(ui->volume_icon_label, LV_SYMBOL_MUTE);
    else if (ui_volume < 50) lv_label_set_text(ui->volume_icon_label, LV_SYMBOL_VOLUME_MID);
    else lv_label_set_text(ui->volume_icon_label, LV_SYMBOL_VOLUME_MAX);
}

static void cleanup_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        ESP_LOGI(TAG, "Status bar is being deleted, cleaning up resources.");
        status_bar_ui_t* ui = (status_bar_ui_t*)lv_event_get_user_data(e);
        if (ui) {
            if (ui->update_timer) lv_timer_del(ui->update_timer);
            free(ui);
        }
    }
}


lv_obj_t* status_bar_create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Status Bar component (Hybrid Layout)");

    status_bar_ui_t* ui = (status_bar_ui_t*)malloc(sizeof(status_bar_ui_t));
    if (!ui) {
        ESP_LOGE(TAG, "Failed to allocate memory for status bar UI");
        return NULL;
    }

    // Contenedor principal
    lv_obj_t* background_bar = lv_obj_create(parent);
    lv_obj_remove_style_all(background_bar);
    lv_obj_set_size(background_bar, LV_PCT(100), 20);
    // CAMBIO DE COLOR: Fondo gris más claro
    lv_obj_set_style_bg_color(background_bar, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_bg_opa(background_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(background_bar, 0, 0);
    lv_obj_set_style_border_width(background_bar, 0, 0);
    lv_obj_align(background_bar, LV_ALIGN_TOP_MID, 0, 0);
    
    // --- SECCIÓN IZQUIERDA (FECHA Y HORA) ---
    ui->datetime_label = lv_label_create(background_bar);
    // CAMBIO DE COLOR: Texto a negro
    lv_obj_set_style_text_color(ui->datetime_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(ui->datetime_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(ui->datetime_label, lv_palette_lighten(LV_PALETTE_GREEN, 3), 0);
    lv_obj_set_style_bg_opa(ui->datetime_label, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_ver(ui->datetime_label, 2, 0);
    lv_obj_set_style_pad_hor(ui->datetime_label, 5, 0);
    lv_obj_set_style_radius(ui->datetime_label, 3, 0);
    lv_obj_align(ui->datetime_label, LV_ALIGN_LEFT_MID, 5, 0);

    // --- SECCIÓN DERECHA (WIFI Y VOLUMEN) ---
    lv_obj_t* right_panel = lv_obj_create(background_bar);
    lv_obj_remove_style_all(right_panel);
    lv_obj_set_style_bg_color(right_panel, lv_palette_lighten(LV_PALETTE_ORANGE, 2), 0);
    lv_obj_set_style_bg_opa(right_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(right_panel, 2, 0); // Padding uniforme alrededor de los iconos
    lv_obj_set_style_radius(right_panel, 3, 0);
    
    // CORRECCIÓN: Usar Flexbox para el layout interno del panel derecho
    lv_obj_set_size(right_panel, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(right_panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(right_panel, LV_FLEX_FLOW_ROW); // Orden normal: de izquierda a derecha
    lv_obj_set_flex_align(right_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(right_panel, 8, 0); // Espacio entre iconos

    // Crear los elementos DENTRO del panel derecho
    ui->wifi_icon_label = lv_label_create(right_panel);
    lv_obj_set_style_text_font(ui->wifi_icon_label, &lv_font_montserrat_14, 0);

    ui->volume_icon_label = lv_label_create(right_panel);
    lv_obj_set_style_text_font(ui->volume_icon_label, &lv_font_montserrat_14, 0);

    ui->volume_text_label = lv_label_create(right_panel);
    lv_obj_set_style_text_font(ui->volume_text_label, &lv_font_montserrat_14, 0);
    // CAMBIO DE COLOR: Texto a negro
    lv_obj_set_style_text_color(ui->volume_text_label, lv_color_black(), 0);
    
    // Alinear el panel derecho completo al borde de la pantalla
    lv_obj_align(right_panel, LV_ALIGN_RIGHT_MID, -5, 0);
    
    // --- GESTIÓN DE RECURSOS ---
    lv_obj_add_event_cb(background_bar, cleanup_event_cb, LV_EVENT_DELETE, ui);
    ui->update_timer = lv_timer_create(update_task, 1000, ui);
    
    update_task(ui->update_timer);

    return background_bar;
}