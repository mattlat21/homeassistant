#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ha_mqtt.h"
#include "lvgl.h"

/** Which HVAC modes the modal offers (matches legacy heater card profiles). */
typedef enum {
    UI_HVAC_CLIMATE_PROFILE_HEATER = 0,
    UI_HVAC_CLIMATE_PROFILE_HEAT_COOL,
    UI_HVAC_CLIMATE_PROFILE_HEAT_COOL_FAN,
    UI_HVAC_CLIMATE_PROFILE_HEAT_COOL_FAN_DRY,
} ui_hvac_climate_profile_t;

/**
 * Modal on `lv_layer_top()` for setting zone mode and target temperature.
 * @param room_name Title (string literal / stable pointer).
 * @param btn_prefix MQTT button prefix without trailing `_` (e.g. `climate_bedroom_1`), or NULL for
 *                   Ollie primary `climate_*` payloads.
 * @param zone_token Opaque id for `ui_hvac_climate_modal_update_if_open()`.
 */
void ui_hvac_climate_modal_open(const char *room_name, ui_hvac_climate_profile_t profile, const char *btn_prefix,
                                unsigned zone_token, float setpoint_c, int8_t hvac_mode, bool have_setpoint);

void ui_hvac_climate_modal_close(void);

/** Refresh setpoint/mode when MQTT updates the zone that currently owns the modal. */
void ui_hvac_climate_modal_update_if_open(unsigned zone_token, float setpoint_c, int8_t hvac_mode, bool have_setpoint);
