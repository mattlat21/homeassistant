#include "ui/components/ui_ollie_fan_modal.h"

#include "ha_mqtt.h"
#include "ui/components/ui_box_1.h"
#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/ui_visual_tokens.h"

#define FAN_MODAL_BACKDROP_OPA ((lv_opa_t)(255 * 82 / 100))
#define FAN_MODAL_PANEL_OPA ((lv_opa_t)(255 * 93 / 100))
#define FAN_MODAL_ROW_BTN_OPA ((lv_opa_t)(255 * 88 / 100))

/** Lives on `lv_layer_top()`, which is shared by all screens, so one instance is enough. */
static lv_obj_t *s_modal;

void ui_ollie_fan_modal_close(void)
{
    if (s_modal != NULL) {
        lv_obj_del(s_modal);
        s_modal = NULL;
    }
}

static void fan_modal_on_deleted(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }
    s_modal = NULL;
}

static void fan_modal_bg_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (lv_event_get_target(e) != s_modal) {
        return;
    }
    ui_ollie_fan_modal_close();
}

static void fan_mode_row_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    const char *payload = (const char *)lv_event_get_user_data(e);
    (void)ha_mqtt_publish_ollie_button(payload);
    ui_ollie_fan_modal_close();
}

static void add_fan_mode_row(lv_obj_t *panel, const char *icon_utf8, const char *title, const char *mqtt_payload)
{
    lv_obj_t *btn = lv_button_create(panel);
    lv_obj_remove_style_all(btn);
    lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, FAN_MODAL_ROW_BTN_OPA, LV_PART_MAIN);
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

void ui_ollie_fan_modal_open(void)
{
    if (s_modal != NULL) {
        ui_ollie_fan_modal_close();
        return;
    }

    lv_display_t *disp = lv_display_get_default();
    if (disp == NULL) {
        return;
    }
    const int32_t dw = lv_display_get_horizontal_resolution(disp);
    const int32_t dh = lv_display_get_vertical_resolution(disp);

    lv_obj_t *ov = lv_obj_create(lv_layer_top());
    s_modal = ov;
    lv_obj_remove_style_all(ov);
    lv_obj_set_size(ov, (lv_coord_t)dw, (lv_coord_t)dh);
    lv_obj_set_style_bg_color(ov, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ov, FAN_MODAL_BACKDROP_OPA, LV_PART_MAIN);
    lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ov, fan_modal_bg_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ov, fan_modal_on_deleted, LV_EVENT_DELETE, NULL);

    lv_obj_t *panel = lv_obj_create(ov);
    lv_obj_remove_style_all(panel);
    ui_box_1_style_apply(panel);
    lv_obj_set_style_bg_opa(panel, FAN_MODAL_PANEL_OPA, LV_PART_MAIN);
    lv_obj_set_width(panel, (lv_coord_t)LV_MIN(dw - 48, 420));
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
