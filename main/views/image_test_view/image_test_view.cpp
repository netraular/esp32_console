#include "image_test_view.h"
#include "../view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "lvgl.h"

// --- Declaración externa de la imagen ---
extern "C" const lv_image_dsc_t icon_1;

// --- Handler de botón ---
static void handle_cancel_press() {
    view_manager_load_view(VIEW_ID_MENU);
}

// --- Función de creación de la vista ---
void image_test_view_create(lv_obj_t *parent) {
    // 1. Crear un título
    lv_obj_t *title_label = lv_label_create(parent);
    lv_label_set_text(title_label, "Test Image");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20); // Alinear arriba al centro

    // 2. Crear el objeto de imagen LVGL
    lv_obj_t *img = lv_img_create(parent);

    // 3. Asignar la fuente de la imagen desde nuestro archivo .c
    lv_img_set_src(img, &icon_1);
    
    // 4. Agrandar la imagen para que se vea mejor (opcional)
    // lv_img_set_zoom(img, 1024); // 256 es 100%, 512 es 200%.

    // 5. Centrar la imagen en la pantalla
    lv_obj_center(img);

    // 6. Crear una etiqueta de instrucciones
    lv_obj_t *info_label = lv_label_create(parent);
    lv_label_set_text(info_label, "Press Cancel to return");
    lv_obj_align(info_label, LV_ALIGN_BOTTOM_MID, 0, -20); // Alinear abajo al centro

    // 7. Registrar el handler del botón para volver al menú
    button_manager_register_view_handler(BUTTON_CANCEL, handle_cancel_press);
}