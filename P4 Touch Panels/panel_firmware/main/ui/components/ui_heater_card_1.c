#include "ui/components/ui_heater_card_1.h"

#include <stdio.h>

#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/ui_visual_tokens.h"

#define HEATER_CARD_ACCENT lv_color_hex(0xF97316)
#define HEATER_CARD_BG lv_color_hex(0xF2F2F7)
#define HEATER_CARD_TEXT lv_color_hex(0x1C1C1E)
#define HEATER_CARD_MUTED lv_color_hex(0x8E8E93)
#define HEATER_CARD_DIVIDER lv_color_hex(0xD1D1D6)
#define HEATER_CARD_STEP_BTN 56

typedef struct {
    lv_obj_t *lbl_status;
    lv_obj_t *dot_status;
    lv_obj_t *lbl_icon;
    lv_obj_t *lbl_current;
    lv_obj_t *lbl_setpoint;
    float current_c;
    float setpoint_c;
    bool heater_on;
    bool climate_control_on;
    ui_heater_card_1_cb_t cb;
    void *user_data;
} heater_card_meta_t;

static void format_temp_degree(char *buf, size_t len, float c)
{
    snprintf(buf, len, "%.1f°", (double)c);
}

static void heater_card_meta_free(lv_event_t *e)
{
    heater_card_meta_t *m = lv_event_get_user_data(e);
    if (m != NULL) {
        lv_free(m);
    }
}

static heater_card_meta_t *heater_card_get_meta(const lv_obj_t *card)
{
    if (card == NULL) {
        return NULL;
    }
    return (heater_card_meta_t *)lv_obj_get_user_data((lv_obj_t *)card);
}

static void refresh_current(heater_card_meta_t *m)
{
    if (m == NULL || m->lbl_current == NULL) {
        return;
    }
    char b[16];
    format_temp_degree(b, sizeof(b), m->current_c);
    lv_label_set_text(m->lbl_current, b);
}

static void refresh_setpoint(heater_card_meta_t *m)
{
    if (m == NULL || m->lbl_setpoint == NULL) {
        return;
    }
    char b[16];
    format_temp_degree(b, sizeof(b), m->setpoint_c);
    lv_label_set_text(m->lbl_setpoint, b);
}

static void refresh_status(heater_card_meta_t *m)
{
    if (m == NULL || m->lbl_status == NULL || m->dot_status == NULL || m->lbl_icon == NULL) {
        return;
    }
    const char *status_text;
    lv_opa_t dot_opa = LV_OPA_COVER;
    if (m->heater_on) {
        status_text = "HEATING";
        lv_label_set_text(m->lbl_icon, UI_HA_ICON_FIRE);
        lv_obj_set_style_text_color(m->lbl_icon, HEATER_CARD_ACCENT, LV_PART_MAIN);
        lv_obj_set_style_border_color(m->dot_status, HEATER_CARD_ACCENT, LV_PART_MAIN);
    } else if (m->climate_control_on) {
        status_text = "CLIMATE";
        lv_label_set_text(m->lbl_icon, UI_HA_ICON_THERMOMETER);
        lv_obj_set_style_text_color(m->lbl_icon, HEATER_CARD_ACCENT, LV_PART_MAIN);
        lv_obj_set_style_border_color(m->dot_status, HEATER_CARD_ACCENT, LV_PART_MAIN);
    } else {
        status_text = "OFF";
        dot_opa = LV_OPA_40;
        lv_label_set_text(m->lbl_icon, UI_HA_ICON_FIRE_OFF);
        lv_obj_set_style_text_color(m->lbl_icon, HEATER_CARD_MUTED, LV_PART_MAIN);
        lv_obj_set_style_border_color(m->dot_status, HEATER_CARD_MUTED, LV_PART_MAIN);
    }
    lv_label_set_text(m->lbl_status, status_text);
    lv_obj_set_style_text_opa(m->lbl_status, dot_opa, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m->dot_status, dot_opa, LV_PART_MAIN);
}

static void emit_cb(heater_card_meta_t *m, ui_heater_card_1_event_t ev)
{
    if (m != NULL && m->cb != NULL) {
        m->cb(ev, m->user_data);
    }
}

static void on_minus(lv_event_t *e)
{
    heater_card_meta_t *m = lv_event_get_user_data(e);
    emit_cb(m, UI_HEATER_CARD_1_EVENT_SETPOINT_DEC);
}

static void on_plus(lv_event_t *e)
{
    heater_card_meta_t *m = lv_event_get_user_data(e);
    emit_cb(m, UI_HEATER_CARD_1_EVENT_SETPOINT_INC);
}

static lv_obj_t *make_vdivider(lv_obj_t *parent, lv_coord_t height)
{
    lv_obj_t *d = lv_obj_create(parent);
    lv_obj_remove_style_all(d);
    lv_obj_set_size(d, 1, height);
    lv_obj_set_style_bg_color(d, HEATER_CARD_DIVIDER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
    return d;
}

static lv_obj_t *make_step_btn(lv_obj_t *parent, const char *symbol, bool accent_ring, heater_card_meta_t *m,
                               lv_event_cb_t cb)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(b, HEATER_CARD_STEP_BTN, HEATER_CARD_STEP_BTN);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(b, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_shadow_offset_y(b, 2, LV_PART_MAIN);
    if (accent_ring) {
        lv_obj_set_style_border_width(b, 3, LV_PART_MAIN);
        lv_obj_set_style_border_color(b, HEATER_CARD_ACCENT, LV_PART_MAIN);
    } else {
        lv_obj_set_style_border_width(b, 0, LV_PART_MAIN);
    }
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, symbol);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, accent_ring ? HEATER_CARD_ACCENT : HEATER_CARD_TEXT, LV_PART_MAIN);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, m);
    return b;
}

static lv_obj_t *make_metric_col(lv_obj_t *parent, const char *title, bool title_accent, lv_obj_t **out_value)
{
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_width(col, LV_SIZE_CONTENT);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 4, LV_PART_MAIN);

    lv_obj_t *lbl_title = lv_label_create(col);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_title, title_accent ? HEATER_CARD_ACCENT : HEATER_CARD_MUTED, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(lbl_title, 1, LV_PART_MAIN);

    lv_obj_t *lbl_val = lv_label_create(col);
    lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_val, title_accent ? HEATER_CARD_ACCENT : HEATER_CARD_TEXT, LV_PART_MAIN);
    if (out_value != NULL) {
        *out_value = lbl_val;
    }
    return col;
}

lv_obj_t *ui_heater_card_1_create(lv_obj_t *parent, lv_coord_t width, float current_temp_c, float setpoint_c,
                                  bool heater_on, bool climate_control_on, ui_heater_card_1_cb_t cb, void *user_data)
{
    if (parent == NULL || width <= 0) {
        return NULL;
    }

    heater_card_meta_t *m = lv_malloc(sizeof(heater_card_meta_t));
    if (m == NULL) {
        return NULL;
    }
    m->current_c = current_temp_c;
    m->setpoint_c = setpoint_c;
    m->heater_on = heater_on;
    m->climate_control_on = climate_control_on;
    m->cb = cb;
    m->user_data = user_data;
    m->lbl_status = NULL;
    m->dot_status = NULL;
    m->lbl_icon = NULL;
    m->lbl_current = NULL;
    m->lbl_setpoint = NULL;

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, width);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(card, 128, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, HEATER_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, UI_BOX_CORNER_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(card, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(card, 14, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card, 12, LV_PART_MAIN);

    lv_obj_t *identity = lv_obj_create(card);
    lv_obj_remove_style_all(identity);
    lv_obj_set_width(identity, LV_SIZE_CONTENT);
    lv_obj_set_height(identity, LV_SIZE_CONTENT);
    lv_obj_clear_flag(identity, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(identity, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(identity, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(identity, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(identity, 10, LV_PART_MAIN);

    lv_obj_t *icon_ring = lv_obj_create(identity);
    lv_obj_remove_style_all(icon_ring);
    lv_obj_set_size(icon_ring, 52, 52);
    lv_obj_set_style_radius(icon_ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(icon_ring, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(icon_ring, HEATER_CARD_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(icon_ring, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(icon_ring, LV_OBJ_FLAG_SCROLLABLE);

    m->lbl_icon = lv_label_create(icon_ring);
    lv_label_set_text(m->lbl_icon, UI_HA_ICON_FIRE);
    lv_obj_set_style_text_font(m->lbl_icon, &ui_font_home_assistant_icons_56, LV_PART_MAIN);
    lv_obj_set_style_text_color(m->lbl_icon, HEATER_CARD_ACCENT, LV_PART_MAIN);
    lv_obj_center(m->lbl_icon);

    lv_obj_t *identity_text = lv_obj_create(identity);
    lv_obj_remove_style_all(identity_text);
    lv_obj_set_width(identity_text, LV_SIZE_CONTENT);
    lv_obj_set_height(identity_text, LV_SIZE_CONTENT);
    lv_obj_clear_flag(identity_text, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(identity_text, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(identity_text, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(identity_text, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(identity_text, 4, LV_PART_MAIN);

    lv_obj_t *lbl_title = lv_label_create(identity_text);
    lv_label_set_text(lbl_title, "HEATER");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_title, HEATER_CARD_TEXT, LV_PART_MAIN);

    lv_obj_t *status_row = lv_obj_create(identity_text);
    lv_obj_remove_style_all(status_row);
    lv_obj_set_width(status_row, LV_SIZE_CONTENT);
    lv_obj_set_height(status_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(status_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(status_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(status_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(status_row, 6, LV_PART_MAIN);

    m->dot_status = lv_obj_create(status_row);
    lv_obj_remove_style_all(m->dot_status);
    lv_obj_set_size(m->dot_status, 8, 8);
    lv_obj_set_style_radius(m->dot_status, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(m->dot_status, HEATER_CARD_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m->dot_status, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(m->dot_status, LV_OBJ_FLAG_SCROLLABLE);

    m->lbl_status = lv_label_create(status_row);
    lv_obj_set_style_text_font(m->lbl_status, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(m->lbl_status, HEATER_CARD_ACCENT, LV_PART_MAIN);

    (void)make_vdivider(card, 72);
    (void)make_metric_col(card, "CURRENT", false, &m->lbl_current);
    (void)make_vdivider(card, 72);
    (void)make_metric_col(card, "DESIRED", true, &m->lbl_setpoint);

    lv_obj_t *btn_row = lv_obj_create(card);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_width(btn_row, LV_SIZE_CONTENT);
    lv_obj_set_height(btn_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(btn_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_row, 10, LV_PART_MAIN);

    (void)make_step_btn(btn_row, LV_SYMBOL_MINUS, false, m, on_minus);
    (void)make_step_btn(btn_row, LV_SYMBOL_PLUS, true, m, on_plus);

    refresh_current(m);
    refresh_setpoint(m);
    refresh_status(m);

    lv_obj_set_user_data(card, m);
    lv_obj_add_event_cb(card, heater_card_meta_free, LV_EVENT_DELETE, m);
    return card;
}

void ui_heater_card_1_set_current_temp(lv_obj_t *card, float temp_c)
{
    heater_card_meta_t *m = heater_card_get_meta(card);
    if (m == NULL) {
        return;
    }
    m->current_c = temp_c;
    refresh_current(m);
}

void ui_heater_card_1_set_setpoint(lv_obj_t *card, float setpoint_c)
{
    heater_card_meta_t *m = heater_card_get_meta(card);
    if (m == NULL) {
        return;
    }
    m->setpoint_c = setpoint_c;
    refresh_setpoint(m);
}

void ui_heater_card_1_set_switch_state(lv_obj_t *card, bool heater_on, bool climate_control_on)
{
    heater_card_meta_t *m = heater_card_get_meta(card);
    if (m == NULL) {
        return;
    }
    m->heater_on = heater_on;
    m->climate_control_on = climate_control_on;
    refresh_status(m);
}
