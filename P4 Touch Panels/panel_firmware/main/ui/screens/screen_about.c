#include "ui/screens/screen_about.h"

#include <stdio.h>

#include "app_prefs.h"
#include "bsp/display.h"
#include "ui/ui_brand_gradient.h"
#include "ui/components/ui_status_bar.h"

#ifndef FW_VER_MAJOR
#define FW_VER_MAJOR 0
#endif
#ifndef FW_VER_MINOR
#define FW_VER_MINOR 0
#endif
#ifndef FW_VER_PATCH
#define FW_VER_PATCH 0
#endif

static lv_obj_t *s_lbl_version;
static lv_obj_t *s_lbl_mac;

static void about_refresh_labels(void)
{
    if (s_lbl_version != NULL) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Firmware v%u.%u.%u", (unsigned)FW_VER_MAJOR, (unsigned)FW_VER_MINOR,
                 (unsigned)FW_VER_PATCH);
        lv_label_set_text(s_lbl_version, buf);
    }
    if (s_lbl_mac != NULL) {
        char mac[24];
        app_prefs_format_sta_mac(mac, sizeof(mac));
        lv_label_set_text(s_lbl_mac, mac);
    }
}

static void about_screen_loaded(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_LOADED) {
        return;
    }
    about_refresh_labels();
}

lv_obj_t *screen_about_create(lv_display_t *disp)
{
    (void)disp;
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    ui_brand_gradient_apply(scr);
    lv_obj_add_event_cb(scr, about_screen_loaded, LV_EVENT_SCREEN_LOADED, NULL);

    lv_obj_t *column = lv_obj_create(scr);
    lv_obj_remove_style_all(column);
    lv_obj_set_width(column, lv_pct(92));
    lv_obj_set_height(column, LV_SIZE_CONTENT);
    lv_obj_align(column, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_row(column, 16, LV_PART_MAIN);
    lv_obj_set_layout(column, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(column, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(column, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card_mac = lv_obj_create(column);
    lv_obj_remove_style_all(card_mac);
    lv_obj_set_width(card_mac, lv_pct(100));
    lv_obj_set_height(card_mac, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(card_mac, 32, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(card_mac, 24, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card_mac, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card_mac, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_radius(card_mac, 24, LV_PART_MAIN);
    lv_obj_set_layout(card_mac, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card_mac, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card_mac, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(card_mac, LV_OBJ_FLAG_SCROLLABLE);

    s_lbl_mac = lv_label_create(card_mac);
    lv_obj_set_style_text_font(s_lbl_mac, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_mac, lv_color_hex(0x1C1C1E), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_lbl_mac, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(s_lbl_mac, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(s_lbl_mac, lv_pct(100));

    lv_obj_t *card_fw = lv_obj_create(column);
    lv_obj_remove_style_all(card_fw);
    lv_obj_set_width(card_fw, lv_pct(100));
    lv_obj_set_height(card_fw, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(card_fw, 28, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card_fw, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card_fw, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_radius(card_fw, 24, LV_PART_MAIN);
    lv_obj_set_layout(card_fw, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card_fw, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card_fw, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(card_fw, LV_OBJ_FLAG_SCROLLABLE);

    s_lbl_version = lv_label_create(card_fw);
    lv_obj_set_style_text_font(s_lbl_version, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_version, lv_color_hex(0x1C1C1E), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_lbl_version, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(s_lbl_version, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(s_lbl_version, lv_pct(100));

    about_refresh_labels();
    (void)ui_status_bar_create(scr);
    return scr;
}
