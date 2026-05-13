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

/** Launcher: 4 columns × 3 rows of tiles (10 apps); row-major order. */
#define HOME_GRID_COLS 4u
#define HOME_GRID_ROWS 3u
/** 200px × 4 columns does not fit 720px (needs 800px); use a cell size that fits with computed gap. */
#define HOME_LAUNCHER_TILE_EDGE_PX 166

lv_obj_t *screen_home_create(lv_display_t *disp)
{
    (void)disp;
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    ui_brand_gradient_apply(scr);

    /* Launcher lives below the status bar. Per-axis gaps + symmetric remainder on edges (no LV_MIN squish). */
    const int32_t launcher_h = BSP_LCD_V_RES - UI_STATUS_BAR_HEIGHT;
    int32_t gap_h = 0;
    int32_t gap_v = 0;
    int32_t pad_x0 = 0;
    int32_t pad_x1 = 0;
    int32_t pad_y0 = 0;
    int32_t pad_y1 = 0;
    ui_layout_grid_symmetric_outer_pads(HOME_LAUNCHER_TILE_EDGE_PX, (int32_t)HOME_GRID_COLS, BSP_LCD_H_RES, &gap_h,
                                        &pad_x0, &pad_x1);
    ui_layout_grid_symmetric_outer_pads(HOME_LAUNCHER_TILE_EDGE_PX, (int32_t)HOME_GRID_ROWS, launcher_h, &gap_v,
                                        &pad_y0, &pad_y1);

    lv_obj_t *mainGrid = lv_obj_create(scr);
    lv_obj_remove_style_all(mainGrid);
    lv_obj_set_style_pad_left(mainGrid, gap_h + pad_x0, LV_PART_MAIN);
    lv_obj_set_style_pad_right(mainGrid, gap_h + pad_x1, LV_PART_MAIN);
    lv_obj_set_style_pad_top(mainGrid, gap_v + pad_y0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(mainGrid, gap_v + pad_y1, LV_PART_MAIN);
    lv_obj_set_size(mainGrid, BSP_LCD_H_RES, launcher_h);
    lv_obj_align(mainGrid, LV_ALIGN_TOP_MID, 0, UI_STATUS_BAR_HEIGHT);

    struct {
        const char *icon;
        const char *label;
        app_id_t app;
        const lv_font_t *icon_font; /* NULL → Home Assistant subset font */
    } const tiles[] = {
        { UI_HA_ICON_TEDDY_BEAR, "Ollie's Room", APP_OLLIE_ROOM, NULL },
        { UI_HA_ICON_GATE, "Front Gate", APP_FRONT_GATE, NULL },
        { UI_HA_ICON_DESK, "Study", APP_STUDY, NULL },
        { LV_SYMBOL_HOME, "Front Door", APP_FRONT_DOOR, &lv_font_montserrat_48 },
        { LV_SYMBOL_IMAGE, "Kitchen", APP_KITCHEN, &lv_font_montserrat_48 },
        { LV_SYMBOL_AUDIO, "Studio", APP_STUDIO, &lv_font_montserrat_48 },
        { UI_HA_ICON_LIGHTBULB, "PipBoy", APP_PIPBOY, NULL },
        { UI_HA_ICON_GAUGE, "Car Sim", APP_DASHBOARD, NULL },
        { LV_SYMBOL_LIST, "About", APP_ABOUT, &lv_font_montserrat_48 },
        { LV_SYMBOL_SETTINGS, "Settings", APP_SETTINGS, &lv_font_montserrat_48 },
    };

    for (unsigned i = 0; i < (unsigned)(sizeof(tiles) / sizeof(tiles[0])); i++) {
        const uint8_t row = (uint8_t)(i / HOME_GRID_COLS);
        const uint8_t col = (uint8_t)(i % HOME_GRID_COLS);
        int32_t x, y;
        if (!ui_layout_grid_rc_to_pos_xy(gap_h, gap_v, HOME_LAUNCHER_TILE_EDGE_PX, row, col, &x, &y)) {
            continue;
        }
        const lv_font_t *icon_font =
            tiles[i].icon_font != NULL ? tiles[i].icon_font : &ui_font_home_assistant_icons_56;
        lv_obj_t *tile = ui_app_launcher_tile_create(mainGrid, tiles[i].icon, tiles[i].label, tiles[i].app, launcher_cb,
                                                     NULL, icon_font, LV_SCALE_NONE, HOME_LAUNCHER_TILE_EDGE_PX);
        if (tile != NULL) {
            lv_obj_set_pos(tile, x, y);
        }
    }

    (void)ui_status_bar_create(scr);

    return scr;
}
