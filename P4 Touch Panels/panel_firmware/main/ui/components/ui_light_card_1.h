#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

typedef void (*ui_light_card_1_power_cb_t)(bool on, void *user_data);
typedef void (*ui_light_card_1_brightness_cb_t)(uint8_t brightness_pct, void *user_data);

/**
 * Light row on a grid: lightbulb power toggle, name, live percentage and a brightness slider.
 * The toggle is a square sized to the card height, so the parent grid track controls how big it is.
 * @a power_cb fires on each toggle tap; @a brightness_cb fires on slider release (not while dragging)
 * and only when the value changed, so a drag produces one publish.
 */
lv_obj_t *ui_light_card_1_create(lv_obj_t *parent, uint8_t row, uint8_t col, uint8_t row_span, uint8_t col_span,
                                 const char *name, ui_light_card_1_power_cb_t power_cb,
                                 ui_light_card_1_brightness_cb_t brightness_cb, void *user_data);

/** Apply state from Home Assistant; never invokes the callbacks. @a brightness_pct is clamped to 0–100. */
void ui_light_card_1_set_state(lv_obj_t *card, bool on, uint8_t brightness_pct);
