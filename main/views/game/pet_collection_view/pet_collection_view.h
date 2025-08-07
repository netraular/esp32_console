#ifndef PET_COLLECTION_VIEW_H
#define PET_COLLECTION_VIEW_H

#include "views/view.h"
#include "models/pet_data_model.h"
#include <vector>

/**
 * @brief Displays the player's collection of discovered and collected pets.
 *
 * This view shows a grid of all available pet types, indicating their
 * status: unknown, discovered (seen but not obtained), or collected.
 */
class PetCollectionView : public View {
public:
    PetCollectionView();
    ~PetCollectionView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- UI Setup ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();
    
    /**
     * @brief Creates a single pet widget for the collection grid.
     * @param parent The parent grid object.
     * @param entry The collection data for this specific pet.
     */
    void create_pet_widget(lv_obj_t* parent, const PetCollectionEntry& entry);

    // --- Actions ---
    void go_back_to_menu();

    // --- Static Callbacks ---
    static void back_button_cb(void* user_data);
};

#endif // PET_COLLECTION_VIEW_H