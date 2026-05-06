#include "ui/screens/screen_study.h"

#include "bsp/display.h"
#include "ha_mqtt.h"

static lv_obj_t *s_btn_heater;
static lv_obj_t *s_lbl_heater;

static void study_apply_heater_state(bool on)
{
    if (s_btn_heater == NULL || s_lbl_heater == NULL) {
        return;
    }
    lv_obj_set_style_bg_color(s_btn_heater, on ? lv_color_hex(0xE53935) : lv_color_hex(0x2A2A2A), LV_PART_MAIN);
    lv_obj_set_style_border_color(s_btn_heater, on ? lv_color_hex(0xFF8A80) : lv_color_hex(0x555555), LV_PART_MAIN);
    lv_label_set_text(s_lbl_heater, "Heater");
}

static void study_heater_state_cb(bool heater_on, void *user_data)
{
    (void)user_data;
    study_apply_heater_state(heater_on);
}

static void study_heater_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    (void)ha_mqtt_publish_ollie_button("study_heater");
}

lv_obj_t *screen_study_create(lv_display_t *disp)
{
    (void)disp;
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    s_btn_heater = lv_button_create(scr);
    lv_obj_set_size(s_btn_heater, 260, 100);
    lv_obj_center(s_btn_heater);
    lv_obj_set_style_radius(s_btn_heater, 18, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_btn_heater, 2, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(s_btn_heater, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(s_btn_heater, LV_OPA_30, LV_PART_MAIN);
    lv_obj_add_event_cb(s_btn_heater, study_heater_btn_cb, LV_EVENT_CLICKED, NULL);

    s_lbl_heater = lv_label_create(s_btn_heater);
    lv_obj_set_style_text_font(s_lbl_heater, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_heater, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(s_lbl_heater);

    study_apply_heater_state(false);
    ha_mqtt_set_study_heater_state_callback(study_heater_state_cb, NULL);
    return scr;
}
