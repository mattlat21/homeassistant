#include "ui/components/ui_box_1.h"

void ui_box_1_style_apply(lv_obj_t *obj)
{
    if (obj == NULL) {
        return;
    }
    lv_obj_set_style_bg_color(obj, UI_BOX_1_BG_COLOR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, UI_BOX_1_BG_OPA, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, UI_BOX_CORNER_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
}

lv_obj_t *ui_box_1_create(lv_obj_t *parent, uint8_t row, uint8_t col, uint8_t row_span, uint8_t col_span)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    ui_box_1_style_apply(box);
    lv_obj_set_grid_cell(box, LV_GRID_ALIGN_STRETCH, col, col_span, LV_GRID_ALIGN_STRETCH, row, row_span);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    return box;
}
