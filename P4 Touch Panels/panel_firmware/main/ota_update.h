#pragma once

#include <stdbool.h>

/** Call early from app_main after NVS init path (after ha_mqtt_init / app_prefs_init). Cancels rollback when valid. */
void ota_update_init(void);

/**
 * Queue an OTA from MQTT JSON: {"url":"https://...","version":"1.2.3","sha256":"<64 hex>","size":123456}
 * url required; version optional (skip if matches running PROJECT_VER); sha256+size optional integrity check before boot swap.
 * @return false if OTA disabled, busy, or queue full
 */
bool ota_update_request_from_mqtt_json(const char *json);
