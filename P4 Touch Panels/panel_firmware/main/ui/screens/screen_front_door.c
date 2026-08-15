#include "ui/screens/screen_front_door.h"

#include <stdint.h>

#include "ha_mqtt.h"
#include "ui/components/ui_light_card_1.h"
#include "ui/components/ui_taskbar.h"
#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/nav.h"
#include "ui/ui_screen_template.h"

typedef struct {
    const char *name;
    /** Slug in the MQTT command payload and in the retained `esp_hmi/data/<slug>/light/…` topics. */
    const char *slug;
    const char *topic_state;
    const char *topic_brightness;
    uint8_t mqtt_light_id;
    lv_obj_t *card;
} front_door_light_t;

static front_door_light_t s_lights[] = {
    { "Outside", "outside", "esp_hmi/data/outside/light/state", "esp_hmi/data/outside/light/brightness",
      HA_MQTT_LIGHT_OUTSIDE, NULL },
    { "Lounge", "lounge", "esp_hmi/data/lounge/light/state", "esp_hmi/data/lounge/light/brightness",
      HA_MQTT_LIGHT_LOUNGE, NULL },
    { "Hallway", "hallway", "esp_hmi/data/hallway/light/state", "esp_hmi/data/hallway/light/brightness",
      HA_MQTT_LIGHT_HALLWAY, NULL },
};

#define FRONT_DOOR_LIGHT_COUNT (sizeof(s_lights) / sizeof(s_lights[0]))

static void front_door_light_power(bool on, void *user_data)
{
    const front_door_light_t *light = (const front_door_light_t *)user_data;
    (void)ha_mqtt_publish_light_power(light->slug, on);
}

static void front_door_light_brightness(uint8_t brightness_pct, void *user_data)
{
    const front_door_light_t *light = (const front_door_light_t *)user_data;
    (void)ha_mqtt_publish_light_brightness(light->slug, brightness_pct);
}

static void front_door_light_state(bool on, uint8_t brightness_pct, void *user_data)
{
    const front_door_light_t *light = (const front_door_light_t *)user_data;
    ui_light_card_1_set_state(light->card, on, brightness_pct);
}

static void front_door_taskbar_nav(void *user_data)
{
    nav_go_to((app_id_t)(uintptr_t)user_data);
}

lv_obj_t *screen_front_door_create(lv_display_t *disp)
{
    ui_screen_template_params_t params;
    ui_screen_template_params_init_defaults(&params);
    params.status_bar = false;
    params.grid_cols = 1;
    params.grid_rows = (uint8_t)FRONT_DOOR_LIGHT_COUNT;
    params.pad_top = 24;
    params.pad_right = 24;
    /* Keep grid content clear of the dock. */
    params.pad_bottom = 24 + UI_TASKBAR_HEIGHT;
    params.pad_left = 24;
    params.row_gap = 16;
    params.col_gap = 16;
    params.bg_color = lv_color_black();
    params.bg_opa = LV_OPA_COVER;

    ui_screen_template_result_t layout;
    if (!ui_screen_template_create(disp, &params, &layout)) {
        return NULL;
    }

    for (unsigned i = 0; i < FRONT_DOOR_LIGHT_COUNT; i++) {
        front_door_light_t *light = &s_lights[i];
        light->card = ui_light_card_1_create(layout.grid, (uint8_t)i, 0, 1, 1, light->name, front_door_light_power,
                                             front_door_light_brightness, light);
        ha_mqtt_configure_light(light->mqtt_light_id, light->topic_state, light->topic_brightness);
        ha_mqtt_set_light_state_callback(light->mqtt_light_id, front_door_light_state, light);
    }

    const ui_taskbar_item_t taskbar_items[] = {
        { LV_SYMBOL_HOME, &lv_font_montserrat_48, NULL, front_door_taskbar_nav, (void *)(uintptr_t)APP_HOME },
        { UI_HA_ICON_TEDDY_BEAR, NULL, NULL, front_door_taskbar_nav, (void *)(uintptr_t)APP_PENNY_ROOM },
        { UI_HA_ICON_GATE, NULL, NULL, front_door_taskbar_nav, (void *)(uintptr_t)APP_FRONT_GATE },
        { UI_HA_ICON_THERMOMETER, NULL, NULL, front_door_taskbar_nav, (void *)(uintptr_t)APP_HVAC },
        { UI_HA_ICON_GAUGE, NULL, NULL, front_door_taskbar_nav, (void *)(uintptr_t)APP_HOUSE_BATTERY },
        { LV_SYMBOL_SETTINGS, &lv_font_montserrat_48, NULL, front_door_taskbar_nav, (void *)(uintptr_t)APP_SETTINGS },
    };
    (void)ui_taskbar_create(layout.screen, taskbar_items,
                            (uint8_t)(sizeof(taskbar_items) / sizeof(taskbar_items[0])));

    return layout.screen;
}
