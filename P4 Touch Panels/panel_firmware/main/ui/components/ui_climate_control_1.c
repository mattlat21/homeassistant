#include "ui/components/ui_climate_control_1.h"

#include <stdio.h>

#include "ui/components/ui_box_1.h"
#include "ui/components/ui_single_selector_tab_1.h"
#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/ui_visual_tokens.h"

#define TEMP_MIN 5.0f
#define TEMP_MAX 30.0f
#define TEMP_STEP 0.5f

/** Square step buttons; inset/radius match ui_single_selector_tab_1 outer pad and segment corners. */
#define CLIMATE_STEP_BTN_SIZE 128

typedef struct {
    lv_obj_t *lbl_current;
    lv_obj_t *lbl_setpoint;
    lv_obj_t *climate_selector;
    lv_obj_t *lbl_heat_icon;
    float setpoint_c;
    float current_c;
    uint8_t climate_mode;
    bool heater_on;
    bool climate_control_on;
    bool suppress_events;
    ui_climate_control_1_cb_t cb;
    void *user_data;
} climate_meta_t;

static void climate_meta_free(lv_event_t *e)
{
    climate_meta_t *m = lv_event_get_user_data(e);
    if (m != NULL) {
        lv_free(m);
    }
}

static void format_temp(char *buf, size_t len, float c)
{
    snprintf(buf, len, "%.1f°C", (double)c);
}

static void refresh_current_label(climate_meta_t *m)
{
    if (m == NULL || m->lbl_current == NULL) {
        return;
    }
    char b[24];
    format_temp(b, sizeof(b), m->current_c);
    lv_label_set_text(m->lbl_current, b);
}

static void refresh_setpoint_label(climate_meta_t *m)
{
    if (m == NULL || m->lbl_setpoint == NULL) {
        return;
    }
    char b[24];
    format_temp(b, sizeof(b), m->setpoint_c);
    lv_label_set_text(m->lbl_setpoint, b);
}

static void refresh_heat_icon(climate_meta_t *m)
{
    if (m == NULL || m->lbl_heat_icon == NULL) {
        return;
    }
    const char *glyph;
    lv_color_t col;
    lv_opa_t opa = LV_OPA_COVER;
    if (m->heater_on) {
        glyph = UI_HA_ICON_FIRE;
        col = UI_SINGLE_SELECTOR_TEXT_COLOR_SELECTED;
        opa = LV_OPA_COVER;
    } else if (m->climate_control_on) {
        glyph = UI_HA_ICON_THERMOMETER;
        col = lv_color_white();
        opa = LV_OPA_70;
    } else {
        glyph = UI_HA_ICON_FIRE_OFF;
        col = lv_color_white();
        opa = (lv_opa_t)(255 * 45 / 100);
    }
    lv_label_set_text(m->lbl_heat_icon, glyph);
    lv_obj_set_style_text_color(m->lbl_heat_icon, col, LV_PART_MAIN);
    lv_obj_set_style_text_opa(m->lbl_heat_icon, opa, LV_PART_MAIN);
}

static void emit_cb(climate_meta_t *m, ui_climate_control_1_event_t ev)
{
    if (m == NULL || m->cb == NULL) {
        return;
    }
    const bool enabled = (m->climate_mode != UI_CLIMATE_CONTROL_1_MODE_OFF);
    m->cb(ev, m->setpoint_c, enabled, m->climate_mode, m->user_data);
}

static void climate_selector_on_change(uint32_t selected_index, void *user_data)
{
    climate_meta_t *m = (climate_meta_t *)user_data;
    if (m == NULL || m->suppress_events) {
        return;
    }
    if (selected_index > UI_CLIMATE_CONTROL_1_MODE_CLIMATE_CONTROL) {
        return;
    }
    m->climate_mode = (uint8_t)selected_index;
    if (m->climate_mode == UI_CLIMATE_CONTROL_1_MODE_OFF) {
        m->heater_on = false;
        m->climate_control_on = false;
    } else if (m->climate_mode == UI_CLIMATE_CONTROL_1_MODE_ON) {
        m->heater_on = true;
        m->climate_control_on = false;
    } else {
        m->climate_control_on = true;
    }
    refresh_heat_icon(m);
    emit_cb(m, UI_CLIMATE_CONTROL_1_EVENT_POWER);
}

static void on_minus(lv_event_t *e)
{
    climate_meta_t *m = lv_event_get_user_data(e);
    if (m == NULL) {
        return;
    }
    emit_cb(m, UI_CLIMATE_CONTROL_1_EVENT_SETPOINT_DEC);
}

static void on_plus(lv_event_t *e)
{
    climate_meta_t *m = lv_event_get_user_data(e);
    if (m == NULL) {
        return;
    }
    emit_cb(m, UI_CLIMATE_CONTROL_1_EVENT_SETPOINT_INC);
}

static lv_obj_t *make_step_btn(lv_obj_t *parent, const char *txt, climate_meta_t *m, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(b, CLIMATE_STEP_BTN_SIZE, CLIMATE_STEP_BTN_SIZE);
    lv_obj_set_style_bg_color(b, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, (lv_opa_t)(255 * 35 / 100), LV_PART_MAIN);
    lv_obj_set_style_radius(b, UI_SINGLE_SELECTOR_SEGMENT_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, m);
    return b;
}

static climate_meta_t *get_meta(const lv_obj_t *widget)
{
    if (widget == NULL) {
        return NULL;
    }
    return (climate_meta_t *)lv_obj_get_user_data((lv_obj_t *)widget);
}

lv_obj_t *ui_climate_control_1_create(lv_obj_t *parent, uint8_t row, uint8_t col, uint8_t row_span, uint8_t col_span,
                                      float current_temp_c, float setpoint_c, bool heater_on, bool climate_control_on,
                                      ui_climate_control_1_cb_t cb, void *user_data)
{
    if (parent == NULL) {
        return NULL;
    }
    climate_meta_t *m = lv_malloc(sizeof(climate_meta_t));
    if (m == NULL) {
        return NULL;
    }
    m->current_c = current_temp_c;
    m->setpoint_c = setpoint_c;
    if (m->setpoint_c < TEMP_MIN) {
        m->setpoint_c = TEMP_MIN;
    }
    if (m->setpoint_c > TEMP_MAX) {
        m->setpoint_c = TEMP_MAX;
    }
    m->heater_on = heater_on;
    m->climate_control_on = climate_control_on;
    if (climate_control_on) {
        m->climate_mode = UI_CLIMATE_CONTROL_1_MODE_CLIMATE_CONTROL;
    } else if (heater_on) {
        m->climate_mode = UI_CLIMATE_CONTROL_1_MODE_ON;
    } else {
        m->climate_mode = UI_CLIMATE_CONTROL_1_MODE_OFF;
    }
    m->suppress_events = false;
    m->cb = cb;
    m->user_data = user_data;

    lv_obj_t *outer = lv_obj_create(parent);
    lv_obj_remove_style_all(outer);
    ui_box_1_style_apply(outer);
    lv_obj_set_grid_cell(outer, LV_GRID_ALIGN_STRETCH, col, col_span, LV_GRID_ALIGN_STRETCH, row, row_span);
    lv_obj_clear_flag(outer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(outer, m);
    lv_obj_add_event_cb(outer, climate_meta_free, LV_EVENT_DELETE, m);

    lv_obj_set_layout(outer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(outer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(outer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(outer, UI_SINGLE_SELECTOR_INNER_PAD_ALL, LV_PART_MAIN);
    lv_obj_set_style_pad_row(outer, UI_SINGLE_SELECTOR_INNER_GAP, LV_PART_MAIN);

    lv_obj_t *top_row = lv_obj_create(outer);
    lv_obj_remove_style_all(top_row);
    lv_obj_clear_flag(top_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(top_row, lv_pct(100));
    lv_obj_set_height(top_row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(top_row, 0, LV_PART_MAIN);
    lv_obj_set_layout(top_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(top_row, LV_FLEX_FLOW_ROW);
    /* Top-align ± with temp column: only outer pad_top sits above this row. */
    lv_obj_set_flex_align(top_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    (void)make_step_btn(top_row, "-", m, on_minus);

    lv_obj_t *temp_col = lv_obj_create(top_row);
    lv_obj_remove_style_all(temp_col);
    lv_obj_clear_flag(temp_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_grow(temp_col, 1);
    lv_obj_set_height(temp_col, LV_SIZE_CONTENT);
    lv_obj_set_layout(temp_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(temp_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(temp_col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(temp_col, 4, LV_PART_MAIN);

    m->lbl_setpoint = lv_label_create(temp_col);
    refresh_setpoint_label(m);
    lv_obj_set_style_text_font(m->lbl_setpoint, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(m->lbl_setpoint, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(m->lbl_setpoint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(m->lbl_setpoint, lv_pct(100));

    m->lbl_current = lv_label_create(temp_col);
    refresh_current_label(m);
    lv_obj_set_style_text_font(m->lbl_current, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(m->lbl_current, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(m->lbl_current, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(m->lbl_current, lv_pct(100));

    m->lbl_heat_icon = lv_label_create(temp_col);
    lv_obj_set_style_text_font(m->lbl_heat_icon, &ui_font_home_assistant_icons_56, LV_PART_MAIN);
    lv_obj_set_style_text_align(m->lbl_heat_icon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(m->lbl_heat_icon, lv_pct(100));
    refresh_heat_icon(m);

    (void)make_step_btn(top_row, "+", m, on_plus);

    lv_obj_t *mid_spacer = lv_obj_create(outer);
    lv_obj_remove_style_all(mid_spacer);
    lv_obj_clear_flag(mid_spacer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(mid_spacer, lv_pct(100));
    lv_obj_set_flex_grow(mid_spacer, 1);

    static const ui_single_selector_item_t climate_items[] = {
        { UI_HA_ICON_FIRE_OFF, "off" },
        { UI_HA_ICON_FIRE, "on" },
        { UI_HA_ICON_THERMOMETER_LOW, "climate control" },
    };
    const uint32_t initial_sel = m->climate_mode <= UI_CLIMATE_CONTROL_1_MODE_CLIMATE_CONTROL ? m->climate_mode : 0u;

    m->climate_selector = ui_single_selector_tab_1_create(outer, 0, 0, 1, 1, true, climate_items,
                                                          sizeof(climate_items) / sizeof(climate_items[0]), initial_sel,
                                                          climate_selector_on_change, m);

    return outer;
}

void ui_climate_control_1_set_current_temp(lv_obj_t *widget, float temp_c)
{
    climate_meta_t *m = get_meta(widget);
    if (m == NULL) {
        return;
    }
    m->current_c = temp_c;
    refresh_current_label(m);
}

void ui_climate_control_1_set_setpoint(lv_obj_t *widget, float setpoint_c)
{
    climate_meta_t *m = get_meta(widget);
    if (m == NULL) {
        return;
    }
    if (setpoint_c < TEMP_MIN) {
        setpoint_c = TEMP_MIN;
    }
    if (setpoint_c > TEMP_MAX) {
        setpoint_c = TEMP_MAX;
    }
    m->setpoint_c = setpoint_c;
    refresh_setpoint_label(m);
}

void ui_climate_control_1_set_climate_on(lv_obj_t *widget, bool on)
{
    ui_climate_control_1_set_mode(widget, on ? UI_CLIMATE_CONTROL_1_MODE_ON : UI_CLIMATE_CONTROL_1_MODE_OFF);
}

void ui_climate_control_1_set_mode(lv_obj_t *widget, uint8_t mode_index)
{
    climate_meta_t *m = get_meta(widget);
    if (m == NULL || m->climate_selector == NULL) {
        return;
    }
    if (mode_index > UI_CLIMATE_CONTROL_1_MODE_CLIMATE_CONTROL) {
        return;
    }
    m->suppress_events = true;
    m->climate_mode = mode_index;
    ui_single_selector_tab_1_set_selected(m->climate_selector, mode_index, false);
    m->suppress_events = false;
    refresh_heat_icon(m);
}

uint8_t ui_climate_control_1_get_mode(const lv_obj_t *widget)
{
    climate_meta_t *m = get_meta(widget);
    if (m == NULL) {
        return UI_CLIMATE_CONTROL_1_MODE_OFF;
    }
    return m->climate_mode;
}

void ui_climate_control_1_set_switch_state(lv_obj_t *widget, bool heater_on, bool climate_control_on)
{
    climate_meta_t *m = get_meta(widget);
    if (m == NULL) {
        return;
    }
    m->heater_on = heater_on;
    m->climate_control_on = climate_control_on;
    refresh_heat_icon(m);
}
