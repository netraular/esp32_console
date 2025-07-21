#ifndef IMAGE_TEST_VIEW_H
#define IMAGE_TEST_VIEW_H

#include "../view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"
#include "lvgl.h"

/**
 * @brief A view to test the LittleFS by reading and displaying a text file.
 *
 * This class serves as a verification step. It attempts to read 'welcome.txt'
 * from the internal LittleFS partition and displays its content.
 */
class ImageTestView : public View {
public:
    ImageTestView();
    ~ImageTestView() override;
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