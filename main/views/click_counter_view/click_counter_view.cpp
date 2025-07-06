#include "click_counter_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "controllers/audio_manager/audio_manager.h"
#include "controllers/data_manager/data_manager.h" // <-- AÑADIR ESTE INCLUDE
#include "esp_log.h"

// Declarar la imagen desde el archivo .c
extern "C" {
    extern const lv_image_dsc_t coin_pile;
}

static const char *TAG = "CLICK_COUNTER_VIEW";

// --- Variables de estado de la vista ---
static lv_obj_t* count_label;
static lv_obj_t* coin_image;
static uint32_t click_count = 0; // <-- CAMBIADO A uint32_t PARA NVS
static const char* SOUND_FILE_PATH = "/sdcard/sounds/bright_earn.wav";
static const char* CLICK_COUNT_KEY = "click_count"; // <-- CLAVE PARA GUARDAR EN NVS

// --- Funciones internas ---

// Actualiza el texto de la etiqueta con el contador actual
static void update_counter_label() {
    if (count_label) {
        lv_label_set_text_fmt(count_label, "%lu", click_count); // <-- CAMBIADO A %lu para uint32_t
    }
}

// Callback que se ejecuta cuando la animación de desvanecimiento ha terminado.
static void anim_ready_cb(lv_anim_t* anim) {
    if (coin_image) {
        lv_obj_add_flag(coin_image, LV_OBJ_FLAG_HIDDEN);
    }
}

// Función wrapper para la animación
static void set_img_opacity_anim_cb(void *var, int32_t v) {
    lv_obj_set_style_img_opa((lv_obj_t*)var, (lv_opa_t)v, 0);
}

// Inicia la animación de desvanecimiento para la imagen
static void start_fade_out_animation() {
    if (!coin_image) return;

    // 1. Asegurarse de que el objeto es visible y completamente opaco antes de empezar.
    lv_anim_del(coin_image, NULL); 
    lv_obj_clear_flag(coin_image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_img_opa(coin_image, LV_OPA_COVER, 0);

    // 2. Configurar la animación
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, coin_image);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_exec_cb(&a, set_img_opacity_anim_cb); 
    lv_anim_set_ready_cb(&a, anim_ready_cb);

    // 3. Iniciar la animación
    lv_anim_start(&a);
    ESP_LOGD(TAG, "Iniciando animación de desvanecimiento.");
}

// --- Manejadores de botones ---

// Se ejecuta al presionar el botón OK
static void handle_ok_press(void* user_data) {
    click_count++;
    update_counter_label();

    // <-- INICIO: GUARDAR DATOS PERSISTENTES -->
    // Guarda el nuevo valor en NVS cada vez que se incrementa.
    if (!data_manager_set_u32(CLICK_COUNT_KEY, click_count)) {
        ESP_LOGE(TAG, "¡Fallo al guardar el contador de clics en NVS!");
    }
    // <-- FIN: GUARDAR DATOS PERSISTENTES -->

    // Comprueba si el contador es un múltiplo de 10
    if (click_count > 0 && click_count % 10 == 0) {
        ESP_LOGI(TAG, "Contador alcanzó %lu, reproduciendo sonido y mostrando animación.", click_count);
        
        audio_manager_play(SOUND_FILE_PATH);
        start_fade_out_animation();
    }
}

// Se ejecuta al presionar el botón Cancel
static void handle_cancel_press(void* user_data) {
    if (coin_image) {
        lv_anim_del(coin_image, NULL);
    }
    audio_manager_stop();
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Función pública para crear la vista ---

void click_counter_view_create(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Creando la vista del contador de clics");
    
    // <-- INICIO: CARGAR DATOS PERSISTENTES -->
    // Intenta cargar el contador desde NVS. Si falla (p. ej. primera vez que se ejecuta),
    // el valor por defecto será 0.
    if (!data_manager_get_u32(CLICK_COUNT_KEY, &click_count)) {
        ESP_LOGI(TAG, "No se encontró un contador previo en NVS. Empezando en 0.");
        click_count = 0;
    } else {
        ESP_LOGI(TAG, "Contador cargado desde NVS: %lu", click_count);
    }
    // <-- FIN: CARGAR DATOS PERSISTENTES -->

    // Crea una etiqueta para el título
    lv_obj_t* title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Click Counter");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);

    // Crea la etiqueta principal para mostrar el número
    count_label = lv_label_create(parent);
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_48, 0);
    lv_obj_center(count_label);
    update_counter_label(); // Muestra el valor inicial (cargado desde NVS o 0)

    // Crea el objeto de imagen para la pila de monedas
    coin_image = lv_img_create(parent);
    lv_img_set_src(coin_image, &coin_pile);
    lv_obj_align_to(coin_image, count_label, LV_ALIGN_OUT_TOP_MID, 0, -10);
    lv_obj_add_flag(coin_image, LV_OBJ_FLAG_HIDDEN);

    // Registra los manejadores de botones para esta vista
    button_manager_register_handler(BUTTON_OK,     BUTTON_EVENT_TAP, handle_ok_press, true, nullptr);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_press, true, nullptr);
    button_manager_register_handler(BUTTON_LEFT,   BUTTON_EVENT_TAP, NULL, true, nullptr);
    button_manager_register_handler(BUTTON_RIGHT,  BUTTON_EVENT_TAP, NULL, true, nullptr);
}