#pragma once

#include <stdint.h>
#include "lvgl.h"

/** Optional press handler; @a user_data is the pointer passed at create time. */
typedef void (*ui_grid_icon_button_cb_t)(void *user_data);

lv_obj_t *ui_grid_icon_button_create(lv_obj_t *parent, uint8_t row, uint8_t col,
                                     uint8_t row_span, uint8_t col_span, const char *icon_symbol,
                                     ui_grid_icon_button_cb_t click_cb, void *click_user_data);
