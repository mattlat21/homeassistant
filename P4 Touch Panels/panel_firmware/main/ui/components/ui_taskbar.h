#pragma once

#include <stdint.h>

#include "lvgl.h"

/** Maximum icons in one taskbar; extra items passed to @ref ui_taskbar_create are ignored. */
#define UI_TASKBAR_MAX_ITEMS 6u

/** Dock pill height (px). */
#define UI_TASKBAR_DOCK_HEIGHT 86
/** Gap between the dock pill and the bottom edge of the screen (px). */
#define UI_TASKBAR_BOTTOM_MARGIN 3
/** Gap between the dock pill and the left/right screen edges (px). */
#define UI_TASKBAR_SIDE_MARGIN 3

/** Total strip height: screens should reserve this much space at the bottom of their content. */
#define UI_TASKBAR_HEIGHT (UI_TASKBAR_DOCK_HEIGHT + UI_TASKBAR_BOTTOM_MARGIN)

/** Square touch target per icon (px). */
#define UI_TASKBAR_ITEM_SIZE 64
#define UI_TASKBAR_ITEM_RADIUS 20
#define UI_TASKBAR_DOCK_RADIUS 20

#define UI_TASKBAR_BG_COLOR lv_color_hex(0xFFFFFF)
/** White at ~20% opacity (frosted look over dark screens). */
#define UI_TASKBAR_BG_OPA ((lv_opa_t)(255 * 20 / 100))
#define UI_TASKBAR_ICON_COLOR lv_color_white()

typedef void (*ui_taskbar_item_cb_t)(void *user_data);

typedef struct {
    /** UTF-8 glyph: Home Assistant icon (`UI_HA_ICON_*`) or `LV_SYMBOL_*`. */
    const char *icon_utf8;
    /** NULL → `ui_font_home_assistant_icons_56`; use a Montserrat font for `LV_SYMBOL_*`. */
    const lv_font_t *icon_font;
    /** NULL → @ref UI_TASKBAR_ICON_COLOR. */
    const lv_color_t *icon_color;
    ui_taskbar_item_cb_t click_cb;
    void *click_user_data;
} ui_taskbar_item_t;

/**
 * Create an iPhone-style dock along the bottom of @a parent (typically a screen, not a laid-out container).
 * The returned object is a full-width transparent strip of @ref UI_TASKBAR_HEIGHT holding the rounded dock pill.
 * Calls @ref ui_taskbar_raise so the dock paints above siblings created earlier.
 * @return NULL if @a parent is NULL, or @a items is NULL with a non-zero @a count.
 */
lv_obj_t *ui_taskbar_create(lv_obj_t *parent, const ui_taskbar_item_t *items, uint8_t count);

/** Set dock pill background colour and opacity (e.g. dark translucent over light screens). */
void ui_taskbar_set_bg(lv_obj_t *bar, lv_color_t color, lv_opa_t opa);

/** Keep the dock above other children of the same parent (call after adding later widgets if needed). */
void ui_taskbar_raise(lv_obj_t *bar);

/** Item button at @a index (0-based, creation order), or NULL if out of range; use to restyle a single icon. */
lv_obj_t *ui_taskbar_get_item(lv_obj_t *bar, uint8_t index);
