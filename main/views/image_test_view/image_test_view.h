#ifndef IMAGE_TEST_VIEW_H
#define IMAGE_TEST_VIEW_H

#include "../view.h"
#include "../view_manager.h"
#include "components/file_explorer/file_explorer.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"
#include "lvgl.h"
#include <string>

/**
 * @brief A view to select and display a PNG image from the SD card using the libpng decoder.
 *
 * This class provides a user interface to browse the SD card for .png files.
 * It leverages LVGL's VFS support to directly load an image from a file path.
 * Upon successful loading, it displays the image dimensions.
 * @note For large images, ensure PSRAM is enabled and configured for LVGL memory
 * and libpng decompression buffers in menuconfig for optimal performance.
 */
class ImageTestView : public View {
public:
    ImageTestView();
    ~ImageTestView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- UI Widgets ---
    lv_obj_t* info_label = nullptr;               //!< Label for displaying general information or error messages.
    lv_obj_t* image_widget = nullptr;             //!< The LVGL image object for displaying the PNG.
    lv_obj_t* image_info_label = nullptr;         //!< A label to show the loaded image's properties (path, dimensions).
    lv_obj_t* file_explorer_host_container = nullptr; //!< A temporary container for the file explorer component.

    // --- State ---
    std::string current_image_path; //!< Stores the path of the currently displayed image.
    
    // --- UI Setup & Logic ---
    /**
     * @brief Creates the initial UI state (welcome message and prompts).
     */
    void create_initial_view();
    /**
     * @brief Clears the current screen and displays the file explorer.
     */
    void show_file_explorer();
    /**
     * @brief Loads and displays a PNG image from the given path.
     * @param path The full path to the PNG file on the SD card (e.g., "/sdcard/image.png").
     */
    void display_image_from_path(const char* path);
    /**
     * @brief Sets up button handlers for the initial view state.
     */
    void setup_initial_button_handlers();

    // --- Instance Methods for Actions (called by static callbacks) ---
    /**
     * @brief Handles the OK button press in the initial state (launches file explorer).
     */
    void on_initial_ok_press();
    /**
     * @brief Handles the Cancel button press (returns to menu or initial view).
     */
    void on_initial_cancel_press();
    /**
     * @brief Callback invoked when a file is selected in the file explorer.
     * @param path The full path of the selected file.
     */
    void on_file_selected(const char* path);
    /**
     * @brief Callback invoked when the file explorer is exited.
     */
    void on_explorer_exit();

    // --- Static Callbacks for Button Manager & C Components (Bridge to instance methods) ---
    static void initial_ok_press_cb(void* user_data);
    static void initial_cancel_press_cb(void* user_data);
    static void file_selected_cb_c(const char* path, void* user_data);
    static void explorer_exit_cb_c(void* user_data);
    /**
     * @brief Event callback for cleaning up the file explorer component when its parent is deleted.
     * This is registered to the `file_explorer_host_container`'s LV_EVENT_DELETE event.
     */
    static void explorer_cleanup_event_cb(lv_event_t * e);
};

#endif // IMAGE_TEST_VIEW_H