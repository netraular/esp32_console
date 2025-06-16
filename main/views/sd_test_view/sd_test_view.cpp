#include "sd_test_view.h"
#include "../view_manager.h"
#include "../file_explorer/file_explorer.h" 
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"

static const char *TAG = "SD_TEST_VIEW";

// --- Static UI object pointers ---
static lv_obj_t *view_parent = NULL;
static lv_obj_t *info_label_widget = NULL;

// --- Function Prototypes ---
static void create_initial_sd_view();
static void show_file_explorer();
static void handle_ok_press_initial();
static void handle_cancel_press_initial();
static void on_file_selected(const char *path);
static void on_explorer_exit();

// --- Callbacks for the File Explorer ---

// Called when a file is selected inside the file explorer.
static void on_file_selected(const char *path) {
    ESP_LOGI(TAG, "File selected in SD Test: %s", path);
    // In a real implementation, you might do something with the file here.
    // For now, we just return to the initial screen of this test view.
    file_explorer_destroy();
    create_initial_sd_view();
}

// Called when the user exits the file explorer (e.g., presses Cancel at root).
static void on_explorer_exit() {
    file_explorer_destroy(); // Clean up explorer resources
    // Return to this view's initial screen, allowing another attempt to open the explorer.
    create_initial_sd_view();
}

// --- Logic to launch the file explorer ---
static void show_file_explorer() {
    // Clean the initial view (labels, etc.) before showing the explorer.
    lv_obj_clean(view_parent);

    // 1. Create a main container and title.
    lv_obj_t * main_cont = lv_obj_create(view_parent);
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, "SD Explorer");
    lv_obj_set_style_text_font(title_label, lv_theme_get_font_large(title_label), 0);
    lv_obj_set_style_margin_bottom(title_label, 10, 0);

    // 2. Create a container to hold the explorer.
    lv_obj_t * explorer_container = lv_obj_create(main_cont);
    lv_obj_remove_style_all(explorer_container);
    lv_obj_set_size(explorer_container, lv_pct(95), lv_pct(85));

    // 3. Create the file explorer instance.
    file_explorer_create(
        explorer_container,
        sd_manager_get_mount_point(),
        on_file_selected,
        on_explorer_exit
    );
}


// --- Button Handlers for the Initial View ---

// "OK" button on the initial screen attempts to mount the SD and open the explorer.
static void handle_ok_press_initial() {
    ESP_LOGI(TAG, "OK button pressed. Forcing SD card re-check...");

    // These potentially blocking operations are now executed in a button callback,
    // not during the view creation, keeping the UI responsive.
    sd_manager_unmount();
    if (sd_manager_mount()) {
        ESP_LOGI(TAG, "Mount successful. Opening the explorer.");
        show_file_explorer(); // On success, transition to the file explorer.
    } else {
        ESP_LOGW(TAG, "SD card mount failed.");
        // If it fails, update the label to inform the user.
        if (info_label_widget) {
            lv_label_set_text(info_label_widget, "Failed to read SD card.\n\n"
                                                 "Check the card and\n"
                                                 "press OK to retry.");
        }
    }
}

// "Cancel" button on the initial screen always returns to the main menu.
static void handle_cancel_press_initial() {
    view_manager_load_view(VIEW_ID_MENU);
}

// --- UI Creation for the Initial View ---
static void create_initial_sd_view() {
    lv_obj_clean(view_parent); // Clean up in case we are returning from the explorer.

    lv_obj_t *label = lv_label_create(view_parent);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_label_set_text(label, "SD Test");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    info_label_widget = lv_label_create(view_parent);
    lv_obj_set_style_text_align(info_label_widget, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(info_label_widget);
    lv_label_set_text(info_label_widget, "Press OK to\nopen the explorer");

    // Register button handlers for this initial screen.
    button_manager_register_view_handler(BUTTON_OK, handle_ok_press_initial);
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press_initial);
}


// --- Main Public Function ---
void sd_test_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating SD Test view (initial screen).");
    view_parent = parent; // Save the parent object.
    
    // The view creation is now a quick function call.
    // No blocking operations are performed here.
    create_initial_sd_view();
}