#pragma once

#include <stdint.h>

#include "lvgl.h"
#include "ui/ui_visual_tokens.h"

/** Apply ui_box_1 background, radius, and border (shared by ui_button_1 / ui_single_selector_tab_1). */
void ui_box_1_style_apply(lv_obj_t *obj);

/**
 * Semi-transparent rounded box placed on a grid parent (LV_LAYOUT_GRID).
 */
lv_obj_t *ui_box_1_create(lv_obj_t *parent, uint8_t row, uint8_t col, uint8_t row_span, uint8_t col_span);
