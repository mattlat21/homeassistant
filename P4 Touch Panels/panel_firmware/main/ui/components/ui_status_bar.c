#include "ui/components/ui_status_bar.h"

void ui_status_bar_set_bg(lv_obj_t *bar, lv_color_t color, lv_opa_t opa)
{
    if (bar == NULL) {
        return;
    }
    lv_obj_set_style_bg_color(bar, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, opa, LV_PART_MAIN);
}

void ui_status_bar_raise(lv_obj_t *bar)
{
    if (bar != NULL) {
        lv_obj_move_foreground(bar);
    }
}

lv_obj_t *ui_status_bar_create(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, lv_pct(100), UI_STATUS_BAR_HEIGHT);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
    ui_status_bar_set_bg(bar, lv_color_black(), LV_OPA_90);

    lv_obj_t *wifi = lv_label_create(bar);
    lv_label_set_text(wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(wifi, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(wifi, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(wifi, LV_ALIGN_RIGHT_MID, -12, 0);

    ui_status_bar_raise(bar);
    return bar;
}
