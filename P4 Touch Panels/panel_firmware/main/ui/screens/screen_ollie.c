#include "ui/screens/screen_ollie.h"

#include <string.h>

#include "esp_log.h"
#include "ha_mqtt.h"
#include <stdint.h>
#include "ui/components/ui_box_1.h"
#include "ui/components/ui_button_1.h"
#include "ui/components/ui_climate_control_1.h"
#include "ui/components/ui_ollie_fan_modal.h"
#include "ui/components/ui_single_selector_tab_1.h"
#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/ui_brand_gradient.h"
#include "ui/ui_layout.h"
#include "ui/ui_visual_tokens.h"

static const char *TAG = "screen_ollie";

/** Must match `input_select.ollie_room_desired_state` options (order = selector index 0..2). */
static const char *const s_ha_room_options[] = { "Normal", "Rest Time", "Sleep Time" };

static lv_obj_t *s_room_selector;
static lv_obj_t *s_climate_widget;

static void ollie_climate_apply_from_mqtt(float setpoint_c, float current_c, bool heater_on, bool climate_control_on,
                                          int8_t hvac_mode, void *user_data)
{
    (void)hvac_mode;
    (void)user_data;
    if (s_climate_widget == NULL) {
        return;
    }
    ui_climate_control_1_set_switch_state(s_climate_widget, heater_on, climate_control_on);
    ui_climate_control_1_set_setpoint(s_climate_widget, setpoint_c);
    ui_climate_control_1_set_current_temp(s_climate_widget, current_c);
    uint8_t mode = climate_control_on ? UI_CLIMATE_CONTROL_1_MODE_CLIMATE_CONTROL
                                       : (heater_on ? UI_CLIMATE_CONTROL_1_MODE_ON : UI_CLIMATE_CONTROL_1_MODE_OFF);
    ui_climate_control_1_set_mode(s_climate_widget, mode);
}

static void ollie_climate_on_ui_event(ui_climate_control_1_event_t event, float setpoint_c, bool climate_enabled,
                                      uint8_t climate_mode, void *user_data)
{
    (void)setpoint_c;
    (void)climate_enabled;
    (void)user_data;
    switch (event) {
    case UI_CLIMATE_CONTROL_1_EVENT_SETPOINT_DEC:
        (void)ha_mqtt_publish_ollie_button("climate_temp_dn");
        break;
    case UI_CLIMATE_CONTROL_1_EVENT_SETPOINT_INC:
        (void)ha_mqtt_publish_ollie_button("climate_temp_up");
        break;
    case UI_CLIMATE_CONTROL_1_EVENT_POWER:
        if (climate_mode == UI_CLIMATE_CONTROL_1_MODE_OFF) {
            (void)ha_mqtt_publish_ollie_button("climate_mode_off");
        } else if (climate_mode == UI_CLIMATE_CONTROL_1_MODE_ON) {
            (void)ha_mqtt_publish_ollie_button("climate_mode_on");
        } else {
            (void)ha_mqtt_publish_ollie_button("climate_mode_cc");
        }
        break;
    default:
        break;
    }
}

static void ollie_screen_unloaded_close_fan_modal(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_UNLOADED) {
        return;
    }
    ui_ollie_fan_modal_close();
}

static void open_fan_modal(void *user_data)
{
    (void)user_data;
    ui_ollie_fan_modal_open();
}

static bool ollie_room_on_mqtt_state(const char *option, void *user_data)
{
    (void)user_data;
    if (s_room_selector == NULL || option == NULL) {
        return false;
    }
    for (size_t i = 0; i < sizeof(s_ha_room_options) / sizeof(s_ha_room_options[0]); i++) {
        if (strcmp(option, s_ha_room_options[i]) == 0) {
            ui_single_selector_tab_1_set_selected(s_room_selector, (uint32_t)i, false);
            return true;
        }
    }
    ESP_LOGW(TAG, "unknown room option from MQTT: %s", option);
    return false;
}

static void ollie_room_on_selector_change(uint32_t selected_index, void *user_data)
{
    (void)user_data;
    if (selected_index >= sizeof(s_ha_room_options) / sizeof(s_ha_room_options[0])) {
        return;
    }
    (void)ha_mqtt_publish_ollie_room_option(s_ha_room_options[selected_index]);
}

static void ollie_mqtt_publish_payload(void *user_data)
{
    const char *payload = (const char *)user_data;
    if (!ha_mqtt_publish_ollie_button(payload)) {
        /* MQTT not up yet or publish failed — ignore for UI responsiveness */
    }
}

lv_obj_t *screen_ollie_create(lv_display_t *disp)
{
    ui_screen_template_params_t params;
    ui_screen_template_params_init_defaults(&params);
    params.status_bar = false;
    params.grid_cols = 4;
    params.grid_rows = 4;

    ui_screen_template_result_t layout;
    if (!ui_screen_template_create(disp, &params, &layout)) {
        return NULL;
    }

    ui_brand_gradient_apply(layout.screen);

    static const ui_single_selector_item_t room_items[] = {
        { UI_HA_ICON_WEATHER_SUNNY, "Normal" },
        { UI_HA_ICON_MUSIC_REST_QUARTER, "Rest Time" },
        { UI_HA_ICON_BED, "Sleep Time" },
    };

    ha_mqtt_set_ollie_room_state_callback(ollie_room_on_mqtt_state, NULL);
    ha_mqtt_set_ollie_climate_state_callback(ollie_climate_apply_from_mqtt, NULL);
    s_room_selector = ui_single_selector_tab_1_create(layout.grid, 0, 0, 1, 4, true, room_items,
                                                      sizeof(room_items) / sizeof(room_items[0]), 0,
                                                      ollie_room_on_selector_change, NULL);

    s_climate_widget = ui_climate_control_1_create(layout.grid, 1, 0, 2, 4, 21.0f, 22.0f, false, false,
                                                   ollie_climate_on_ui_event, NULL);

    lv_color_t ollie_btn_fg = lv_color_white();
    const lv_color_t *ollie_btn_fg_p = &ollie_btn_fg;

    (void)ui_button_1_create(layout.grid, 3, 0, 1, 2, UI_HA_ICON_LIGHTBULB, NULL, ollie_btn_fg_p, ollie_btn_fg_p,
                             ollie_mqtt_publish_payload, "light");
    (void)ui_button_1_create(layout.grid, 3, 2, 1, 2, UI_HA_ICON_FAN, NULL, ollie_btn_fg_p, ollie_btn_fg_p,
                             open_fan_modal, NULL);

    lv_obj_add_event_cb(layout.screen, ollie_screen_unloaded_close_fan_modal, LV_EVENT_SCREEN_UNLOADED, NULL);

    return layout.screen;
}
