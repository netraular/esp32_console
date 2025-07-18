#include "speaker_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "components/file_explorer/file_explorer.h"
#include "components/audio_player_component/audio_player_component.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SPEAKER_TEST_VIEW";

// --- Module-Scope State ---
// Parent object for the view, typically the screen.
static lv_obj_t *view_parent_obj = nullptr;
// A label used to display informational messages.
static lv_obj_t *info_label = nullptr;

// --- Forward Declarations ---
static void create_initial_view();
static void show_file_explorer();
static void on_player_exit();
static void explorer_cleanup_cb(lv_event_t * e);

// --- Callbacks and Event Handlers ---

/**
 * @brief Event callback to clean up the file explorer resources.
 *
 * CRITICAL: The file_explorer component allocates memory and resources that are
 * not part of the LVGL object tree. This callback is attached to the
 * LV_EVENT_DELETE of the explorer's container object. When the container is
 * deleted (e.g., by lv_obj_clean when switching views), this event is triggered,
 * ensuring that file_explorer_destroy() is called to prevent resource leaks.
 * This is essential for robust view switching, especially when a view can be
 * closed at any time by the global ON/OFF button.
 */
static void explorer_cleanup_cb(lv_event_t * e) {
    ESP_LOGI(TAG, "Explorer container deleted. Calling file_explorer_destroy() to free resources.");
    file_explorer_destroy();
}

/**
 * @brief Callback for when the user exits the audio player component.
 *
 * This function is passed to the audio_player_component. When the user exits
 * the player, it cleans the screen and restores the initial view of the speaker test.
 */
static void on_player_exit() {
    ESP_LOGI(TAG, "Exiting audio player, returning to initial speaker test view.");
    // When the user exits the player, return to the initial view.
    // The player component is an LVGL child of the screen, so it will be
    // automatically deleted by the lv_obj_clean call inside create_initial_view.
    create_initial_view();
}

/**
 * @brief Callback for when a file is selected in the file explorer.
 *
 * Checks if the selected file is a .wav file. If so, it cleans the screen
 * (which destroys the file explorer via its cleanup event) and creates the
 * audio player component.
 *
 * @param path The full path of the selected file.
 */
static void on_audio_file_selected(const char *path) {
    const char *ext = strrchr(path, '.');
    if (ext && (strcasecmp(ext, ".wav") == 0)) {
        ESP_LOGI(TAG, "WAV file selected: %s. Starting player.", path);
        // Clean the current screen. This will trigger the explorer_cleanup_cb,
        // which correctly destroys the file_explorer instance.
        lv_obj_clean(view_parent_obj);

        // Create the audio player component. It's assumed that the component is
        // self-contained and will clean up its own resources (like audio tasks or timers)
        // when its main LVGL object is deleted (e.g., via an LV_EVENT_DELETE handler).
        audio_player_component_create(view_parent_obj, path, on_player_exit);
    } else {
        ESP_LOGW(TAG, "File selected is not a .wav file: %s", path);
        // Optional: Show a popup message to the user before returning to the explorer.
    }
}

/**
 * @brief Callback for when the user navigates back from the root of the file explorer.
 */
static void on_explorer_exit_from_root() {
    ESP_LOGI(TAG, "Exited file explorer from root. Returning to initial view.");
    // The call to create_initial_view() will clean the screen, which triggers
    // the deletion of the explorer's container and its associated cleanup callback.
    create_initial_view();
}

/**
 * @brief Button handler for the initial view's OK button.
 */
static void handle_ok_press_initial(void* user_data) {
    if (sd_manager_check_ready()) {
        show_file_explorer();
    } else if (info_label) {
        lv_label_set_text(info_label, "Failed to read SD card.\nCheck card and press OK to retry.");
    }
}

/**
 * @brief Button handler for the initial view's CANCEL button.
 */
static void handle_cancel_press_initial(void* user_data) {
    // Return to the main menu.
    view_manager_load_view(VIEW_ID_MENU);
}


// --- UI Creation Logic ---

/**
 * @brief Cleans the screen and displays the file explorer.
 *
 * This function creates a container for the file explorer and attaches the
 * essential cleanup callback to it, ensuring no resource leaks occur.
 */
static void show_file_explorer() {
    lv_obj_clean(view_parent_obj);

    // This container will host the explorer and holds the cleanup callback.
    lv_obj_t * explorer_container = lv_obj_create(view_parent_obj);
    lv_obj_remove_style_all(explorer_container);
    lv_obj_set_size(explorer_container, lv_pct(100), lv_pct(100));

    // Attach the cleanup function to the container's delete event.
    lv_obj_add_event_cb(explorer_container, explorer_cleanup_cb, LV_EVENT_DELETE, nullptr);

    // Create the file_explorer inside the container that now has the callback.
    // Pass nullptr for callbacks that are not needed (long_press, action).
    file_explorer_create(
        explorer_container,
        sd_manager_get_mount_point(),
        on_audio_file_selected,
        nullptr,
        nullptr,
        on_explorer_exit_from_root
    );
}

/**
 * @brief Cleans the screen and creates the initial "welcome" UI for this view.
 *
 * It sets up the title, an info label, and registers the initial button handlers.
 */
static void create_initial_view() {
    // This is the entry point for the view and also the return point from other states.
    // Cleaning the parent object ensures that any previous UI (explorer, player) is removed.
    lv_obj_clean(view_parent_obj);

    lv_obj_t *title_label = lv_label_create(view_parent_obj);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(title_label, "Speaker Test");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    info_label = lv_label_create(view_parent_obj);
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label);
    lv_label_set_text(info_label, "Press OK to select\na .wav audio file.");

    // Register handlers for this specific view state.
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_ok_press_initial, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press_initial, true, nullptr);

    // Nullify handlers for other buttons to prevent unexpected behavior from default handlers.
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, NULL, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, NULL, true, nullptr);
}

// --- Public API ---

void speaker_test_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Speaker Test View");
    view_parent_obj = parent;
    create_initial_view();
}