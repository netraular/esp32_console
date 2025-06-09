# Audio Visualizer Component

## Description
A custom LVGL widget that displays a dynamic, bar-style audio visualizer using an LVGL Canvas. It's designed to be efficient and visually appealing.

## Features
-   **High Bar Count:** Supports a configurable number of bars (up to 32), ideal for detailed spectrum visualization.
-   **Smart Layout:** Intelligently centers and spaces the bars for a polished, professional look regardless of the bar count or widget size.
-   **Gradient Colors:** Renders bars with a smooth color gradient (from blue to red by default) for a modern aesthetic.
-   **Efficient:** Drawn on an LVGL Canvas, redrawing only when data changes.
-   **Decoupled:** Fully decoupled from the audio source; it accepts a simple array of `uint8_t` values to display.

## How to Use

1.  **Create the Widget:**
    Add the visualizer to any parent object, like a screen or a container. The typical usage is with 32 bars.
    ```cpp
    #include "components/audio_visualizer/audio_visualizer.h"

    #define VISUALIZER_BAR_COUNT 32
    
    // ...
    lv_obj_t* visualizer = audio_visualizer_create(parent_container, VISUALIZER_BAR_COUNT);
    lv_obj_set_size(visualizer, 240, 100); // Set desired size
    lv_obj_align(visualizer, LV_ALIGN_CENTER, 0, 0);
    ```

2.  **Update its Values:**
    Periodically, provide an array of `uint8_t` values (0-255) representing the height of each bar. This is typically done in an `lv_timer` or by reading from a queue.
    ```cpp
    uint8_t bar_heights[VISUALIZER_BAR_COUNT];
    // Populate bar_heights with 32 values from your audio processing logic...
    
    audio_visualizer_set_values(visualizer, bar_heights);
    ```

3.  **Customize Appearance (Optional):**
    You can override the default gradient with a single solid color.
    ```cpp
    // Set all bars to a solid red color
    audio_visualizer_set_bar_color(visualizer, lv_color_hex(0xFF0000)); 
    ```