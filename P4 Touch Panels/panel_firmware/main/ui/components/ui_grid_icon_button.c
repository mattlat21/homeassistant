#include "ui/components/ui_grid_icon_button.h"

typedef struct {
    uint8_t row;
    uint8_t col;
    uint8_t row_span;
    uint8_t col_span;
    ui_grid_icon_button_cb_t click_cb;
    void *click_user_data;
} grid_cell_meta_t;

static void grid_meta_free(lv_event_t *e)
{
    grid_cell_meta_t *meta = lv_event_get_user_data(e);
    if (meta != NULL) {
        lv_free(meta);
    }
}

static void grid_btn_click(lv_event_t *e)
{
    grid_cell_meta_t *meta = lv_event_get_user_data(e);
    if (meta != NULL && meta->click_cb != NULL) {
        meta->click_cb(meta->click_user_data);
    }
}

lv_obj_t *ui_grid_icon_button_create(lv_obj_t *parent, uint8_t row, uint8_t col,
                                     uint8_t row_span, uint8_t col_span, const char *icon_symbol,
                                     ui_grid_icon_button_cb_t click_cb, void *click_user_data)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xE5E5EA), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 16, LV_PART_MAIN);
    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, col_span,
                         LV_GRID_ALIGN_STRETCH, row, row_span);

    grid_cell_meta_t *meta = lv_malloc(sizeof(grid_cell_meta_t));
    if (meta != NULL) {
        meta->row = row;
        meta->col = col;
        meta->row_span = row_span;
        meta->col_span = col_span;
        meta->click_cb = click_cb;
        meta->click_user_data = click_user_data;
        lv_obj_set_user_data(btn, meta);
        lv_obj_add_event_cb(btn, grid_btn_click, LV_EVENT_CLICKED, meta);
        lv_obj_add_event_cb(btn, grid_meta_free, LV_EVENT_DELETE, meta);
    }

    lv_obj_t *icon = lv_label_create(btn);
    lv_label_set_text(icon, icon_symbol != NULL ? icon_symbol : LV_SYMBOL_DUMMY);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_center(icon);

    return btn;
}
