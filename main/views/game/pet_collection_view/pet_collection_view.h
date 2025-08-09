#ifndef PET_COLLECTION_VIEW_H
#define PET_COLLECTION_VIEW_H

#include "views/view.h"
#include "models/pet_data_model.h"
#include <vector>
#include "lvgl.h"

/**
 * @brief Displays the player's collection of discovered and collected pets.
 *
 * This view shows a grid of all available pet types, indicating their
 * status: unknown, discovered (seen but not obtained), or collected.
 * It supports navigation via left/right buttons.
 */
class PetCollectionView : public View {
public:
    PetCollectionView();
    ~PetCollectionView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- UI State ---
    int selected_index;
    std::vector<lv_obj_t*> pet_widgets;

    // --- UI Setup ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();
    
    lv_obj_t* create_pet_widget(lv_obj_t* parent, const PetCollectionEntry& entry, uint8_t col, uint8_t row);

    // --- UI Logic ---
    void update_selection_style();

    // --- Actions ---
    void go_back_to_menu();
    void on_left_press();
    void on_right_press();

    // --- Static Callbacks ---
    static void back_button_cb(void* user_data);
    static void left_press_cb(void* user_data);
    static void right_press_cb(void* user_data);
};

#endif // PET_COLLECTION_VIEW_H