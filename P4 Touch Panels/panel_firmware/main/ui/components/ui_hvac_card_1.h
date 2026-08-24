#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ha_mqtt.h"
#include "lvgl.h"

/**
 * Compact read-only HVAC zone tile for the HVAC overview grid.
 * Shows room name, current/set temperatures, and HVAC mode as a full card background colour.
 */
lv_obj_t *ui_hvac_card_1_create(lv_obj_t *parent, uint8_t row, uint8_t col, const char *room_name);

void ui_hvac_card_1_set_room_name(lv_obj_t *card, const char *room_name);
void ui_hvac_card_1_set_current_temp(lv_obj_t *card, float temp_c);
void ui_hvac_card_1_set_setpoint(lv_obj_t *card, float setpoint_c);

/** When false, hides the Set line (temp-only cards). Default true. */
void ui_hvac_card_1_set_setpoint_visible(lv_obj_t *card, bool visible);

/** @param hvac_mode `HA_MQTT_CLIMATE_HVAC_*` (unknown/off/heat/cool/fan). */
void ui_hvac_card_1_set_mode(lv_obj_t *card, int8_t hvac_mode);

float ui_hvac_card_1_get_setpoint(const lv_obj_t *card);
int8_t ui_hvac_card_1_get_mode(const lv_obj_t *card);
bool ui_hvac_card_1_has_setpoint(const lv_obj_t *card);

/** Makes the card tappable; @a cb receives `LV_EVENT_CLICKED` with @a user_data. */
void ui_hvac_card_1_set_click_cb(lv_obj_t *card, lv_event_cb_t cb, void *user_data);
