#include "ui/components/ui_hvac_climate_modal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ha_mqtt.h"
#include "ui/components/ui_box_1.h"
#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/ui_visual_tokens.h"

#define MODAL_BACKDROP_OPA ((lv_opa_t)(255 * 82 / 100))
#define MODAL_PANEL_OPA ((lv_opa_t)(255 * 93 / 100))
#define MODAL_ROW_BTN_OPA ((lv_opa_t)(255 * 88 / 100))
#define MODAL_MODE_BTN_MIN_H 72
#define MODAL_STEP_BTN_SIZE 72
#define MODAL_TEMP_STEP 0.5f
#define MODAL_TEMP_MIN 5.0f
#define MODAL_TEMP_MAX 30.0f

#define MODAL_MODE_HEAT lv_color_hex(0xF97316)
#define MODAL_MODE_COOL lv_color_hex(0x3B82F6)
#define MODAL_MODE_FAN lv_color_hex(0x22C55E)
#define MODAL_MODE_OFF_MUTED lv_color_hex(0x8E8E93)

/** Lives on `lv_layer_top()`, which is shared by all screens, so one instance is enough. */
static lv_obj_t *s_modal;
static lv_obj_t *s_lbl_setpoint;
static lv_obj_t *s_setpoint_row;
static lv_obj_t *s_mode_btns[4];
static const char *s_btn_prefix;
static unsigned s_zone_token;
static float s_setpoint_c;
static int8_t s_hvac_mode;
static bool s_have_setpoint;
static bool s_open;

static void publish_suffix(const char *suffix)
{
    if (suffix == NULL) {
        return;
    }
    if (s_btn_prefix == NULL) {
        char legacy[48];
        const int n = snprintf(legacy, sizeof(legacy), "climate_%s", suffix);
        if (n > 0 && (size_t)n < sizeof(legacy)) {
            (void)ha_mqtt_publish_ollie_button(legacy);
        }
        return;
    }
    char payload[64];
    const int n = snprintf(payload, sizeof(payload), "%s_%s", s_btn_prefix, suffix);
    if (n > 0 && (size_t)n < sizeof(payload)) {
        (void)ha_mqtt_publish_ollie_button(payload);
    }
}

static bool mode_option_visible(ui_hvac_climate_profile_t profile, int8_t mode)
{
    if (mode == HA_MQTT_CLIMATE_HVAC_OFF) {
        return true;
    }
    if (profile == UI_HVAC_CLIMATE_PROFILE_HEAT_COOL_FAN) {
        return mode == HA_MQTT_CLIMATE_HVAC_HEAT || mode == HA_MQTT_CLIMATE_HVAC_COOL ||
               mode == HA_MQTT_CLIMATE_HVAC_FAN;
    }
    if (profile == UI_HVAC_CLIMATE_PROFILE_HEAT_COOL) {
        return mode == HA_MQTT_CLIMATE_HVAC_HEAT || mode == HA_MQTT_CLIMATE_HVAC_COOL;
    }
    return mode == HA_MQTT_CLIMATE_HVAC_HEAT;
}

static bool setpoint_controls_visible(int8_t mode)
{
    return mode == HA_MQTT_CLIMATE_HVAC_HEAT || mode == HA_MQTT_CLIMATE_HVAC_COOL;
}

static lv_color_t accent_for_mode(int8_t mode)
{
    switch (mode) {
    case HA_MQTT_CLIMATE_HVAC_HEAT:
        return MODAL_MODE_HEAT;
    case HA_MQTT_CLIMATE_HVAC_COOL:
        return MODAL_MODE_COOL;
    case HA_MQTT_CLIMATE_HVAC_FAN:
        return MODAL_MODE_FAN;
    default:
        return MODAL_MODE_OFF_MUTED;
    }
}

static void format_setpoint(char *buf, size_t len, float c, bool have)
{
    if (!have) {
        snprintf(buf, len, "Set -");
        return;
    }
    const int tenths = (int)lroundf(c * 10.0f);
    snprintf(buf, len, "Set %.1f°", (double)(tenths / 10.0f));
}

static void refresh_setpoint_label(void)
{
    if (s_lbl_setpoint == NULL) {
        return;
    }
    char b[24];
    format_setpoint(b, sizeof(b), s_setpoint_c, s_have_setpoint);
    lv_label_set_text(s_lbl_setpoint, b);
}

static void refresh_setpoint_row_visibility(void)
{
    if (s_setpoint_row == NULL) {
        return;
    }
    if (setpoint_controls_visible(s_hvac_mode)) {
        lv_obj_clear_flag(s_setpoint_row, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_setpoint_row, LV_OBJ_FLAG_HIDDEN);
    }
}

static void refresh_mode_highlights(void)
{
    static const int8_t k_modes[4] = {
        HA_MQTT_CLIMATE_HVAC_OFF,
        HA_MQTT_CLIMATE_HVAC_HEAT,
        HA_MQTT_CLIMATE_HVAC_COOL,
        HA_MQTT_CLIMATE_HVAC_FAN,
    };
    for (unsigned i = 0; i < 4; i++) {
        lv_obj_t *btn = s_mode_btns[i];
        if (btn == NULL) {
            continue;
        }
        const int8_t mode = k_modes[i];
        const bool selected = (s_hvac_mode == mode) ||
                              (mode == HA_MQTT_CLIMATE_HVAC_OFF &&
                               (s_hvac_mode == HA_MQTT_CLIMATE_HVAC_UNKNOWN || s_hvac_mode == HA_MQTT_CLIMATE_HVAC_OFF));
        const lv_color_t accent = accent_for_mode(mode);
        if (selected) {
            lv_obj_set_style_bg_color(btn, accent, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), lv_color_white(), LV_PART_MAIN);
            if (lv_obj_get_child_cnt(btn) > 1) {
                lv_obj_set_style_text_color(lv_obj_get_child(btn, 1), lv_color_white(), LV_PART_MAIN);
            }
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(btn, MODAL_ROW_BTN_OPA, LV_PART_MAIN);
            lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), accent, LV_PART_MAIN);
            if (lv_obj_get_child_cnt(btn) > 1) {
                lv_obj_set_style_text_color(lv_obj_get_child(btn, 1), UI_BOX_1_LABEL_COLOR, LV_PART_MAIN);
            }
        }
    }
}

static void apply_state(float setpoint_c, int8_t hvac_mode, bool have_setpoint)
{
    s_setpoint_c = setpoint_c;
    s_hvac_mode = hvac_mode;
    s_have_setpoint = have_setpoint;
    refresh_setpoint_label();
    refresh_setpoint_row_visibility();
    refresh_mode_highlights();
}

void ui_hvac_climate_modal_close(void)
{
    if (s_modal != NULL) {
        lv_obj_del(s_modal);
        s_modal = NULL;
    }
    s_lbl_setpoint = NULL;
    s_setpoint_row = NULL;
    memset(s_mode_btns, 0, sizeof(s_mode_btns));
    s_btn_prefix = NULL;
    s_open = false;
}

static void modal_on_deleted(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }
    s_modal = NULL;
    s_lbl_setpoint = NULL;
    s_setpoint_row = NULL;
    memset(s_mode_btns, 0, sizeof(s_mode_btns));
    s_btn_prefix = NULL;
    s_open = false;
}

static void modal_bg_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (lv_event_get_target(e) != s_modal) {
        return;
    }
    ui_hvac_climate_modal_close();
}

static void mode_btn_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    const int8_t mode = (int8_t)(intptr_t)lv_event_get_user_data(e);
    switch (mode) {
    case HA_MQTT_CLIMATE_HVAC_OFF:
        publish_suffix("mode_off");
        break;
    case HA_MQTT_CLIMATE_HVAC_HEAT:
        publish_suffix("mode_cc");
        break;
    case HA_MQTT_CLIMATE_HVAC_COOL:
        publish_suffix("mode_cool");
        break;
    case HA_MQTT_CLIMATE_HVAC_FAN:
        publish_suffix("mode_fan");
        break;
    default:
        return;
    }
    apply_state(s_setpoint_c, mode, s_have_setpoint);
}

static void temp_step_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!setpoint_controls_visible(s_hvac_mode)) {
        return;
    }
    const int dir = (int)(intptr_t)lv_event_get_user_data(e);
    if (dir < 0) {
        publish_suffix("temp_dn");
        if (s_have_setpoint) {
            float next = s_setpoint_c - MODAL_TEMP_STEP;
            if (next < MODAL_TEMP_MIN) {
                next = MODAL_TEMP_MIN;
            }
            apply_state(next, s_hvac_mode, true);
        }
    } else {
        publish_suffix("temp_up");
        if (s_have_setpoint) {
            float next = s_setpoint_c + MODAL_TEMP_STEP;
            if (next > MODAL_TEMP_MAX) {
                next = MODAL_TEMP_MAX;
            }
            apply_state(next, s_hvac_mode, true);
        }
    }
}

static lv_obj_t *add_mode_btn(lv_obj_t *row, const char *icon_utf8, const lv_font_t *icon_font, const char *title,
                              int8_t mode, unsigned slot)
{
    lv_obj_t *btn = lv_button_create(row);
    lv_obj_remove_style_all(btn);
    lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, MODAL_ROW_BTN_OPA, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, UI_SINGLE_SELECTOR_SEGMENT_RADIUS, LV_PART_MAIN);
    lv_obj_set_height(btn, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(btn, MODAL_MODE_BTN_MIN_H, LV_PART_MAIN);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_style_pad_ver(btn, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(btn, 8, LV_PART_MAIN);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(btn, 4, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, mode_btn_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)mode);

    lv_obj_t *ic = lv_label_create(btn);
    lv_label_set_text(ic, icon_utf8);
    lv_obj_set_style_text_font(ic, icon_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(ic, accent_for_mode(mode), LV_PART_MAIN);

    lv_obj_t *lab = lv_label_create(btn);
    lv_label_set_text(lab, title);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lab, UI_BOX_1_LABEL_COLOR, LV_PART_MAIN);

    if (slot < 4) {
        s_mode_btns[slot] = btn;
    }
    return btn;
}

static lv_obj_t *add_step_btn(lv_obj_t *row, const char *symbol, int dir)
{
    lv_obj_t *btn = lv_button_create(row);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, MODAL_STEP_BTN_SIZE, MODAL_STEP_BTN_SIZE);
    lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, MODAL_ROW_BTN_OPA, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, UI_SINGLE_SELECTOR_SEGMENT_RADIUS, LV_PART_MAIN);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(btn, temp_step_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)dir);

    lv_obj_t *lab = lv_label_create(btn);
    lv_label_set_text(lab, symbol);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(lab, UI_BOX_1_LABEL_COLOR, LV_PART_MAIN);
    return btn;
}

void ui_hvac_climate_modal_open(const char *room_name, ui_hvac_climate_profile_t profile, const char *btn_prefix,
                                unsigned zone_token, float setpoint_c, int8_t hvac_mode, bool have_setpoint)
{
    ui_hvac_climate_modal_close();

    lv_display_t *disp = lv_display_get_default();
    if (disp == NULL) {
        return;
    }
    const int32_t dw = lv_display_get_horizontal_resolution(disp);
    const int32_t dh = lv_display_get_vertical_resolution(disp);

    s_btn_prefix = btn_prefix;
    s_zone_token = zone_token;
    s_open = true;

    lv_obj_t *ov = lv_obj_create(lv_layer_top());
    s_modal = ov;
    lv_obj_remove_style_all(ov);
    lv_obj_set_size(ov, (lv_coord_t)dw, (lv_coord_t)dh);
    lv_obj_set_style_bg_color(ov, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ov, MODAL_BACKDROP_OPA, LV_PART_MAIN);
    lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ov, modal_bg_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ov, modal_on_deleted, LV_EVENT_DELETE, NULL);

    lv_obj_t *panel = lv_obj_create(ov);
    lv_obj_remove_style_all(panel);
    ui_box_1_style_apply(panel);
    lv_obj_set_style_bg_opa(panel, MODAL_PANEL_OPA, LV_PART_MAIN);
    lv_obj_set_width(panel, (lv_coord_t)LV_MIN(dw - 40, 520));
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(panel, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_row(panel, 16, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, room_name != NULL ? room_name : "Climate");
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, UI_BOX_1_LABEL_COLOR, LV_PART_MAIN);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    lv_obj_t *mode_row = lv_obj_create(panel);
    lv_obj_remove_style_all(mode_row);
    lv_obj_set_width(mode_row, lv_pct(100));
    lv_obj_set_height(mode_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(mode_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(mode_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(mode_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mode_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(mode_row, 10, LV_PART_MAIN);

    (void)add_mode_btn(mode_row, LV_SYMBOL_POWER, &lv_font_montserrat_32, "Off", HA_MQTT_CLIMATE_HVAC_OFF, 0);
    if (mode_option_visible(profile, HA_MQTT_CLIMATE_HVAC_HEAT)) {
        (void)add_mode_btn(mode_row, UI_HA_ICON_FIRE, &ui_font_home_assistant_icons_56, "Heat",
                           HA_MQTT_CLIMATE_HVAC_HEAT, 1);
    }
    if (mode_option_visible(profile, HA_MQTT_CLIMATE_HVAC_COOL)) {
        (void)add_mode_btn(mode_row, UI_HA_ICON_THERMOMETER_LOW, &ui_font_home_assistant_icons_56, "Cool",
                           HA_MQTT_CLIMATE_HVAC_COOL, 2);
    }
    if (mode_option_visible(profile, HA_MQTT_CLIMATE_HVAC_FAN)) {
        (void)add_mode_btn(mode_row, UI_HA_ICON_FAN, &ui_font_home_assistant_icons_56, "Fan", HA_MQTT_CLIMATE_HVAC_FAN,
                           3);
    }

    s_setpoint_row = lv_obj_create(panel);
    lv_obj_remove_style_all(s_setpoint_row);
    lv_obj_set_width(s_setpoint_row, lv_pct(100));
    lv_obj_set_height(s_setpoint_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(s_setpoint_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(s_setpoint_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(s_setpoint_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_setpoint_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_setpoint_row, 16, LV_PART_MAIN);

    (void)add_step_btn(s_setpoint_row, LV_SYMBOL_MINUS, -1);

    s_lbl_setpoint = lv_label_create(s_setpoint_row);
    lv_obj_set_style_text_font(s_lbl_setpoint, &lv_font_montserrat_26, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_setpoint, UI_BOX_1_LABEL_COLOR, LV_PART_MAIN);
    lv_obj_set_flex_grow(s_lbl_setpoint, 1);
    lv_obj_set_style_text_align(s_lbl_setpoint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    (void)add_step_btn(s_setpoint_row, LV_SYMBOL_PLUS, 1);

    apply_state(setpoint_c, hvac_mode, have_setpoint);
}

void ui_hvac_climate_modal_update_if_open(unsigned zone_token, float setpoint_c, int8_t hvac_mode, bool have_setpoint)
{
    if (!s_open || s_modal == NULL || zone_token != s_zone_token) {
        return;
    }
    apply_state(setpoint_c, hvac_mode, have_setpoint);
}
