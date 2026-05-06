#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "ui/nav.h"

/** Call after `nvs_flash_init()` (e.g. from `app_main` after `ha_mqtt_init`). */
void app_prefs_init(void);

/** NVS-backed default screen after loading splash; falls back to APP_HOME if unset/invalid. */
app_id_t app_prefs_get_default_app(void);

/** Persist default screen; returns false if id invalid. */
bool app_prefs_set_default_app(app_id_t id);

/** Parse MQTT slug; returns false if unknown. */
bool app_prefs_set_default_app_from_slug(const char *slug);

/** Parse slug to `app_id_t` without writing NVS (e.g. remote screen commands). */
bool app_prefs_parse_app_slug(const char *slug, app_id_t *out);

/** Null-terminated slug for MQTT / Settings (e.g. `home`). */
bool app_prefs_slug_for_app(app_id_t id, char *out, size_t out_sz);

/** Short display title for Settings UI. */
const char *app_prefs_display_name_for_app(app_id_t id);

/** Connected STA SSID into `out` (UTF-8), or "(not connected)". */
void app_prefs_format_wifi_ssid(char *out, size_t out_sz);

/** STA MAC `AA:BB:…` (uppercase); falls back to base MAC if STA is unavailable. */
void app_prefs_format_sta_mac(char *out, size_t out_sz);

/** Idle timeout target and period; `seconds == 0` means disabled (`out_app` still valid cache). */
void app_prefs_get_idle_timeout(app_id_t *out_app, uint32_t *out_sec);

/** Persist idle navigation; `sec == 0` disables (still stores `app` for later re-enable). */
bool app_prefs_set_idle_timeout(app_id_t app, uint32_t sec);
