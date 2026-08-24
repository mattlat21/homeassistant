#include "ui/components/ui_hvac_card_1.h"

#include <math.h>
#include <stdio.h>

#include "ui/ui_visual_tokens.h"
#include "ui/components/ui_taskbar.h"

#define HVAC_CARD_RADIUS UI_TASKBAR_DOCK_RADIUS
#define HVAC_CARD_PAD 8
#define HVAC_CARD_BORDER_W 5
/** Current / set Y offsets from card vertical centre (negative = up). Fixed — not linked to each other at runtime. */
#define HVAC_CARD_CURRENT_Y_OFF 13
#define HVAC_CARD_SET_Y_OFF 58
#define HVAC_CARD_MUTED lv_color_hex(0x8E8E93)
#define HVAC_CARD_TEXT lv_color_white()
#define HVAC_CARD_MODE_HEAT lv_color_hex(0xF97316)
#define HVAC_CARD_MODE_COOL lv_color_hex(0x3B82F6)
#define HVAC_CARD_MODE_FAN lv_color_hex(0x22C55E)

typedef struct {
    lv_obj_t *card;
    lv_obj_t *lbl_room;
    lv_obj_t *lbl_current;
    lv_obj_t *lbl_setpoint;
    float current_c;
    float setpoint_c;
    int8_t hvac_mode;
    bool have_current;
    bool have_setpoint;
} hvac_card_meta_t;

static void hvac_card_meta_free(lv_event_t *e)
{
    hvac_card_meta_t *m = lv_event_get_user_data(e);
    if (m != NULL) {
        lv_free(m);
    }
}

static hvac_card_meta_t *hvac_card_get_meta(const lv_obj_t *card)
{
    if (card == NULL) {
        return NULL;
    }
    return (hvac_card_meta_t *)lv_obj_get_user_data((lv_obj_t *)card);
}

static void format_temp_degree(char *buf, size_t len, float c, bool one_decimal)
{
    const int tenths = (int)lroundf(c * 10.0f);
    if (!one_decimal && (tenths % 10) == 0) {
        snprintf(buf, len, "%d°", tenths / 10);
    } else {
        snprintf(buf, len, "%.1f°", (double)(tenths / 10.0f));
    }
}

static void refresh_current(hvac_card_meta_t *m)
{
    if (m == NULL || m->lbl_current == NULL) {
        return;
    }
    if (!m->have_current) {
        lv_label_set_text(m->lbl_current, "-");
        return;
    }
    char b[16];
    format_temp_degree(b, sizeof(b), m->current_c, false);
    lv_label_set_text(m->lbl_current, b);
}

static void refresh_setpoint(hvac_card_meta_t *m)
{
    if (m == NULL || m->lbl_setpoint == NULL) {
        return;
    }
    if (!m->have_setpoint) {
        lv_label_set_text(m->lbl_setpoint, "Set -");
        return;
    }
    char b[24];
    char temp[16];
    format_temp_degree(temp, sizeof(temp), m->setpoint_c, true);
    snprintf(b, sizeof(b), "Set %s", temp);
    lv_label_set_text(m->lbl_setpoint, b);
}

static lv_color_t mode_color(int8_t mode)
{
    switch (mode) {
    case HA_MQTT_CLIMATE_HVAC_HEAT:
        return HVAC_CARD_MODE_HEAT;
    case HA_MQTT_CLIMATE_HVAC_COOL:
        return HVAC_CARD_MODE_COOL;
    case HA_MQTT_CLIMATE_HVAC_FAN:
        return HVAC_CARD_MODE_FAN;
    default:
        return HVAC_CARD_MUTED;
    }
}

static void refresh_mode(hvac_card_meta_t *m)
{
    if (m == NULL || m->card == NULL) {
        return;
    }
    const lv_color_t col = mode_color(m->hvac_mode);
    const bool inactive = (m->hvac_mode == HA_MQTT_CLIMATE_HVAC_OFF ||
                           m->hvac_mode == HA_MQTT_CLIMATE_HVAC_UNKNOWN);
    lv_obj_set_style_border_color(m->card, col, LV_PART_MAIN);
    lv_obj_set_style_border_opa(m->card, inactive ? LV_OPA_40 : LV_OPA_COVER, LV_PART_MAIN);
}

static void apply_card_style(lv_obj_t *card)
{
    lv_obj_remove_style_all(card);
    lv_obj_set_style_bg_color(card, UI_BOX_1_BG_COLOR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, UI_BOX_1_BG_OPA, LV_PART_MAIN);
    lv_obj_set_style_radius(card, HVAC_CARD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, HVAC_CARD_BORDER_W, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, HVAC_CARD_MUTED, LV_PART_MAIN);
    lv_obj_set_style_border_opa(card, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, HVAC_CARD_PAD, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *ui_hvac_card_1_create(lv_obj_t *parent, uint8_t row, uint8_t col, const char *room_name)
{
    if (parent == NULL) {
        return NULL;
    }

    hvac_card_meta_t *m = lv_malloc(sizeof(hvac_card_meta_t));
    if (m == NULL) {
        return NULL;
    }
    *m = (hvac_card_meta_t){
        .current_c = 0.0f,
        .setpoint_c = 0.0f,
        .hvac_mode = HA_MQTT_CLIMATE_HVAC_UNKNOWN,
        .have_current = false,
        .have_setpoint = false,
    };

    lv_obj_t *card = lv_obj_create(parent);
    apply_card_style(card);
    lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_user_data(card, m);
    lv_obj_add_event_cb(card, hvac_card_meta_free, LV_EVENT_DELETE, m);
    m->card = card;

    m->lbl_room = lv_label_create(card);
    lv_label_set_text(m->lbl_room, room_name != NULL ? room_name : "");
    lv_obj_set_style_text_font(m->lbl_room, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(m->lbl_room, HVAC_CARD_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_align(m->lbl_room, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(m->lbl_room, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(m->lbl_room, lv_pct(100));
    lv_obj_set_style_max_height(m->lbl_room, 56, LV_PART_MAIN);
    lv_obj_align(m->lbl_room, LV_ALIGN_TOP_MID, 0, 0);

    m->lbl_current = lv_label_create(card);
    lv_label_set_text(m->lbl_current, "-");
    lv_obj_set_style_text_font(m->lbl_current, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(m->lbl_current, HVAC_CARD_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_align(m->lbl_current, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(m->lbl_current, lv_pct(100));
    /* Fixed to card centre so 1- vs 2-line room names do not shift it. */
    lv_obj_align(m->lbl_current, LV_ALIGN_CENTER, 0, HVAC_CARD_CURRENT_Y_OFF);

    m->lbl_setpoint = lv_label_create(card);
    lv_label_set_text(m->lbl_setpoint, "Set -");
    lv_obj_set_style_text_font(m->lbl_setpoint, &lv_font_montserrat_26, LV_PART_MAIN);
    lv_obj_set_style_text_color(m->lbl_setpoint, HVAC_CARD_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_align(m->lbl_setpoint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(m->lbl_setpoint, lv_pct(100));
    /* Fixed to card centre (just under the current-temp band), not dependent on lbl_current. */
    lv_obj_align(m->lbl_setpoint, LV_ALIGN_CENTER, 0, HVAC_CARD_SET_Y_OFF);

    refresh_mode(m);
    return card;
}

void ui_hvac_card_1_set_room_name(lv_obj_t *card, const char *room_name)
{
    hvac_card_meta_t *m = hvac_card_get_meta(card);
    if (m == NULL || m->lbl_room == NULL) {
        return;
    }
    lv_label_set_text(m->lbl_room, room_name != NULL ? room_name : "");
}

void ui_hvac_card_1_set_current_temp(lv_obj_t *card, float temp_c)
{
    hvac_card_meta_t *m = hvac_card_get_meta(card);
    if (m == NULL) {
        return;
    }
    m->current_c = temp_c;
    m->have_current = true;
    refresh_current(m);
}

void ui_hvac_card_1_set_setpoint(lv_obj_t *card, float setpoint_c)
{
    hvac_card_meta_t *m = hvac_card_get_meta(card);
    if (m == NULL) {
        return;
    }
    m->setpoint_c = setpoint_c;
    m->have_setpoint = true;
    refresh_setpoint(m);
}

void ui_hvac_card_1_set_setpoint_visible(lv_obj_t *card, bool visible)
{
    hvac_card_meta_t *m = hvac_card_get_meta(card);
    if (m == NULL || m->lbl_setpoint == NULL) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(m->lbl_setpoint, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(m->lbl_setpoint, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_hvac_card_1_set_mode(lv_obj_t *card, int8_t hvac_mode)
{
    hvac_card_meta_t *m = hvac_card_get_meta(card);
    if (m == NULL) {
        return;
    }
    m->hvac_mode = hvac_mode;
    refresh_mode(m);
}
