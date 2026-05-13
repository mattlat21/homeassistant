#include "ui/screens/screen_study.h"

#include "ha_mqtt.h"
#include "ui/components/ui_button_1.h"
#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/ui_screen_template.h"

static lv_obj_t *s_btn_heater;

static void study_apply_heater_state(bool on)
{
    if (s_btn_heater == NULL) {
        return;
    }
    lv_obj_set_style_bg_color(s_btn_heater, on ? lv_color_hex(0xE53935) : lv_color_hex(0x2A2A2A), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_btn_heater, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_btn_heater, on ? lv_color_hex(0xFF8A80) : lv_color_hex(0x555555), LV_PART_MAIN);
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
    params.bg_color = lv_color_black();
    params.bg_opa = LV_OPA_COVER;
    params.pad_top = 24;
    params.pad_right = 24;
    params.pad_bottom = 24;
    params.pad_left = 24;
    params.row_gap = 16;
    params.col_gap = 16;

    ui_screen_template_result_t layout;
    if (!ui_screen_template_create(disp, &params, &layout)) {
        return NULL;
    }

    lv_color_t fg = lv_color_white();
    const lv_color_t *fg_p = &fg;

    s_btn_heater = ui_button_1_create(layout.grid, 1, 1, 1, 1, UI_HA_ICON_FIRE, "Heater", fg_p, fg_p,
                                      study_heater_publish, NULL);

    study_apply_heater_state(false);
    ha_mqtt_set_study_heater_state_callback(study_heater_state_cb, NULL);
    return layout.screen;
}
