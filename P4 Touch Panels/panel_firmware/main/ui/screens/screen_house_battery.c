#include "ui/screens/screen_house_battery.h"

#include <stdio.h>

#include "bsp/display.h"
#include "ha_mqtt.h"
#include "ui/components/ui_gauge_1.h"
#include "ui/components/ui_taskbar.h"

/** Gap between the screen top and the title label (px). */
#define HOUSE_BATTERY_TITLE_TOP 24
/** Approximate height of the montserrat_24 title label (px). */
#define HOUSE_BATTERY_TITLE_HEIGHT 34

static lv_obj_t *s_soc_gauge;

static void house_battery_apply_soc(float soc_percent)
{
    if (s_soc_gauge == NULL) {
        return;
    }
    if (soc_percent < 0.0f) {
        soc_percent = 0.0f;
    } else if (soc_percent > 100.0f) {
        soc_percent = 100.0f;
    }
    const int pct = (int)(soc_percent + 0.5f);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    ui_gauge_1_set_value_text(s_soc_gauge, buf);
    ui_gauge_1_set_percent(s_soc_gauge, pct);
}

static void house_battery_soc_cb(float soc_percent, void *user_data)
{
    (void)user_data;
    house_battery_apply_soc(soc_percent);
}

lv_obj_t *screen_house_battery_create(lv_display_t *disp)
{
    (void)disp;
    s_soc_gauge = NULL;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "House Battery");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, HOUSE_BATTERY_TITLE_TOP);

    const int32_t gauge_size = 380;
    /*
     * Keep the requested text size at or below the largest built-in Montserrat (48). Larger values make
     * ui_gauge_1 fall back to LVGL transform-scale, whose layer render hangs the LVGL task on this target
     * (screen freezes half-drawn).
     */
    s_soc_gauge = ui_gauge_1_create(scr, gauge_size, 22, 48, "--%");
    if (s_soc_gauge != NULL) {
        ui_gauge_1_set_percent(s_soc_gauge, 0);
        /* Centre in the band between the title and the dock. */
        const int32_t top_reserved = HOUSE_BATTERY_TITLE_TOP + HOUSE_BATTERY_TITLE_HEIGHT;
        lv_obj_align(s_soc_gauge, LV_ALIGN_CENTER, 0, (top_reserved - UI_TASKBAR_HEIGHT) / 2);
    }

    (void)ui_taskbar_attach_standard(scr);

    ha_mqtt_set_house_battery_soc_callback(house_battery_soc_cb, NULL);

    return scr;
}
