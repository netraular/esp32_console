# Audio Visualizer Component

## Description
A custom, reusable LVGL widget that displays a dynamic, bar-style audio visualizer using an LVGL Canvas. It is designed to be efficient and visually appealing, and is decoupled from any specific audio source.

## Features
-   **High Bar Count:** Supports a configurable number of bars (up to 32), ideal for detailed spectrum visualization.
-   **Smart Layout:** Intelligently centers and spaces the bars for a polished look, regardless of the bar count or widget size.
-   **Gradient Colors:** Renders bars with a smooth color gradient (from blue to red by default) for a modern aesthetic.
-   **Efficient:** Drawn on an LVGL Canvas, redrawing only when data changes to save CPU cycles.
-   **Decoupled:** Fully independent of the audio source; it accepts a simple array of `uint8_t` values to set bar heights.

## How to Use

1.  **Create the Widget:**
    Add the visualizer to any parent object, like a screen or a container.
    ```cpp
    #include "components/audio_visualizer/audio_visualizer.h"

    #define VISUALIZER_BAR_COUNT 32
    
    // In your view's create function:
    lv_obj_t* visualizer = audio_visualizer_create(parent_container, VISUALIZER_BAR_COUNT);
    lv_obj_set_size(visualizer, 240, 100); // Set desired size
    lv_obj_align(visualizer, LV_ALIGN_CENTER, 0, 0);
    ```

2.  **Update its Values:**
    Periodically, provide an array of `uint8_t` values (0-255) representing the height of each bar. This is typically done in an `lv_timer` or by reading data from a queue provided by the `audio_manager`.
    ```cpp
    // In an update loop or timer:
    visualizer_data_t viz_data;
    if (xQueueReceive(audio_manager_get_visualizer_queue(), &viz_data, 0) == pdPASS) {
        audio_visualizer_set_values(visualizer, viz_data.bar_values);
    }
    ```

3.  **Customize Appearance (Optional):**
    You can override the default gradient with a single solid color.
    ```cpp
    // Set all bars to a solid green color
    audio_visualizer_set_bar_color(visualizer, lv_palette_main(LV_PALETTE_GREEN)); 
    ```