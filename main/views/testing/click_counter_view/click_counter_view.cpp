#include "click_counter_view.h"
#include "views/view_manager.h"

static const char *TAG = "CLICK_COUNTER_VIEW";

// --- Constants Initialization ---
const char* ClickCounterView::SOUND_FILE_PATH = "/sdcard/sounds/bright_earn.wav";
const char* ClickCounterView::CLICK_COUNT_KEY = "click_count";

// --- Lifecycle Methods ---
ClickCounterView::ClickCounterView() {
    ESP_LOGI(TAG, "ClickCounterView constructed");
    // Load the persistent count from NVS when the view is created.
    if (!data_manager_get_u32(CLICK_COUNT_KEY, &click_count)) {
        ESP_LOGI(TAG, "No previous count found in NVS. Starting at 0.");
        click_count = 0;
    } else {
        ESP_LOGI(TAG, "Loaded count from NVS: %lu", click_count);
    }
}

ClickCounterView::~ClickCounterView() {
    ESP_LOGI(TAG, "ClickCounterView destructed");
    // Stop any running animations on the image object before it's deleted.
    if (coin_image) {
        lv_anim_del(coin_image, nullptr);
    }
    // Stop any sound that might be playing when leaving the view.
    audio_manager_stop();
    // No need to delete LVGL objects manually. The ViewManager will clean the parent,
    // which deletes all children widgets. Pointers will become invalid.
}

void ClickCounterView::create(lv_obj_t* parent) {
    ESP_LOGI(TAG, "Creating Click Counter view UI");
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);
    
    setup_ui(container);
    setup_button_handlers();
}

// --- UI Setup ---
void ClickCounterView::setup_ui(lv_obj_t* parent) {
    // Create a title label
    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Click Counter");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    // Create the main label to show the number
    count_label = lv_label_create(parent);
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_48, 0);
    lv_obj_center(count_label);
    update_counter_label(); // Show initial value

    // Create the image object for the coin pile
    coin_image = lv_img_create(parent);
    lv_img_set_src(coin_image, &coin_pile);
    lv_obj_align_to(coin_image, count_label, LV_ALIGN_OUT_TOP_MID, 0, -10);
    lv_obj_add_flag(coin_image, LV_OBJ_FLAG_HIDDEN);
}

void ClickCounterView::update_counter_label() {
    if (count_label) {
        lv_label_set_text_fmt(count_label, "%lu", click_count);
    }
}

// --- Button Handling ---
void ClickCounterView::setup_button_handlers() {
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, ClickCounterView::ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, ClickCounterView::cancel_press_cb, true, this);
}

// --- Instance Methods ---
void ClickCounterView::on_ok_press() {
    click_count++;
    update_counter_label();

    // Save the new value to NVS.
    if (!data_manager_set_u32(CLICK_COUNT_KEY, click_count)) {
        ESP_LOGE(TAG, "Failed to save click count to NVS!");
    }

    // Every 10 clicks, play a sound and show an animation.
    if (click_count > 0 && click_count % 10 == 0) {
        ESP_LOGI(TAG, "Count reached %lu, playing sound and showing animation.", click_count);
        audio_manager_play(SOUND_FILE_PATH);
        start_fade_out_animation();
    }
}

void ClickCounterView::on_cancel_press() {
    ESP_LOGI(TAG, "Cancel pressed, returning to menu.");
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Animation Logic ---
void ClickCounterView::start_fade_out_animation() {
    if (!coin_image) return;

    // Ensure any previous animation on this object is stopped.
    lv_anim_del(coin_image, nullptr);
    
    // Make the object visible and fully opaque before starting the new animation.
    lv_obj_clear_flag(coin_image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_img_opa(coin_image, LV_OPA_COVER, 0);

    // Configure the animation
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, coin_image);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_exec_cb(&a, anim_set_opacity_cb);
    lv_anim_set_ready_cb(&a, anim_ready_cb);

    // Start the animation
    lv_anim_start(&a);
    ESP_LOGD(TAG, "Starting fade-out animation.");
}

// --- Static Callbacks (Bridges) ---
void ClickCounterView::ok_press_cb(void* user_data) {
    static_cast<ClickCounterView*>(user_data)->on_ok_press();
}

void ClickCounterView::cancel_press_cb(void* user_data) {
    static_cast<ClickCounterView*>(user_data)->on_cancel_press();
}

// Static callback for the animation to set opacity.
void ClickCounterView::anim_set_opacity_cb(void* var, int32_t v) {
    lv_obj_set_style_img_opa(static_cast<lv_obj_t*>(var), (lv_opa_t)v, 0);
}

// Static callback executed when the fade-out animation is finished.
void ClickCounterView::anim_ready_cb(lv_anim_t* anim) {
    // The object being animated is stored in the 'var' field of the animation.
    lv_obj_add_flag(static_cast<lv_obj_t*>(anim->var), LV_OBJ_FLAG_HIDDEN);
}