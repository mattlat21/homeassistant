#include "ui/screens/screen_study.h"

#include <stdint.h>

#include "ha_mqtt.h"
#include "ui/components/ui_box_1.h"
#include "ui/components/ui_button_1.h"
#include "ui/components/ui_taskbar.h"
#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/ui_screen_template.h"
#include "ui/ui_visual_tokens.h"

static lv_obj_t *s_btn_heater;

static void study_heater_set_label_colors(lv_color_t color)
{
    if (s_btn_heater == NULL) {
        return;
    }
    const uint32_t n = lv_obj_get_child_cnt(s_btn_heater);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_set_style_text_color(lv_obj_get_child(s_btn_heater, i), color, LV_PART_MAIN);
    }
}

/** Idle: same semi-transparent white tile as Ollie light/fan. Active: solid accent fill with dark text. */
static void study_apply_heater_state(bool on)
{
    if (s_btn_heater == NULL) {
        return;
    }
    if (on) {
        lv_obj_set_style_bg_color(s_btn_heater, UI_ACCENT_COLOR, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_btn_heater, LV_OPA_COVER, LV_PART_MAIN);
        study_heater_set_label_colors(UI_ACCENT_TEXT_COLOR);
    } else {
        ui_box_1_style_apply(s_btn_heater);
        study_heater_set_label_colors(UI_SINGLE_SELECTOR_TEXT_COLOR_IDLE);
    }
    lv_obj_set_style_border_width(s_btn_heater, 0, LV_PART_MAIN);
}

static void study_heater_state_cb(bool heater_on, void *user_data)
{
    (void)user_data;
    study_apply_heater_state(heater_on);
}

static void study_heater_publish(void *user_data)
{
    (void)user_data;
    (void)ha_mqtt_publish_ollie_button("study_heater");
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

    s_btn_heater = ui_button_1_create(layout.grid, 1, 1, 1, 1, UI_HA_ICON_FIRE, "Heater", fg_p, fg_p,
                                      study_heater_publish, NULL);

    /* ui_button_1 lays out icon then name; enlarge the name label only on this screen. */
    if (s_btn_heater != NULL && lv_obj_get_child_cnt(s_btn_heater) >= 2) {
        lv_obj_set_style_text_font(lv_obj_get_child(s_btn_heater, 1), &lv_font_montserrat_32, LV_PART_MAIN);
    }

    (void)ui_taskbar_attach_standard(layout.screen);

    study_apply_heater_state(false);
    ha_mqtt_set_study_heater_state_callback(study_heater_state_cb, NULL);
    return layout.screen;
}
