#include "ui/screens/screen_hvac.h"

#include "bsp/display.h"
#include "ui/components/ui_status_bar.h"
#include "ui/ui_brand_gradient.h"

lv_obj_t *screen_hvac_create(lv_display_t *disp)
{
    (void)disp;
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    ui_brand_gradient_apply(scr);

    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "HVAC");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(lbl);

    (void)ui_status_bar_create(scr);

    return scr;
}
