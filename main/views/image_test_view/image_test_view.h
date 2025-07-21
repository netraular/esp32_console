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
 * @brief A view to select and display a PNG image from the SD card using LVGL's built-in decoder.
 *
 * This class provides a user interface to browse the SD card for .png files.
 * It leverages LVGL's integrated LodePNG decoder and VFS support to directly
 * load an image from a file path.
 */
class ImageTestView : public View {
public:
    ImageTestView();
    ~ImageTestView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- UI Widgets ---
    lv_obj_t* info_label = nullptr;
    lv_obj_t* image_widget = nullptr;
    lv_obj_t* file_explorer_host_container = nullptr;

    // --- State ---
    std::string current_image_path;
    
    // --- UI Setup & Logic ---
    void create_initial_view();
    void show_file_explorer();
    void display_image_from_path(const char* path);
    void setup_initial_button_handlers();

    // --- Instance Methods for Actions ---
    void on_initial_ok_press();
    void on_initial_cancel_press();
    void on_file_selected(const char* path);
    void on_explorer_exit();

    // --- Static Callbacks for Button Manager & C Components (Bridge to instance methods) ---
    static void initial_ok_press_cb(void* user_data);
    static void initial_cancel_press_cb(void* user_data);
    static void file_selected_cb_c(const char* path, void* user_data);
    static void explorer_exit_cb_c(void* user_data);
    static void explorer_cleanup_event_cb(lv_event_t * e);
};

#endif // IMAGE_TEST_VIEW_H