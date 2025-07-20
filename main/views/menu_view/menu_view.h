#ifndef MENU_VIEW_H
#define MENU_VIEW_H

#include "../view.h"
#include "../view_manager.h" // For view_id_t enum
#include "lvgl.h"

/**
 * @brief Represents the main menu screen of the application.
 *
 * This class encapsulates the logic for displaying a list of selectable
 * application views and handling navigation between them. It is managed
 * by the ViewManager.
 */
class MenuView : public View {
public:
    /**
     * @brief Constructs a MenuView object.
     */
    MenuView();

    /**
     * @brief Destroys the MenuView object.
     *
     * This is called automatically by the ViewManager when switching views.
     * The destructor is trivial here, as this view has no dynamic resources
     * to clean up (LVGL objects are handled by the manager).
     */
    ~MenuView() override;

    /**
     * @brief Creates the UI elements for the menu view.
     *
     * This method is called by the ViewManager after the view is instantiated.
     * It sets up the UI and registers the necessary button handlers.
     *
     * @param parent The parent LVGL object to create the view on.
     */
    void create(lv_obj_t* parent) override;

private:
    // --- UI and State Members ---
    lv_obj_t* main_label = nullptr;
    int selected_view_index = 0;

    // --- Static Menu Data ---
    // These are static because they are the same for all instances of the menu.
    static const char* view_options[];
    static const view_id_t view_ids[];
    static const int num_options;

    // --- UI Setup ---
    void setup_ui(lv_obj_t* parent);
    void setup_button_handlers();
    void update_menu_label();

    // --- Instance Methods for Button Actions ---
    void on_left_press();
    void on_right_press();
    void on_ok_press();
    void on_cancel_press();

    // --- Static Callbacks for Button Manager ---
    // These static functions act as a bridge to call the instance methods.
    static void handle_left_press_cb(void* user_data);
    static void handle_right_press_cb(void* user_data);
    static void handle_ok_press_cb(void* user_data);
    static void handle_cancel_press_cb(void* user_data);
};

#endif // MENU_VIEW_H