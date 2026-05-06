#pragma once

#include <stdint.h>

#include "lvgl.h"
#include "ui/nav.h"

void ui_idle_timeout_init(lv_display_t *disp);

/** LVGL thread: update runtime config (call after `app_prefs_set_idle_timeout` or from prefs load). */
void ui_idle_timeout_configure(app_id_t target_app, uint32_t sec);
