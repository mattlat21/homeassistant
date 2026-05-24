#pragma once

#include <stdint.h>

#include "lvgl.h"

void ui_display_power_init(lv_display_t *disp);

/** LVGL thread: update runtime config (call after `app_prefs_set_display_power` or from prefs load). */
void ui_display_power_configure(uint8_t normal_pct, uint8_t dim_pct, uint32_t dim_sec, uint32_t off_sec);
