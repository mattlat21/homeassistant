#pragma once

/**
 * @file ui_gauge_1.h
 *
 * @c gauge_1 — reusable semi-circular gauge (car-dial style) for multiple screens.
 *
 * Layout: translucent white circle → @c lv_arc (0–100%) → centered text label.
 * Update the label and/or arc at runtime with the @c ui_gauge_1_set_* helpers.
 */

#include <stdint.h>

#include "lvgl.h"

/**
 * Builds the widget tree and returns the root @c lv_obj_t * (the circular dial).
 *
 * Child objects (@c lv_arc, @c lv_label) are owned by LVGL; private pointers are
 * stored in heap-allocated user_data and released on root delete.
 *
 * @param parent            LVGL parent (screen, container, grid cell, …).
 * @param size_px           Outer width/height in pixels (square bounding box).
 * @param arc_thickness_px  Stroke width for both track and indicator; clamped to
 *                          [2, size_px/3] so the arc stays visible.
 * @param text_size_px      Target nominal height in px. Built-in Montserrat stops at 48;
 *                          larger values use the 48 pt bitmap (if enabled) plus LVGL
 *                          transform scale (capped, see @c GAUGE_1_LABEL_SCALE_MAX in .c).
 * @param value_text        Initial @c lv_label string; NULL is treated as empty.
 *
 * @return Root gauge object, or NULL if parent/size invalid or allocation failed.
 */
lv_obj_t *ui_gauge_1_create(lv_obj_t *parent, int32_t size_px, int32_t arc_thickness_px, int32_t text_size_px,
                            const char *value_text);

/**
 * Replaces the center label text (e.g. after formatting a number in a caller buffer).
 *
 * @param gauge     Root object returned by @ref ui_gauge_1_create.
 * @param value_text New UTF-8 string; NULL clears to empty.
 */
void ui_gauge_1_set_value_text(lv_obj_t *gauge, const char *value_text);

/**
 * Re-applies font selection for the center label (same bucketing as @ref ui_gauge_1_create).
 *
 * Use this when changing nominal text size at runtime. Same rules as @ref ui_gauge_1_create
 * (sizes above the largest bitmap Montserrat are scaled; see @c GAUGE_1_LABEL_SCALE_MAX).
 *
 * @param text_size_px Requested nominal height in px.
 */
void ui_gauge_1_set_text_size(lv_obj_t *gauge, int32_t text_size_px);

/**
 * Sets the arc indicator to a percentage of the 0–100 logical range.
 *
 * Values outside [0, 100] are clamped. Label text is not auto-synced; update it
 * separately via @ref ui_gauge_1_set_value_text if you show a numeric percentage.
 *
 * @param gauge          Root object from @ref ui_gauge_1_create.
 * @param percent_0_100  Progress along the arc (matches @c lv_arc range).
 */
void ui_gauge_1_set_percent(lv_obj_t *gauge, int32_t percent_0_100);
