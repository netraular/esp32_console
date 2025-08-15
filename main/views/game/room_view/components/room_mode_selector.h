#ifndef ROOM_MODE_SELECTOR_H
#define ROOM_MODE_SELECTOR_H

#include "lvgl.h"
#include <functional>

// Define the modes available in the room view.
// This enum is also used by RoomView.
enum class RoomMode {
    CURSOR,
    PET,
    DECORATE
};

/**
 * @brief A UI component to select the active mode for the RoomView.
 *
 * This class creates and manages a modal list that allows the user
 * to switch between different room interaction modes like controlling a cursor,
 * watching a pet, or entering a decoration mode.
 */
class RoomModeSelector {
public:
    /**
     * @brief Constructs the mode selector.
     * @param parent The parent LVGL object to create the menu on.
     * @param on_mode_selected_cb A callback function that is invoked when a mode is chosen.
     * @param on_cancel_cb A callback function that is invoked when the menu is cancelled.
     */
    RoomModeSelector(lv_obj_t* parent, std::function<void(RoomMode)> on_mode_selected_cb, std::function<void()> on_cancel_cb);
    
    /**
     * @brief Destroys the mode selector and its associated LVGL objects.
     */
    ~RoomModeSelector();

    /**
     * @brief Shows the mode selector menu and registers its button handlers.
     */
    void show();

    /**
     * @brief Hides the mode selector menu and unregisters its button handlers.
     */
    void hide();

    /**
     * @brief Checks if the mode selector is currently visible.
     * @return True if the menu is visible, false otherwise.
     */
    bool is_visible() const;

private:
    void create_ui(lv_obj_t* parent);
    void setup_button_handlers();
    void remove_button_handlers();
    void init_styles();
    void reset_styles();
    
    void on_nav_up();
    void on_nav_down();
    void on_select();
    void on_cancel();

    // --- LVGL Members ---
    lv_obj_t* container = nullptr;
    lv_group_t* input_group = nullptr;
    lv_style_t style_focused;
    bool styles_initialized = false;

    // --- State & Callbacks ---
    std::function<void(RoomMode)> on_mode_selected_callback;
    std::function<void()> on_cancel_callback;

    // --- Static callbacks for button manager ---
    static void handle_nav_up_cb(void* user_data);
    static void handle_nav_down_cb(void* user_data);
    static void handle_select_cb(void* user_data);
    static void handle_cancel_cb(void* user_data);
};

#endif // ROOM_MODE_SELECTOR_H