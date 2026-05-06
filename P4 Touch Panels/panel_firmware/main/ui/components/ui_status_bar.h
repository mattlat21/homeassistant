#pragma once

#include "lvgl.h"

/** Fixed height of the status bar (px). */
#define UI_STATUS_BAR_HEIGHT 35

/**
 * Create a full-width status bar along the top of @a parent (typically a screen).
 * Default background: black at 90% opacity. Wi‑Fi symbol on the right; more icons later.
 * Calls @ref ui_status_bar_raise so the bar paints above siblings created earlier.
 */
lv_obj_t *ui_status_bar_create(lv_obj_t *parent);

/** Set status bar background colour and opacity. */
void ui_status_bar_set_bg(lv_obj_t *bar, lv_color_t color, lv_opa_t opa);

/** Keep the bar above other children of the same parent (call after adding later widgets if needed). */
void ui_status_bar_raise(lv_obj_t *bar);
