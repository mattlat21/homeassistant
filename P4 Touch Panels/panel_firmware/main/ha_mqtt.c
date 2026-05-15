#include "ha_mqtt.h"

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_event.h"
#include "esp_hosted.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "app_prefs.h"
#include "ota_update.h"
#include "ui/nav.h"
#include "ui/ui_idle_timeout.h"

static const char *TAG = "ha_mqtt";

static esp_mqtt_client_handle_t s_client;
static bool s_mqtt_started;
static volatile bool s_mqtt_connected;

/** e.g. esp_hmi/device/aabbccddeeff — used in discovery topic path and device identifiers */
static char s_node_id[40];
/** Panel presses: <s_node_id>/status/button_press (JSON `{"button": "…"}`) */
static char s_button_press_topic[88];
/** Retained JSON device info: <s_node_id>/status/parameters */
static char s_status_parameters_topic[88];
/** Retained slug: <s_node_id>/status/current_screen */
static char s_status_current_screen_topic[88];
/** Retained ON/OFF: <s_node_id>/status/mqtt_connected (OFF via broker Last Will) */
static char s_status_mqtt_connected_topic[88];
/** JSON device identifier string (same as s_node_id) */
static char s_device_identifier[40];
static char s_mac_colon[18];
/** `esp_hmi/device/<aabbccddeeff>/cmd/set_default_screen` — plain slug payload (QoS 1). */
static char s_set_default_screen_topic[72];
/** Immediate UI navigation JSON `{"screen":"<slug>"}` (does not change NVS default). */
static char s_switch_screen_topic[72];
/** JSON `{ "slug": "…", "seconds": <n> }` or `{ "slug": "…", "duration_ms": <n> }`. */
static char s_switch_screen_temp_topic[88];
/** JSON `{"screen":"<slug>","seconds":<n>}` — `seconds` 0 disables idle return. */
static char s_set_idle_timeout_topic[88];
/** Any payload triggers delayed reboot after a short FreeRTOS deferral. */
static char s_reboot_cmd_topic[72];
#if CONFIG_SCREEN_TEST_OTA_ENABLE
/** `<s_node_id>/<SCREEN_TEST_OTA_MQTT_CMD_SUFFIX>` — JSON OTA command (QoS 1). */
static char s_ota_topic[96];
#endif

#define HA_MQTT_ROOM_TOPIC_MAX 160
#define HA_MQTT_OTA_PAYLOAD_MAX 768
#define HA_MQTT_ROOM_OPTION_MAX 48
#define HA_MQTT_CLIMATE_TOPIC_MAX 160
#define HA_MQTT_GATE_STATE_MAX 64
#define HA_MQTT_SCALAR_PAYLOAD_MAX 64
#define HA_MQTT_PRESS_JSON_MAX 160
#define HA_MQTT_NAV_TEMP_JSON_MAX 288

static char s_room_state_topic[HA_MQTT_ROOM_TOPIC_MAX];
static char s_room_set_topic[HA_MQTT_ROOM_TOPIC_MAX];
static char s_last_applied_room_option[HA_MQTT_ROOM_OPTION_MAX];
static ha_mqtt_room_state_cb_t s_room_state_cb;
static void *s_room_state_cb_ud;

static char s_climate_topic_setpoint[HA_MQTT_CLIMATE_TOPIC_MAX];
static char s_climate_topic_current[HA_MQTT_CLIMATE_TOPIC_MAX];
static char s_climate_topic_heater[HA_MQTT_CLIMATE_TOPIC_MAX];
static char s_climate_topic_control[HA_MQTT_CLIMATE_TOPIC_MAX];
static char s_front_gate_state_topic[HA_MQTT_CLIMATE_TOPIC_MAX];
static char s_study_heater_state_topic[HA_MQTT_CLIMATE_TOPIC_MAX];
static float s_climate_cache_sp;
static float s_climate_cache_cur;
static bool s_climate_cache_heat;
static bool s_climate_cache_control;
/** Bits: setpoint, current, heater, control — UI updates only once all retained fields received. */
static uint8_t s_climate_seen_mask;
static float s_climate_last_sp;
static float s_climate_last_cur;
static bool s_climate_last_heat;
static bool s_climate_last_control;
static bool s_climate_have_last_applied;
/** True while an lv_async climate apply is queued; further dispatches coalesce (read cache when callback runs). */
static bool s_climate_async_pending;
static ha_mqtt_ollie_climate_apply_cb_t s_climate_apply_cb;
static void *s_climate_apply_cb_ud;
static ha_mqtt_front_gate_state_cb_t s_front_gate_state_cb;
static void *s_front_gate_state_cb_ud;
static ha_mqtt_study_heater_state_cb_t s_study_heater_state_cb;
static void *s_study_heater_state_cb_ud;

#define CLIMATE_SEEN_SP (1u << 0)
#define CLIMATE_SEEN_CUR (1u << 1)
#define CLIMATE_SEEN_HEAT (1u << 2)
#define CLIMATE_SEEN_CTRL (1u << 3)
#define CLIMATE_SEEN_ALL 0x0Fu

typedef struct {
    char option[HA_MQTT_ROOM_OPTION_MAX];
} room_state_async_msg_t;

typedef struct {
    char state[HA_MQTT_GATE_STATE_MAX];
} front_gate_state_async_msg_t;

typedef struct {
    bool heater_on;
} study_heater_state_async_msg_t;

typedef enum {
    MQTT_NAV_ASYNC_SWITCH = 1,
    MQTT_NAV_ASYNC_SWITCH_TEMP,
    MQTT_NAV_ASYNC_REBOOT,
    MQTT_NAV_ASYNC_SET_IDLE_TIMEOUT,
} mqtt_nav_async_op_t;

typedef struct {
    uint8_t op;
    app_id_t app;
    uint32_t duration_ms;
} mqtt_nav_async_msg_t;

static const struct {
    const char *slug;
    const char *subtype;
    const char *payload;
} s_ollie_buttons[] = {
    { "light", "button_1", "light" },
    { "fan_off", "button_2", "fan_off" },
    { "fan_1", "button_3", "fan_1" },
    { "fan_2", "button_4", "fan_2" },
    { "fan_3", "button_5", "fan_3" },
    { "climate_dn", "button_6", "climate_temp_dn" },
    { "climate_up", "button_7", "climate_temp_up" },
    { "climate_m_off", "button_8", "climate_mode_off" },
    { "climate_m_on", "button_9", "climate_mode_on" },
    { "climate_m_cc", "button_10", "climate_mode_cc" },
};

/** Room mode taps: same @ref s_button_press_topic; `button` value is the option string (Normal, …). */
static const struct {
    const char *slug;
    const char *subtype;
    const char *payload;
} s_ollie_room_triggers[] = {
    { "room_normal", "room_mode_1", "Normal" },
    { "room_rest", "room_mode_2", "Rest Time" },
    { "room_sleep", "room_mode_3", "Sleep Time" },
};

static void build_ids_from_mac(const uint8_t mac[6])
{
    snprintf(s_node_id, sizeof(s_node_id), "esp_hmi/device/%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3],
             mac[4], mac[5]);
    snprintf(s_device_identifier, sizeof(s_device_identifier), "%s", s_node_id);
    snprintf(s_button_press_topic, sizeof(s_button_press_topic), "%s/status/button_press", s_node_id);
    snprintf(s_status_parameters_topic, sizeof(s_status_parameters_topic), "%s/status/parameters", s_node_id);
    snprintf(s_status_current_screen_topic, sizeof(s_status_current_screen_topic), "%s/status/current_screen", s_node_id);
    snprintf(s_status_mqtt_connected_topic, sizeof(s_status_mqtt_connected_topic), "%s/status/mqtt_connected", s_node_id);
    snprintf(s_mac_colon, sizeof(s_mac_colon), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
             mac[4], mac[5]);

    if (CONFIG_SCREEN_TEST_MQTT_ROOM_STATE_TOPIC_OVERRIDE[0] != '\0') {
        strncpy(s_room_state_topic, CONFIG_SCREEN_TEST_MQTT_ROOM_STATE_TOPIC_OVERRIDE, sizeof(s_room_state_topic) - 1);
        s_room_state_topic[sizeof(s_room_state_topic) - 1] = '\0';
    } else {
        strncpy(s_room_state_topic, CONFIG_SCREEN_TEST_MQTT_ROOM_STATE_TOPIC, sizeof(s_room_state_topic) - 1);
        s_room_state_topic[sizeof(s_room_state_topic) - 1] = '\0';
    }
    if (CONFIG_SCREEN_TEST_MQTT_ROOM_SET_TOPIC_OVERRIDE[0] != '\0') {
        strncpy(s_room_set_topic, CONFIG_SCREEN_TEST_MQTT_ROOM_SET_TOPIC_OVERRIDE, sizeof(s_room_set_topic) - 1);
        s_room_set_topic[sizeof(s_room_set_topic) - 1] = '\0';
    } else {
        snprintf(s_room_set_topic, sizeof(s_room_set_topic), "%s/%s", s_node_id, CONFIG_SCREEN_TEST_MQTT_ROOM_SET_SUFFIX);
    }

    strncpy(s_climate_topic_setpoint, CONFIG_SCREEN_TEST_MQTT_CLIMATE_SETPOINT_TOPIC,
            sizeof(s_climate_topic_setpoint) - 1);
    s_climate_topic_setpoint[sizeof(s_climate_topic_setpoint) - 1] = '\0';
    strncpy(s_climate_topic_current, CONFIG_SCREEN_TEST_MQTT_CLIMATE_CURRENT_TOPIC, sizeof(s_climate_topic_current) - 1);
    s_climate_topic_current[sizeof(s_climate_topic_current) - 1] = '\0';
    strncpy(s_climate_topic_heater, CONFIG_SCREEN_TEST_MQTT_CLIMATE_HEATER_TOPIC, sizeof(s_climate_topic_heater) - 1);
    s_climate_topic_heater[sizeof(s_climate_topic_heater) - 1] = '\0';
    strncpy(s_climate_topic_control, CONFIG_SCREEN_TEST_MQTT_CLIMATE_CONTROL_TOPIC, sizeof(s_climate_topic_control) - 1);
    s_climate_topic_control[sizeof(s_climate_topic_control) - 1] = '\0';
    strncpy(s_front_gate_state_topic, CONFIG_SCREEN_TEST_MQTT_FRONT_GATE_STATE_TOPIC, sizeof(s_front_gate_state_topic) - 1);
    s_front_gate_state_topic[sizeof(s_front_gate_state_topic) - 1] = '\0';
    snprintf(s_study_heater_state_topic, sizeof(s_study_heater_state_topic), "esp_hmi/data/study/heater_on");

    snprintf(s_set_default_screen_topic, sizeof(s_set_default_screen_topic), "%s/cmd/set_default_screen", s_node_id);
    snprintf(s_switch_screen_topic, sizeof(s_switch_screen_topic), "%s/cmd/switch_screen", s_node_id);
    snprintf(s_switch_screen_temp_topic, sizeof(s_switch_screen_temp_topic), "%s/cmd/switch_screen_temp", s_node_id);
    snprintf(s_set_idle_timeout_topic, sizeof(s_set_idle_timeout_topic), "%s/cmd/set_idle_timeout", s_node_id);
    snprintf(s_reboot_cmd_topic, sizeof(s_reboot_cmd_topic), "%s/cmd/reboot", s_node_id);
#if CONFIG_SCREEN_TEST_OTA_ENABLE
    snprintf(s_ota_topic, sizeof(s_ota_topic), "%s/%s", s_node_id, CONFIG_SCREEN_TEST_OTA_MQTT_CMD_SUFFIX);
#endif
}

static void publish_device_status_parameters(esp_mqtt_client_handle_t client);

static void mqtt_reboot_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(250));
    esp_restart();
}

static void mqtt_nav_async_fn(void *user_data)
{
    mqtt_nav_async_msg_t *m = (mqtt_nav_async_msg_t *)user_data;
    if (m == NULL) {
        return;
    }
    if (m->op == MQTT_NAV_ASYNC_REBOOT) {
        free(m);
        if (xTaskCreate(mqtt_reboot_task, "mqtt_reboot", 4096, NULL, 5, NULL) != pdPASS) {
            ESP_LOGE(TAG, "reboot task create failed; restarting now");
            esp_restart();
        }
        return;
    }
    if (m->op == MQTT_NAV_ASYNC_SWITCH) {
        nav_go_to(m->app);
    } else if (m->op == MQTT_NAV_ASYNC_SWITCH_TEMP) {
        nav_go_to_temporarily(m->app, m->duration_ms);
    } else if (m->op == MQTT_NAV_ASYNC_SET_IDLE_TIMEOUT) {
        app_id_t prev_app;
        uint32_t prev_sec;
        app_prefs_get_idle_timeout(&prev_app, &prev_sec);
        bool ok = app_prefs_set_idle_timeout(m->app, m->duration_ms);
        ui_idle_timeout_configure(m->app, m->duration_ms);
        app_id_t new_app;
        uint32_t new_sec;
        app_prefs_get_idle_timeout(&new_app, &new_sec);
        if (ok && (prev_app != new_app || prev_sec != new_sec)) {
            publish_device_status_parameters(s_client);
        }
    }
    free(m);
}

static bool schedule_mqtt_nav_async(const mqtt_nav_async_msg_t *payload)
{
    mqtt_nav_async_msg_t *m = (mqtt_nav_async_msg_t *)calloc(1, sizeof(mqtt_nav_async_msg_t));
    if (m == NULL) {
        return false;
    }
    *m = *payload;
    if (lv_async_call(mqtt_nav_async_fn, m) != LV_RESULT_OK) {
        free(m);
        return false;
    }
    return true;
}

/** JSON body with required string `"screen"` (named app slug); returns false if missing/unknown. */
static bool parse_switch_screen_json(const char *json, app_id_t *out_app)
{
    if (json == NULL || out_app == NULL) {
        return false;
    }
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        return false;
    }
    const cJSON *j_scr = cJSON_GetObjectItemCaseSensitive(root, "screen");
    if (!cJSON_IsString(j_scr) || j_scr->valuestring == NULL || j_scr->valuestring[0] == '\0') {
        cJSON_Delete(root);
        return false;
    }
    app_id_t id;
    if (!app_prefs_parse_app_slug(j_scr->valuestring, &id)) {
        cJSON_Delete(root);
        return false;
    }
    cJSON_Delete(root);
    *out_app = id;
    return true;
}

/** Parse temp switch JSON; fills `out_ms` clamped to [250, 86400000]. Accepts `"screen"` or `"slug"`. */
static bool parse_switch_screen_temp_json(const char *json, app_id_t *out_app, uint32_t *out_ms)
{
    if (json == NULL || out_app == NULL || out_ms == NULL) {
        return false;
    }
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        return false;
    }

    const cJSON *j_slug = cJSON_GetObjectItemCaseSensitive(root, "screen");
    if (j_slug == NULL) {
        j_slug = cJSON_GetObjectItemCaseSensitive(root, "slug");
    }
    if (!cJSON_IsString(j_slug) || j_slug->valuestring == NULL || j_slug->valuestring[0] == '\0') {
        cJSON_Delete(root);
        return false;
    }
    app_id_t id;
    if (!app_prefs_parse_app_slug(j_slug->valuestring, &id)) {
        cJSON_Delete(root);
        return false;
    }

    double span_ms = -1.0;
    const cJSON *j_ms = cJSON_GetObjectItemCaseSensitive(root, "duration_ms");
    const cJSON *j_sec = cJSON_GetObjectItemCaseSensitive(root, "seconds");
    if (cJSON_IsNumber(j_ms)) {
        span_ms = cJSON_GetNumberValue(j_ms);
    } else if (cJSON_IsNumber(j_sec)) {
        span_ms = cJSON_GetNumberValue(j_sec) * 1000.0;
    }
    cJSON_Delete(root);

    if (!isfinite(span_ms) || span_ms < 250.0) {
        span_ms = 250.0;
    }
    if (span_ms > 86400000.0) {
        span_ms = 86400000.0;
    }
    *out_app = id;
    *out_ms = (uint32_t)(span_ms + 0.5);
    return true;
}

/** `{ "screen": "<slug>", "seconds": <n> }` — seconds may be 0 (disabled). */
static bool parse_set_idle_timeout_json(const char *json, app_id_t *out_app, uint32_t *out_sec)
{
    if (json == NULL || out_app == NULL || out_sec == NULL) {
        return false;
    }
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        return false;
    }
    const cJSON *j_slug = cJSON_GetObjectItemCaseSensitive(root, "screen");
    if (!cJSON_IsString(j_slug) || j_slug->valuestring == NULL || j_slug->valuestring[0] == '\0') {
        cJSON_Delete(root);
        return false;
    }
    app_id_t id;
    if (!app_prefs_parse_app_slug(j_slug->valuestring, &id)) {
        cJSON_Delete(root);
        return false;
    }
    const cJSON *j_sec = cJSON_GetObjectItemCaseSensitive(root, "seconds");
    if (!cJSON_IsNumber(j_sec)) {
        cJSON_Delete(root);
        return false;
    }
    double v = cJSON_GetNumberValue(j_sec);
    cJSON_Delete(root);
    if (!isfinite(v) || v < 0.0) {
        return false;
    }
    if (v > (double)(UINT32_MAX - 1)) {
        v = (double)(UINT32_MAX - 1);
    }
    *out_app = id;
    *out_sec = (uint32_t)(v + 0.5);
    return true;
}

static void trim_payload_edges(char *s)
{
    if (s == NULL) {
        return;
    }
    /* leading */
    char *p = s;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    if (p != s) {
        memmove(s, p, strlen(p) + 1);
    }
    /* trailing */
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[--n] = '\0';
    }
}

static void room_state_async_fn(void *user_data)
{
    room_state_async_msg_t *m = (room_state_async_msg_t *)user_data;
    if (m == NULL) {
        return;
    }
    bool applied = false;
    if (s_room_state_cb != NULL) {
        applied = s_room_state_cb(m->option, s_room_state_cb_ud);
    }
    if (applied) {
        strncpy(s_last_applied_room_option, m->option, sizeof(s_last_applied_room_option) - 1);
        s_last_applied_room_option[sizeof(s_last_applied_room_option) - 1] = '\0';
    }
    lv_free(m);
}

static void subscribe_room_state_topic(void)
{
    if (s_client == NULL || s_room_state_topic[0] == '\0') {
        return;
    }
    int mid = esp_mqtt_client_subscribe(s_client, s_room_state_topic, 1);
    if (mid < 0) {
        ESP_LOGE(TAG, "subscribe room state failed: %s", s_room_state_topic);
    } else {
        ESP_LOGI(TAG, "subscribed room state: %s", s_room_state_topic);
    }
}

static void subscribe_climate_state_topics(void)
{
    if (s_client == NULL) {
        return;
    }
    const char *topics[] = {
        s_climate_topic_setpoint,
        s_climate_topic_current,
        s_climate_topic_heater,
        s_climate_topic_control,
    };
    for (size_t i = 0; i < sizeof(topics) / sizeof(topics[0]); i++) {
        if (topics[i] == NULL || topics[i][0] == '\0') {
            continue;
        }
        int mid = esp_mqtt_client_subscribe(s_client, topics[i], 1);
        if (mid < 0) {
            ESP_LOGE(TAG, "subscribe climate topic failed: %s", topics[i]);
        } else {
            ESP_LOGI(TAG, "subscribed climate: %s", topics[i]);
        }
    }
}

static void subscribe_front_gate_state_topic(void)
{
    if (s_client == NULL || s_front_gate_state_topic[0] == '\0') {
        return;
    }
    int mid = esp_mqtt_client_subscribe(s_client, s_front_gate_state_topic, 1);
    if (mid < 0) {
        ESP_LOGE(TAG, "subscribe front gate state failed: %s", s_front_gate_state_topic);
    } else {
        ESP_LOGI(TAG, "subscribed front gate state: %s", s_front_gate_state_topic);
    }
}

static void subscribe_study_heater_state_topic(void)
{
    if (s_client == NULL || s_study_heater_state_topic[0] == '\0') {
        return;
    }
    int mid = esp_mqtt_client_subscribe(s_client, s_study_heater_state_topic, 1);
    if (mid < 0) {
        ESP_LOGE(TAG, "subscribe study heater state failed: %s", s_study_heater_state_topic);
    } else {
        ESP_LOGI(TAG, "subscribed study heater state: %s", s_study_heater_state_topic);
    }
}

static void subscribe_set_default_screen_topic(void)
{
    if (s_client == NULL || s_set_default_screen_topic[0] == '\0') {
        return;
    }
    int mid = esp_mqtt_client_subscribe(s_client, s_set_default_screen_topic, 1);
    if (mid < 0) {
        ESP_LOGE(TAG, "subscribe set_default_screen failed: %s", s_set_default_screen_topic);
    } else {
        ESP_LOGI(TAG, "subscribed set_default_screen: %s", s_set_default_screen_topic);
    }
}

static void subscribe_remote_screen_topics(void)
{
    if (s_client == NULL) {
        return;
    }
    const struct {
        const char *topic;
        const char *label;
    } subs[] = {
        { s_switch_screen_topic, "switch_screen" },
        { s_switch_screen_temp_topic, "switch_screen_temp" },
        { s_set_idle_timeout_topic, "set_idle_timeout" },
        { s_reboot_cmd_topic, "reboot" },
    };
    for (size_t i = 0; i < sizeof(subs) / sizeof(subs[0]); i++) {
        if (subs[i].topic == NULL || subs[i].topic[0] == '\0') {
            continue;
        }
        int mid = esp_mqtt_client_subscribe(s_client, subs[i].topic, 1);
        if (mid < 0) {
            ESP_LOGE(TAG, "subscribe %s failed: %s", subs[i].label, subs[i].topic);
        } else {
            ESP_LOGI(TAG, "subscribed %s: %s", subs[i].label, subs[i].topic);
        }
    }
}

#if CONFIG_SCREEN_TEST_OTA_ENABLE
static void subscribe_ota_topic(void)
{
    if (s_client == NULL || s_ota_topic[0] == '\0') {
        return;
    }
    int mid = esp_mqtt_client_subscribe(s_client, s_ota_topic, 1);
    if (mid < 0) {
        ESP_LOGE(TAG, "subscribe OTA failed: %s", s_ota_topic);
    } else {
        ESP_LOGI(TAG, "subscribed OTA: %s", s_ota_topic);
    }
}
#endif

static bool parse_float_scalar(char *payload, float *out)
{
    trim_payload_edges(payload);
    if (payload[0] == '\0') {
        return false;
    }
    char *end = NULL;
    float v = strtof(payload, &end);
    if (end == payload || !isfinite((double)v)) {
        return false;
    }
    *out = v;
    return true;
}

static bool parse_bool_scalar(char *payload, bool *out)
{
    trim_payload_edges(payload);
    if (payload[0] == '\0') {
        return false;
    }
    if (strcasecmp(payload, "true") == 0 || strcasecmp(payload, "on") == 0 || strcasecmp(payload, "yes") == 0) {
        *out = true;
        return true;
    }
    if (strcasecmp(payload, "false") == 0 || strcasecmp(payload, "off") == 0 || strcasecmp(payload, "no") == 0) {
        *out = false;
        return true;
    }
    if (strcmp(payload, "1") == 0) {
        *out = true;
        return true;
    }
    if (strcmp(payload, "0") == 0) {
        *out = false;
        return true;
    }
    return false;
}

/** Climate control topic: legacy bool scalars, or HA `climate` entity state (`heat` → UI climate mode on, `off` → off). */
static bool parse_climate_control_scalar(char *payload, bool *out)
{
    if (parse_bool_scalar(payload, out)) {
        return true;
    }
    trim_payload_edges(payload);
    if (payload[0] == '\0') {
        return false;
    }
    if (strcasecmp(payload, "heat") == 0 || strcasecmp(payload, "auto") == 0 || strcasecmp(payload, "cool") == 0 ||
        strcasecmp(payload, "dry") == 0 || strcasecmp(payload, "fan_only") == 0) {
        *out = true;
        return true;
    }
    return false;
}

static void climate_state_async_fn(void *user_data)
{
    (void)user_data;
    s_climate_async_pending = false;

    if (s_climate_apply_cb == NULL || s_climate_seen_mask != CLIMATE_SEEN_ALL) {
        return;
    }

    /* Read live cache on LVGL thread so multiple MQTT updates coalesce; avoids stale snapshot order (heater vs control). */
    if (s_climate_have_last_applied && s_climate_last_sp == s_climate_cache_sp && s_climate_last_cur == s_climate_cache_cur &&
        s_climate_last_heat == s_climate_cache_heat && s_climate_last_control == s_climate_cache_control) {
        return;
    }

    float sp = s_climate_cache_sp;
    float cur = s_climate_cache_cur;
    bool heat = s_climate_cache_heat;
    bool cc = s_climate_cache_control;

    s_climate_apply_cb(sp, cur, heat, cc, s_climate_apply_cb_ud);

    s_climate_last_sp = sp;
    s_climate_last_cur = cur;
    s_climate_last_heat = heat;
    s_climate_last_control = cc;
    s_climate_have_last_applied = true;
}

static void front_gate_state_async_fn(void *user_data)
{
    front_gate_state_async_msg_t *m = (front_gate_state_async_msg_t *)user_data;
    if (m == NULL) {
        return;
    }
    if (s_front_gate_state_cb != NULL) {
        s_front_gate_state_cb(m->state, s_front_gate_state_cb_ud);
    }
    lv_free(m);
}

static void study_heater_state_async_fn(void *user_data)
{
    study_heater_state_async_msg_t *m = (study_heater_state_async_msg_t *)user_data;
    if (m == NULL) {
        return;
    }
    if (s_study_heater_state_cb != NULL) {
        s_study_heater_state_cb(m->heater_on, s_study_heater_state_cb_ud);
    }
    lv_free(m);
}

static void try_dispatch_climate_from_cache(void)
{
    if (s_climate_apply_cb == NULL || s_climate_seen_mask != CLIMATE_SEEN_ALL) {
        return;
    }
    if (s_climate_have_last_applied && s_climate_last_sp == s_climate_cache_sp && s_climate_last_cur == s_climate_cache_cur &&
        s_climate_last_heat == s_climate_cache_heat && s_climate_last_control == s_climate_cache_control) {
        return;
    }
    if (s_climate_async_pending) {
        return;
    }
    s_climate_async_pending = true;
    if (lv_async_call(climate_state_async_fn, NULL) != LV_RESULT_OK) {
        s_climate_async_pending = false;
        ESP_LOGW(TAG, "lv_async_call(climate_state) failed");
        return;
    }
}

static void publish_mqtt_connected_on(esp_mqtt_client_handle_t client)
{
    if (client == NULL || s_status_mqtt_connected_topic[0] == '\0') {
        return;
    }
    int msg_id = esp_mqtt_client_publish(client, s_status_mqtt_connected_topic, "ON", 2, 1, 1);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "publish mqtt_connected ON failed");
    }
}

static void publish_discovery_configs(esp_mqtt_client_handle_t client)
{
    const char *dev_name = CONFIG_SCREEN_TEST_HA_DEVICE_NAME;

    for (size_t i = 0; i < sizeof(s_ollie_buttons) / sizeof(s_ollie_buttons[0]); i++) {
        char topic[96];
        snprintf(topic, sizeof(topic), "homeassistant/device_automation/%s/ollie_%s/config", s_node_id,
                 s_ollie_buttons[i].slug);

        char payload[896];
        int len = snprintf(payload, sizeof(payload),
                           "{"
                           "\"platform\":\"device_automation\","
                           "\"automation_type\":\"trigger\","
                           "\"type\":\"button_short_press\","
                           "\"subtype\":\"%s\","
                           "\"topic\":\"%s\","
                           "\"payload\":\"%s\","
                           "\"value_template\":\"{{ value_json.button }}\","
                           "\"device\":{"
                           "\"identifiers\":[\"%s\"],"
                           "\"name\":\"%s\","
                           "\"manufacturer\":\"Espressif\","
                           "\"model\":\"ESP32-P4 Touch\","
                           "\"connections\":[[\"mac\",\"%s\"]]"
                           "}"
                           "}",
                           s_ollie_buttons[i].subtype, s_button_press_topic, s_ollie_buttons[i].payload,
                           s_device_identifier,
                           dev_name, s_mac_colon);
        if (len <= 0 || (size_t)len >= sizeof(payload)) {
            ESP_LOGE(TAG, "discovery JSON truncated for %s", s_ollie_buttons[i].slug);
            continue;
        }

        int msg_id = esp_mqtt_client_publish(client, topic, payload, len, 1, 1);
        if (msg_id < 0) {
            ESP_LOGE(TAG, "discovery publish failed for %s", s_ollie_buttons[i].slug);
        } else {
            ESP_LOGI(TAG, "discovery sent: %s", topic);
        }
    }

    for (size_t i = 0; i < sizeof(s_ollie_room_triggers) / sizeof(s_ollie_room_triggers[0]); i++) {
        char topic[96];
        snprintf(topic, sizeof(topic), "homeassistant/device_automation/%s/ollie_%s/config", s_node_id,
                 s_ollie_room_triggers[i].slug);

        char payload[896];
        int len = snprintf(payload, sizeof(payload),
                           "{"
                           "\"platform\":\"device_automation\","
                           "\"automation_type\":\"trigger\","
                           "\"type\":\"button_short_press\","
                           "\"subtype\":\"%s\","
                           "\"topic\":\"%s\","
                           "\"payload\":\"%s\","
                           "\"value_template\":\"{{ value_json.button }}\","
                           "\"device\":{"
                           "\"identifiers\":[\"%s\"],"
                           "\"name\":\"%s\","
                           "\"manufacturer\":\"Espressif\","
                           "\"model\":\"ESP32-P4 Touch\","
                           "\"connections\":[[\"mac\",\"%s\"]]"
                           "}"
                           "}",
                           s_ollie_room_triggers[i].subtype, s_button_press_topic, s_ollie_room_triggers[i].payload,
                           s_device_identifier, dev_name, s_mac_colon);
        if (len <= 0 || (size_t)len >= sizeof(payload)) {
            ESP_LOGE(TAG, "discovery JSON truncated for %s", s_ollie_room_triggers[i].slug);
            continue;
        }

        int msg_id = esp_mqtt_client_publish(client, topic, payload, len, 1, 1);
        if (msg_id < 0) {
            ESP_LOGE(TAG, "discovery publish failed for %s", s_ollie_room_triggers[i].slug);
        } else {
            ESP_LOGI(TAG, "discovery sent: %s", topic);
        }
    }

    /* Home Assistant MQTT: current screen + MQTT connectivity */
    const char *id_tail = strrchr(s_device_identifier, '/');
    const char *id_suffix = (id_tail != NULL && id_tail[1] != '\0') ? id_tail + 1 : "node";

    char topic_ha[128];
    char payload_ha[960];
    int len_ha;

    snprintf(topic_ha, sizeof(topic_ha), "homeassistant/sensor/esp_hmi_%s_current_screen/config", id_suffix);
    len_ha = snprintf(payload_ha, sizeof(payload_ha),
                      "{"
                      "\"name\":\"Current screen\","
                      "\"unique_id\":\"esp_hmi_%s_current_screen\","
                      "\"state_topic\":\"%s\","
                      "\"device\":{"
                      "\"identifiers\":[\"%s\"],"
                      "\"name\":\"%s\","
                      "\"manufacturer\":\"Espressif\","
                      "\"model\":\"ESP32-P4 Touch\","
                      "\"connections\":[[\"mac\",\"%s\"]]"
                      "}"
                      "}",
                      id_suffix, s_status_current_screen_topic, s_device_identifier, dev_name, s_mac_colon);
    if (len_ha > 0 && (size_t)len_ha < sizeof(payload_ha)) {
        int msg_id = esp_mqtt_client_publish(client, topic_ha, payload_ha, len_ha, 1, 1);
        if (msg_id < 0) {
            ESP_LOGE(TAG, "discovery publish failed: current_screen");
        } else {
            ESP_LOGI(TAG, "discovery sent: %s", topic_ha);
        }
    } else {
        ESP_LOGE(TAG, "discovery JSON truncated: current_screen");
    }

    snprintf(topic_ha, sizeof(topic_ha), "homeassistant/binary_sensor/esp_hmi_%s_mqtt_connected/config", id_suffix);
    len_ha = snprintf(payload_ha, sizeof(payload_ha),
                        "{"
                        "\"name\":\"MQTT connected\","
                        "\"unique_id\":\"esp_hmi_%s_mqtt_connected\","
                        "\"device_class\":\"connectivity\","
                        "\"state_topic\":\"%s\","
                        "\"payload_on\":\"ON\","
                        "\"payload_off\":\"OFF\","
                        "\"device\":{"
                        "\"identifiers\":[\"%s\"],"
                        "\"name\":\"%s\","
                        "\"manufacturer\":\"Espressif\","
                        "\"model\":\"ESP32-P4 Touch\","
                        "\"connections\":[[\"mac\",\"%s\"]]"
                        "}"
                        "}",
                        id_suffix, s_status_mqtt_connected_topic, s_device_identifier, dev_name, s_mac_colon);
    if (len_ha > 0 && (size_t)len_ha < sizeof(payload_ha)) {
        int msg_id = esp_mqtt_client_publish(client, topic_ha, payload_ha, len_ha, 1, 1);
        if (msg_id < 0) {
            ESP_LOGE(TAG, "discovery publish failed: mqtt_connected");
        } else {
            ESP_LOGI(TAG, "discovery sent: %s", topic_ha);
        }
    } else {
        ESP_LOGE(TAG, "discovery JSON truncated: mqtt_connected");
    }
}

static void publish_device_status_parameters(esp_mqtt_client_handle_t client)
{
    if (client == NULL || s_status_parameters_topic[0] == '\0') {
        return;
    }
    cJSON *j = cJSON_CreateObject();
    if (j == NULL) {
        ESP_LOGW(TAG, "status/parameters: cJSON_CreateObject failed");
        return;
    }

    (void)cJSON_AddStringToObject(j, "node_id", s_device_identifier);
    (void)cJSON_AddStringToObject(j, "device_id", s_device_identifier);
    (void)cJSON_AddStringToObject(j, "mac", s_mac_colon);

    const char *mac_tail = strrchr(s_device_identifier, '/');
    if (mac_tail != NULL && mac_tail[1] != '\0') {
        (void)cJSON_AddStringToObject(j, "mac_hex_lower", mac_tail + 1);
    }

    const esp_app_desc_t *app = esp_app_get_description();
    if (app != NULL) {
        (void)cJSON_AddStringToObject(j, "firmware_version", app->version);
        (void)cJSON_AddStringToObject(j, "project_name", app->project_name);
        (void)cJSON_AddStringToObject(j, "compile_time", app->time);
        (void)cJSON_AddStringToObject(j, "compile_date", app->date);
        (void)cJSON_AddStringToObject(j, "app_idf_version", app->idf_ver);
    }
    (void)cJSON_AddStringToObject(j, "esp_idf_version", esp_get_idf_version());

#ifdef CONFIG_IDF_TARGET
    (void)cJSON_AddStringToObject(j, "chip_target", CONFIG_IDF_TARGET);
#endif

    esp_chip_info_t chip = { 0 };
    esp_chip_info(&chip);
    (void)cJSON_AddNumberToObject(j, "chip_cores", chip.cores);
    (void)cJSON_AddNumberToObject(j, "chip_revision", chip.revision);

    (void)cJSON_AddStringToObject(j, "ha_device_name", CONFIG_SCREEN_TEST_HA_DEVICE_NAME);

    wifi_ap_record_t ap = { 0 };
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK && ap.ssid[0] != '\0') {
        (void)cJSON_AddStringToObject(j, "wifi_ssid", (const char *)ap.ssid);
    } else {
        (void)cJSON_AddNullToObject(j, "wifi_ssid");
    }

    char slug_tmp[24];
    app_id_t def_app = app_prefs_get_default_app();
    if (app_prefs_slug_for_app(def_app, slug_tmp, sizeof(slug_tmp))) {
        (void)cJSON_AddStringToObject(j, "default_screen", slug_tmp);
    }
    app_id_t idle_app;
    uint32_t idle_sec_u32 = 0;
    app_prefs_get_idle_timeout(&idle_app, &idle_sec_u32);
    if (app_prefs_slug_for_app(idle_app, slug_tmp, sizeof(slug_tmp))) {
        (void)cJSON_AddStringToObject(j, "idle_timeout_screen", slug_tmp);
    }
    (void)cJSON_AddNumberToObject(j, "idle_timeout_seconds", (double)idle_sec_u32);

    char *printed = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    if (printed == NULL) {
        ESP_LOGW(TAG, "status/parameters: cJSON_PrintUnformatted failed");
        return;
    }

    int len = (int)strlen(printed);
    int msg_id = esp_mqtt_client_publish(client, s_status_parameters_topic, printed, len, 1, 1);
    cJSON_free(printed);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "status/parameters publish failed");
    } else {
        ESP_LOGI(TAG, "published status/parameters (retained): %s", s_status_parameters_topic);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        s_mqtt_connected = true;
        publish_discovery_configs(s_client);
        subscribe_room_state_topic();
        subscribe_climate_state_topics();
        subscribe_front_gate_state_topic();
        subscribe_study_heater_state_topic();
        subscribe_set_default_screen_topic();
        subscribe_remote_screen_topics();
#if CONFIG_SCREEN_TEST_OTA_ENABLE
        subscribe_ota_topic();
#endif
        publish_device_status_parameters(s_client);
        publish_mqtt_connected_on(s_client);
        ha_mqtt_publish_current_screen_state();
        ESP_LOGI(TAG, "status/button_press topic: %s", s_button_press_topic);
        ESP_LOGI(TAG, "status/parameters topic: %s", s_status_parameters_topic);
        ESP_LOGI(TAG, "status/current_screen topic: %s", s_status_current_screen_topic);
        ESP_LOGI(TAG, "status/mqtt_connected topic: %s", s_status_mqtt_connected_topic);
        ESP_LOGI(TAG, "cmd/set_idle_timeout topic: %s", s_set_idle_timeout_topic);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        s_mqtt_connected = false;
        break;
    case MQTT_EVENT_DATA: {
        esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)event_data;
        if (ev == NULL || ev->topic_len <= 0 || ev->data_len < 0) {
            break;
        }
        if ((size_t)ev->topic_len >= HA_MQTT_ROOM_TOPIC_MAX) {
            break;
        }

        char tbuf[HA_MQTT_ROOM_TOPIC_MAX];
        memcpy(tbuf, ev->topic, (size_t)ev->topic_len);
        tbuf[ev->topic_len] = '\0';
        if (strcmp(tbuf, s_room_state_topic) == 0) {
            if (ev->data_len >= HA_MQTT_ROOM_OPTION_MAX) {
                ESP_LOGW(TAG, "room state payload too long (%d)", ev->data_len);
                break;
            }
            char payload[HA_MQTT_ROOM_OPTION_MAX];
            memcpy(payload, ev->data, (size_t)ev->data_len);
            payload[ev->data_len] = '\0';
            trim_payload_edges(payload);
            if (payload[0] == '\0') {
                break;
            }
            if (strcmp(payload, s_last_applied_room_option) == 0) {
                break;
            }
            room_state_async_msg_t *msg = (room_state_async_msg_t *)lv_malloc(sizeof(room_state_async_msg_t));
            if (msg == NULL) {
                ESP_LOGE(TAG, "room state async alloc failed");
                break;
            }
            strncpy(msg->option, payload, sizeof(msg->option) - 1);
            msg->option[sizeof(msg->option) - 1] = '\0';
            if (lv_async_call(room_state_async_fn, msg) != LV_RESULT_OK) {
                lv_free(msg);
                ESP_LOGW(TAG, "lv_async_call(room_state) failed");
            }
            break;
        }

        if (strcmp(tbuf, s_climate_topic_setpoint) == 0) {
            if (ev->data_len <= 0 || ev->data_len >= HA_MQTT_SCALAR_PAYLOAD_MAX) {
                ESP_LOGW(TAG, "climate setpoint payload bad len (%d)", ev->data_len);
                break;
            }
            char payload[HA_MQTT_SCALAR_PAYLOAD_MAX];
            memcpy(payload, ev->data, (size_t)ev->data_len);
            payload[ev->data_len] = '\0';
            float v = 0.0f;
            if (parse_float_scalar(payload, &v)) {
                s_climate_cache_sp = v;
                s_climate_seen_mask |= CLIMATE_SEEN_SP;
                try_dispatch_climate_from_cache();
            } else {
                ESP_LOGW(TAG, "climate setpoint parse failed");
            }
            break;
        }
        if (strcmp(tbuf, s_climate_topic_current) == 0) {
            if (ev->data_len <= 0 || ev->data_len >= HA_MQTT_SCALAR_PAYLOAD_MAX) {
                ESP_LOGW(TAG, "climate current payload bad len (%d)", ev->data_len);
                break;
            }
            char payload[HA_MQTT_SCALAR_PAYLOAD_MAX];
            memcpy(payload, ev->data, (size_t)ev->data_len);
            payload[ev->data_len] = '\0';
            float v = 0.0f;
            if (parse_float_scalar(payload, &v)) {
                s_climate_cache_cur = v;
                s_climate_seen_mask |= CLIMATE_SEEN_CUR;
                try_dispatch_climate_from_cache();
            } else {
                ESP_LOGW(TAG, "climate current parse failed");
            }
            break;
        }
        if (strcmp(tbuf, s_climate_topic_heater) == 0) {
            if (ev->data_len <= 0 || ev->data_len >= HA_MQTT_SCALAR_PAYLOAD_MAX) {
                ESP_LOGW(TAG, "climate heater payload bad len (%d)", ev->data_len);
                break;
            }
            char payload[HA_MQTT_SCALAR_PAYLOAD_MAX];
            memcpy(payload, ev->data, (size_t)ev->data_len);
            payload[ev->data_len] = '\0';
            bool b = false;
            if (parse_bool_scalar(payload, &b)) {
                s_climate_cache_heat = b;
                s_climate_seen_mask |= CLIMATE_SEEN_HEAT;
                try_dispatch_climate_from_cache();
            } else {
                ESP_LOGW(TAG, "climate heater parse failed");
            }
            break;
        }
        if (strcmp(tbuf, s_climate_topic_control) == 0) {
            if (ev->data_len <= 0 || ev->data_len >= HA_MQTT_SCALAR_PAYLOAD_MAX) {
                ESP_LOGW(TAG, "climate control payload bad len (%d)", ev->data_len);
                break;
            }
            char payload[HA_MQTT_SCALAR_PAYLOAD_MAX];
            memcpy(payload, ev->data, (size_t)ev->data_len);
            payload[ev->data_len] = '\0';
            bool b = false;
            if (parse_climate_control_scalar(payload, &b)) {
                s_climate_cache_control = b;
                s_climate_seen_mask |= CLIMATE_SEEN_CTRL;
                try_dispatch_climate_from_cache();
            } else {
                ESP_LOGW(TAG, "climate control parse failed");
            }
            break;
        }
        if (strcmp(tbuf, s_front_gate_state_topic) == 0) {
            if (ev->data_len <= 0 || ev->data_len >= HA_MQTT_GATE_STATE_MAX) {
                ESP_LOGW(TAG, "front gate state payload bad len (%d)", ev->data_len);
                break;
            }
            front_gate_state_async_msg_t *msg = (front_gate_state_async_msg_t *)lv_malloc(sizeof(front_gate_state_async_msg_t));
            if (msg == NULL) {
                ESP_LOGE(TAG, "front gate state async alloc failed");
                break;
            }
            memcpy(msg->state, ev->data, (size_t)ev->data_len);
            msg->state[ev->data_len] = '\0';
            trim_payload_edges(msg->state);
            if (msg->state[0] == '\0') {
                lv_free(msg);
                break;
            }
            if (lv_async_call(front_gate_state_async_fn, msg) != LV_RESULT_OK) {
                lv_free(msg);
                ESP_LOGW(TAG, "lv_async_call(front_gate_state) failed");
            }
            break;
        }
        if (strcmp(tbuf, s_study_heater_state_topic) == 0) {
            if (ev->data_len <= 0 || ev->data_len >= HA_MQTT_SCALAR_PAYLOAD_MAX) {
                ESP_LOGW(TAG, "study heater state payload bad len (%d)", ev->data_len);
                break;
            }
            char payload[HA_MQTT_SCALAR_PAYLOAD_MAX];
            memcpy(payload, ev->data, (size_t)ev->data_len);
            payload[ev->data_len] = '\0';
            bool b = false;
            if (!parse_bool_scalar(payload, &b)) {
                ESP_LOGW(TAG, "study heater state parse failed");
                break;
            }
            study_heater_state_async_msg_t *msg =
                (study_heater_state_async_msg_t *)lv_malloc(sizeof(study_heater_state_async_msg_t));
            if (msg == NULL) {
                ESP_LOGE(TAG, "study heater state async alloc failed");
                break;
            }
            msg->heater_on = b;
            if (lv_async_call(study_heater_state_async_fn, msg) != LV_RESULT_OK) {
                lv_free(msg);
                ESP_LOGW(TAG, "lv_async_call(study_heater_state) failed");
            }
            break;
        }
        if (strcmp(tbuf, s_switch_screen_topic) == 0) {
            if (ev->data_len <= 0 || ev->data_len >= HA_MQTT_NAV_TEMP_JSON_MAX) {
                ESP_LOGW(TAG, "switch_screen payload bad len (%d)", ev->data_len);
                break;
            }
            char jsonbuf[HA_MQTT_NAV_TEMP_JSON_MAX];
            memcpy(jsonbuf, ev->data, (size_t)ev->data_len);
            jsonbuf[ev->data_len] = '\0';
            app_id_t id;
            if (!parse_switch_screen_json(jsonbuf, &id)) {
                ESP_LOGW(TAG, "switch_screen invalid JSON or unknown screen slug");
                break;
            }
            mqtt_nav_async_msg_t req = {.op = MQTT_NAV_ASYNC_SWITCH, .app = id, .duration_ms = 0 };
            if (schedule_mqtt_nav_async(&req)) {
                char slug_dbg[48];
                (void)app_prefs_slug_for_app(id, slug_dbg, sizeof(slug_dbg));
                ESP_LOGI(TAG, "switch_screen -> %s", slug_dbg);
            } else {
                ESP_LOGW(TAG, "switch_screen lv_async_call failed");
            }
            break;
        }
        if (strcmp(tbuf, s_switch_screen_temp_topic) == 0) {
            if (ev->data_len <= 0 || ev->data_len >= HA_MQTT_NAV_TEMP_JSON_MAX) {
                ESP_LOGW(TAG, "switch_screen_temp payload bad len (%d)", ev->data_len);
                break;
            }
            char jsonbuf[HA_MQTT_NAV_TEMP_JSON_MAX];
            memcpy(jsonbuf, ev->data, (size_t)ev->data_len);
            jsonbuf[ev->data_len] = '\0';
            app_id_t id;
            uint32_t duration_ms = 0;
            if (!parse_switch_screen_temp_json(jsonbuf, &id, &duration_ms)) {
                ESP_LOGW(TAG, "switch_screen_temp invalid JSON/slug/duration");
                break;
            }
            mqtt_nav_async_msg_t req = {.op = MQTT_NAV_ASYNC_SWITCH_TEMP, .app = id, .duration_ms = duration_ms };
            if (schedule_mqtt_nav_async(&req)) {
                char slug_dbg[48];
                (void)app_prefs_slug_for_app(id, slug_dbg, sizeof(slug_dbg));
                ESP_LOGI(TAG, "switch_screen_temp -> %s for %" PRIu32 " ms", slug_dbg, (uint32_t)duration_ms);
            } else {
                ESP_LOGW(TAG, "switch_screen_temp lv_async_call failed");
            }
            break;
        }
        if (strcmp(tbuf, s_reboot_cmd_topic) == 0) {
            mqtt_nav_async_msg_t req = {.op = MQTT_NAV_ASYNC_REBOOT, .app = APP_HOME, .duration_ms = 0 };
            if (schedule_mqtt_nav_async(&req)) {
                ESP_LOGI(TAG, "reboot command scheduled");
            } else {
                ESP_LOGW(TAG, "reboot lv_async_call failed");
            }
            break;
        }
        if (strcmp(tbuf, s_set_idle_timeout_topic) == 0) {
            if (ev->data_len <= 0 || ev->data_len >= HA_MQTT_NAV_TEMP_JSON_MAX) {
                ESP_LOGW(TAG, "set_idle_timeout payload bad len (%d)", ev->data_len);
                break;
            }
            char jsonbuf[HA_MQTT_NAV_TEMP_JSON_MAX];
            memcpy(jsonbuf, ev->data, (size_t)ev->data_len);
            jsonbuf[ev->data_len] = '\0';
            app_id_t id;
            uint32_t sec = 0;
            if (!parse_set_idle_timeout_json(jsonbuf, &id, &sec)) {
                ESP_LOGW(TAG, "set_idle_timeout invalid JSON/slug/seconds");
                break;
            }
            mqtt_nav_async_msg_t req = {.op = MQTT_NAV_ASYNC_SET_IDLE_TIMEOUT, .app = id, .duration_ms = sec };
            if (schedule_mqtt_nav_async(&req)) {
                char slug_dbg[48];
                (void)app_prefs_slug_for_app(id, slug_dbg, sizeof(slug_dbg));
                ESP_LOGI(TAG, "set_idle_timeout -> screen=%s seconds=%" PRIu32, slug_dbg, sec);
            } else {
                ESP_LOGW(TAG, "set_idle_timeout lv_async_call failed");
            }
            break;
        }
        if (strcmp(tbuf, s_set_default_screen_topic) == 0) {
            if (ev->data_len <= 0 || ev->data_len >= HA_MQTT_SCALAR_PAYLOAD_MAX) {
                ESP_LOGW(TAG, "set_default_screen payload bad len (%d)", ev->data_len);
                break;
            }
            char payload[HA_MQTT_SCALAR_PAYLOAD_MAX];
            memcpy(payload, ev->data, (size_t)ev->data_len);
            payload[ev->data_len] = '\0';
            trim_payload_edges(payload);
            if (payload[0] == '\0') {
                break;
            }
            app_id_t want_id;
            if (!app_prefs_parse_app_slug(payload, &want_id)) {
                ESP_LOGW(TAG, "set_default_screen unknown slug: %s", payload);
                break;
            }
            if (app_prefs_get_default_app() == want_id) {
                break;
            }
            if (app_prefs_set_default_app(want_id)) {
                ESP_LOGI(TAG, "default screen persisted: %s", payload);
                publish_device_status_parameters(s_client);
            }
            break;
        }
#if CONFIG_SCREEN_TEST_OTA_ENABLE
        if (strcmp(tbuf, s_ota_topic) == 0) {
            if (ev->data_len <= 0 || ev->data_len >= HA_MQTT_OTA_PAYLOAD_MAX) {
                ESP_LOGW(TAG, "OTA payload bad len (%d)", ev->data_len);
                break;
            }
            char payload_ota[HA_MQTT_OTA_PAYLOAD_MAX];
            memcpy(payload_ota, ev->data, (size_t)ev->data_len);
            payload_ota[ev->data_len] = '\0';
            trim_payload_edges(payload_ota);
            if (payload_ota[0] == '\0') {
                break;
            }
            if (ota_update_request_from_mqtt_json(payload_ota)) {
                ESP_LOGI(TAG, "OTA update queued");
            } else {
                ESP_LOGW(TAG, "OTA update not queued (disabled, busy, or bad request)");
            }
            break;
        }
#endif
        break;
    }
    default:
        break;
    }
}

static void start_mqtt_client(void)
{
    if (s_mqtt_started) {
        return;
    }
    s_mqtt_started = true;

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_SCREEN_TEST_MQTT_BROKER_URI,
        /* Default 6 KiB is tight when discovery + TLS + long topic paths run in mqtt_task (stack canary fault). */
        .task.stack_size = 12288,
    };

    if (CONFIG_SCREEN_TEST_MQTT_USER[0] != '\0') {
        mqtt_cfg.credentials.username = CONFIG_SCREEN_TEST_MQTT_USER;
    }
    if (CONFIG_SCREEN_TEST_MQTT_PASSWORD[0] != '\0') {
        mqtt_cfg.credentials.authentication.password = CONFIG_SCREEN_TEST_MQTT_PASSWORD;
    }

    mqtt_cfg.session.last_will.topic = s_status_mqtt_connected_topic;
    mqtt_cfg.session.last_will.msg = "OFF";
    mqtt_cfg.session.last_will.msg_len = 3;
    mqtt_cfg.session.last_will.qos = 1;
    mqtt_cfg.session.last_will.retain = 1;

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "esp_mqtt_client_init failed");
        return;
    }
    esp_mqtt_client_register_event(s_client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
    esp_err_t err = esp_mqtt_client_start(s_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_mqtt_client_start: %s", esp_err_to_name(err));
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *d = (const wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "WiFi disconnected reason=%u, retrying", (unsigned)d->reason);
        s_mqtt_connected = false;
        esp_wifi_connect();
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "got IP");
        uint8_t mac[6];
        if (esp_wifi_get_mac(WIFI_IF_STA, mac) != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_get_mac failed");
            return;
        }
        build_ids_from_mac(mac);
        ESP_LOGI(TAG, "status/button_press MQTT topic: %s", s_button_press_topic);
        start_mqtt_client();
    }
}

bool ha_mqtt_is_connected(void)
{
    return s_mqtt_connected;
}

void ha_mqtt_publish_current_screen_state(void)
{
    if (!s_mqtt_connected || s_client == NULL || s_status_current_screen_topic[0] == '\0') {
        return;
    }
    const char *slug = "unknown";
    char buf[24];
    if (nav_is_on_registered_screen()) {
        app_id_t cur = nav_get_current_app();
        if (app_prefs_slug_for_app(cur, buf, sizeof(buf))) {
            slug = buf;
        }
    }
    int slen = (int)strlen(slug);
    int msg_id = esp_mqtt_client_publish(s_client, s_status_current_screen_topic, slug, slen, 1, 1);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "publish current_screen failed");
    }
}

bool ha_mqtt_publish_ollie_button(const char *payload)
{
    if (payload == NULL || !s_mqtt_connected || s_client == NULL) {
        return false;
    }
    size_t plen = strlen(payload);
    if (plen == 0) {
        return false;
    }
    char body[HA_MQTT_PRESS_JSON_MAX];
    int n = snprintf(body, sizeof(body), "{\"button\": \"%s\"}", payload);
    if (n <= 0 || (size_t)n >= sizeof(body)) {
        return false;
    }
    int msg_id = esp_mqtt_client_publish(s_client, s_button_press_topic, body, n, 0, 0);
    return msg_id >= 0;
}

void ha_mqtt_set_ollie_room_state_callback(ha_mqtt_room_state_cb_t cb, void *user_data)
{
    s_room_state_cb = cb;
    s_room_state_cb_ud = user_data;
}

void ha_mqtt_set_ollie_climate_state_callback(ha_mqtt_ollie_climate_apply_cb_t cb, void *user_data)
{
    s_climate_apply_cb = cb;
    s_climate_apply_cb_ud = user_data;
}

void ha_mqtt_set_front_gate_state_callback(ha_mqtt_front_gate_state_cb_t cb, void *user_data)
{
    s_front_gate_state_cb = cb;
    s_front_gate_state_cb_ud = user_data;
}

void ha_mqtt_set_study_heater_state_callback(ha_mqtt_study_heater_state_cb_t cb, void *user_data)
{
    s_study_heater_state_cb = cb;
    s_study_heater_state_cb_ud = user_data;
}

bool ha_mqtt_publish_ollie_room_option(const char *option)
{
    if (option == NULL || option[0] == '\0' || !s_mqtt_connected || s_client == NULL) {
        return false;
    }
    if (s_button_press_topic[0] == '\0') {
        return false;
    }
    size_t olen = strlen(option);
    if (olen == 0 || olen >= sizeof(s_last_applied_room_option)) {
        return false;
    }
    char body[HA_MQTT_PRESS_JSON_MAX];
    int n = snprintf(body, sizeof(body), "{\"button\": \"%s\"}", option);
    if (n <= 0 || (size_t)n >= sizeof(body)) {
        return false;
    }
    const char *topic = s_button_press_topic;
    if (CONFIG_SCREEN_TEST_MQTT_ROOM_SET_TOPIC_OVERRIDE[0] != '\0' && s_room_set_topic[0] != '\0') {
        topic = s_room_set_topic;
    }
    int msg_id = esp_mqtt_client_publish(s_client, topic, body, n, 1, 0);
    if (msg_id < 0) {
        return false;
    }
    strncpy(s_last_applied_room_option, option, sizeof(s_last_applied_room_option) - 1);
    s_last_applied_room_option[sizeof(s_last_applied_room_option) - 1] = '\0';
    return true;
}

void ha_mqtt_init(void)
{
    if (CONFIG_SCREEN_TEST_WIFI_SSID[0] == '\0') {
        ESP_LOGW(TAG, "SCREEN_TEST_WIFI_SSID empty — skipping Wi-Fi/MQTT");
        return;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    } else if (ret == ESP_ERR_INVALID_STATE) {
        ret = ESP_OK;
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* SDIO transport to ESP32-C6: (re)open link after netif + event loop (see esp_hosted examples). */
    ESP_ERROR_CHECK((esp_err_t)esp_hosted_connect_to_slave());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t inst_any_id;
    esp_event_handler_instance_t inst_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL,
                                                        &inst_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL,
                                                        &inst_got_ip));

    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.sta.ssid, CONFIG_SCREEN_TEST_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, CONFIG_SCREEN_TEST_WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    uint8_t sta_mac[6];
    ret = esp_efuse_mac_get_default(sta_mac);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_efuse_mac_get_default failed: %s", esp_err_to_name(ret));
    } else {
        ret = esp_wifi_set_mac(WIFI_IF_STA, sta_mac);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_set_mac(WIFI_IF_STA) failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "STA MAC set from eFuse: %02x:%02x:%02x:%02x:%02x:%02x", sta_mac[0], sta_mac[1],
                     sta_mac[2], sta_mac[3], sta_mac[4], sta_mac[5]);
        }
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi STA starting (SSID=%s)", CONFIG_SCREEN_TEST_WIFI_SSID);
}
