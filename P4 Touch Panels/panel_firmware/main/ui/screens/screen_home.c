#include "ui/screens/screen_home.h"
#include "ui/ui_brand_gradient.h"
#include "ui/ui_layout.h"
#include "ui/components/ui_app_launcher_tile.h"
#include "ui/components/ui_status_bar.h"
#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/nav.h"
#include "bsp/display.h"

static void launcher_cb(app_id_t app, void *user_ctx)
{
    (void)user_ctx;
    nav_go_to(app);
}

/** Launcher: 3×3 cell grid, five tiles (row-major cells 0–4). */
#define HOME_GRID_N 3u

lv_obj_t *screen_home_create(lv_display_t *disp)
{
    (void)disp;
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    ui_brand_gradient_apply(scr);

    /* Launcher lives below the status bar so top padding matches side/bottom gaps (not eaten by the overlay bar). */
    const int32_t launcher_h = BSP_LCD_V_RES - UI_STATUS_BAR_HEIGHT;
    const int32_t gap = ui_layout_grid_gap_for_screen(UI_APP_LAUNCHER_TILE_EDGE_PX, UI_APP_LAUNCHER_TILE_EDGE_PX,
                                                      (int32_t)HOME_GRID_N, (int32_t)HOME_GRID_N, BSP_LCD_H_RES,
                                                      launcher_h);

    lv_obj_t *mainGrid = lv_obj_create(scr);
    lv_obj_remove_style_all(mainGrid);
    lv_obj_set_style_pad_all(mainGrid, gap, LV_PART_MAIN);
    lv_obj_set_size(mainGrid, BSP_LCD_H_RES, launcher_h);
    lv_obj_align(mainGrid, LV_ALIGN_TOP_MID, 0, UI_STATUS_BAR_HEIGHT);

    struct {
        const char *icon;
        const char *label;
        app_id_t app;
        const lv_font_t *icon_font; /* NULL → Home Assistant subset font */
    } const tiles[] = {
        { UI_HA_ICON_TEDDY_BEAR, "Ollie's Room", APP_OLLIE_ROOM, NULL },
        { UI_HA_ICON_GAUGE, "Dashboard", APP_DASHBOARD, NULL },
        { UI_HA_ICON_GATE, "Front Gate", APP_FRONT_GATE, NULL },
        { UI_HA_ICON_LIGHTBULB, "PipBoy", APP_PIPBOY, NULL },
        { LV_SYMBOL_SETTINGS, "Settings", APP_SETTINGS, &lv_font_montserrat_48 },
        { UI_HA_ICON_DESK, "Study", APP_STUDY, NULL },
    };

    for (unsigned i = 0; i < (unsigned)(sizeof(tiles) / sizeof(tiles[0])); i++) {
        const uint8_t row = (uint8_t)(i / HOME_GRID_N);
        const uint8_t col = (uint8_t)(i % HOME_GRID_N);
        int32_t x, y;
        if (!ui_layout_grid_rc_to_pos(gap, UI_APP_LAUNCHER_TILE_EDGE_PX, (uint8_t)HOME_GRID_N, row, col, &x, &y)) {
            continue;
        }
        const lv_font_t *icon_font =
            tiles[i].icon_font != NULL ? tiles[i].icon_font : &ui_font_home_assistant_icons_56;
        lv_obj_t *tile = ui_app_launcher_tile_create(mainGrid, tiles[i].icon, tiles[i].label, tiles[i].app, launcher_cb,
                                                     NULL, icon_font, LV_SCALE_NONE * 2);
        if (tile != NULL) {
            lv_obj_set_pos(tile, x, y);
        }
    }

    (void)ui_status_bar_create(scr);

    return scr;
}
