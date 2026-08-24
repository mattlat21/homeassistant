#include "ui/screens/screen_study.h"

#include <stdint.h>

#include "ha_mqtt.h"
#include "ui/components/ui_box_1.h"
#include "ui/components/ui_button_1.h"
#include "ui/components/ui_hvac_climate_modal.h"
#include "ui/components/ui_taskbar.h"
#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/ui_screen_template.h"
#include "ui/ui_visual_tokens.h"

static lv_obj_t *s_btn_hvac;
static float s_setpoint_c = 21.0f;
static int8_t s_hvac_mode = HA_MQTT_CLIMATE_HVAC_OFF;
static bool s_have_setpoint;

#define STUDY_HVAC_ZONE_TOKEN 100u

static int8_t study_resolve_mode(bool heater_on, bool climate_control_on, int8_t hvac_mode)
{
    if (hvac_mode != HA_MQTT_CLIMATE_HVAC_UNKNOWN) {
        return hvac_mode;
    }
    if (climate_control_on || heater_on) {
        return HA_MQTT_CLIMATE_HVAC_HEAT;
    }
    return HA_MQTT_CLIMATE_HVAC_OFF;
}

static void study_hvac_set_label_colors(lv_color_t color)
{
    if (s_btn_hvac == NULL) {
        return;
    }
    const uint32_t n = lv_obj_get_child_cnt(s_btn_hvac);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_set_style_text_color(lv_obj_get_child(s_btn_hvac, i), color, LV_PART_MAIN);
    }
}

/** Idle: same semi-transparent white tile as Ollie light/fan. Active: solid accent fill with dark text. */
static void study_apply_hvac_state(int8_t hvac_mode)
{
    if (s_btn_hvac == NULL) {
        return;
    }
    if (hvac_mode != HA_MQTT_CLIMATE_HVAC_OFF && hvac_mode != HA_MQTT_CLIMATE_HVAC_UNKNOWN) {
        lv_obj_set_style_bg_color(s_btn_hvac, UI_ACCENT_COLOR, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_btn_hvac, LV_OPA_COVER, LV_PART_MAIN);
        study_hvac_set_label_colors(UI_ACCENT_TEXT_COLOR);
    } else {
        ui_box_1_style_apply(s_btn_hvac);
        study_hvac_set_label_colors(UI_SINGLE_SELECTOR_TEXT_COLOR_IDLE);
    }
    lv_obj_set_style_border_width(s_btn_hvac, 0, LV_PART_MAIN);
}

static void study_climate_state_cb(float setpoint_c, float current_c, bool heater_on, bool climate_control_on,
                                   int8_t hvac_mode, void *user_data)
{
    (void)current_c;
    (void)user_data;
    s_setpoint_c = setpoint_c;
    s_hvac_mode = study_resolve_mode(heater_on, climate_control_on, hvac_mode);
    s_have_setpoint = true;
    study_apply_hvac_state(s_hvac_mode);
    ui_hvac_climate_modal_update_if_open(STUDY_HVAC_ZONE_TOKEN, s_setpoint_c, s_hvac_mode, s_have_setpoint);
}

static void study_open_hvac_modal(void *user_data)
{
    (void)user_data;
    ui_hvac_climate_modal_open("Study", UI_HVAC_CLIMATE_PROFILE_HEAT_COOL_FAN, "climate_study", STUDY_HVAC_ZONE_TOKEN,
                               s_setpoint_c, s_hvac_mode, s_have_setpoint);
}

static void study_screen_unloaded(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_UNLOADED) {
        return;
    }
    ui_hvac_climate_modal_close();
}

lv_obj_t *screen_study_create(lv_display_t *disp)
{
    ui_screen_template_params_t params;
    ui_screen_template_params_init_defaults(&params);
    params.status_bar = false;
    params.grid_cols = 3;
    params.grid_rows = 3;
    params.pad_top = 24;
    params.pad_right = 24;
    /* Keep grid content clear of the dock. */
    params.pad_bottom = 24 + UI_TASKBAR_HEIGHT;
    params.pad_left = 24;
    params.row_gap = 16;
    params.col_gap = 16;
    params.bg_color = lv_color_black();
    params.bg_opa = LV_OPA_COVER;

    ui_screen_template_result_t layout;
    if (!ui_screen_template_create(disp, &params, &layout)) {
        return NULL;
    }

    lv_color_t fg = lv_color_white();
    const lv_color_t *fg_p = &fg;

    ha_mqtt_configure_hvac_zone(HA_MQTT_HVAC_ZONE_STUDY, "esp_hmi/data/study/climate/setpoint",
                                "esp_hmi/data/study/climate/current", "esp_hmi/data/study/climate/heater_on",
                                "esp_hmi/data/study/climate/control");
    ha_mqtt_add_hvac_zone_climate_callback(HA_MQTT_HVAC_ZONE_STUDY, study_climate_state_cb, NULL);

    s_btn_hvac = ui_button_1_create(layout.grid, 1, 1, 1, 1, UI_HA_ICON_THERMOMETER, "HVAC", fg_p, fg_p,
                                    study_open_hvac_modal, NULL);

    /* ui_button_1 lays out icon then name; enlarge the name label only on this screen. */
    if (s_btn_hvac != NULL && lv_obj_get_child_cnt(s_btn_hvac) >= 2) {
        lv_obj_set_style_text_font(lv_obj_get_child(s_btn_hvac, 1), &lv_font_montserrat_32, LV_PART_MAIN);
    }

    lv_obj_add_event_cb(layout.screen, study_screen_unloaded, LV_EVENT_SCREEN_UNLOADED, NULL);

    (void)ui_taskbar_attach_standard(layout.screen);

    study_apply_hvac_state(HA_MQTT_CLIMATE_HVAC_OFF);
    return layout.screen;
}
