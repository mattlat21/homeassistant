#pragma once

#include "lvgl.h"
#include "sdkconfig.h"

#define UI_LOADING_DURATION_MS CONFIG_SCREEN_TEST_LOADING_MS

lv_obj_t *screen_loading_create(lv_display_t *disp);

void screen_loading_begin(lv_obj_t *screen);
