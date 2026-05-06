#pragma once

#include <stdbool.h>

/**
 * Start Wi‑Fi STA (from Kconfig) and MQTT client. On MQTT connect, publishes
 * retained Home Assistant MQTT discovery for Ollie Room device triggers (buttons + room modes).
 * No-op if WiFi SSID is empty.
 */
void ha_mqtt_init(void);

/** True after MQTT_EVENT_CONNECTED until disconnect. */
bool ha_mqtt_is_connected(void);

/**
 * Publish a button event on `esp_hmi/device/<MAC>/status/button_press` as JSON `{"button": "<payload>"}` (QoS 0, not retained).
 * @return false if MQTT is not connected or publish failed to enqueue.
 */
bool ha_mqtt_publish_ollie_button(const char *payload);

/**
 * Called on the LVGL thread with a null-terminated option string from the room state MQTT topic
 * (e.g. Normal, Rest Time, Sleep Time). Return true if the UI applied the value; false to skip
 * updating last-applied (unknown option, widget not ready) so a later message can retry.
 */
typedef bool (*ha_mqtt_room_state_cb_t)(const char *option, void *user_data);

/** Register handler for room mode state topic (may be called before MQTT is up). */
void ha_mqtt_set_ollie_room_state_callback(ha_mqtt_room_state_cb_t cb, void *user_data);

/**
 * Publish selected room mode as JSON `{"button": "<option>"}` on `esp_hmi/device/<MAC>/status/button_press` (QoS 1, not retained).
 * If Kconfig `SCREEN_TEST_MQTT_ROOM_SET_TOPIC_OVERRIDE` is set, publishes the same JSON there instead.
 * Updates last-applied cache so identical retained state echoes are ignored.
 * @return false if disconnected or publish failed.
 */
bool ha_mqtt_publish_ollie_room_option(const char *option);

/**
 * LVGL-thread callback: apply climate widget from retained per-field MQTT topics (see asyncapi / README).
 * @param heater_on mirrored from MQTT `SCREEN_TEST_MQTT_CLIMATE_HEATER_TOPIC` (e.g. `switch.bedroom_3_heater` in HA)
 * @param climate_control_on true when HA climate HVAC mode is active (e.g. `heat`); false for `off` (see MQTT control topic)
 */
typedef void (*ha_mqtt_ollie_climate_apply_cb_t)(float setpoint_c, float current_c, bool heater_on, bool climate_control_on,
                                                 void *user_data);

/** Register handler for merged climate state from MQTT (safe to call before MQTT connects). */
void ha_mqtt_set_ollie_climate_state_callback(ha_mqtt_ollie_climate_apply_cb_t cb, void *user_data);

/**
 * Called on the LVGL thread with a null-terminated Front Gate state string from MQTT
 * (e.g. Closed, Partially Open, open).
 */
typedef void (*ha_mqtt_front_gate_state_cb_t)(const char *state, void *user_data);

/** Register handler for Front Gate state topic (safe to call before MQTT connects). */
void ha_mqtt_set_front_gate_state_callback(ha_mqtt_front_gate_state_cb_t cb, void *user_data);

/** Called on the LVGL thread with Study heater state mirrored from MQTT (`true` = on). */
typedef void (*ha_mqtt_study_heater_state_cb_t)(bool heater_on, void *user_data);

/** Register handler for Study heater retained state topic (safe to call before MQTT connects). */
void ha_mqtt_set_study_heater_state_callback(ha_mqtt_study_heater_state_cb_t cb, void *user_data);
