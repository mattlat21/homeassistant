#include "ui/components/ui_light_card_1.h"

#include "ui/components/ui_box_1.h"
#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/ui_visual_tokens.h"

/** Inset around the on/off button (card top/left/bottom); the toggle spans the card height minus twice this. */
#define LIGHT_CARD_PAD_TOGGLE 10
/** Card right inset, keeping the name and slider clear of the rounded edge. */
#define LIGHT_CARD_PAD_RIGHT 20
/** Gap between the on/off button and the name + slider column. */
#define LIGHT_CARD_GAP 20
/** Placeholder square used until the first layout pass reports the real card height. */
#define LIGHT_CARD_TOGGLE_SIZE_INITIAL 84
#define LIGHT_CARD_SLIDER_HEIGHT 60
#define LIGHT_CARD_SLIDER_RADIUS 10
/** Grab margin around the track, so a drag that strays off the bar keeps working. */
#define LIGHT_CARD_SLIDER_EXT_CLICK 12

typedef struct {
    lv_obj_t *toggle;
    lv_obj_t *icon;
    lv_obj_t *percent_label;
    lv_obj_t *slider;
    ui_light_card_1_power_cb_t power_cb;
    ui_light_card_1_brightness_cb_t brightness_cb;
    void *user_data;
    bool on;
    /** Last value handed to @ref brightness_cb; suppresses repeat publishes on release without a drag. */
    uint8_t last_sent_pct;
    /** Current square toggle edge; latches the size-changed handler against relayout loops. */
    int32_t toggle_size;
} light_card_meta_t;

static void light_card_meta_free(lv_event_t *e)
{
    light_card_meta_t *meta = lv_event_get_user_data(e);
    if (meta != NULL) {
        lv_free(meta);
    }
}

static void light_card_apply_visuals(light_card_meta_t *meta)
{
    const lv_color_t accent = UI_SINGLE_SELECTOR_TEXT_COLOR_SELECTED;
    const lv_color_t idle = UI_SINGLE_SELECTOR_TEXT_COLOR_IDLE;

    lv_obj_set_style_bg_opa(meta->toggle, meta->on ? (lv_opa_t)(255 * 70 / 100) : UI_BOX_1_BG_OPA, LV_PART_MAIN);
    lv_obj_set_style_text_color(meta->icon, meta->on ? accent : idle, LV_PART_MAIN);
    lv_obj_set_style_bg_color(meta->slider, meta->on ? accent : lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(meta->slider, meta->on ? LV_OPA_COVER : (lv_opa_t)(255 * 45 / 100), LV_PART_INDICATOR);
}

/** Grid tracks size the card, so the square toggle can only be sized once the card has been laid out. */
static void light_card_on_size_changed(lv_event_t *e)
{
    light_card_meta_t *meta = lv_event_get_user_data(e);
    if (meta == NULL || meta->toggle == NULL) {
        return;
    }
    const int32_t edge = lv_obj_get_content_height(lv_event_get_target(e));
    if (edge <= 0 || edge == meta->toggle_size) {
        return;
    }
    meta->toggle_size = edge;
    lv_obj_set_size(meta->toggle, edge, edge);
}

static void light_card_update_percent_label(light_card_meta_t *meta, int32_t pct)
{
    lv_label_set_text_fmt(meta->percent_label, "%d%%", (int)pct);
}

static void light_card_toggle_clicked(lv_event_t *e)
{
    light_card_meta_t *meta = lv_event_get_user_data(e);
    if (meta == NULL) {
        return;
    }
    meta->on = !meta->on;
    light_card_apply_visuals(meta);
    if (meta->power_cb != NULL) {
        meta->power_cb(meta->on, meta->user_data);
    }
}

static void light_card_slider_changed(lv_event_t *e)
{
    light_card_meta_t *meta = lv_event_get_user_data(e);
    if (meta == NULL) {
        return;
    }
    light_card_update_percent_label(meta, lv_slider_get_value(meta->slider));
}

static void light_card_slider_released(lv_event_t *e)
{
    light_card_meta_t *meta = lv_event_get_user_data(e);
    if (meta == NULL) {
        return;
    }
    const uint8_t pct = (uint8_t)lv_slider_get_value(meta->slider);
    if (pct == meta->last_sent_pct) {
        return;
    }
    meta->last_sent_pct = pct;
    if (meta->brightness_cb != NULL) {
        meta->brightness_cb(pct, meta->user_data);
    }
}

void ui_light_card_1_set_state(lv_obj_t *card, bool on, uint8_t brightness_pct)
{
    if (card == NULL) {
        return;
    }
    light_card_meta_t *meta = (light_card_meta_t *)lv_obj_get_user_data(card);
    if (meta == NULL) {
        return;
    }
    if (brightness_pct > 100) {
        brightness_pct = 100;
    }
    meta->on = on;
    meta->last_sent_pct = brightness_pct;
    lv_slider_set_value(meta->slider, brightness_pct, LV_ANIM_OFF);
    light_card_update_percent_label(meta, brightness_pct);
    light_card_apply_visuals(meta);
}

lv_obj_t *ui_light_card_1_create(lv_obj_t *parent, uint8_t row, uint8_t col, uint8_t row_span, uint8_t col_span,
                                 const char *name, ui_light_card_1_power_cb_t power_cb,
                                 ui_light_card_1_brightness_cb_t brightness_cb, void *user_data)
{
    light_card_meta_t *meta = lv_malloc(sizeof(light_card_meta_t));
    if (meta == NULL) {
        return NULL;
    }
    lv_memzero(meta, sizeof(*meta));
    meta->power_cb = power_cb;
    meta->brightness_cb = brightness_cb;
    meta->user_data = user_data;

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    ui_box_1_style_apply(card);
    lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, col, col_span, LV_GRID_ALIGN_STRETCH, row, row_span);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_top(card, LIGHT_CARD_PAD_TOGGLE, LV_PART_MAIN);
    lv_obj_set_style_pad_left(card, LIGHT_CARD_PAD_TOGGLE, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(card, LIGHT_CARD_PAD_TOGGLE, LV_PART_MAIN);
    lv_obj_set_style_pad_right(card, LIGHT_CARD_PAD_RIGHT, LV_PART_MAIN);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card, LIGHT_CARD_GAP, LV_PART_MAIN);
    lv_obj_set_user_data(card, meta);
    lv_obj_add_event_cb(card, light_card_meta_free, LV_EVENT_DELETE, meta);
    lv_obj_add_event_cb(card, light_card_on_size_changed, LV_EVENT_SIZE_CHANGED, meta);

    meta->toggle = lv_button_create(card);
    lv_obj_remove_style_all(meta->toggle);
    ui_box_1_style_apply(meta->toggle);
    lv_obj_set_size(meta->toggle, LIGHT_CARD_TOGGLE_SIZE_INITIAL, LIGHT_CARD_TOGGLE_SIZE_INITIAL);
    lv_obj_clear_flag(meta->toggle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(meta->toggle, light_card_toggle_clicked, LV_EVENT_CLICKED, meta);

    meta->icon = lv_label_create(meta->toggle);
    lv_label_set_text(meta->icon, UI_HA_ICON_LIGHTBULB);
    lv_obj_set_style_text_font(meta->icon, &ui_font_home_assistant_icons_56, LV_PART_MAIN);
    lv_obj_center(meta->icon);

    lv_obj_t *column = lv_obj_create(card);
    lv_obj_remove_style_all(column);
    lv_obj_set_height(column, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(column, 1);
    lv_obj_clear_flag(column, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(column, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(column, 12, LV_PART_MAIN);

    lv_obj_t *header = lv_obj_create(column);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *name_label = lv_label_create(header);
    lv_label_set_text(name_label, name != NULL ? name : "");
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(name_label, lv_color_white(), LV_PART_MAIN);

    meta->percent_label = lv_label_create(header);
    lv_obj_set_style_text_font(meta->percent_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(meta->percent_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_opa(meta->percent_label, (lv_opa_t)(255 * 70 / 100), LV_PART_MAIN);

    meta->slider = lv_slider_create(column);
    lv_obj_remove_style_all(meta->slider);
    lv_obj_set_width(meta->slider, lv_pct(100));
    lv_obj_set_height(meta->slider, LIGHT_CARD_SLIDER_HEIGHT);
    lv_slider_set_range(meta->slider, 0, 100);
    lv_obj_set_style_bg_color(meta->slider, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(meta->slider, (lv_opa_t)(255 * 25 / 100), LV_PART_MAIN);
    lv_obj_set_style_radius(meta->slider, LIGHT_CARD_SLIDER_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_radius(meta->slider, LIGHT_CARD_SLIDER_RADIUS, LV_PART_INDICATOR);
    /* Track-only look: the knob still drives the drag, it just paints nothing. */
    lv_obj_set_style_bg_opa(meta->slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(meta->slider, 0, LV_PART_KNOB);
    lv_obj_set_ext_click_area(meta->slider, LIGHT_CARD_SLIDER_EXT_CLICK);
    lv_obj_add_event_cb(meta->slider, light_card_slider_changed, LV_EVENT_VALUE_CHANGED, meta);
    lv_obj_add_event_cb(meta->slider, light_card_slider_released, LV_EVENT_RELEASED, meta);

    light_card_update_percent_label(meta, 0);
    light_card_apply_visuals(meta);
    return card;
}
