#include "ui/screens/screen_settings.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_prefs.h"
#include "bsp/display.h"
#include "ui/ui_brand_gradient.h"
#include "ui/components/ui_taskbar.h"
#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/nav.h"

static lv_obj_t *s_lbl_wifi;
static lv_obj_t *s_lbl_default;

static void settings_refresh_labels(void)
{
    if (s_lbl_wifi != NULL) {
        char ssid[64];
        app_prefs_format_wifi_ssid(ssid, sizeof(ssid));
        char line[96];
        snprintf(line, sizeof(line), "Wi-Fi: %s", ssid);
        lv_label_set_text(s_lbl_wifi, line);
    }
    if (s_lbl_default != NULL) {
        app_id_t def = app_prefs_get_default_app();
        char slug[24];
        app_prefs_slug_for_app(def, slug, sizeof(slug));
        char line[128];
        snprintf(line, sizeof(line), "Default screen: %s (%s)", app_prefs_display_name_for_app(def), slug);
        lv_label_set_text(s_lbl_default, line);
    }
}

static void settings_screen_loaded(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOADED) {
        return;
    }
    settings_refresh_labels();
}

static void settings_taskbar_nav(void *user_data)
{
    nav_go_to((app_id_t)(uintptr_t)user_data);
}

lv_obj_t *screen_settings_create(lv_display_t *disp)
{
    (void)disp;
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    ui_brand_gradient_apply(scr);
    lv_obj_add_event_cb(scr, settings_screen_loaded, LV_EVENT_SCREEN_LOADED, NULL);

    lv_obj_t *panel = lv_obj_create(scr);
    lv_obj_remove_style_all(panel);
    lv_obj_set_width(panel, lv_pct(92));
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, -(UI_TASKBAR_HEIGHT / 2));
    lv_obj_set_style_pad_all(panel, 28, LV_PART_MAIN);
    lv_obj_set_style_pad_row(panel, 20, LV_PART_MAIN);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 24, LV_PART_MAIN);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    s_lbl_wifi = lv_label_create(panel);
    lv_obj_set_style_text_font(s_lbl_wifi, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_wifi, lv_color_hex(0x1C1C1E), LV_PART_MAIN);
    lv_label_set_long_mode(s_lbl_wifi, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(s_lbl_wifi, lv_pct(100));

    s_lbl_default = lv_label_create(panel);
    lv_obj_set_style_text_font(s_lbl_default, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_default, lv_color_hex(0x1C1C1E), LV_PART_MAIN);
    lv_label_set_long_mode(s_lbl_default, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(s_lbl_default, lv_pct(100));

    settings_refresh_labels();

    const ui_taskbar_item_t taskbar_items[] = {
        { LV_SYMBOL_HOME, &lv_font_montserrat_48, NULL, settings_taskbar_nav, (void *)(uintptr_t)APP_HOME },
        { UI_HA_ICON_TEDDY_BEAR, NULL, NULL, settings_taskbar_nav, (void *)(uintptr_t)APP_PENNY_ROOM },
        { UI_HA_ICON_GATE, NULL, NULL, settings_taskbar_nav, (void *)(uintptr_t)APP_FRONT_GATE },
        { UI_HA_ICON_THERMOMETER, NULL, NULL, settings_taskbar_nav, (void *)(uintptr_t)APP_HVAC },
        { LV_SYMBOL_BATTERY_FULL, &lv_font_montserrat_48, NULL, settings_taskbar_nav,
          (void *)(uintptr_t)APP_HOUSE_BATTERY },
        { LV_SYMBOL_SETTINGS, &lv_font_montserrat_48, NULL, settings_taskbar_nav, (void *)(uintptr_t)APP_SETTINGS },
    };
    (void)ui_taskbar_create(scr, taskbar_items, (uint8_t)(sizeof(taskbar_items) / sizeof(taskbar_items[0])));

    return scr;
}
