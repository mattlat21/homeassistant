#include "ui/screens/screen_house_battery.h"

#include <stdio.h>

#include "bsp/display.h"
#include "ha_mqtt.h"
#include "ui/components/ui_gauge_1.h"
#include "ui/components/ui_status_bar.h"
#include "ui/ui_brand_gradient.h"

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
    ui_brand_gradient_apply(scr);

    (void)ui_status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "House Battery");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, UI_STATUS_BAR_HEIGHT + 12);

    const int32_t gauge_size = 380;
    s_soc_gauge = ui_gauge_1_create(scr, gauge_size, 22, 96, "--%");
    if (s_soc_gauge != NULL) {
        ui_gauge_1_set_percent(s_soc_gauge, 0);
        lv_obj_align(s_soc_gauge, LV_ALIGN_CENTER, 0, (UI_STATUS_BAR_HEIGHT + 12) / 2);
    }

    ha_mqtt_set_house_battery_soc_callback(house_battery_soc_cb, NULL);

    return scr;
}
