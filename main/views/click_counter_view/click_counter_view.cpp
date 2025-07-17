#include "click_counter_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/data_manager/data_manager.h"
#include "esp_log.h"

// Declare the image from the .c file
extern "C" {
    extern const lv_image_dsc_t coin_pile;
}

static const char *TAG = "CLICK_COUNTER_VIEW";

// --- View state variables ---
static lv_obj_t* count_label = NULL;
static lv_obj_t* coin_image = NULL;
static uint32_t click_count = 0; // Use uint32_t for NVS
static const char* SOUND_FILE_PATH = "/sdcard/sounds/bright_earn.wav";
static const char* CLICK_COUNT_KEY = "click_count"; // Key for saving to NVS

// --- Forward declarations ---
static void cleanup_view(void);

// --- Internal Functions ---

// Updates the label's text with the current count
static void update_counter_label() {
    if (count_label) {
        lv_label_set_text_fmt(count_label, "%lu", click_count); // Use %lu for uint32_t
    }
}

// Callback executed when the fade-out animation is finished.
static void anim_ready_cb(lv_anim_t* anim) {
    if (coin_image) {
        lv_obj_add_flag(coin_image, LV_OBJ_FLAG_HIDDEN);
    }
}

// Wrapper function for the animation
static void set_img_opacity_anim_cb(void *var, int32_t v) {
    lv_obj_set_style_img_opa((lv_obj_t*)var, (lv_opa_t)v, 0);
}

// Starts the fade-out animation for the image
static void start_fade_out_animation() {
    if (!coin_image) return;

    // 1. Make sure the object is visible and fully opaque before starting.
    lv_anim_del(coin_image, NULL);
    lv_obj_clear_flag(coin_image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_img_opa(coin_image, LV_OPA_COVER, 0);

    // 2. Configure the animation
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, coin_image);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_exec_cb(&a, set_img_opacity_anim_cb);
    lv_anim_set_ready_cb(&a, anim_ready_cb);

    // 3. Start the animation
    lv_anim_start(&a);
    ESP_LOGD(TAG, "Starting fade-out animation.");
}

// --- Button Handlers ---

// Executed when the OK button is pressed
static void handle_ok_press(void* user_data) {
    click_count++;
    update_counter_label();

    // --- START: PERSISTENT DATA ---
    // Save the new value to NVS every time it's incremented.
    if (!data_manager_set_u32(CLICK_COUNT_KEY, click_count)) {
        ESP_LOGE(TAG, "Failed to save click count to NVS!");
    }
    // --- END: PERSISTENT DATA ---

    // Check if the count is a multiple of 10
    if (click_count > 0 && click_count % 10 == 0) {
        ESP_LOGI(TAG, "Count reached %lu, playing sound and showing animation.", click_count);

        audio_manager_play(SOUND_FILE_PATH);
        start_fade_out_animation();
    }
}

// Executed when the Cancel button is pressed
static void handle_cancel_press(void* user_data) {
    // Simply load the previous view. Cleanup will be handled by the DELETE event.
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Lifecycle Functions ---

// Central cleanup function for this view
static void cleanup_view(void) {
    ESP_LOGD(TAG, "Cleaning up Click Counter view resources.");
    if (coin_image) {
        lv_anim_del(coin_image, NULL); // Stop any running animations
    }
    audio_manager_stop(); // Stop any sound that might be playing

    // Nullify static pointers to prevent use-after-free issues
    count_label = NULL;
    coin_image = NULL;
}

// Event handler for the view's container
static void view_event_cb(lv_event_t * e) {
    // Use the official LVGL v9 getter function
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        ESP_LOGI(TAG, "Click Counter view is being deleted, cleaning up resources.");
        cleanup_view();
    }
}

// --- Public function to create the view ---

void click_counter_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Click Counter view");

    // --- START: LOAD PERSISTENT DATA ---
    // Try to load the count from NVS. If it fails (e.g., first run), default to 0.
    if (!data_manager_get_u32(CLICK_COUNT_KEY, &click_count)) {
        ESP_LOGI(TAG, "No previous count found in NVS. Starting at 0.");
        click_count = 0;
    } else {
        ESP_LOGI(TAG, "Loaded count from NVS: %lu", click_count);
    }
    // --- END: LOAD PERSISTENT DATA ---

    // Create a container for the view. This is crucial for event handling.
    lv_obj_t* view_container = lv_obj_create(parent);
    lv_obj_remove_style_all(view_container);
    lv_obj_set_size(view_container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(view_container);
    // Add the delete event callback to the container
    lv_obj_add_event_cb(view_container, view_event_cb, LV_EVENT_DELETE, NULL);

    // Create a title label (parented to the new container)
    lv_obj_t* title_label = lv_label_create(view_container);
    lv_label_set_text(title_label, "Click Counter");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    // Create the main label to show the number
    count_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_48, 0);
    lv_obj_center(count_label);
    update_counter_label(); // Show initial value (from NVS or 0)

    // Create the image object for the coin pile
    coin_image = lv_img_create(view_container);
    lv_img_set_src(coin_image, &coin_pile);
    lv_obj_align_to(coin_image, count_label, LV_ALIGN_OUT_TOP_MID, 0, -10);
    lv_obj_add_flag(coin_image, LV_OBJ_FLAG_HIDDEN);

    // Register button handlers for this view
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_ok_press, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, NULL, true, nullptr); // No action
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, NULL, true, nullptr); // No action
}