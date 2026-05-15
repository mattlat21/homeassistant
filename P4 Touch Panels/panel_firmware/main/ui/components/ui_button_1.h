#pragma once

#include <stdint.h>

#include "lvgl.h"

typedef void (*ui_button_1_cb_t)(void *user_data);

/**
 * Rounded semi-transparent button on a grid. Optional icon (ollie icon font) and/or name (Montserrat).
 * If both are set, icon is above text (column); if only one, it is centered.
 * @param icon_color If non-NULL, used for the icon label; otherwise UI_BOX_1_LABEL_COLOR.
 * @param text_color If non-NULL, used for the name label; otherwise UI_BOX_1_LABEL_COLOR.
 */
lv_obj_t *ui_button_1_create(lv_obj_t *parent, uint8_t row, uint8_t col, uint8_t row_span, uint8_t col_span,
                             const char *icon_utf8, const char *name, const lv_color_t *icon_color,
                             const lv_color_t *text_color, ui_button_1_cb_t click_cb, void *click_user_data);
