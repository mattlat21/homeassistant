#include "ui/components/ui_app_launcher_tile.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "esp_timer.h"

// #region agent log
static void agent_debug_log(const char *run_id, const char *hypothesis_id, const char *location, const char *message,
                            const char *data_json)
{
    FILE *f = fopen("/Users/matt/Documents/Repos/GitHub/projects/ESP32 Screen Testing/screen_test_1/.cursor/debug-60072b.log", "a");
    if (f == NULL) {
        return;
    }
    int64_t ts = esp_timer_get_time() / 1000;
    fprintf(f,
            "{\"sessionId\":\"60072b\",\"runId\":\"%s\",\"hypothesisId\":\"%s\",\"location\":\"%s\",\"message\":\"%s\","
            "\"data\":%s,\"timestamp\":%" PRId64 "}\n",
            run_id, hypothesis_id, location, message, data_json, ts);
    fclose(f);
}
// #endregion

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
    // #region agent log
    if (caption != NULL && strcmp(caption, "Study") == 0) {
        char data[256];
        snprintf(data, sizeof(data),
                 "{\"caption\":\"Study\",\"app\":%d,\"icon_ptr\":\"%p\",\"font_ptr\":\"%p\",\"icon_b0\":%u,\"icon_b1\":%u,"
                 "\"icon_b2\":%u,\"icon_b3\":%u,\"scale\":%ld}",
                 (int)app, (const void *)icon_symbol, (const void *)icon_font,
                 icon_symbol != NULL ? (unsigned char)icon_symbol[0] : 0u,
                 icon_symbol != NULL ? (unsigned char)icon_symbol[1] : 0u,
                 icon_symbol != NULL ? (unsigned char)icon_symbol[2] : 0u,
                 icon_symbol != NULL ? (unsigned char)icon_symbol[3] : 0u, (long)icon_transform_scale);
        agent_debug_log("run-initial", "H1", "ui_app_launcher_tile.c:create-entry", "Study tile input", data);
    }
    // #endregion

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
    // #region agent log
    if (caption != NULL && strcmp(caption, "Study") == 0) {
        char data[256];
        const char *resolved_text = lv_label_get_text(icon);
        snprintf(data, sizeof(data),
                 "{\"resolved_text\":\"%s\",\"resolved_font_ptr\":\"%p\",\"fallback_used\":%s}",
                 resolved_text != NULL ? resolved_text : "",
                 (const void *)(icon_font != NULL ? icon_font : &lv_font_montserrat_24),
                 icon_font == NULL ? "true" : "false");
        agent_debug_log("run-initial", "H2", "ui_app_launcher_tile.c:after-label-set",
                        "Study tile label/font resolved", data);
    }
    // #endregion

    if (icon_transform_scale > 0 && icon_transform_scale != LV_SCALE_NONE) {
        lv_obj_add_flag(tile, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_set_style_transform_scale(icon, icon_transform_scale, LV_PART_MAIN);
        lv_obj_update_layout(tile);
        lv_obj_set_style_transform_pivot_x(icon, lv_obj_get_width(icon) / 2, LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(icon, lv_obj_get_height(icon) / 2, LV_PART_MAIN);
        // #region agent log
        if (caption != NULL && strcmp(caption, "Study") == 0) {
            char data[192];
            snprintf(data, sizeof(data), "{\"w\":%ld,\"h\":%ld}", (long)lv_obj_get_width(icon), (long)lv_obj_get_height(icon));
            agent_debug_log("run-initial", "H3", "ui_app_launcher_tile.c:after-scale",
                            "Study icon layout after transform", data);
        }
        // #endregion
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
