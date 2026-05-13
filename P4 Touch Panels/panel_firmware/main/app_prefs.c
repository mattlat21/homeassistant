#include "app_prefs.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "esp_mac.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "app_prefs";

#define PREFS_NS "esp_hmi"
#define PREFS_KEY_DEF_APP "def_app"
#define PREFS_KEY_IDLE_APP "idle_app"
#define PREFS_KEY_IDLE_SEC "idle_sec"

#define IDLE_TIMEOUT_SEC_MAX (86400u * 365u * 10u)

static bool s_inited;
static app_id_t s_cached_default = APP_HOME;
static bool s_have_cached;
static app_id_t s_cached_idle_app = APP_HOME;
static uint32_t s_cached_idle_sec;

static bool app_id_valid(app_id_t id)
{
    return (unsigned)id < (unsigned)APP_COUNT;
}

void app_prefs_init(void)
{
    if (s_inited) {
        return;
    }
    s_inited = true;

    nvs_handle_t h;
    esp_err_t err = nvs_open(PREFS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open(%s): %s", PREFS_NS, esp_err_to_name(err));
        s_have_cached = false;
        return;
    }
    uint8_t v = (uint8_t)APP_HOME;
    err = nvs_get_u8(h, PREFS_KEY_DEF_APP, &v);
    if (err == ESP_OK && app_id_valid((app_id_t)v)) {
        s_cached_default = (app_id_t)v;
        s_have_cached = true;
    } else {
        s_have_cached = false;
    }

    uint8_t idle_app_byte = (uint8_t)APP_HOME;
    err = nvs_get_u8(h, PREFS_KEY_IDLE_APP, &idle_app_byte);
    if (err == ESP_OK && app_id_valid((app_id_t)idle_app_byte)) {
        s_cached_idle_app = (app_id_t)idle_app_byte;
    } else {
        s_cached_idle_app = APP_HOME;
    }
    uint32_t idle_sec = 0;
    err = nvs_get_u32(h, PREFS_KEY_IDLE_SEC, &idle_sec);
    if (err == ESP_OK) {
        s_cached_idle_sec = idle_sec > IDLE_TIMEOUT_SEC_MAX ? IDLE_TIMEOUT_SEC_MAX : idle_sec;
    } else {
        s_cached_idle_sec = 0;
    }

    nvs_close(h);
}

app_id_t app_prefs_get_default_app(void)
{
    if (s_have_cached && app_id_valid(s_cached_default)) {
        return s_cached_default;
    }
    return APP_HOME;
}

static esp_err_t prefs_write_u8(uint8_t v)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(PREFS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u8(h, PREFS_KEY_DEF_APP, v);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

bool app_prefs_set_default_app(app_id_t id)
{
    if (!app_id_valid(id)) {
        return false;
    }
    if ((s_have_cached && id == s_cached_default) || (!s_have_cached && id == APP_HOME)) {
        return true;
    }
    esp_err_t err = prefs_write_u8((uint8_t)id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set default app failed: %s", esp_err_to_name(err));
        return false;
    }
    s_cached_default = id;
    s_have_cached = true;
    return true;
}

bool app_prefs_parse_app_slug(const char *slug, app_id_t *out)
{
    if (slug == NULL || slug[0] == '\0' || out == NULL) {
        return false;
    }
    if (strcasecmp(slug, "home") == 0) {
        *out = APP_HOME;
        return true;
    }
    if (strcasecmp(slug, "ollie_room") == 0) {
        *out = APP_OLLIE_ROOM;
        return true;
    }
    if (strcasecmp(slug, "dashboard") == 0) {
        *out = APP_DASHBOARD;
        return true;
    }
    if (strcasecmp(slug, "front_gate") == 0) {
        *out = APP_FRONT_GATE;
        return true;
    }
    if (strcasecmp(slug, "pipboy") == 0) {
        *out = APP_PIPBOY;
        return true;
    }
    if (strcasecmp(slug, "settings") == 0) {
        *out = APP_SETTINGS;
        return true;
    }
    if (strcasecmp(slug, "study") == 0) {
        *out = APP_STUDY;
        return true;
    }
    if (strcasecmp(slug, "about") == 0) {
        *out = APP_ABOUT;
        return true;
    }
    if (strcasecmp(slug, "front_door") == 0) {
        *out = APP_FRONT_DOOR;
        return true;
    }
    if (strcasecmp(slug, "kitchen") == 0) {
        *out = APP_KITCHEN;
        return true;
    }
    if (strcasecmp(slug, "studio") == 0) {
        *out = APP_STUDIO;
        return true;
    }
    return false;
}

bool app_prefs_set_default_app_from_slug(const char *slug)
{
    app_id_t id;
    if (!app_prefs_parse_app_slug(slug, &id)) {
        return false;
    }
    return app_prefs_set_default_app(id);
}

bool app_prefs_slug_for_app(app_id_t id, char *out, size_t out_sz)
{
    if (out == NULL || out_sz == 0) {
        return false;
    }
    const char *slug = NULL;
    switch (id) {
    case APP_HOME:
        slug = "home";
        break;
    case APP_OLLIE_ROOM:
        slug = "ollie_room";
        break;
    case APP_DASHBOARD:
        slug = "dashboard";
        break;
    case APP_FRONT_GATE:
        slug = "front_gate";
        break;
    case APP_PIPBOY:
        slug = "pipboy";
        break;
    case APP_SETTINGS:
        slug = "settings";
        break;
    case APP_STUDY:
        slug = "study";
        break;
    case APP_ABOUT:
        slug = "about";
        break;
    case APP_FRONT_DOOR:
        slug = "front_door";
        break;
    case APP_KITCHEN:
        slug = "kitchen";
        break;
    case APP_STUDIO:
        slug = "studio";
        break;
    default:
        return false;
    }
    snprintf(out, out_sz, "%s", slug);
    return true;
}

const char *app_prefs_display_name_for_app(app_id_t id)
{
    switch (id) {
    case APP_HOME:
        return "Home";
    case APP_OLLIE_ROOM:
        return "Ollie's Room";
    case APP_DASHBOARD:
        return "Car Sim";
    case APP_FRONT_GATE:
        return "Front Gate";
    case APP_PIPBOY:
        return "PipBoy";
    case APP_SETTINGS:
        return "Settings";
    case APP_STUDY:
        return "Study";
    case APP_ABOUT:
        return "About";
    case APP_FRONT_DOOR:
        return "Front Door";
    case APP_KITCHEN:
        return "Kitchen";
    case APP_STUDIO:
        return "Studio";
    default:
        return "?";
    }
}

void app_prefs_format_sta_mac(char *out, size_t out_sz)
{
    if (out == NULL || out_sz == 0) {
        return;
    }
    uint8_t mac[6] = { 0 };
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) != ESP_OK) {
        if (esp_efuse_mac_get_default(mac) != ESP_OK) {
            snprintf(out, out_sz, "(unknown)");
            return;
        }
    }
    snprintf(out, out_sz, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void app_prefs_get_idle_timeout(app_id_t *out_app, uint32_t *out_sec)
{
    if (out_app != NULL) {
        *out_app = s_cached_idle_app;
    }
    if (out_sec != NULL) {
        *out_sec = s_cached_idle_sec;
    }
}

bool app_prefs_set_idle_timeout(app_id_t app, uint32_t sec)
{
    if (!app_id_valid(app)) {
        return false;
    }
    if (sec > IDLE_TIMEOUT_SEC_MAX) {
        sec = IDLE_TIMEOUT_SEC_MAX;
    }
    if (app == s_cached_idle_app && sec == s_cached_idle_sec) {
        return true;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open(PREFS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set idle nvs_open: %s", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_u8(h, PREFS_KEY_IDLE_APP, (uint8_t)app);
    if (err == ESP_OK) {
        err = nvs_set_u32(h, PREFS_KEY_IDLE_SEC, sec);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set idle timeout failed: %s", esp_err_to_name(err));
        return false;
    }
    s_cached_idle_app = app;
    s_cached_idle_sec = sec;
    return true;
}

void app_prefs_format_wifi_ssid(char *out, size_t out_sz)
{
    if (out == NULL || out_sz == 0) {
        return;
    }
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
        snprintf(out, out_sz, "(not connected)");
        return;
    }
    const size_t n = sizeof(ap.ssid);
    size_t len = 0;
    while (len < n && ap.ssid[len] != 0) {
        len++;
    }
    if (len == 0) {
        snprintf(out, out_sz, "(not connected)");
        return;
    }
    if (len >= out_sz) {
        len = out_sz - 1;
    }
    memcpy(out, ap.ssid, len);
    out[len] = '\0';
}
