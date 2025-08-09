#ifndef PET_HUB_VIEW_H
#define PET_HUB_VIEW_H

#include "views/view.h"

/**
 * @brief A simple view that logs the pet collection status to the console.
 *
 * This view serves as a debug or informational screen. Upon creation, it
 * fetches the current pet collection from the PetManager and prints the
 * status of each pet (name, discovered, collected) to the ESP-IDF log.
 * The UI simply displays a confirmation message and allows the user
 * to return to the main menu.
 */
class PetHubView : public View {
public:
    PetHubView();
    ~PetHubView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- UI Setup ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();

    // --- Logic ---
    void log_collection_status();

    // --- Actions ---
    void go_back_to_menu();

    // --- Static Callbacks ---
    static void back_button_cb(void* user_data);
};

#endif // PET_HUB_VIEW_H