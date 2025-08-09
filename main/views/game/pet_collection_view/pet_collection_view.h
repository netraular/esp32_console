#ifndef PET_COLLECTION_VIEW_H
#define PET_COLLECTION_VIEW_H

#include "views/view.h"
#include <vector>

/**
 * @brief A view that displays a visual encyclopedia of all available pets.
 *
 * This view uses a custom scrollable container with manually managed "tiles"
 * for each pet to optimize performance. The selected item is highlighted with
 * a border, and the list scrolls automatically upon navigation.
 */
class PetCollectionView : public View {
public:
    PetCollectionView();
    ~PetCollectionView() override;
    void create(lv_obj_t* parent) override;

private:
    // --- UI Widgets ---
    lv_obj_t* scrollable_container = nullptr;
    lv_style_t style_focus;

    // --- State ---
    std::vector<lv_obj_t*> tile_items; // To hold pointers to custom tile objects for navigation
    int selected_index = -1;           // -1 means no selection

    // --- UI Setup & Logic ---
    void setup_ui(lv_obj_t* parent);
    void populate_container();
    void setup_button_handlers();
    void update_selection();

    // --- Actions ---
    void on_nav_up();
    void on_nav_down();
    void go_back_to_menu();

    // --- Static Callbacks for Button Manager ---
    static void nav_up_cb(void* user_data);
    static void nav_down_cb(void* user_data);
    static void back_button_cb(void* user_data);
};

#endif // PET_COLLECTION_VIEW_H