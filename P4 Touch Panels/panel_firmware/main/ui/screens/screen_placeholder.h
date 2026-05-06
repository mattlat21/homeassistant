#pragma once

#include <stdbool.h>

#include "lvgl.h"

/**
 * @param with_status_bar If true, a @ref ui_status_bar is added along the top (e.g. Notes).
 */
lv_obj_t *screen_placeholder_create(lv_display_t *disp, const char *title, bool with_status_bar);
