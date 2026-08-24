#pragma once

#include <stdint.h>

#include "ha_mqtt.h"
#include "lvgl.h"

/**
 * Compact read-only HVAC zone tile for the HVAC overview grid.
 * Shows room name, current/set temperatures, and HVAC mode as a coloured border.
 */
lv_obj_t *ui_hvac_card_1_create(lv_obj_t *parent, uint8_t row, uint8_t col, const char *room_name);

void ui_hvac_card_1_set_room_name(lv_obj_t *card, const char *room_name);
void ui_hvac_card_1_set_current_temp(lv_obj_t *card, float temp_c);
void ui_hvac_card_1_set_setpoint(lv_obj_t *card, float setpoint_c);

/** @param hvac_mode `HA_MQTT_CLIMATE_HVAC_*` (unknown/off/heat/cool/fan). */
void ui_hvac_card_1_set_mode(lv_obj_t *card, int8_t hvac_mode);
