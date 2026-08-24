#pragma once

#include "lvgl.h"

/** Black frame around the coloured brand panel (matches home / About / Settings / Debug). */
#define UI_BRAND_FRAME_MARGIN_PX 3

/** Same diagonal violet → magenta gradient as the home screen (UI brand colors). */
void ui_brand_gradient_apply(lv_obj_t *obj);

/**
 * Paint @a scr solid black and create an inset rounded brand gradient panel (margin + dock radius).
 * Parent content to @a scr or the returned panel; attach the taskbar to @a scr.
 * @return Brand panel, or NULL if @a scr is NULL.
 */
lv_obj_t *ui_brand_framed_panel_create(lv_obj_t *scr);
