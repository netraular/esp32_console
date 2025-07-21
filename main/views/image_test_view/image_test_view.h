#ifndef IMAGE_TEST_VIEW_H
#define IMAGE_TEST_VIEW_H

#include "../view.h" // Base class
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"
#include "lvgl.h"

// Forward declare the image data from the C file
extern "C" const lv_image_dsc_t icon_1;

/**
 * @brief A simple view to display a pre-compiled image asset.
 *
 * This class demonstrates how to load a static image asset that has been
 * converted to a C array and display it using an LVGL image widget. It provides
 * a single action to return to the main menu.
 */
class ImageTestView : public View {
public:
    /**
     * @brief Constructs the ImageTestView.
     */
    ImageTestView();

    /**
     * @brief Destroys the ImageTestView.
     */
    ~ImageTestView() override;

    /**
     * @brief Creates the UI widgets for the view.
     * @param parent The parent LVGL object to create the view on.
     */
    void create(lv_obj_t* parent) override;

private:
    // --- UI Setup & Logic ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();

    // --- Instance Method for Button Actions ---
    void on_cancel_press();

    // --- Static Callback for Button Manager (Bridge to instance method) ---
    static void cancel_press_cb(void* user_data);
};

#endif // IMAGE_TEST_VIEW_H