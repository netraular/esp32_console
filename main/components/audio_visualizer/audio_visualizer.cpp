#include "audio_visualizer.h"
#include <string.h>
#include "esp_log.h"

static const char* TAG = "AUDIO_VISUALIZER";

// Estructura para almacenar el estado del visualizador
typedef struct {
    lv_obj_t* canvas; // Puntero al objeto canvas
    uint8_t bar_count;
    uint8_t values[AUDIO_VISUALIZER_MAX_BARS];
    lv_color_t bar_color;
    lv_color_t bg_color;
} audio_visualizer_t;


// --- Función interna para redibujar las barras en el canvas ---
static void redraw_bars(audio_visualizer_t* viz) {
    if (!viz || !viz->canvas) return;

    // 1. Limpiar el canvas con el color de fondo
    lv_canvas_fill_bg(viz->canvas, viz->bg_color, LV_OPA_COVER);
    
    // 2. Preparar el descriptor de dibujo para las barras
    lv_layer_t layer;
    lv_canvas_init_layer(viz->canvas, &layer);

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = viz->bar_color;
    rect_dsc.radius = 2;

    // 3. Calcular dimensiones y dibujar cada barra
    lv_coord_t canvas_w = lv_obj_get_width(viz->canvas);
    lv_coord_t canvas_h = lv_obj_get_height(viz->canvas);

    if (viz->bar_count > 0) {
        lv_coord_t total_bar_width_units = (viz->bar_count * 2 - 1);
        if (total_bar_width_units == 0) {
            lv_canvas_finish_layer(viz->canvas, &layer);
            return;
        }
        
        lv_coord_t bar_w = canvas_w / total_bar_width_units;
        lv_coord_t space_w = bar_w;

        for (int i = 0; i < viz->bar_count; i++) {
            lv_coord_t bar_h = (viz->values[i] * canvas_h) / 255;
            if (bar_h < 1 && viz->values[i] > 0) bar_h = 1;

            if (bar_h > 0) {
                lv_area_t bar_area;
                bar_area.x1 = i * (bar_w + space_w);
                bar_area.x2 = bar_area.x1 + bar_w - 1;
                bar_area.y2 = canvas_h - 1;
                bar_area.y1 = bar_area.y2 - bar_h + 1;
                
                lv_draw_rect(&layer, &rect_dsc, &bar_area);
            }
        }
    }
    
    // 4. Finalizar la capa para que se muestren los cambios
    lv_canvas_finish_layer(viz->canvas, &layer);
}

lv_obj_t* audio_visualizer_create(lv_obj_t* parent, uint8_t bar_count) {
    // --- CORRECCIÓN FINAL ---
    // La macro ya incluye 'static', así que no lo repetimos.
    // El buffer se declarará estáticamente aquí y persistirá.
    #define CANVAS_WIDTH 240
    #define CANVAS_HEIGHT 100
    LV_DRAW_BUF_DEFINE_STATIC(draw_buf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_COLOR_FORMAT_NATIVE);
    
    // Esta macro ahora se puede llamar sin problemas.
    LV_DRAW_BUF_INIT_STATIC(draw_buf);
    // --- FIN DE LA CORRECCIÓN ---


    if (bar_count > AUDIO_VISUALIZER_MAX_BARS) {
        ESP_LOGE(TAG, "Bar count %d exceeds max %d", bar_count, AUDIO_VISUALIZER_MAX_BARS);
        bar_count = AUDIO_VISUALIZER_MAX_BARS;
    }

    // El objeto principal será un contenedor
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));

    // Dentro del contenedor, creamos el canvas
    lv_obj_t* canvas = lv_canvas_create(cont);
    lv_canvas_set_draw_buf(canvas, &draw_buf);
    lv_obj_set_size(canvas, lv_pct(100), lv_pct(100));
    lv_obj_center(canvas);

    // Asignar memoria para la estructura de datos
    audio_visualizer_t* viz = (audio_visualizer_t*)lv_malloc(sizeof(audio_visualizer_t));
    LV_ASSERT_MALLOC(viz);
    if(viz == NULL) {
        lv_obj_del(cont);
        return NULL;
    }

    memset(viz, 0, sizeof(audio_visualizer_t));
    viz->canvas = canvas;
    viz->bar_count = bar_count;
    viz->bar_color = lv_theme_get_color_primary(cont);
    viz->bg_color = lv_color_hex(0x222222);

    // Guardamos el puntero a nuestros datos en el contenedor principal
    lv_obj_set_user_data(cont, viz);
    
    redraw_bars(viz);

    return cont; // Devolvemos el contenedor
}


void audio_visualizer_set_values(lv_obj_t* visualizer_cont, uint8_t* values) {
    audio_visualizer_t* viz = (audio_visualizer_t*)lv_obj_get_user_data(visualizer_cont);
    if (!viz) return;

    if (memcmp(viz->values, values, viz->bar_count * sizeof(uint8_t)) != 0) {
        memcpy(viz->values, values, viz->bar_count * sizeof(uint8_t));
        redraw_bars(viz);
    }
}


void audio_visualizer_set_bar_color(lv_obj_t* visualizer_cont, lv_color_t color) {
    audio_visualizer_t* viz = (audio_visualizer_t*)lv_obj_get_user_data(visualizer_cont);
    if (!viz) return;
    
    if(lv_color_to_u32(viz->bar_color) != lv_color_to_u32(color)) {
        viz->bar_color = color;
        redraw_bars(viz);
    }
}