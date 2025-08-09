#ifndef PET_VIEW_H
#define PET_VIEW_H

#include "views/view.h"
#include "components/popup_manager/popup_manager.h" // For popup_result_t

/**
 * @brief A view to display the current pet's status and evolution.
 *
 * This view interacts with the PetManager to show the pet's current stage,
 * name, and care points. It includes debug controls to simulate progress.
 */
class PetView : public View {
public:
    PetView();
    ~PetView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- UI Widgets ---
    lv_obj_t* pet_display_obj = nullptr; // This will be an lv_image widget
    lv_obj_t* pet_name_label = nullptr;
    lv_obj_t* pet_points_label = nullptr;
    lv_obj_t* pet_time_label = nullptr;
    lv_obj_t* pet_cycle_label = nullptr;

    // --- Timer ---
    lv_timer_t* update_timer = nullptr;

    // --- UI Setup & Logic ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();
    void update_view();

    // --- Actions ---
    void add_care_points();
    void on_force_new_pet();
    void handle_force_new_pet_result(popup_result_t result);
    void go_back_to_menu();

    // --- Static Callbacks ---
    static void update_view_cb(lv_timer_t* timer);
    static void add_points_cb(void* user_data);
    static void force_new_pet_cb(void* user_data);
    static void force_new_pet_popup_cb(popup_result_t result, void* user_data);
    static void back_button_cb(void* user_data);
};

#endif // PET_VIEW_H