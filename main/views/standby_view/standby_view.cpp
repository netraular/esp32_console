#include "standby_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/wifi_manager/wifi_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/power_manager/power_manager.h"
#include "components/status_bar_component/status_bar_component.h"
#include "esp_log.h"
#include <time.h>
#include <cstring> 

static const char *TAG = "STANDBY_VIEW";

// --- Configuración y variables para el control de volumen ---
#define VOLUME_REPEAT_PERIOD_MS 200

static lv_timer_t *volume_up_timer = NULL;
static lv_timer_t *volume_down_timer = NULL;

// --- Variables para los widgets de la vista ---
static lv_obj_t *center_time_label;
static lv_obj_t *center_date_label;
static lv_obj_t *loading_label;
static lv_timer_t *update_timer = NULL;
static bool is_time_synced = false;

// --- Estado para el popup de apagado ---
static lv_obj_t *shutdown_popup_container = NULL;
static lv_group_t *shutdown_popup_group = NULL;
static lv_style_t style_popup_focused;
static lv_style_t style_popup_normal;

// --- Prototipos ---
static void destroy_shutdown_popup();
static void handle_on_off_tap_for_sleep(void* user_data);
static void register_main_view_handlers();

// --- Tarea para actualizar el reloj central ---
static void update_center_clock_task(lv_timer_t *timer) {
    if (!center_time_label || !center_date_label || !loading_label) return;

    if (wifi_manager_is_connected()) {
        if (!is_time_synced) {
            is_time_synced = true;
            lv_obj_add_flag(loading_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(center_time_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(center_date_label, LV_OBJ_FLAG_HIDDEN);
        }
        char time_str[16];
        char date_str[64];
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);
        strftime(date_str, sizeof(date_str), "%A, %d %B", &timeinfo);
        lv_label_set_text(center_time_label, time_str);
        lv_label_set_text(center_date_label, date_str);
    } else {
        if (is_time_synced) {
            is_time_synced = false;
            lv_obj_clear_flag(loading_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(center_time_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(center_date_label, LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(loading_label, "Connecting...");
    }
}

// --- Manejadores de botones principales ---
static void handle_cancel_press(void *user_data) {
    ESP_LOGI(TAG, "CANCEL pressed, loading menu.");
    view_manager_load_view(VIEW_ID_MENU);
}

// --- NUEVO: Manejador para el TAP del botón ON/OFF ---
static void handle_on_off_tap_for_sleep(void* user_data) {
    ESP_LOGI(TAG, "ON/OFF TAP detected, entering light sleep.");
    // Detener cualquier timer de volumen activo antes de dormir para evitar problemas al despertar
    if (volume_up_timer) { lv_timer_del(volume_up_timer); volume_up_timer = NULL; }
    if (volume_down_timer) { lv_timer_del(volume_down_timer); volume_down_timer = NULL; }
    power_manager_enter_light_sleep();
}

// --- Lógica del Popup de Apagado ---

static void handle_popup_left_press(void *user_data) {
    if (shutdown_popup_group) lv_group_focus_prev(shutdown_popup_group);
}
static void handle_popup_right_press(void *user_data) {
    if (shutdown_popup_group) lv_group_focus_next(shutdown_popup_group);
}
static void handle_popup_cancel(void *user_data) {
    destroy_shutdown_popup();
}

static void handle_popup_ok(void *user_data) {
    if (!shutdown_popup_group) return;
    lv_obj_t *focused_btn = lv_group_get_focused(shutdown_popup_group);
    if (!focused_btn) return;

    lv_obj_t *label = lv_obj_get_child(focused_btn, 0);
    const char *action_text = lv_label_get_text(label);

    if (strcmp(action_text, "Turn Off") == 0) {
        ESP_LOGI(TAG, "User confirmed shutdown. Entering deep sleep.");
        power_manager_enter_deep_sleep();
    } else { 
        destroy_shutdown_popup();
    }
}

static void create_shutdown_popup() {
    if (shutdown_popup_container) return; 
    ESP_LOGI(TAG, "Creating shutdown confirmation popup.");

    lv_style_init(&style_popup_normal);
    lv_style_set_bg_color(&style_popup_normal, lv_color_white());
    lv_style_set_text_color(&style_popup_normal, lv_color_black());
    lv_style_set_border_color(&style_popup_normal, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_border_width(&style_popup_normal, 1);

    lv_style_init(&style_popup_focused);
    lv_style_set_bg_color(&style_popup_focused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_color(&style_popup_focused, lv_color_white());

    shutdown_popup_container = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(shutdown_popup_container);
    lv_obj_set_size(shutdown_popup_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(shutdown_popup_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(shutdown_popup_container, LV_OPA_70, 0);
    lv_obj_clear_flag(shutdown_popup_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *msgbox = lv_msgbox_create(shutdown_popup_container);
    lv_msgbox_add_title(msgbox, "Turn Off Device");
    lv_msgbox_add_text(msgbox, "Reset  button will need to be pressed to start the device again.");
    lv_obj_t* btn_cancel = lv_msgbox_add_footer_button(msgbox, "Cancel");
    lv_obj_t* btn_ok = lv_msgbox_add_footer_button(msgbox, "Turn Off");
    lv_obj_center(msgbox);
    lv_obj_set_width(msgbox, 200);
    
    lv_obj_add_style(btn_cancel, &style_popup_normal, LV_STATE_DEFAULT);
    lv_obj_add_style(btn_cancel, &style_popup_focused, LV_STATE_FOCUSED);
    lv_obj_add_style(btn_ok, &style_popup_normal, LV_STATE_DEFAULT);
    lv_obj_add_style(btn_ok, &style_popup_focused, LV_STATE_FOCUSED);

    shutdown_popup_group = lv_group_create();
    lv_group_add_obj(shutdown_popup_group, btn_cancel);
    lv_group_add_obj(shutdown_popup_group, btn_ok);
    lv_group_set_default(shutdown_popup_group);
    lv_group_focus_obj(btn_cancel); 

    button_manager_unregister_view_handlers(); 
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_popup_ok, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_popup_cancel, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, handle_popup_left_press, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, handle_popup_right_press, true, nullptr);
}

static void destroy_shutdown_popup() {
    if (!shutdown_popup_container) return;
    ESP_LOGI(TAG, "Destroying shutdown popup and restoring main handlers.");

    if (shutdown_popup_group) {
        if (lv_group_get_default() == shutdown_popup_group) lv_group_set_default(NULL);
        lv_group_del(shutdown_popup_group);
        shutdown_popup_group = NULL;
    }
    lv_obj_del(shutdown_popup_container);
    shutdown_popup_container = NULL;

    // Restaurar los manejadores de la vista principal
    register_main_view_handlers();
}

// --- Función centralizada para registrar los manejadores de la vista Standby ---
static void register_main_view_handlers() {
    button_manager_unregister_view_handlers();
    
    // --- Lógica del botón ON/OFF: TAP para dormir, LONG_PRESS para popup ---
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_LONG_PRESS_START, [](void*d){ create_shutdown_popup(); }, true, nullptr);
    button_manager_register_handler(BUTTON_ON_OFF, BUTTON_EVENT_TAP, handle_on_off_tap_for_sleep, true, nullptr);

    // --- Otros botones ---
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_LONG_PRESS_START, [](void*d){ audio_manager_volume_up(); status_bar_update_volume_display(); volume_up_timer = lv_timer_create([](lv_timer_t*t){audio_manager_volume_up(); status_bar_update_volume_display();}, VOLUME_REPEAT_PERIOD_MS, NULL);}, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT,  BUTTON_EVENT_LONG_PRESS_START, [](void*d){ audio_manager_volume_down(); status_bar_update_volume_display(); volume_down_timer = lv_timer_create([](lv_timer_t*t){audio_manager_volume_down(); status_bar_update_volume_display();}, VOLUME_REPEAT_PERIOD_MS, NULL);}, true, nullptr);
    
    // Detener los timers de repetición de volumen al soltar el botón
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_PRESS_UP, [](void*d){ if(volume_up_timer){lv_timer_del(volume_up_timer); volume_up_timer=NULL;}}, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT,  BUTTON_EVENT_PRESS_UP, [](void*d){ if(volume_down_timer){lv_timer_del(volume_down_timer); volume_down_timer=NULL;}}, true, nullptr);
}

static void cleanup_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        ESP_LOGI(TAG, "Standby view container is being deleted, cleaning up resources.");
        destroy_shutdown_popup();
        if (update_timer) {
            lv_timer_del(update_timer);
            update_timer = NULL;
        }
        if (volume_up_timer) { lv_timer_del(volume_up_timer); volume_up_timer = NULL; }
        if (volume_down_timer) { lv_timer_del(volume_down_timer); volume_down_timer = NULL; }
        
        center_time_label = NULL;
        center_date_label = NULL;
        loading_label = NULL;
        is_time_synced = false;
    }
}

void standby_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creating Standby View");
    is_time_synced = false;
    shutdown_popup_container = NULL;
    shutdown_popup_group = NULL;
    volume_up_timer = NULL;
    volume_down_timer = NULL;

    lv_obj_t *view_container = lv_obj_create(parent);
    lv_obj_remove_style_all(view_container);
    lv_obj_set_size(view_container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(view_container);

    status_bar_create(view_container);

    loading_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(loading_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(loading_label, "Loading...");
    lv_obj_center(loading_label);

    center_time_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(center_time_label, &lv_font_montserrat_48, 0);
    lv_obj_align(center_time_label, LV_ALIGN_CENTER, 0, -20);
    lv_obj_add_flag(center_time_label, LV_OBJ_FLAG_HIDDEN);

    center_date_label = lv_label_create(view_container);
    lv_obj_set_style_text_font(center_date_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(center_date_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(center_date_label, LV_ALIGN_CENTER, 0, 25);
    lv_obj_add_flag(center_date_label, LV_OBJ_FLAG_HIDDEN);
    
    update_timer = lv_timer_create(update_center_clock_task, 1000, NULL);
    update_center_clock_task(update_timer);

    lv_obj_add_event_cb(view_container, cleanup_event_cb, LV_EVENT_DELETE, NULL);

    // Registrar los manejadores de botones para la vista principal
    register_main_view_handlers();
}