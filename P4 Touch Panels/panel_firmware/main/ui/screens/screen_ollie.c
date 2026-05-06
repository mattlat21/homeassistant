#include "ui/screens/screen_ollie.h"

#include <string.h>

#include "esp_log.h"
#include "ha_mqtt.h"
#include <stdint.h>
#include "ui/components/ui_box_1.h"
#include "ui/components/ui_button_1.h"
#include "ui/components/ui_climate_control_1.h"
#include "ui/components/ui_single_selector_tab_1.h"
#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/ui_brand_gradient.h"
#include "ui/ui_layout.h"
#include "ui/ui_visual_tokens.h"

/* Fan modal: backdrop + panel/button opacities (screen_ollie only). */
#define OLLIE_FAN_MODAL_BACKDROP_OPA ((lv_opa_t)(255 * 82 / 100))
#define OLLIE_FAN_MODAL_PANEL_OPA ((lv_opa_t)(255 * 93 / 100))
#define OLLIE_FAN_MODAL_ROW_BTN_OPA ((lv_opa_t)(255 * 88 / 100))

static const char *TAG = "screen_ollie";

/** Must match `input_select.ollie_room_desired_state` options (order = selector index 0..2). */
static const char *const s_ha_room_options[] = { "Normal", "Rest Time", "Sleep Time" };

static lv_obj_t *s_room_selector;
static lv_obj_t *s_climate_widget;
static lv_display_t *s_ollie_disp;
static lv_obj_t *s_fan_modal;

static void ollie_mqtt_publish_payload(void *user_data);

static void ollie_climate_apply_from_mqtt(float setpoint_c, float current_c, bool heater_on, bool climate_control_on,
                                          void *user_data)
{
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

static void fan_modal_close(void)
{
    if (s_fan_modal != NULL) {
        lv_obj_del(s_fan_modal);
        s_fan_modal = NULL;
    }
}

static void fan_modal_on_deleted(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }
    s_fan_modal = NULL;
}

static void fan_modal_bg_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (lv_event_get_target(e) != s_fan_modal) {
        return;
    }
    fan_modal_close();
}

static void fan_mode_row_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    const char *payload = (const char *)lv_event_get_user_data(e);
    ollie_mqtt_publish_payload((void *)payload);
    fan_modal_close();
}

static void add_fan_mode_row(lv_obj_t *panel, const char *icon_utf8, const char *title, const char *mqtt_payload)
{
    lv_obj_t *btn = lv_button_create(panel);
    lv_obj_remove_style_all(btn);
    lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, OLLIE_FAN_MODAL_ROW_BTN_OPA, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, UI_SINGLE_SELECTOR_SEGMENT_RADIUS, LV_PART_MAIN);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_set_height(btn, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(btn, 76, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(btn, 22, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(btn, 16, LV_PART_MAIN);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn, 14, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, fan_mode_row_clicked, LV_EVENT_CLICKED, (void *)mqtt_payload);

    lv_obj_t *ic = lv_label_create(btn);
    lv_label_set_text(ic, icon_utf8);
    lv_obj_set_style_text_font(ic, &ui_font_home_assistant_icons_56, LV_PART_MAIN);
    lv_obj_set_style_text_color(ic, UI_BOX_1_LABEL_COLOR, LV_PART_MAIN);

    lv_obj_t *lab = lv_label_create(btn);
    lv_label_set_text(lab, title);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(lab, UI_BOX_1_LABEL_COLOR, LV_PART_MAIN);
    lv_obj_set_flex_grow(lab, 1);
}

static void ollie_screen_unloaded_close_fan_modal(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_UNLOADED) {
        return;
    }
    fan_modal_close();
}

static void open_fan_modal(void *user_data)
{
    (void)user_data;
    if (s_ollie_disp == NULL) {
        return;
    }
    if (s_fan_modal != NULL) {
        fan_modal_close();
        return;
    }

    const int32_t dw = lv_display_get_horizontal_resolution(s_ollie_disp);
    const int32_t dh = lv_display_get_vertical_resolution(s_ollie_disp);

    lv_obj_t *ov = lv_obj_create(lv_layer_top());
    s_fan_modal = ov;
    lv_obj_remove_style_all(ov);
    lv_obj_set_size(ov, (lv_coord_t)dw, (lv_coord_t)dh);
    lv_obj_set_style_bg_color(ov, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ov, OLLIE_FAN_MODAL_BACKDROP_OPA, LV_PART_MAIN);
    lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ov, fan_modal_bg_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ov, fan_modal_on_deleted, LV_EVENT_DELETE, NULL);

    lv_obj_t *panel = lv_obj_create(ov);
    lv_obj_remove_style_all(panel);
    ui_box_1_style_apply(panel);
    lv_obj_set_style_bg_opa(panel, OLLIE_FAN_MODAL_PANEL_OPA, LV_PART_MAIN);
    {
        const lv_coord_t pw = (lv_coord_t)LV_MIN(dw - 48, 420);
        lv_obj_set_width(panel, pw);
    }
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    /* LVGL 9: no LV_FLEX_ALIGN_STRETCH; rows use lv_pct(100) width. */
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(panel, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_row(panel, 12, LV_PART_MAIN);

    add_fan_mode_row(panel, UI_HA_ICON_FAN_OFF, "Fan off", "fan_off");
    add_fan_mode_row(panel, UI_HA_ICON_FAN_SPEED_1, "Low", "fan_1");
    add_fan_mode_row(panel, UI_HA_ICON_FAN_SPEED_2, "Medium", "fan_2");
    add_fan_mode_row(panel, UI_HA_ICON_FAN_SPEED_3, "High", "fan_3");
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

    s_ollie_disp = disp;

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
