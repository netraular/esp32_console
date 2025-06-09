# Audio Visualizer Component

## Description
A custom LVGL widget that displays a simple bar-style audio visualizer (also known as a spectrum analyzer or VU meter).

## Features
-   Customizable number of bars.
-   Customizable bar color.
-   Lightweight, uses LVGL's custom draw events.
-   Decoupled from the audio source; accepts an array of values to display.

## How to Use

1.  **Create the Widget:**
    Add the visualizer to any parent object, like a screen or a container.
    ```cpp
    #include "components/audio_visualizer/audio_visualizer.h"

    #define VISUALIZER_BAR_COUNT 8
    
    // ...
    lv_obj_t* visualizer = audio_visualizer_create(parent_container, VISUALIZER_BAR_COUNT);
    lv_obj_set_size(visualizer, 200, 80);
    lv_obj_align(visualizer, LV_ALIGN_CENTER, 0, 0);
    ```

2.  **Update its Values:**
    Periodically, provide an array of `uint8_t` values (0-255) representing the height of each bar. This is typically done in an `lv_timer`.
    ```cpp
    uint8_t bar_heights[VISUALIZER_BAR_COUNT] = {10, 50, 120, 200, 180, 100, 40, 5};
    audio_visualizer_set_values(visualizer, bar_heights);
    ```

3.  **Customize Appearance (Optional):**
    ```cpp
    audio_visualizer_set_bar_color(visualizer, lv_color_hex(0xFF0000)); // Set bars to red
    ```