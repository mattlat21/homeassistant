#include "ui/screens/screen_hvac_legacy.h"

#include <stdint.h>

#include "bsp/display.h"
#include "ha_mqtt.h"
#include "ui/components/ui_legacy_heater_card_1.h"

#define HVAC_LEGACY_HEATER_CARD_COUNT 5
#define HVAC_LEGACY_CARD_GAP_PX 10
#define HVAC_LEGACY_CIRCLE_MARGIN_PX 20
#define HVAC_LEGACY_BUTTON_GAP_PX 10

typedef struct {
    const char *room_name;
    ui_legacy_heater_card_1_profile_t profile;
    const char *btn_prefix;
    int8_t mqtt_zone;
} hvac_legacy_card_def_t;

static lv_obj_t *s_heater_cards[HVAC_LEGACY_HEATER_CARD_COUNT];

static const hvac_legacy_card_def_t s_card_defs[HVAC_LEGACY_HEATER_CARD_COUNT] = {
    { "Ollie's Room", UI_LEGACY_HEATER_CARD_PROFILE_HEATER, NULL, -1 },
    { "Our Bedroom", UI_LEGACY_HEATER_CARD_PROFILE_HEATER, "climate_bedroom_1", HA_MQTT_HVAC_ZONE_BEDROOM_1 },
    { "Upstairs Bedroom", UI_LEGACY_HEATER_CARD_PROFILE_HEAT_COOL_FAN, "climate_upstairs_bedroom",
      HA_MQTT_HVAC_ZONE_UPSTAIRS_BEDROOM },
    { "Studio", UI_LEGACY_HEATER_CARD_PROFILE_HEAT_COOL, "climate_studio", HA_MQTT_HVAC_ZONE_STUDIO },
    { "Server Rack", UI_LEGACY_HEATER_CARD_PROFILE_FAN, "climate_server_rack", HA_MQTT_HVAC_ZONE_SERVER_RACK },
};

static void hvac_legacy_apply_heater_card(lv_obj_t *card, float setpoint_c, float current_c, bool heater_on,
                                          bool climate_control_on, int8_t hvac_mode)
{
    if (card == NULL) {
        return;
    }
    ui_legacy_heater_card_1_set_switch_state(card, heater_on, climate_control_on);
    ui_legacy_heater_card_1_set_hvac_mode(card, hvac_mode);
    ui_legacy_heater_card_1_set_setpoint(card, setpoint_c);
    ui_legacy_heater_card_1_set_current_temp(card, current_c);
}

static void hvac_legacy_climate_apply_card0(float setpoint_c, float current_c, bool heater_on, bool climate_control_on,
                                            int8_t hvac_mode, void *user_data)
{
    (void)hvac_mode;
    (void)user_data;
    hvac_legacy_apply_heater_card(s_heater_cards[0], setpoint_c, current_c, heater_on, climate_control_on,
                                  HA_MQTT_CLIMATE_HVAC_UNKNOWN);
}

static void hvac_legacy_climate_apply_zone(float setpoint_c, float current_c, bool heater_on, bool climate_control_on,
                                           int8_t hvac_mode, void *user_data)
{
    const unsigned card_idx = (unsigned)(uintptr_t)user_data;
    if (card_idx >= HVAC_LEGACY_HEATER_CARD_COUNT) {
        return;
    }
    hvac_legacy_apply_heater_card(s_heater_cards[card_idx], setpoint_c, current_c, heater_on, climate_control_on,
                                  hvac_mode);
}

static void hvac_legacy_publish_action(const hvac_legacy_card_def_t *def, const char *suffix)
{
    if (def == NULL || suffix == NULL) {
        return;
    }
    if (def->btn_prefix == NULL) {
        char legacy[48];
        const int n = snprintf(legacy, sizeof(legacy), "climate_%s", suffix);
        if (n > 0 && (size_t)n < sizeof(legacy)) {
            (void)ha_mqtt_publish_ollie_button(legacy);
        }
        return;
    }
    char payload[64];
    const int n = snprintf(payload, sizeof(payload), "%s_%s", def->btn_prefix, suffix);
    if (n > 0 && (size_t)n < sizeof(payload)) {
        (void)ha_mqtt_publish_ollie_button(payload);
    }
}

static void hvac_legacy_heater_on_ui_event(ui_legacy_heater_card_1_event_t event, void *user_data)
{
    const unsigned card_idx = (unsigned)(uintptr_t)user_data;
    if (card_idx >= HVAC_LEGACY_HEATER_CARD_COUNT) {
        return;
    }
    const hvac_legacy_card_def_t *def = &s_card_defs[card_idx];

    switch (event) {
    case UI_LEGACY_HEATER_CARD_1_EVENT_SETPOINT_DEC:
        hvac_legacy_publish_action(def, "temp_dn");
        break;
    case UI_LEGACY_HEATER_CARD_1_EVENT_SETPOINT_INC:
        hvac_legacy_publish_action(def, "temp_up");
        break;
    case UI_LEGACY_HEATER_CARD_1_EVENT_MODE_OFF:
        hvac_legacy_publish_action(def, "mode_off");
        break;
    case UI_LEGACY_HEATER_CARD_1_EVENT_MODE_HEATING:
        hvac_legacy_publish_action(def, "mode_cc");
        break;
    case UI_LEGACY_HEATER_CARD_1_EVENT_MODE_FAN:
        hvac_legacy_publish_action(def, "mode_fan");
        break;
    case UI_LEGACY_HEATER_CARD_1_EVENT_MODE_COOLING:
        hvac_legacy_publish_action(def, "mode_cool");
        break;
    default:
        break;
    }
}

lv_obj_t *screen_hvac_legacy_create(lv_display_t *disp)
{
    (void)disp;
    for (unsigned i = 0; i < HVAC_LEGACY_HEATER_CARD_COUNT; i++) {
        s_heater_cards[i] = NULL;
    }

    ha_mqtt_configure_hvac_zone(HA_MQTT_HVAC_ZONE_BEDROOM_1, "esp_hmi/data/bedroom1/climate/setpoint",
                                "esp_hmi/data/bedroom1/climate/current", "esp_hmi/data/bedroom1/climate/heater_on",
                                "esp_hmi/data/bedroom1/climate/control");
    ha_mqtt_configure_hvac_zone(HA_MQTT_HVAC_ZONE_UPSTAIRS_BEDROOM, "esp_hmi/data/upstairs_bedroom/climate/setpoint",
                                "esp_hmi/data/upstairs_bedroom/climate/current",
                                "esp_hmi/data/upstairs_bedroom/climate/heater_on",
                                "esp_hmi/data/upstairs_bedroom/climate/control");
    ha_mqtt_configure_hvac_zone(HA_MQTT_HVAC_ZONE_STUDIO, "esp_hmi/data/studio/climate/setpoint",
                                "esp_hmi/data/studio/climate/current", "esp_hmi/data/studio/climate/heater_on",
                                "esp_hmi/data/studio/climate/control");
    ha_mqtt_configure_hvac_zone(HA_MQTT_HVAC_ZONE_SERVER_RACK, "esp_hmi/data/server_rack/climate/setpoint",
                                "esp_hmi/data/server_rack/climate/current", "esp_hmi/data/server_rack/climate/heater_on",
                                "esp_hmi/data/server_rack/climate/control");

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(scr, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(scr, HVAC_LEGACY_CARD_GAP_PX, LV_PART_MAIN);

    const lv_coord_t card_h = (lv_coord_t)((BSP_LCD_V_RES - ((HVAC_LEGACY_HEATER_CARD_COUNT - 1) * HVAC_LEGACY_CARD_GAP_PX)) /
                                             HVAC_LEGACY_HEATER_CARD_COUNT);

    for (unsigned i = 0; i < HVAC_LEGACY_HEATER_CARD_COUNT; i++) {
        const hvac_legacy_card_def_t *def = &s_card_defs[i];
        s_heater_cards[i] = ui_legacy_heater_card_1_create(
            scr, BSP_LCD_H_RES, def->room_name, def->profile, 21.0f, 22.0f, false, false,
            def->profile == UI_LEGACY_HEATER_CARD_PROFILE_BLANK ? NULL : hvac_legacy_heater_on_ui_event, (void *)(uintptr_t)i,
            card_h, HVAC_LEGACY_CIRCLE_MARGIN_PX, HVAC_LEGACY_BUTTON_GAP_PX);
        if (s_heater_cards[i] != NULL) {
            lv_obj_set_width(s_heater_cards[i], BSP_LCD_H_RES);
            lv_obj_set_height(s_heater_cards[i], card_h);
            lv_obj_set_flex_grow(s_heater_cards[i], 1);
        }
    }

    ha_mqtt_add_ollie_climate_state_callback(hvac_legacy_climate_apply_card0, NULL);
    ha_mqtt_add_hvac_zone_climate_callback(HA_MQTT_HVAC_ZONE_BEDROOM_1, hvac_legacy_climate_apply_zone,
                                           (void *)(uintptr_t)1);
    ha_mqtt_add_hvac_zone_climate_callback(HA_MQTT_HVAC_ZONE_UPSTAIRS_BEDROOM, hvac_legacy_climate_apply_zone,
                                          (void *)(uintptr_t)2);
    ha_mqtt_add_hvac_zone_climate_callback(HA_MQTT_HVAC_ZONE_STUDIO, hvac_legacy_climate_apply_zone,
                                           (void *)(uintptr_t)3);
    ha_mqtt_add_hvac_zone_climate_callback(HA_MQTT_HVAC_ZONE_SERVER_RACK, hvac_legacy_climate_apply_zone,
                                           (void *)(uintptr_t)4);

    return scr;
}
