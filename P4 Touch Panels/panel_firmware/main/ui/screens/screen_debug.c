#include "ui/screens/screen_debug.h"

#include <inttypes.h>
#include <stdio.h>

#include "bsp/display.h"
#include "sys_debug.h"
#include "ui/ui_brand_gradient.h"
#include "ui/components/ui_taskbar.h"

static lv_obj_t *s_lbl_boot;
static lv_obj_t *s_lbl_heap;
static lv_obj_t *s_lbl_internal;
static lv_obj_t *s_lbl_spiram;
static lv_obj_t *s_lbl_dma;
static lv_timer_t *s_refresh_timer;

static void debug_set_label(lv_obj_t *lbl, const char *text)
{
    if (lbl != NULL && text != NULL) {
        lv_label_set_text(lbl, text);
    }
}

static void debug_refresh_labels(void)
{
    sys_debug_snapshot_t snap;
    sys_debug_get_snapshot(&snap);

    char line[160];
    char a[32], b[32], c[32];

    snprintf(line, sizeof(line), "Boot: #%" PRIu32 "  reason: %s", snap.boot_count,
             snap.restart_reason != NULL ? snap.restart_reason : "?");
    debug_set_label(s_lbl_boot, line);

    sys_debug_format_bytes(snap.heap_free, a, sizeof(a));
    sys_debug_format_bytes(snap.heap_min_free, b, sizeof(b));
    snprintf(line, sizeof(line), "Heap free: %s  (min: %s)", a, b);
    debug_set_label(s_lbl_heap, line);

    sys_debug_format_bytes(snap.internal_free, a, sizeof(a));
    sys_debug_format_bytes(snap.internal_total, b, sizeof(b));
    sys_debug_format_bytes(snap.internal_largest, c, sizeof(c));
    snprintf(line, sizeof(line), "Internal: %s free / %s  (largest %s)", a, b, c);
    debug_set_label(s_lbl_internal, line);

    sys_debug_format_bytes(snap.spiram_free, a, sizeof(a));
    sys_debug_format_bytes(snap.spiram_total, b, sizeof(b));
    sys_debug_format_bytes(snap.spiram_largest, c, sizeof(c));
    snprintf(line, sizeof(line), "SPIRAM: %s free / %s  (largest %s)", a, b, c);
    debug_set_label(s_lbl_spiram, line);

    sys_debug_format_bytes(snap.dma_free, a, sizeof(a));
    snprintf(line, sizeof(line), "DMA free: %s", a);
    debug_set_label(s_lbl_dma, line);
}

static void debug_timer_cb(lv_timer_t *t)
{
    (void)t;
    debug_refresh_labels();
}

static void debug_screen_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SCREEN_LOADED) {
        debug_refresh_labels();
        if (s_refresh_timer == NULL) {
            s_refresh_timer = lv_timer_create(debug_timer_cb, 1000, NULL);
        } else {
            lv_timer_resume(s_refresh_timer);
        }
    } else if (code == LV_EVENT_SCREEN_UNLOADED) {
        if (s_refresh_timer != NULL) {
            lv_timer_pause(s_refresh_timer);
        }
    }
}

static lv_obj_t *debug_make_label(lv_obj_t *panel)
{
    lv_obj_t *lbl = lv_label_create(panel);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x1C1C1E), LV_PART_MAIN);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(lbl, lv_pct(100));
    return lbl;
}

lv_obj_t *screen_debug_create(lv_display_t *disp)
{
    (void)disp;
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    (void)ui_brand_framed_panel_create(scr);
    lv_obj_add_event_cb(scr, debug_screen_event, LV_EVENT_ALL, NULL);

    lv_obj_t *panel = lv_obj_create(scr);
    lv_obj_remove_style_all(panel);
    lv_obj_set_width(panel, lv_pct(92));
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, -(UI_TASKBAR_HEIGHT / 2));
    lv_obj_set_style_pad_all(panel, 28, LV_PART_MAIN);
    lv_obj_set_style_pad_row(panel, 14, LV_PART_MAIN);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 24, LV_PART_MAIN);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_26, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0x1C1C1E), LV_PART_MAIN);
    lv_label_set_text(title, "Debug");

    s_lbl_boot = debug_make_label(panel);
    s_lbl_heap = debug_make_label(panel);
    s_lbl_internal = debug_make_label(panel);
    s_lbl_spiram = debug_make_label(panel);
    s_lbl_dma = debug_make_label(panel);

    debug_refresh_labels();

    (void)ui_taskbar_attach_standard(scr);
    return scr;
}
