#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

typedef enum {
    UI_HEATER_CARD_1_EVENT_SETPOINT_DEC = 0,
    UI_HEATER_CARD_1_EVENT_SETPOINT_INC,
    UI_HEATER_CARD_1_EVENT_MODE_OFF,
    UI_HEATER_CARD_1_EVENT_MODE_HEATING,
    UI_HEATER_CARD_1_EVENT_MODE_COOLING,
    UI_HEATER_CARD_1_EVENT_MODE_FAN,
} ui_heater_card_1_event_t;

typedef void (*ui_heater_card_1_cb_t)(ui_heater_card_1_event_t event, void *user_data);

/**
 * Horizontal heater summary card: icon + status, current/desired temps, −/+ step buttons.
 * Tapping the left icon opens an in-card mode picker (cancel + heating / cooling / fan / off).
 * Matches the Ollie room climate MQTT fields (setpoint, current, heater_on, climate_control_on).
 * @param min_height_px Minimum card height (0 = default 128).
 * @param circle_margin_px Margin around icon and outer step-button edges (0 = default 8).
 * @param button_gap_px Horizontal gap between step buttons (0 = default 10).
 * @param room_name Label shown at the top between the icon and step buttons (may be NULL).
 */
lv_obj_t *ui_heater_card_1_create(lv_obj_t *parent, lv_coord_t width, const char *room_name, float current_temp_c,
                                  float setpoint_c, bool heater_on, bool climate_control_on, ui_heater_card_1_cb_t cb,
                                  void *user_data, lv_coord_t min_height_px, lv_coord_t circle_margin_px,
                                  lv_coord_t button_gap_px);

void ui_heater_card_1_set_current_temp(lv_obj_t *card, float temp_c);
void ui_heater_card_1_set_setpoint(lv_obj_t *card, float setpoint_c);
void ui_heater_card_1_set_switch_state(lv_obj_t *card, bool heater_on, bool climate_control_on);
