#pragma once

#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

typedef struct {
    const char *icon;
    const char *name;
} ui_single_selector_item_t;

typedef void (*ui_single_selector_tab_1_cb_t)(uint32_t selected_index, void *user_data);

/**
 * Rounded container; inner row/column of segment buttons (single selection).
 * If @p parent uses @c LV_LAYOUT_GRID, @p row / @p col / @p row_span / @p col_span place the widget on the grid.
 * Otherwise (e.g. flex parent) those args are ignored and the outer widget is full width, content height.
 * Icons use ui_font_home_assistant_icons_56 when set; names use Montserrat 24, wrapped.
 */
lv_obj_t *ui_single_selector_tab_1_create(lv_obj_t *parent, uint8_t row, uint8_t col, uint8_t row_span,
                                          uint8_t col_span, bool horizontal, const ui_single_selector_item_t *items,
                                          size_t item_count, uint32_t initial_selected,
                                          ui_single_selector_tab_1_cb_t on_change, void *user_data);

uint32_t ui_single_selector_tab_1_get_selected(const lv_obj_t *widget);

/**
 * Set selected segment by index. Refreshes styles.
 * @param notify If true, invokes @c on_change (use false for external/MQTT-driven updates).
 */
void ui_single_selector_tab_1_set_selected(lv_obj_t *widget, uint32_t index, bool notify);
