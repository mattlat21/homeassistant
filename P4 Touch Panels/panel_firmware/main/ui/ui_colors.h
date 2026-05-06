/**
 * Standard UI palette (hex values for reference; use the LV_COLOR macros with LVGL APIs).
 */
#pragma once

#include "lvgl.h"

/** #6233FF */
#define UI_HEX_BRAND_VIOLET 0x6233FFu
/** #970094 */
#define UI_HEX_BRAND_MAGENTA 0x970094u

#define UI_COLOR_BRAND_VIOLET  LV_COLOR_MAKE(0x62, 0x33, 0xFF)
#define UI_COLOR_BRAND_MAGENTA LV_COLOR_MAKE(0x97, 0x00, 0x94)

static inline lv_color_t ui_color_brand_violet(void)
{
    return lv_color_hex(UI_HEX_BRAND_VIOLET);
}

static inline lv_color_t ui_color_brand_magenta(void)
{
    return lv_color_hex(UI_HEX_BRAND_MAGENTA);
}
