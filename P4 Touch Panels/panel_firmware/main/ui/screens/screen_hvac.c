#include "ui/screens/screen_hvac.h"

#include "bsp/display.h"
#include "ha_mqtt.h"
#include "ui/components/ui_heater_card_1.h"

#define HVAC_HEATER_CARD_COUNT 5
#define HVAC_CARD_GAP_PX 10
#define HVAC_CIRCLE_MARGIN_PX 20
#define HVAC_BUTTON_GAP_PX 10

static lv_obj_t *s_heater_cards[HVAC_HEATER_CARD_COUNT];

static void hvac_apply_heater_card(lv_obj_t *card, float setpoint_c, float current_c, bool heater_on,
                                   bool climate_control_on)
{
    if (card == NULL) {
        return;
    }
    ui_heater_card_1_set_switch_state(card, heater_on, climate_control_on);
    ui_heater_card_1_set_setpoint(card, setpoint_c);
    ui_heater_card_1_set_current_temp(card, current_c);
}

static void hvac_climate_apply_from_mqtt(float setpoint_c, float current_c, bool heater_on, bool climate_control_on,
                                         void *user_data)
{
    (void)user_data;
    for (unsigned i = 0; i < HVAC_HEATER_CARD_COUNT; i++) {
        hvac_apply_heater_card(s_heater_cards[i], setpoint_c, current_c, heater_on, climate_control_on);
    }
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
    for (unsigned i = 0; i < HVAC_HEATER_CARD_COUNT; i++) {
        s_heater_cards[i] = NULL;
    }

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(scr, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(scr, HVAC_CARD_GAP_PX, LV_PART_MAIN);

    const lv_coord_t card_h =
        (lv_coord_t)((BSP_LCD_V_RES - ((HVAC_HEATER_CARD_COUNT - 1) * HVAC_CARD_GAP_PX)) / HVAC_HEATER_CARD_COUNT);

    for (unsigned i = 0; i < HVAC_HEATER_CARD_COUNT; i++) {
        s_heater_cards[i] = ui_heater_card_1_create(scr, BSP_LCD_H_RES, 21.0f, 22.0f, false, false,
                                                    hvac_heater_on_ui_event, NULL, card_h, HVAC_CIRCLE_MARGIN_PX,
                                                    HVAC_BUTTON_GAP_PX);
        if (s_heater_cards[i] != NULL) {
            lv_obj_set_width(s_heater_cards[i], BSP_LCD_H_RES);
            lv_obj_set_height(s_heater_cards[i], card_h);
            lv_obj_set_flex_grow(s_heater_cards[i], 1);
        }
    }

    ha_mqtt_add_ollie_climate_state_callback(hvac_climate_apply_from_mqtt, NULL);

    return scr;
}
