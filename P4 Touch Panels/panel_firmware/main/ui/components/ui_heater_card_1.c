#include "ui/components/ui_heater_card_1.h"

#include <math.h>
#include <stdio.h>

#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/ui_visual_tokens.h"

#define HEATER_CARD_ACCENT lv_color_hex(0xF97316)
#define HEATER_CARD_BG lv_color_hex(0xF2F2F7)
#define HEATER_CARD_TEXT lv_color_hex(0x1C1C1E)
#define HEATER_CARD_MUTED lv_color_hex(0x8E8E93)
#define HEATER_CARD_DIVIDER lv_color_hex(0xD1D1D6)
#define HEATER_CARD_EDGE_MARGIN 8
#define HEATER_CARD_BUTTON_GAP 10
#define HEATER_CARD_MODE_COLOR_COOL lv_color_hex(0x3B82F6)
#define HEATER_CARD_MODE_COLOR_FAN lv_color_hex(0x22C55E)

#define HEATER_CARD_TEMP_WIDTH_SAMPLE "99.9°"
#define HEATER_CARD_MODE_OPTION_COUNT 4

typedef enum {
    HEATER_MODE_UI_OFF = 0,
    HEATER_MODE_UI_HEATING,
    HEATER_MODE_UI_COOLING,
    HEATER_MODE_UI_FAN,
} heater_mode_ui_t;

typedef struct heater_card_meta heater_card_meta_t;

typedef struct {
    ui_heater_card_1_event_t event;
    heater_card_meta_t *meta;
} heater_mode_click_t;

struct heater_card_meta {
    lv_obj_t *lbl_status;
    lv_obj_t *dot_status;
    lv_obj_t *lbl_icon;
    lv_obj_t *lbl_current;
    lv_obj_t *lbl_setpoint;
    lv_obj_t *icon_btn;
    lv_obj_t *cancel_btn;
    lv_obj_t *normal_layer;
    lv_obj_t *mode_layer;
    lv_obj_t *btn_minus;
    lv_obj_t *btn_plus;
    lv_obj_t *mode_btns[HEATER_CARD_MODE_OPTION_COUNT];
    float current_c;
    float setpoint_c;
    bool heater_on;
    bool climate_control_on;
    bool mode_picker_open;
    ui_heater_card_1_cb_t cb;
    void *user_data;
    heater_mode_click_t mode_clicks[HEATER_CARD_MODE_OPTION_COUNT];
};

static void format_current_temp_degree(char *buf, size_t len, float c)
{
    const int tenths = (int)lroundf(c * 10.0f);
    if ((tenths % 10) == 0) {
        snprintf(buf, len, "%d°", tenths / 10);
    } else {
        snprintf(buf, len, "%.1f°", (double)(tenths / 10.0f));
    }
}

static void format_setpoint_temp_degree(char *buf, size_t len, float c)
{
    const int tenths = (int)lroundf(c * 10.0f);
    snprintf(buf, len, "%.1f°", (double)(tenths / 10.0f));
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
    format_current_temp_degree(b, sizeof(b), m->current_c);
    lv_label_set_text(m->lbl_current, b);
}

static void refresh_setpoint(heater_card_meta_t *m)
{
    if (m == NULL || m->lbl_setpoint == NULL) {
        return;
    }
    char b[16];
    format_setpoint_temp_degree(b, sizeof(b), m->setpoint_c);
    lv_label_set_text(m->lbl_setpoint, b);
}

static void set_step_button_slot_active(lv_obj_t *btn, bool active)
{
    if (btn == NULL) {
        return;
    }
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_HIDDEN);
    if (active) {
        lv_obj_set_style_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_set_style_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void refresh_step_buttons(heater_card_meta_t *m)
{
    if (m == NULL || m->btn_minus == NULL || m->btn_plus == NULL || m->mode_picker_open) {
        return;
    }
    set_step_button_slot_active(m->btn_minus, m->climate_control_on);
    set_step_button_slot_active(m->btn_plus, m->climate_control_on);
}

static void refresh_status_icon(heater_card_meta_t *m)
{
    if (m == NULL || m->lbl_icon == NULL || m->icon_btn == NULL) {
        return;
    }
    if (!m->climate_control_on) {
        lv_label_set_text(m->lbl_icon, LV_SYMBOL_POWER);
        lv_obj_set_style_text_font(m->lbl_icon, &lv_font_montserrat_32, LV_PART_MAIN);
        lv_obj_set_style_text_color(m->lbl_icon, HEATER_CARD_MUTED, LV_PART_MAIN);
        lv_obj_set_style_border_color(m->icon_btn, HEATER_CARD_MUTED, LV_PART_MAIN);
    } else if (m->heater_on) {
        lv_label_set_text(m->lbl_icon, UI_HA_ICON_FIRE);
        lv_obj_set_style_text_font(m->lbl_icon, &ui_font_home_assistant_icons_56, LV_PART_MAIN);
        lv_obj_set_style_text_color(m->lbl_icon, HEATER_CARD_ACCENT, LV_PART_MAIN);
        lv_obj_set_style_border_color(m->icon_btn, HEATER_CARD_ACCENT, LV_PART_MAIN);
    } else {
        lv_label_set_text(m->lbl_icon, UI_HA_ICON_FIRE);
        lv_obj_set_style_text_font(m->lbl_icon, &ui_font_home_assistant_icons_56, LV_PART_MAIN);
        lv_obj_set_style_text_color(m->lbl_icon, HEATER_CARD_MUTED, LV_PART_MAIN);
        lv_obj_set_style_border_color(m->icon_btn, HEATER_CARD_MUTED, LV_PART_MAIN);
    }
}

static void refresh_status(heater_card_meta_t *m)
{
    if (m == NULL || m->lbl_status == NULL || m->dot_status == NULL) {
        return;
    }
    const char *status_text;
    lv_opa_t dot_opa = LV_OPA_COVER;
    lv_color_t dot_color = HEATER_CARD_MUTED;
    if (m->climate_control_on && m->heater_on) {
        status_text = "HEATING";
        dot_color = HEATER_CARD_ACCENT;
    } else if (m->climate_control_on) {
        status_text = "CLIMATE";
        dot_color = HEATER_CARD_MUTED;
    } else {
        status_text = "OFF";
        dot_opa = LV_OPA_40;
    }
    lv_label_set_text(m->lbl_status, status_text);
    lv_obj_set_style_text_color(m->lbl_status, dot_color, LV_PART_MAIN);
    lv_obj_set_style_text_opa(m->lbl_status, dot_opa, LV_PART_MAIN);
    lv_obj_set_style_bg_color(m->dot_status, dot_color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m->dot_status, dot_opa, LV_PART_MAIN);
    refresh_status_icon(m);
    refresh_step_buttons(m);
}

static heater_mode_ui_t active_mode_from_state(bool heater_on, bool climate_control_on)
{
    (void)heater_on;
    if (!climate_control_on) {
        return HEATER_MODE_UI_OFF;
    }
    return HEATER_MODE_UI_HEATING;
}

static bool mode_option_is_actionable(ui_heater_card_1_event_t event)
{
    return event == UI_HEATER_CARD_1_EVENT_MODE_OFF || event == UI_HEATER_CARD_1_EVENT_MODE_HEATING;
}

static void refresh_mode_highlights(heater_card_meta_t *m)
{
    if (m == NULL) {
        return;
    }
    const heater_mode_ui_t active = active_mode_from_state(m->heater_on, m->climate_control_on);
    for (unsigned i = 0; i < HEATER_CARD_MODE_OPTION_COUNT; i++) {
        if (m->mode_btns[i] == NULL) {
            continue;
        }
        const bool selected = ((heater_mode_ui_t)i == active);
        lv_obj_set_style_border_width(m->mode_btns[i], selected ? 3 : 2, LV_PART_MAIN);
    }
}

static void emit_cb(heater_card_meta_t *m, ui_heater_card_1_event_t ev)
{
    if (m != NULL && m->cb != NULL) {
        m->cb(ev, m->user_data);
    }
}

static void set_mode_picker_open(heater_card_meta_t *m, bool open)
{
    if (m == NULL || m->mode_picker_open == open) {
        return;
    }
    m->mode_picker_open = open;
    if (open) {
        lv_obj_add_flag(m->normal_layer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(m->btn_minus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(m->btn_plus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(m->mode_layer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(m->icon_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(m->cancel_btn, LV_OBJ_FLAG_HIDDEN);
        refresh_mode_highlights(m);
    } else {
        lv_obj_remove_flag(m->normal_layer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(m->mode_layer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(m->icon_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(m->cancel_btn, LV_OBJ_FLAG_HIDDEN);
        refresh_step_buttons(m);
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

static void on_icon_open_mode(lv_event_t *e)
{
    heater_card_meta_t *m = lv_event_get_user_data(e);
    set_mode_picker_open(m, true);
}

static void on_cancel_mode(lv_event_t *e)
{
    heater_card_meta_t *m = lv_event_get_user_data(e);
    set_mode_picker_open(m, false);
}

static void on_mode_option(lv_event_t *e)
{
    heater_mode_click_t *mc = lv_event_get_user_data(e);
    if (mc == NULL || mc->meta == NULL || !mode_option_is_actionable(mc->event)) {
        return;
    }
    emit_cb(mc->meta, mc->event);
    set_mode_picker_open(mc->meta, false);
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

static void set_obj_margins(lv_obj_t *obj, lv_coord_t margin_top, lv_coord_t margin_bottom, lv_coord_t margin_left,
                            lv_coord_t margin_right)
{
    lv_obj_set_style_margin_top(obj, margin_top, LV_PART_MAIN);
    lv_obj_set_style_margin_bottom(obj, margin_bottom, LV_PART_MAIN);
    lv_obj_set_style_margin_left(obj, margin_left, LV_PART_MAIN);
    lv_obj_set_style_margin_right(obj, margin_right, LV_PART_MAIN);
}

static void style_circle_button(lv_obj_t *btn, lv_color_t accent, bool selected)
{
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, selected ? 3 : 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, accent, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_shadow_offset_y(btn, 2, LV_PART_MAIN);
}

static lv_obj_t *make_left_circle_btn(lv_obj_t *parent, lv_coord_t size, lv_coord_t margin_px, bool accent_ring)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(b, size, size);
    set_obj_margins(b, margin_px, margin_px, margin_px, margin_px);
    style_circle_button(b, accent_ring ? HEATER_CARD_ACCENT : HEATER_CARD_MUTED, false);
    return b;
}

static lv_obj_t *make_step_btn(lv_obj_t *parent, lv_coord_t size, const char *symbol, bool accent_ring,
                               heater_card_meta_t *m, lv_event_cb_t cb, lv_coord_t margin_top, lv_coord_t margin_bottom,
                               lv_coord_t margin_left, lv_coord_t margin_right)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(b, size, size);
    set_obj_margins(b, margin_top, margin_bottom, margin_left, margin_right);
    style_circle_button(b, accent_ring ? HEATER_CARD_ACCENT : HEATER_CARD_TEXT, accent_ring);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, symbol);
    lv_obj_set_style_text_font(l, size >= 96 ? &lv_font_montserrat_32 : &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, accent_ring ? HEATER_CARD_ACCENT : HEATER_CARD_TEXT, LV_PART_MAIN);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, m);
    return b;
}

static lv_obj_t *make_mode_option(lv_obj_t *parent, lv_coord_t circle_sz, const char *icon_glyph, const char *label,
                                  lv_color_t color, heater_card_meta_t *m, ui_heater_card_1_event_t event,
                                  heater_mode_click_t *click_ud, lv_obj_t **out_btn)
{
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_remove_style_all(col);
    lv_obj_set_width(col, 0);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 6, LV_PART_MAIN);

    lv_obj_t *btn = lv_button_create(col);
    lv_obj_remove_style_all(btn);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn, circle_sz, circle_sz);
    style_circle_button(btn, color, false);

    lv_obj_t *icon = lv_label_create(btn);
    lv_label_set_text(icon, icon_glyph);
    lv_obj_set_style_text_font(icon, &ui_font_home_assistant_icons_56, LV_PART_MAIN);
    lv_obj_set_style_text_color(icon, color, LV_PART_MAIN);
    lv_obj_center(icon);

    lv_obj_t *lbl = lv_label_create(col);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, color, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(lbl, 1, LV_PART_MAIN);

    click_ud->meta = m;
    click_ud->event = event;
    lv_obj_add_event_cb(btn, on_mode_option, LV_EVENT_CLICKED, click_ud);

    if (out_btn != NULL) {
        *out_btn = btn;
    }
    return col;
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
    lv_label_set_text(lbl_val, HEATER_CARD_TEMP_WIDTH_SAMPLE);
    lv_obj_update_layout(lbl_val);
    const lv_coord_t temp_w = lv_obj_get_width(lbl_val);
    lv_obj_set_width(lbl_val, temp_w);
    lv_obj_set_style_text_align(lbl_val, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(lbl_val, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(col, temp_w);
    if (out_value != NULL) {
        *out_value = lbl_val;
    }
    return col;
}

lv_obj_t *ui_heater_card_1_create(lv_obj_t *parent, lv_coord_t width, const char *room_name, float current_temp_c,
                                  float setpoint_c, bool heater_on, bool climate_control_on, ui_heater_card_1_cb_t cb,
                                  void *user_data, lv_coord_t min_height_px, lv_coord_t circle_margin_px,
                                  lv_coord_t button_gap_px)
{
    if (parent == NULL || width <= 0) {
        return NULL;
    }
    if (min_height_px <= 0) {
        min_height_px = 128;
    }
    if (circle_margin_px <= 0) {
        circle_margin_px = HEATER_CARD_EDGE_MARGIN;
    }
    if (button_gap_px <= 0) {
        button_gap_px = HEATER_CARD_BUTTON_GAP;
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
    m->mode_picker_open = false;
    m->lbl_status = NULL;
    m->dot_status = NULL;
    m->lbl_icon = NULL;
    m->lbl_current = NULL;
    m->lbl_setpoint = NULL;
    m->icon_btn = NULL;
    m->cancel_btn = NULL;
    m->normal_layer = NULL;
    m->mode_layer = NULL;
    m->btn_minus = NULL;
    m->btn_plus = NULL;
    for (unsigned i = 0; i < HEATER_CARD_MODE_OPTION_COUNT; i++) {
        m->mode_btns[i] = NULL;
        m->mode_clicks[i].meta = m;
        m->mode_clicks[i].event = UI_HEATER_CARD_1_EVENT_MODE_OFF;
    }

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, width);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(card, min_height_px, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, HEATER_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, UI_BOX_CORNER_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const lv_coord_t edge_margin = circle_margin_px;
    const lv_coord_t btn_sz = (lv_coord_t)(min_height_px - (edge_margin * 2));
    const lv_coord_t mode_circle_sz = btn_sz;

    m->icon_btn = make_left_circle_btn(card, btn_sz, edge_margin, true);
    m->lbl_icon = lv_label_create(m->icon_btn);
    lv_obj_set_style_text_font(m->lbl_icon, &ui_font_home_assistant_icons_56, LV_PART_MAIN);
    lv_obj_center(m->lbl_icon);
    lv_obj_add_event_cb(m->icon_btn, on_icon_open_mode, LV_EVENT_CLICKED, m);

    m->cancel_btn = make_left_circle_btn(card, btn_sz, edge_margin, false);
    lv_obj_t *cancel_lbl = lv_label_create(m->cancel_btn);
    lv_label_set_text(cancel_lbl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(cancel_lbl, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(cancel_lbl, HEATER_CARD_MUTED, LV_PART_MAIN);
    lv_obj_center(cancel_lbl);
    lv_obj_add_event_cb(m->cancel_btn, on_cancel_mode, LV_EVENT_CLICKED, m);
    lv_obj_add_flag(m->cancel_btn, LV_OBJ_FLAG_HIDDEN);

    m->normal_layer = lv_obj_create(card);
    lv_obj_remove_style_all(m->normal_layer);
    lv_obj_set_width(m->normal_layer, 0);
    lv_obj_set_height(m->normal_layer, LV_PCT(100));
    lv_obj_set_flex_grow(m->normal_layer, 1);
    lv_obj_clear_flag(m->normal_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(m->normal_layer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(m->normal_layer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m->normal_layer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *content_col = lv_obj_create(m->normal_layer);
    lv_obj_remove_style_all(content_col);
    lv_obj_set_width(content_col, 0);
    lv_obj_set_height(content_col, LV_PCT(100));
    lv_obj_set_flex_grow(content_col, 1);
    lv_obj_clear_flag(content_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(content_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_top(content_col, edge_margin, LV_PART_MAIN);
    lv_obj_set_style_pad_right(content_col, 8, LV_PART_MAIN);

    if (room_name != NULL && room_name[0] != '\0') {
        lv_obj_t *lbl_room = lv_label_create(content_col);
        lv_label_set_text(lbl_room, room_name);
        lv_obj_set_style_text_font(lbl_room, &lv_font_montserrat_32, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl_room, HEATER_CARD_TEXT, LV_PART_MAIN);
        lv_obj_set_width(lbl_room, LV_PCT(100));
        lv_label_set_long_mode(lbl_room, LV_LABEL_LONG_DOT);
    }

    lv_obj_t *body = lv_obj_create(content_col);
    lv_obj_remove_style_all(body);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_height(body, 0);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(body, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(body, 8, LV_PART_MAIN);

    lv_obj_t *identity_text = lv_obj_create(body);
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

    (void)make_vdivider(body, 64);
    (void)make_metric_col(body, "CURRENT", false, &m->lbl_current);
    (void)make_vdivider(body, 64);
    (void)make_metric_col(body, "DESIRED", true, &m->lbl_setpoint);

    m->mode_layer = lv_obj_create(card);
    lv_obj_remove_style_all(m->mode_layer);
    lv_obj_set_width(m->mode_layer, 0);
    lv_obj_set_height(m->mode_layer, LV_PCT(100));
    lv_obj_set_flex_grow(m->mode_layer, 1);
    lv_obj_clear_flag(m->mode_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(m->mode_layer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(m->mode_layer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(m->mode_layer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(m->mode_layer, LV_OBJ_FLAG_HIDDEN);

    (void)make_mode_option(m->mode_layer, mode_circle_sz, UI_HA_ICON_FIRE, "HEATING", HEATER_CARD_ACCENT, m,
                           UI_HEATER_CARD_1_EVENT_MODE_HEATING, &m->mode_clicks[HEATER_MODE_UI_HEATING],
                           &m->mode_btns[HEATER_MODE_UI_HEATING]);
    (void)make_vdivider(m->mode_layer, (lv_coord_t)(mode_circle_sz + 24));
    (void)make_mode_option(m->mode_layer, mode_circle_sz, UI_HA_ICON_THERMOMETER_LOW, "COOLING",
                           HEATER_CARD_MODE_COLOR_COOL, m, UI_HEATER_CARD_1_EVENT_MODE_COOLING,
                           &m->mode_clicks[HEATER_MODE_UI_COOLING], &m->mode_btns[HEATER_MODE_UI_COOLING]);
    (void)make_vdivider(m->mode_layer, (lv_coord_t)(mode_circle_sz + 24));
    (void)make_mode_option(m->mode_layer, mode_circle_sz, UI_HA_ICON_FAN, "FAN", HEATER_CARD_MODE_COLOR_FAN, m,
                           UI_HEATER_CARD_1_EVENT_MODE_FAN, &m->mode_clicks[HEATER_MODE_UI_FAN],
                           &m->mode_btns[HEATER_MODE_UI_FAN]);
    (void)make_vdivider(m->mode_layer, (lv_coord_t)(mode_circle_sz + 24));
    (void)make_mode_option(m->mode_layer, mode_circle_sz, UI_HA_ICON_RADIATOR_OFF, "OFF", HEATER_CARD_MUTED, m,
                           UI_HEATER_CARD_1_EVENT_MODE_OFF, &m->mode_clicks[HEATER_MODE_UI_OFF],
                           &m->mode_btns[HEATER_MODE_UI_OFF]);

    m->btn_minus = make_step_btn(card, btn_sz, LV_SYMBOL_MINUS, false, m, on_minus, edge_margin, edge_margin, 0,
                                 button_gap_px);
    m->btn_plus =
        make_step_btn(card, btn_sz, LV_SYMBOL_PLUS, true, m, on_plus, edge_margin, edge_margin, 0, edge_margin);

    refresh_current(m);
    refresh_setpoint(m);
    refresh_status(m);
    refresh_step_buttons(m);

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
    if (m->mode_picker_open) {
        refresh_mode_highlights(m);
    }
}
