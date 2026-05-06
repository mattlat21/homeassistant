#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

/** Maximum columns/rows for stack-built grid templates. */
#define UI_SCREEN_TEMPLATE_GRID_MAX 12

typedef struct {
    bool status_bar;
    uint8_t grid_cols;
    uint8_t grid_rows;
    int32_t pad_top;
    int32_t pad_right;
    int32_t pad_bottom;
    int32_t pad_left;
    int32_t row_gap;
    int32_t col_gap;
    lv_color_t bg_color;
    lv_opa_t bg_opa;
} ui_screen_template_params_t;

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *content;
    lv_obj_t *grid;
    lv_obj_t *status_bar;
} ui_screen_template_result_t;

/**
 * Defaults: status_bar true; grid 6×6; padding and gaps 5; bg white @ LV_OPA_COVER.
 */
void ui_screen_template_params_init_defaults(ui_screen_template_params_t *params);

/**
 * Build screen → content area → LVGL grid. Optional status bar on top.
 * @return false if grid dimensions are 0 or exceed UI_SCREEN_TEMPLATE_GRID_MAX, or @a out is NULL.
 */
bool ui_screen_template_create(lv_display_t *disp, const ui_screen_template_params_t *params,
                               ui_screen_template_result_t *out);
