#include "ui/screens/screen_hvac.h"

#include "bsp/display.h"
#include "ha_mqtt.h"
#include "ui/components/ui_heater_card_1.h"
#include "ui/components/ui_status_bar.h"
#include "ui/ui_brand_gradient.h"

static lv_obj_t *s_heater_card;

static void hvac_climate_apply_from_mqtt(float setpoint_c, float current_c, bool heater_on, bool climate_control_on,
                                       void *user_data)
{
    (void)user_data;
    if (s_heater_card == NULL) {
        return;
    }
    ui_heater_card_1_set_switch_state(s_heater_card, heater_on, climate_control_on);
    ui_heater_card_1_set_setpoint(s_heater_card, setpoint_c);
    ui_heater_card_1_set_current_temp(s_heater_card, current_c);
}

static void hvac_heater_on_ui_event(ui_heater_card_1_event_t event, void *user_data)
{
    (void)user_data;
    switch (event) {
    case UI_HEATER_CARD_1_EVENT_SETPOINT_DEC:
        (void)ha_mqtt_publish_ollie_button("climate_temp_dn");
        break;
    case UI_HEATER_CARD_1_EVENT_SETPOINT_INC:
        (void)ha_mqtt_publish_ollie_button("climate_temp_up");
        break;
    default:
        break;
    }
}

lv_obj_t *screen_hvac_create(lv_display_t *disp)
{
    (void)disp;
    s_heater_card = NULL;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    ui_brand_gradient_apply(scr);

    (void)ui_status_bar_create(scr);

    const lv_coord_t card_w = (lv_coord_t)(BSP_LCD_H_RES - 48);
    s_heater_card = ui_heater_card_1_create(scr, card_w, 21.0f, 22.0f, false, false, hvac_heater_on_ui_event, NULL);
    if (s_heater_card != NULL) {
        lv_obj_align(s_heater_card, LV_ALIGN_CENTER, 0, (UI_STATUS_BAR_HEIGHT + 8) / 2);
    }

    ha_mqtt_add_ollie_climate_state_callback(hvac_climate_apply_from_mqtt, NULL);

    return scr;
}
