#include "ui/components/ui_app_launcher_tile.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    ui_app_launcher_tile_cb_t cb;
    void *user_ctx;
    app_id_t app;
} launcher_tile_ud_t;

static void tile_clicked(lv_event_t *e)
{
    launcher_tile_ud_t *ud = lv_event_get_user_data(e);
    if (ud != NULL && ud->cb != NULL) {
        ud->cb(ud->app, ud->user_ctx);
    }
}

lv_obj_t *ui_app_launcher_tile_create(lv_obj_t *parent, const char *icon_symbol,
                                      const char *caption, app_id_t app,
                                      ui_app_launcher_tile_cb_t cb, void *user_ctx,
                                      const lv_font_t *icon_font, int32_t icon_transform_scale)
{
    launcher_tile_ud_t *ud = malloc(sizeof(launcher_tile_ud_t));
    if (ud == NULL) {
        return NULL;
    }
    ud->cb = cb;
    ud->user_ctx = user_ctx;
    ud->app = app;

    lv_obj_t *tile = lv_obj_create(parent);
    lv_obj_remove_style_all(tile);
    lv_obj_set_size(tile, UI_APP_LAUNCHER_TILE_EDGE_PX, UI_APP_LAUNCHER_TILE_EDGE_PX);
    lv_obj_set_style_bg_color(tile, lv_color_hex(0xF2F2F7), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(tile, 44, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(tile, 16, LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_y(tile, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(tile, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tile, 24, LV_PART_MAIN);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    /* Center icon + caption as a block vertically and horizontally inside the tile. */
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tile, tile_clicked, LV_EVENT_CLICKED, ud);

    lv_obj_t *icon = lv_label_create(tile);
    lv_label_set_text(icon, icon_symbol != NULL ? icon_symbol : LV_SYMBOL_FILE);
    lv_obj_set_style_text_font(icon, icon_font != NULL ? icon_font : &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x007AFF), LV_PART_MAIN);
    lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    if (icon_transform_scale > 0 && icon_transform_scale != LV_SCALE_NONE) {
        lv_obj_add_flag(tile, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_set_style_transform_scale(icon, icon_transform_scale, LV_PART_MAIN);
        lv_obj_update_layout(tile);
        lv_obj_set_style_transform_pivot_x(icon, lv_obj_get_width(icon) / 2, LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(icon, lv_obj_get_height(icon) / 2, LV_PART_MAIN);
    }

    lv_obj_t *label = lv_label_create(tile);
    lv_label_set_text(label, caption != NULL ? caption : "");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(0x1C1C1E), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(label, lv_pct(100));
    /* Space between icon and caption (column flex). */
    lv_obj_set_style_margin_top(label, 28, LV_PART_MAIN);

    return tile;
}
