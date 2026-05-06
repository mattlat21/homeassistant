#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

typedef enum {
    /** Local setpoint value was updated (not used for MQTT script steppers). */
    UI_CLIMATE_CONTROL_1_EVENT_SETPOINT = 0,
    /** − button: request step down (e.g. MQTT script.decrease_ollie_temp). */
    UI_CLIMATE_CONTROL_1_EVENT_SETPOINT_DEC,
    /** + button: request step up (e.g. MQTT script.increase_ollie_temp). */
    UI_CLIMATE_CONTROL_1_EVENT_SETPOINT_INC,
    UI_CLIMATE_CONTROL_1_EVENT_POWER,
} ui_climate_control_1_event_t;

/** Climate mode: 0 = off, 1 = on, 2 = climate control. */
#define UI_CLIMATE_CONTROL_1_MODE_OFF 0u
#define UI_CLIMATE_CONTROL_1_MODE_ON 1u
#define UI_CLIMATE_CONTROL_1_MODE_CLIMATE_CONTROL 2u

/**
 * @param setpoint_c Current displayed setpoint (after SETPOINT); for DEC/INC unchanged.
 * @param climate_enabled True if mode is not off (same as @p climate_mode != 0).
 * @param climate_mode 0 off, 1 on, 2 climate control.
 */
typedef void (*ui_climate_control_1_cb_t)(ui_climate_control_1_event_t event, float setpoint_c, bool climate_enabled,
                                          uint8_t climate_mode, void *user_data);

/**
 * Rounded semi-transparent panel (ui_box_1 look) with:
 * - Large square − / + step buttons; setpoint centered between them, current temp below, then a centered
 *   status icon: **fire** if heater on, else **thermometer** if climate-control boolean on, else **fire-off**.
 * - Flexible vertical gap, then a full-width horizontal selector: **off** | **on** | **climate control**
 *   (mdi:fire-off, mdi:fire, mdi:thermometer-low).
 *
 * @param heater_on Initial heater-on state from HA/MQTT (e.g. `switch.bedroom_3_heater`); selector **on** if climate_control_on is false.
 * @param climate_control_on Initial “climate control” mode from HA/MQTT (e.g. HVAC `heat` vs `off` on `climate.*`).
 *
 * Pass @p widget (the returned object) to the `ui_climate_control_1_set_*` helpers.
 */
lv_obj_t *ui_climate_control_1_create(lv_obj_t *parent, uint8_t row, uint8_t col, uint8_t row_span, uint8_t col_span,
                                      float current_temp_c, float setpoint_c, bool heater_on, bool climate_control_on,
                                      ui_climate_control_1_cb_t cb, void *user_data);

void ui_climate_control_1_set_current_temp(lv_obj_t *widget, float temp_c);
void ui_climate_control_1_set_setpoint(lv_obj_t *widget, float setpoint_c);
/** Maps to mode off (0) or on (1); does not select climate control (2). */
void ui_climate_control_1_set_climate_on(lv_obj_t *widget, bool on);
void ui_climate_control_1_set_mode(lv_obj_t *widget, uint8_t mode_index);
uint8_t ui_climate_control_1_get_mode(const lv_obj_t *widget);
/** Mirror heater on + climate-control boolean (from MQTT / HA) for the status icon. */
void ui_climate_control_1_set_switch_state(lv_obj_t *widget, bool heater_on, bool climate_control_on);
