#include "popup_test_view.h"
#include "views/view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "components/status_bar_component/status_bar_component.h"
#include "esp_log.h"

static const char *TAG = "POPUP_TEST_VIEW";

// --- Constructor & Destructor ---
PopupTestView::PopupTestView() {
    ESP_LOGI(TAG, "PopupTestView constructed");
}

PopupTestView::~PopupTestView() {
    ESP_LOGI(TAG, "PopupTestView destructed");
    // --- FIX: Ensure ALL timers are deleted on destruction ---
    if (loading_timer) {
        lv_timer_del(loading_timer);
        loading_timer = nullptr;
    }
    if (info_update_timer) {
        lv_timer_del(info_update_timer);
        info_update_timer = nullptr;
    }
}

// --- View Lifecycle ---
void PopupTestView::create(lv_obj_t* parent) {
    container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(container);

    setup_ui(container);
    setup_button_handlers();
}

// --- UI & Handler Setup ---
void PopupTestView::setup_ui(lv_obj_t* parent) {
    status_bar_create(parent);

    info_label = lv_label_create(parent);
    lv_obj_set_width(info_label, LV_PCT(90));
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info_label, LV_ALIGN_CENTER, 0, 0);
    update_info_label();
}

void PopupTestView::setup_button_handlers() {
    button_manager_unregister_view_handlers();
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, ok_press_cb, true, this);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, cancel_press_cb, true, this);
}

void PopupTestView::update_info_label() {
    // If this is called, it means any pending info update is done or cancelled.
    if(info_update_timer) {
        lv_timer_del(info_update_timer);
        info_update_timer = nullptr;
    }

    switch (current_demo_state) {
        case STATE_DEMO_ALERT:
            lv_label_set_text(info_label, "Press OK to show an Alert popup.");
            break;
        case STATE_DEMO_CONFIRMATION:
            lv_label_set_text(info_label, "Press OK to show a Confirmation popup.");
            break;
        case STATE_DEMO_LOADING:
            lv_label_set_text(info_label, "Press OK to show a Loading popup for 3 seconds.");
            break;
        default:
            break;
    }
    lv_obj_align(info_label, LV_ALIGN_CENTER, 0, 0);
}

// --- Popup Logic ---

void PopupTestView::show_next_popup() {
    // Cancel any pending info update timer before showing a new popup
    if (info_update_timer) {
        lv_timer_del(info_update_timer);
        info_update_timer = nullptr;
    }
    update_info_label(); // Reset label to instruction

    switch (current_demo_state) {
        case STATE_DEMO_ALERT:
            show_alert_popup();
            break;
        case STATE_DEMO_CONFIRMATION:
            show_confirmation_popup();
            break;
        case STATE_DEMO_LOADING:
            show_loading_popup();
            break;
        default:
            break;
    }
}

void PopupTestView::show_alert_popup() {
    ESP_LOGI(TAG, "Showing alert popup");
    popup_manager_show_alert("Alert", "This is a simple alert.", popup_result_cb, this);
}

void PopupTestView::show_confirmation_popup() {
    ESP_LOGI(TAG, "Showing confirmation popup");
    popup_manager_show_confirmation(
        "Confirmation",
        "Please choose an action.",
        "Accept", "Decline",
        popup_result_cb, this
    );
}

void PopupTestView::show_loading_popup() {
    ESP_LOGI(TAG, "Showing loading popup");
    popup_manager_show_loading("Processing...");

    // Create a one-shot timer to simulate a background task
    loading_timer = lv_timer_create(loading_finished_cb, 3000, this);
    lv_timer_set_repeat_count(loading_timer, 1);
}

void PopupTestView::create_info_update_timer() {
    // Ensure no other timer is running
    if (info_update_timer) {
        lv_timer_del(info_update_timer);
    }
    info_update_timer = lv_timer_create(info_update_timer_cb, 2000, this);
    lv_timer_set_repeat_count(info_update_timer, 1);
}

void PopupTestView::handle_popup_result(popup_result_t result) {
    const char* result_str = "UNKNOWN";
    switch(result) {
        case POPUP_RESULT_PRIMARY:   result_str = "PRIMARY";   break;
        case POPUP_RESULT_SECONDARY: result_str = "SECONDARY"; break;
        case POPUP_RESULT_DISMISSED: result_str = "DISMISSED"; break;
    }
    ESP_LOGI(TAG, "Popup closed with result: %s", result_str);
    lv_label_set_text_fmt(info_label, "Last popup result:\n%s", result_str);
    lv_obj_align(info_label, LV_ALIGN_CENTER, 0, 0);

    // Advance to the next demo state
    current_demo_state = (PopupDemoState)(((int)current_demo_state + 1) % STATE_COUNT);

    // IMPORTANT: Re-enable the view's input handlers after the popup closes.
    setup_button_handlers();

    // Use a managed timer to switch back to the main instruction label.
    create_info_update_timer();
}

void PopupTestView::handle_loading_finished() {
    ESP_LOGI(TAG, "Loading finished");
    loading_timer = nullptr; // Timer will be deleted automatically by LVGL
    popup_manager_hide_loading();

    lv_label_set_text(info_label, "Loading finished!");
    lv_obj_align(info_label, LV_ALIGN_CENTER, 0, 0);

    // Advance to the next demo state
    current_demo_state = (PopupDemoState)(((int)current_demo_state + 1) % STATE_COUNT);

    // IMPORTANT: Re-enable the view's input handlers after loading is hidden.
    setup_button_handlers();

    // Use a managed timer to switch back to the main instruction label.
    create_info_update_timer();
}

// --- Instance Methods for Button Actions ---
void PopupTestView::on_ok_press() {
    show_next_popup();
}

void PopupTestView::on_cancel_press() {
    ESP_LOGI(TAG, "Cancel pressed, returning to menu.");
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Static Callback Bridges ---
void PopupTestView::ok_press_cb(void* user_data) {
    static_cast<PopupTestView*>(user_data)->on_ok_press();
}

void PopupTestView::cancel_press_cb(void* user_data) {
    static_cast<PopupTestView*>(user_data)->on_cancel_press();
}

void PopupTestView::popup_result_cb(popup_result_t result, void* user_data) {
    static_cast<PopupTestView*>(user_data)->handle_popup_result(result);
}

void PopupTestView::loading_finished_cb(lv_timer_t* timer) {
    static_cast<PopupTestView*>(lv_timer_get_user_data(timer))->handle_loading_finished();
}

// --- NEW STATIC CALLBACK ---
void PopupTestView::info_update_timer_cb(lv_timer_t* timer) {
    PopupTestView* self = static_cast<PopupTestView*>(lv_timer_get_user_data(timer));
    // The timer's job is done, so we nullify the handle in the view object.
    // The timer itself is deleted by LVGL since its repeat count is 1.
    self->info_update_timer = nullptr;
    self->update_info_label();
}