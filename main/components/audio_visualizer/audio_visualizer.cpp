#include "audio_visualizer.h"
#include <string.h>
#include "esp_log.h"

static const char* TAG = "AUDIO_VISUALIZER";

// Structure to store the state of the visualizer instance.
typedef struct {
    lv_obj_t* canvas;   // Pointer to the canvas object for drawing.
    uint8_t bar_count;
    uint8_t values[AUDIO_VISUALIZER_MAX_BARS];
    lv_color_t bar_color; // Solid bar color (overrides gradient if used).
    lv_color_t bg_color;  // Background color.
} audio_visualizer_t;


// --- Internal function to redraw the bars on the canvas ---
static void redraw_bars(audio_visualizer_t* viz) {
    if (!viz || !viz->canvas) return;

    // 1. Clear the canvas with the background color.
    lv_canvas_fill_bg(viz->canvas, viz->bg_color, LV_OPA_COVER);
    
    // 2. Prepare the drawing layer and descriptor for the bars.
    lv_layer_t layer;
    lv_canvas_init_layer(viz->canvas, &layer);

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.radius = 2; // Rounded corners for the bars.

    // Colors for the default gradient.
    lv_color_t start_color = lv_palette_main(LV_PALETTE_BLUE);
    lv_color_t end_color = lv_palette_main(LV_PALETTE_RED);

    // 3. Calculate dimensions and draw each bar.
    lv_coord_t canvas_w = lv_obj_get_width(viz->canvas);
    lv_coord_t canvas_h = lv_obj_get_height(viz->canvas);

    if (viz->bar_count > 0) {
        // --- Smart Centering Logic ---
        
        // 1. Define the width of each bar and the space between them.
        //    Adjusting these values can change the visualizer's look.
        //    For 32 bars on a 240px screen, 5px width and 2px space is a good fit.
        const lv_coord_t bar_w = 5;
        const lv_coord_t space_w = 2;

        // 2. Calculate the total width required for all bars and spaces.
        //    (N bars) + (N-1 spaces)
        lv_coord_t total_bars_width = (viz->bar_count * bar_w) + ((viz->bar_count - 1) * space_w);

        // 3. Calculate the initial left margin to center the entire block of bars.
        //    The remaining space is divided by two.
        lv_coord_t start_x = (canvas_w - total_bars_width) / 2;
        
        // --- End of Centering Logic ---

        for (int i = 0; i < viz->bar_count; i++) {
            // Calculate the color for each individual bar to create the gradient effect.
            uint8_t mix_ratio = (i * 255) / (viz->bar_count - 1);
            rect_dsc.bg_color = lv_color_mix(start_color, end_color, mix_ratio);

            // Calculate the bar's height based on its value (0-255).
            lv_coord_t bar_h = (viz->values[i] * canvas_h) / 255;
            if (bar_h < 3 && viz->values[i] > 0) bar_h = 3; // Minimum height to make radius visible.

            if (bar_h > 0) {
                lv_area_t bar_area;
                // 4. Position the bar using the calculated starting offset and fixed widths.
                bar_area.x1 = start_x + (i * (bar_w + space_w));
                bar_area.x2 = bar_area.x1 + bar_w - 1;
                bar_area.y2 = canvas_h - 1; // Bars are drawn from the bottom up.
                bar_area.y1 = bar_area.y2 - bar_h + 1;
                
                lv_draw_rect(&layer, &rect_dsc, &bar_area);
            }
        }
    }
    
    // 5. Finalize the layer to apply the drawings to the canvas.
    lv_canvas_finish_layer(viz->canvas, &layer);
}

// --- Public Functions ---

lv_obj_t* audio_visualizer_create(lv_obj_t* parent, uint8_t bar_count) {
    #define CANVAS_WIDTH 240
    #define CANVAS_HEIGHT 100
    
    // Statically allocate the draw buffer for the canvas.
    LV_DRAW_BUF_DEFINE_STATIC(draw_buf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_COLOR_FORMAT_NATIVE);

    if (bar_count > AUDIO_VISUALIZER_MAX_BARS) {
        ESP_LOGE(TAG, "Bar count %d exceeds max %d. Clamping to max.", bar_count, AUDIO_VISUALIZER_MAX_BARS);
        bar_count = AUDIO_VISUALIZER_MAX_BARS;
    }

    // The main object is a container, which holds the canvas.
    // This makes layout management easier.
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));

    // Create the canvas inside the container.
    lv_obj_t* canvas = lv_canvas_create(cont);
    lv_canvas_set_draw_buf(canvas, &draw_buf);
    lv_obj_set_size(canvas, lv_pct(100), lv_pct(100));
    lv_obj_center(canvas);

    // Allocate memory for the visualizer's state data.
    audio_visualizer_t* viz = (audio_visualizer_t*)lv_malloc(sizeof(audio_visualizer_t));
    LV_ASSERT_MALLOC(viz);
    if(viz == NULL) {
        lv_obj_del(cont);
        return NULL;
    }

    // Initialize the state structure.
    memset(viz, 0, sizeof(audio_visualizer_t));
    viz->canvas = canvas;
    viz->bar_count = bar_count;
    viz->bar_color = lv_theme_get_color_primary(cont); // Default solid color (not used by default).
    viz->bg_color = lv_color_hex(0x222222); // Dark background.

    // Attach the state data to the container object.
    lv_obj_set_user_data(cont, viz);
    
    // Perform the initial draw.
    redraw_bars(viz);

    return cont; // Return the container object.
}


void audio_visualizer_set_values(lv_obj_t* visualizer_cont, uint8_t* values) {
    audio_visualizer_t* viz = (audio_visualizer_t*)lv_obj_get_user_data(visualizer_cont);
    if (!viz) return;

    // Redraw only if the values have actually changed to save CPU cycles.
    if (memcmp(viz->values, values, viz->bar_count * sizeof(uint8_t)) != 0) {
        memcpy(viz->values, values, viz->bar_count * sizeof(uint8_t));
        redraw_bars(viz);
    }
}


void audio_visualizer_set_bar_color(lv_obj_t* visualizer_cont, lv_color_t color) {
    audio_visualizer_t* viz = (audio_visualizer_t*)lv_obj_get_user_data(visualizer_cont);
    if (!viz) return;
    
    // Note: This function is not currently used as the default is a gradient.
    // To make it work, the `redraw_bars` function would need to be modified
    // to use `viz->bar_color` instead of calculating a gradient.
    if(lv_color_to_u32(viz->bar_color) != lv_color_to_u32(color)) {
        viz->bar_color = color;
        redraw_bars(viz);
    }
}