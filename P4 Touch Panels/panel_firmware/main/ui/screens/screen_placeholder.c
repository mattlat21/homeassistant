#include "ui/screens/screen_placeholder.h"
#include "ui/components/ui_status_bar.h"
#include "bsp/display.h"

lv_obj_t *screen_placeholder_create(lv_display_t *disp, const char *title, bool with_status_bar)
{
    (void)disp;
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xF2F2F7), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, title != NULL ? title : "App");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_center(lbl);

    if (with_status_bar) {
        (void)ui_status_bar_create(scr);
    }

    return scr;
}
