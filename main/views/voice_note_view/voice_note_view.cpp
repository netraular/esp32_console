#include "voice_note_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/audio_recorder/audio_recorder.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "esp_log.h"
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

static const char *TAG = "VOICE_NOTE_VIEW";

// --- Module-Scope UI Objects and State ---
static lv_obj_t* status_label;
static lv_obj_t* time_label;
static lv_obj_t* icon_label;
static lv_timer_t* ui_update_timer = NULL; // Must be initialized to NULL
static char current_filepath[256];

// --- Forward Declarations ---
static void ui_update_timer_cb(lv_timer_t* timer);
static void voice_note_view_delete_cb(lv_event_t * e);
static void update_ui_for_state(audio_recorder_state_t state);

/**
 * @brief Formats a time in seconds to a "MM:SS" string.
 */
static void format_time(char* buf, size_t buf_size, uint32_t time_s) {
    snprintf(buf, buf_size, "%02lu:%02lu", time_s / 60, time_s % 60);
}

/**
 * @brief Updates the UI elements based on the current recorder state.
 */
static void update_ui_for_state(audio_recorder_state_t state) {
    switch (state) {
        case RECORDER_STATE_IDLE:
            lv_label_set_text(status_label, "OK: Record | Right: Play Notes");
            lv_label_set_text(time_label, "00:00");
            lv_label_set_text(icon_label, LV_SYMBOL_AUDIO); // Microphone icon
            lv_obj_set_style_text_color(icon_label, lv_color_white(), 0);
            break;
        case RECORDER_STATE_RECORDING:
            lv_label_set_text(status_label, "Recording note...");
            lv_label_set_text(icon_label, LV_SYMBOL_STOP);
            lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_RED), 0);
            break;
        case RECORDER_STATE_SAVING:
            lv_label_set_text(status_label, "Saving note...");
            lv_label_set_text(icon_label, LV_SYMBOL_SAVE);
            lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
            break;
        case RECORDER_STATE_CANCELLING:
            lv_label_set_text(status_label, "Cancelling...");
            lv_label_set_text(icon_label, LV_SYMBOL_TRASH);
            lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_GREY), 0);
            break;
        case RECORDER_STATE_ERROR:
            lv_label_set_text(status_label, "Error! Check SD card.");
            lv_label_set_text(icon_label, LV_SYMBOL_WARNING);
            lv_obj_set_style_text_color(icon_label, lv_palette_main(LV_PALETTE_RED), 0);
            break;
    }
}

/**
 * @brief Timer callback to periodically refresh the UI with recorder status.
 */
static void ui_update_timer_cb(lv_timer_t* timer) {
    // Keep track of the state to only update the UI on change.
    static audio_recorder_state_t last_state = (audio_recorder_state_t)-1;
    audio_recorder_state_t current_state = audio_recorder_get_state();

    if (current_state != last_state) {
        ESP_LOGD(TAG, "Recorder state changed from %d to %d", last_state, current_state);
        update_ui_for_state(current_state);
        last_state = current_state;
    }

    // Continuously update the timer while recording.
    if (current_state == RECORDER_STATE_RECORDING) {
        char time_buf[16];
        format_time(time_buf, sizeof(time_buf), audio_recorder_get_duration_s());
        lv_label_set_text(time_label, time_buf);
    }
}

/**
 * @brief Handles the OK button press to start or stop recording.
 */
static void handle_ok_press(void* user_data) {
    audio_recorder_state_t state = audio_recorder_get_state();

    if (state == RECORDER_STATE_IDLE || state == RECORDER_STATE_ERROR) {
        // --- Start Recording ---
        if (!sd_manager_check_ready()) {
            ESP_LOGE(TAG, "SD card not ready. Aborting recording.");
            update_ui_for_state(RECORDER_STATE_ERROR);
            return;
        }

        // Ensure the '/sdcard/notes' directory exists.
        const char* mount_point = sd_manager_get_mount_point();
        char notes_dir[128];
        snprintf(notes_dir, sizeof(notes_dir), "%s/notes", mount_point);
        
        struct stat st;
        if (stat(notes_dir, &st) == -1) {
            ESP_LOGI(TAG, "Directory '%s' not found. Creating...", notes_dir);
            if (!sd_manager_create_directory(notes_dir)) {
                ESP_LOGE(TAG, "Failed to create directory '%s'", notes_dir);
                update_ui_for_state(RECORDER_STATE_ERROR);
                return;
            }
        }

        // Generate a filename based on the current timestamp.
        time_t now = time(NULL);
        struct tm* timeinfo = localtime(&now);
        char filename[64];
        strftime(filename, sizeof(filename), "note_%Y%m%d_%H%M%S.wav", timeinfo);

        snprintf(current_filepath, sizeof(current_filepath), "%s/%s", notes_dir, filename);

        ESP_LOGI(TAG, "Starting new voice note: %s", current_filepath);
        if (!audio_recorder_start(current_filepath)) {
            ESP_LOGE(TAG, "Failed to start audio recorder.");
            update_ui_for_state(RECORDER_STATE_ERROR);
        }
    } else if (state == RECORDER_STATE_RECORDING) {
        // --- Stop and Save Recording ---
        ESP_LOGI(TAG, "Stopping voice note recording and saving file.");
        audio_recorder_stop();
    }
}

/**
 * @brief Handles the Right button press to switch to the player view.
 */
static void handle_right_press(void* user_data) {
    audio_recorder_state_t state = audio_recorder_get_state();

    // Only allow switching views if the recorder is idle or in an error state.
    if (state == RECORDER_STATE_IDLE || state == RECORDER_STATE_ERROR) {
        ESP_LOGI(TAG, "Right press detected, loading voice note player.");
        view_manager_load_view(VIEW_ID_VOICE_NOTE_PLAYER);
    } else {
        ESP_LOGW(TAG, "Ignoring right press, recorder is busy (state: %d)", state);
    }
}

/**
 * @brief Handles the Cancel button press.
 * If recording, it cancels the recording. Otherwise, it exits to the main menu.
 */
static void handle_cancel_press(void* user_data) {
    audio_recorder_state_t state = audio_recorder_get_state();

    if (state == RECORDER_STATE_RECORDING) {
        ESP_LOGI(TAG, "Cancel pressed during recording. Discarding file.");
        audio_recorder_cancel();
    } else {
        ESP_LOGI(TAG, "Cancel pressed. Returning to menu.");
        view_manager_load_view(VIEW_ID_MENU);
    }
}

/**
 * @brief Event callback triggered when the view's main container is deleted.
 * This is the crucial cleanup step to prevent resource leaks and crashes.
 */
static void voice_note_view_delete_cb(lv_event_t * e) {
    ESP_LOGI(TAG, "Voice note view is being deleted. Cleaning up resources.");
    
    // Always delete the timer to prevent it from running after the view is gone.
    if (ui_update_timer) {
        lv_timer_delete(ui_update_timer);
        ui_update_timer = NULL;
    }

    // If the view is destroyed mid-recording (e.g., by the ON/OFF button),
    // cancel the recording to avoid leaving an orphaned, partial file.
    if (audio_recorder_get_state() == RECORDER_STATE_RECORDING) {
        ESP_LOGW(TAG, "View deleted during recording. Cancelling to prevent orphaned file.");
        audio_recorder_cancel();
    }
}

/**
 * @brief Public function to create all UI elements for the voice note view.
 */
void voice_note_view_create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Voice Note View.");

    // Create a main container for the view.
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // *** CRITICAL: Attach the cleanup callback to the container's delete event. ***
    lv_obj_add_event_cb(cont, voice_note_view_delete_cb, LV_EVENT_DELETE, NULL);

    // --- Create UI Widgets ---
    lv_obj_t* title_label = lv_label_create(cont);
    lv_label_set_text(title_label, "Voice Notes");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);

    icon_label = lv_label_create(cont);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_48, 0);

    time_label = lv_label_create(cont);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_28, 0);

    status_label = lv_label_create(cont);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, 0);

    // Set the initial UI state based on the recorder's current state.
    update_ui_for_state(audio_recorder_get_state());

    // Create a timer to periodically update the UI.
    ui_update_timer = lv_timer_create(ui_update_timer_cb, 250, NULL);

    // Register button handlers for this view.
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_ok_press, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, handle_right_press, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, NULL, true, nullptr); // No action for left button
}