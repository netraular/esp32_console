#ifndef LITTLEFS_TEST_VIEW_H
#define LITTLEFS_TEST_VIEW_H

#include "views/view.h"
#include "views/view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"
#include "lvgl.h"

/**
 * @brief A view to test the LittleFS by reading and displaying a text file.
 *
 * This class serves as a verification step. It attempts to read 'welcome.txt'
 * from the internal LittleFS partition and displays its content.
 */
class LittlefsTestView : public View {
public:
    LittlefsTestView();
    ~LittlefsTestView() override;
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

#endif // LITTLEFS_TEST_VIEW_H