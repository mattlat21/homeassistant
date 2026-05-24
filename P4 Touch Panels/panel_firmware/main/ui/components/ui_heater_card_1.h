#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

typedef enum {
    UI_HEATER_CARD_1_EVENT_SETPOINT_DEC = 0,
    UI_HEATER_CARD_1_EVENT_SETPOINT_INC,
} ui_heater_card_1_event_t;

typedef void (*ui_heater_card_1_cb_t)(ui_heater_card_1_event_t event, void *user_data);

/**
 * Horizontal heater summary card: icon + status, current/desired temps, −/+ step buttons.
 * Matches the Ollie room climate MQTT fields (setpoint, current, heater_on, climate_control_on).
 */
lv_obj_t *ui_heater_card_1_create(lv_obj_t *parent, lv_coord_t width, float current_temp_c, float setpoint_c,
                                  bool heater_on, bool climate_control_on, ui_heater_card_1_cb_t cb, void *user_data);

void ui_heater_card_1_set_current_temp(lv_obj_t *card, float temp_c);
void ui_heater_card_1_set_setpoint(lv_obj_t *card, float setpoint_c);
void ui_heater_card_1_set_switch_state(lv_obj_t *card, bool heater_on, bool climate_control_on);
