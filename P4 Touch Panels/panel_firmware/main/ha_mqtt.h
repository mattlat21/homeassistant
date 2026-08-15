#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * Start Wi‑Fi STA (from Kconfig) and MQTT client. On MQTT connect, publishes
 * retained Home Assistant MQTT discovery for Ollie Room device triggers (buttons + room modes).
 * No-op if WiFi SSID is empty.
 */
void ha_mqtt_init(void);

/** True after MQTT_EVENT_CONNECTED until disconnect. */
bool ha_mqtt_is_connected(void);

/**
 * Publish OTA progress to `esp_hmi/device/<MAC>/status/ota_progress` (QoS 1, not retained).
 * @param state One of: idle, starting, downloading, verifying, success, failed
 * @param percent 0–100 (meaningful for downloading / verifying)
 * @param version Target firmware version string (may be empty)
 * @param error_msg Optional error text when state is failed (ASCII)
 */
void ha_mqtt_publish_ota_progress(const char *state, int percent, const char *version, const char *error_msg);

/**
 * Publish retained current UI screen slug to `esp_hmi/device/<MAC>/status/current_screen` (QoS 1).
 * Safe from LVGL thread after navigation; no-op if MQTT disconnected or topic not built yet.
 */
void ha_mqtt_publish_current_screen_state(void);

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
 * If Kconfig `ESP_HMI_MQTT_ROOM_SET_TOPIC_OVERRIDE` is set, publishes the same JSON there instead.
 * Updates last-applied cache so identical retained state echoes are ignored.
 * @return false if disconnected or publish failed.
 */
bool ha_mqtt_publish_ollie_room_option(const char *option);

/**
 * LVGL-thread callback: apply climate widget from retained per-field MQTT topics (see asyncapi / README).
 * @param heater_on mirrored from MQTT `ESP_HMI_MQTT_CLIMATE_HEATER_TOPIC` (e.g. `switch.bedroom_3_heater` in HA)
 * @param climate_control_on true when HA climate HVAC mode is active (e.g. `heat`); false for `off` (see MQTT control topic)
 */
/** Parsed from climate `control` MQTT (HA entity state); unknown (-1) for legacy bool payloads. */
#define HA_MQTT_CLIMATE_HVAC_UNKNOWN (-1)
#define HA_MQTT_CLIMATE_HVAC_OFF 0
#define HA_MQTT_CLIMATE_HVAC_HEAT 1
#define HA_MQTT_CLIMATE_HVAC_COOL 2
#define HA_MQTT_CLIMATE_HVAC_FAN 3

typedef void (*ha_mqtt_ollie_climate_apply_cb_t)(float setpoint_c, float current_c, bool heater_on, bool climate_control_on,
                                                 int8_t hvac_mode, void *user_data);

/** Register handler for merged climate state from MQTT (safe to call before MQTT connects). */
void ha_mqtt_set_ollie_climate_state_callback(ha_mqtt_ollie_climate_apply_cb_t cb, void *user_data);

/** Add another climate state listener (e.g. HVAC screen); does not remove existing listeners. */
void ha_mqtt_add_ollie_climate_state_callback(ha_mqtt_ollie_climate_apply_cb_t cb, void *user_data);

/** HVAC screen: extra climate zones beyond the primary Kconfig bedroom3 topics (Ollie room). */
#define HA_MQTT_HVAC_ZONE_BEDROOM_1 0
#define HA_MQTT_HVAC_ZONE_SERVER_RACK 1
#define HA_MQTT_HVAC_ZONE_UPSTAIRS_BEDROOM 2
#define HA_MQTT_HVAC_ZONE_STUDIO 3
#define HA_MQTT_HVAC_EXTRA_ZONE_MAX 4

/**
 * Register retained climate topic quartet for an extra HVAC zone (bedroom1, server rack, …).
 * Safe before MQTT connect; subscribes on next connect.
 */
void ha_mqtt_configure_hvac_zone(uint8_t zone_id, const char *topic_setpoint, const char *topic_current,
                               const char *topic_heater_on, const char *topic_control);

/** Single LVGL-thread listener for one extra HVAC zone (replaces prior callback for that zone). */
void ha_mqtt_set_hvac_zone_climate_callback(uint8_t zone_id, ha_mqtt_ollie_climate_apply_cb_t cb, void *user_data);

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

/** Light slots for retained state topics (see @ref ha_mqtt_configure_light). */
#define HA_MQTT_LIGHT_OUTSIDE 0
#define HA_MQTT_LIGHT_LOUNGE 1
#define HA_MQTT_LIGHT_HALLWAY 2
#define HA_MQTT_LIGHT_MAX 4

/** Called on the LVGL thread with retained light state; @a brightness_pct is 0–100. */
typedef void (*ha_mqtt_light_state_cb_t)(bool on, uint8_t brightness_pct, void *user_data);

/**
 * Register the retained topic pair for a light slot (`…/light/state` bool, `…/light/brightness` 0–100).
 * Safe before MQTT connect; subscribes on next connect. Pass NULL @a topic_brightness for a non-dimmable light
 * (the listener then fires on state alone). Clears any callback previously set for the slot.
 */
void ha_mqtt_configure_light(uint8_t light_id, const char *topic_state, const char *topic_brightness);

/** Single LVGL-thread listener per light slot (replaces prior callback); fires immediately if state is cached. */
void ha_mqtt_set_light_state_callback(uint8_t light_id, ha_mqtt_light_state_cb_t cb, void *user_data);

/**
 * Publish JSON `{"light": "<slug>", "power": "on"|"off"}` on `esp_hmi/device/<MAC>/status/light_set`
 * (QoS 0, not retained).
 * @return false if MQTT is not connected or publish failed to enqueue.
 */
bool ha_mqtt_publish_light_power(const char *light_slug, bool on);

/** As @ref ha_mqtt_publish_light_power, with JSON `{"light": "<slug>", "brightness": <0–100>}`. */
bool ha_mqtt_publish_light_brightness(const char *light_slug, uint8_t brightness_pct);

/** Called on the LVGL thread with battery state of charge 0–100 (percent). */
typedef void (*ha_mqtt_house_battery_soc_cb_t)(float soc_percent, void *user_data);

/** Register handler for House Battery SOC topic (safe to call before MQTT connects). */
void ha_mqtt_set_house_battery_soc_callback(ha_mqtt_house_battery_soc_cb_t cb, void *user_data);
