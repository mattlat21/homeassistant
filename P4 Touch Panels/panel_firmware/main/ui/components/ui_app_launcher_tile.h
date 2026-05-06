#pragma once

#include "lvgl.h"
#include "ui/nav.h"

/** Square edge length for the launcher tile (must match home grid cell size). */
#define UI_APP_LAUNCHER_TILE_EDGE_PX 200

typedef void (*ui_app_launcher_tile_cb_t)(app_id_t app, void *user_ctx);

/**
 * @param icon_font  Font for the icon label; if NULL, @c lv_font_montserrat_24 is used (LVGL symbols).
 * @param icon_transform_scale  0 = no extra scaling; otherwise @c lv_obj_set_style_transform_scale (256 = 100%,
 *                              512 = 200%, see @c LV_SCALE_NONE).
 */
lv_obj_t *ui_app_launcher_tile_create(lv_obj_t *parent, const char *icon_symbol,
                                      const char *caption, app_id_t app,
                                      ui_app_launcher_tile_cb_t cb, void *user_ctx,
                                      const lv_font_t *icon_font, int32_t icon_transform_scale);
