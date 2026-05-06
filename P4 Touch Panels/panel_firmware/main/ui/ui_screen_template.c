#include "ui/ui_screen_template.h"

#include "bsp/display.h"
#include "ui/components/ui_status_bar.h"

void ui_screen_template_params_init_defaults(ui_screen_template_params_t *params)
{
    if (params == NULL) {
        return;
    }
    params->status_bar = true;
    params->grid_cols = 6;
    params->grid_rows = 6;
    params->pad_top = 15;
    params->pad_right = 15;
    params->pad_bottom = 15;
    params->pad_left = 15;
    params->row_gap = 15;
    params->col_gap = 15;
    params->bg_color = lv_color_white();
    params->bg_opa = LV_OPA_COVER;
}

bool ui_screen_template_create(lv_display_t *disp, const ui_screen_template_params_t *params,
                               ui_screen_template_result_t *out)
{
    (void)disp;
    if (params == NULL || out == NULL) {
        return false;
    }
    if (params->grid_cols == 0 || params->grid_rows == 0 || params->grid_cols > UI_SCREEN_TEMPLATE_GRID_MAX ||
        params->grid_rows > UI_SCREEN_TEMPLATE_GRID_MAX) {
        return false;
    }

    /*
     * LVGL stores pointers to column/row descriptor arrays (see lv_obj_set_grid_dsc_array → style).
     * Stack arrays would dangle after this function returns and crash in count_tracks on next layout.
     */
    lv_coord_t *col_dsc =
        (lv_coord_t *)lv_malloc((size_t)(params->grid_cols + 1) * sizeof(lv_coord_t));
    lv_coord_t *row_dsc =
        (lv_coord_t *)lv_malloc((size_t)(params->grid_rows + 1) * sizeof(lv_coord_t));
    if (col_dsc == NULL || row_dsc == NULL) {
        if (col_dsc != NULL) {
            lv_free(col_dsc);
        }
        if (row_dsc != NULL) {
            lv_free(row_dsc);
        }
        return false;
    }
    for (unsigned c = 0; c < params->grid_cols; c++) {
        col_dsc[c] = LV_GRID_FR(1);
    }
    col_dsc[params->grid_cols] = LV_GRID_TEMPLATE_LAST;
    for (unsigned r = 0; r < params->grid_rows; r++) {
        row_dsc[r] = LV_GRID_FR(1);
    }
    row_dsc[params->grid_rows] = LV_GRID_TEMPLATE_LAST;

    out->screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(out->screen);
    lv_obj_set_size(out->screen, BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_obj_set_style_bg_color(out->screen, params->bg_color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(out->screen, params->bg_opa, LV_PART_MAIN);

    const lv_coord_t content_y = params->status_bar ? (lv_coord_t)UI_STATUS_BAR_HEIGHT : 0;
    const lv_coord_t content_h = (lv_coord_t)(BSP_LCD_V_RES - content_y);

    out->content = lv_obj_create(out->screen);
    lv_obj_remove_style_all(out->content);
    lv_obj_set_size(out->content, BSP_LCD_H_RES, content_h);
    lv_obj_set_pos(out->content, 0, content_y);

    out->grid = lv_obj_create(out->content);
    lv_obj_remove_style_all(out->grid);
    lv_obj_set_size(out->grid, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(out->grid, LV_LAYOUT_GRID);

    lv_obj_set_grid_dsc_array(out->grid, col_dsc, row_dsc);

    lv_obj_set_style_pad_top(out->grid, params->pad_top, LV_PART_MAIN);
    lv_obj_set_style_pad_right(out->grid, params->pad_right, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(out->grid, params->pad_bottom, LV_PART_MAIN);
    lv_obj_set_style_pad_left(out->grid, params->pad_left, LV_PART_MAIN);
    lv_obj_set_style_pad_row(out->grid, params->row_gap, LV_PART_MAIN);
    lv_obj_set_style_pad_column(out->grid, params->col_gap, LV_PART_MAIN);

    if (params->status_bar) {
        out->status_bar = ui_status_bar_create(out->screen);
    } else {
        out->status_bar = NULL;
    }

    return true;
}
