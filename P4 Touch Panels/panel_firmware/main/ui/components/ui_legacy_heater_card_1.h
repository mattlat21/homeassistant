#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

typedef enum {
    UI_LEGACY_HEATER_CARD_1_EVENT_SETPOINT_DEC = 0,
    UI_LEGACY_HEATER_CARD_1_EVENT_SETPOINT_INC,
    UI_LEGACY_HEATER_CARD_1_EVENT_MODE_OFF,
    UI_LEGACY_HEATER_CARD_1_EVENT_MODE_HEATING,
    UI_LEGACY_HEATER_CARD_1_EVENT_MODE_COOLING,
    UI_LEGACY_HEATER_CARD_1_EVENT_MODE_FAN,
} ui_legacy_heater_card_1_event_t;

typedef void (*ui_legacy_heater_card_1_cb_t)(ui_legacy_heater_card_1_event_t event, void *user_data);

/** Which modes appear in the picker and how the status icon behaves. */
typedef enum {
    UI_LEGACY_HEATER_CARD_PROFILE_HEATER = 0,
    UI_LEGACY_HEATER_CARD_PROFILE_FAN,
    UI_LEGACY_HEATER_CARD_PROFILE_HEAT_COOL_FAN,
    UI_LEGACY_HEATER_CARD_PROFILE_HEAT_COOL,
    UI_LEGACY_HEATER_CARD_PROFILE_BLANK,
} ui_legacy_heater_card_1_profile_t;

/**
 * Horizontal heater summary card: icon + status, current/desired temps, −/+ step buttons.
 * Tapping the left icon opens an in-card mode picker (cancel + heating / cooling / fan / off).
 * Matches the Ollie room climate MQTT fields (setpoint, current, heater_on, climate_control_on).
 * @param min_height_px Minimum card height (0 = default 128).
 * @param circle_margin_px Margin around icon and outer step-button edges (0 = default 8).
 * @param button_gap_px Horizontal gap between step buttons (0 = default 10).
 * @param room_name Label shown at the top between the icon and step buttons (may be NULL).
 */
lv_obj_t *ui_legacy_heater_card_1_create(lv_obj_t *parent, lv_coord_t width, const char *room_name,
                                  ui_legacy_heater_card_1_profile_t profile, float current_temp_c, float setpoint_c,
                                  bool heater_on, bool climate_control_on, ui_legacy_heater_card_1_cb_t cb, void *user_data,
                                  lv_coord_t min_height_px, lv_coord_t circle_margin_px, lv_coord_t button_gap_px);

void ui_legacy_heater_card_1_set_current_temp(lv_obj_t *card, float temp_c);
void ui_legacy_heater_card_1_set_setpoint(lv_obj_t *card, float setpoint_c);
void ui_legacy_heater_card_1_set_switch_state(lv_obj_t *card, bool heater_on, bool climate_control_on);

/** @param hvac_mode `HA_MQTT_CLIMATE_HVAC_*` from MQTT control topic; unknown (-1) keeps profile defaults. */
void ui_legacy_heater_card_1_set_hvac_mode(lv_obj_t *card, int8_t hvac_mode);
