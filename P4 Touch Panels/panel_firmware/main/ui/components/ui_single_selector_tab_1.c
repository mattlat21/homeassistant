#include "ui/components/ui_single_selector_tab_1.h"

#include "ui/components/ui_box_1.h"
#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/ui_visual_tokens.h"

typedef struct {
    lv_obj_t *inner;
    uint32_t selected;
    size_t count;
    ui_single_selector_tab_1_cb_t on_change;
    void *user_data;
} sel_meta_t;

/** Deferred layout for segment (avoid lv_obj_update_layout during create → recursion / stack use). */
typedef struct {
    lv_obj_t *ic;
    lv_obj_t *tx;
    bool layout_applied;
} seg_layout_ud_t;

static void apply_selector_icon_scale(lv_obj_t *seg, lv_obj_t *ic);

static void seg_layout_ud_free(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }
    seg_layout_ud_t *ud = lv_event_get_user_data(e);
    if(ud != NULL) {
        lv_free(ud);
    }
}

static void segment_layout_on_size(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_SIZE_CHANGED) {
        return;
    }
    lv_obj_t *seg = lv_event_get_target(e);
    seg_layout_ud_t *ud = lv_event_get_user_data(e);
    if(ud == NULL) {
        return;
    }
    if(ud->layout_applied) {
        return;
    }
    if(lv_obj_get_width(seg) < 8 || lv_obj_get_height(seg) < 8) {
        return;
    }
    ud->layout_applied = true;
    if(ud->tx != NULL) {
        lv_obj_set_width(ud->tx, lv_pct(100));
    }
    if(ud->ic != NULL) {
        apply_selector_icon_scale(seg, ud->ic);
    }
}

static void apply_segment_style(lv_obj_t *seg, bool selected)
{
    ui_box_1_style_apply(seg);
    lv_obj_set_style_radius(seg, UI_SINGLE_SELECTOR_SEGMENT_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(seg, selected ? (lv_opa_t)(255 * 70 / 100) : (lv_opa_t)(255 * 25 / 100), LV_PART_MAIN);
}

static void apply_segment_label_colors(lv_obj_t *seg, bool selected)
{
    if (seg == NULL) {
        return;
    }
    const lv_color_t col = selected ? UI_SINGLE_SELECTOR_TEXT_COLOR_SELECTED : UI_SINGLE_SELECTOR_TEXT_COLOR_IDLE;
    const uint32_t n = lv_obj_get_child_cnt(seg);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *ch = lv_obj_get_child(seg, i);
        lv_obj_set_style_text_color(ch, col, LV_PART_MAIN);
    }
}

static void selector_refresh(sel_meta_t *meta)
{
    if (meta == NULL || meta->inner == NULL) {
        return;
    }
    const uint32_t n = lv_obj_get_child_cnt(meta->inner);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *c = lv_obj_get_child(meta->inner, i);
        apply_segment_style(c, i == meta->selected);
        apply_segment_label_colors(c, i == meta->selected);
    }
}

static void selector_meta_free(lv_event_t *e)
{
    sel_meta_t *meta = lv_event_get_user_data(e);
    if (meta != NULL) {
        lv_free(meta);
    }
}

static void segment_click(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *seg = lv_event_get_current_target(e);
    sel_meta_t *meta = lv_event_get_user_data(e);
    if (meta == NULL || seg == NULL) {
        return;
    }
    const int32_t idx = lv_obj_get_index(seg);
    if (idx < 0) {
        return;
    }
    const uint32_t u = (uint32_t)idx;
    if (u >= meta->count || u == meta->selected) {
        return;
    }
    meta->selected = u;
    selector_refresh(meta);
    if (meta->on_change != NULL) {
        meta->on_change(u, meta->user_data);
    }
}

static void apply_selector_icon_scale(lv_obj_t *seg, lv_obj_t *ic)
{
    if(seg == NULL || ic == NULL) {
        return;
    }
    /* Avoid lv_obj_update_layout here — can recurse during flex + wrapped label layout. */
    const lv_coord_t iw = lv_obj_get_width(ic);
    const lv_coord_t ih = lv_obj_get_height(ic);
    if(iw < 1 || ih < 1) {
        return;
    }
#if UI_SINGLE_SELECTOR_ICON_SCALE != 256
    lv_obj_set_style_transform_pivot_x(ic, iw / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(ic, ih / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_x(ic, UI_SINGLE_SELECTOR_ICON_SCALE, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(ic, UI_SINGLE_SELECTOR_ICON_SCALE, LV_PART_MAIN);
#endif
}

static void style_selector_name_label(lv_obj_t *tx)
{
    lv_obj_set_style_text_font(tx, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(tx, UI_SINGLE_SELECTOR_TEXT_COLOR_IDLE, LV_PART_MAIN);
    lv_obj_set_style_text_align(tx, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    /* Default label long mode is WRAP; width applied in segment_layout_on_size. */
}

static void segment_add_labels(lv_obj_t *seg, const char *icon_utf8, const char *name)
{
    const bool has_icon = (icon_utf8 != NULL && icon_utf8[0] != '\0');
    const bool has_name = (name != NULL && name[0] != '\0');

    lv_obj_set_layout(seg, LV_LAYOUT_FLEX);
    lv_obj_set_style_pad_all(seg, 6, LV_PART_MAIN);

    if (has_icon && has_name) {
        lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(seg, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(seg, 4, LV_PART_MAIN);

        lv_obj_t *ic = lv_label_create(seg);
        lv_label_set_text(ic, icon_utf8);
        lv_obj_set_style_text_font(ic, &ui_font_home_assistant_icons_56, LV_PART_MAIN);
        lv_obj_set_style_text_color(ic, UI_SINGLE_SELECTOR_TEXT_COLOR_IDLE, LV_PART_MAIN);
        lv_obj_set_style_text_align(ic, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

        lv_obj_t *tx = lv_label_create(seg);
        lv_label_set_text(tx, name);
        style_selector_name_label(tx);

        seg_layout_ud_t *lud = lv_malloc(sizeof(seg_layout_ud_t));
        if(lud != NULL) {
            lud->ic = ic;
            lud->tx = tx;
            lud->layout_applied = false;
            lv_obj_add_event_cb(seg, seg_layout_ud_free, LV_EVENT_DELETE, lud);
            lv_obj_add_event_cb(seg, segment_layout_on_size, LV_EVENT_SIZE_CHANGED, lud);
        }
    } else if (has_icon) {
        lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(seg, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t *ic = lv_label_create(seg);
        lv_label_set_text(ic, icon_utf8);
        lv_obj_set_style_text_font(ic, &ui_font_home_assistant_icons_56, LV_PART_MAIN);
        lv_obj_set_style_text_color(ic, UI_SINGLE_SELECTOR_TEXT_COLOR_IDLE, LV_PART_MAIN);
    } else if (has_name) {
        lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(seg, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t *tx = lv_label_create(seg);
        lv_label_set_text(tx, name);
        style_selector_name_label(tx);
        seg_layout_ud_t *lud = lv_malloc(sizeof(seg_layout_ud_t));
        if(lud != NULL) {
            lud->ic = NULL;
            lud->tx = tx;
            lud->layout_applied = false;
            lv_obj_add_event_cb(seg, seg_layout_ud_free, LV_EVENT_DELETE, lud);
            lv_obj_add_event_cb(seg, segment_layout_on_size, LV_EVENT_SIZE_CHANGED, lud);
        }
    }
}

lv_obj_t *ui_single_selector_tab_1_create(lv_obj_t *parent, uint8_t row, uint8_t col, uint8_t row_span,
                                          uint8_t col_span, bool horizontal, const ui_single_selector_item_t *items,
                                          size_t item_count, uint32_t initial_selected,
                                          ui_single_selector_tab_1_cb_t on_change, void *user_data)
{
    if (parent == NULL || items == NULL || item_count == 0) {
        return NULL;
    }

    const bool parent_is_grid = (lv_obj_get_style_layout(parent, LV_PART_MAIN) == LV_LAYOUT_GRID);

    sel_meta_t *meta = lv_malloc(sizeof(sel_meta_t));
    if (meta == NULL) {
        return NULL;
    }
    meta->selected = (initial_selected < item_count) ? initial_selected : 0;
    meta->count = item_count;
    meta->on_change = on_change;
    meta->user_data = user_data;

    lv_obj_t *outer = lv_obj_create(parent);
    lv_obj_remove_style_all(outer);
    ui_box_1_style_apply(outer);
    if (parent_is_grid) {
        lv_obj_set_grid_cell(outer, LV_GRID_ALIGN_STRETCH, col, col_span, LV_GRID_ALIGN_STRETCH, row, row_span);
    } else {
        lv_obj_set_width(outer, lv_pct(100));
        lv_obj_set_height(outer, LV_SIZE_CONTENT);
    }
    lv_obj_clear_flag(outer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(outer, meta);
    lv_obj_add_event_cb(outer, selector_meta_free, LV_EVENT_DELETE, meta);

    lv_obj_t *inner = lv_obj_create(outer);
    lv_obj_remove_style_all(inner);
    lv_obj_set_width(inner, lv_pct(100));
    /*
     * Grid parent: outer has a definite height from stretch → inner can use pct height.
     * Flex/content parent: outer is LV_SIZE_CONTENT; inner pct height collapses (oh/ih stay 0).
     */
    lv_obj_set_height(inner, parent_is_grid ? lv_pct(100) : LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(inner, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(inner, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(inner, UI_SINGLE_SELECTOR_INNER_PAD_ALL, LV_PART_MAIN);
    lv_obj_set_style_pad_column(inner, UI_SINGLE_SELECTOR_INNER_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_row(inner, UI_SINGLE_SELECTOR_INNER_GAP, LV_PART_MAIN);
    lv_obj_clear_flag(inner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(inner, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(inner, horizontal ? LV_FLEX_FLOW_ROW : LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(inner, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    meta->inner = inner;

    for (size_t i = 0; i < item_count; i++) {
        lv_obj_t *seg = lv_obj_create(inner);
        lv_obj_remove_style_all(seg);
        lv_obj_add_flag(seg, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(seg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_grow(seg, 1);
        if (horizontal) {
            lv_obj_set_height(seg, parent_is_grid ? lv_pct(100) : LV_SIZE_CONTENT);
        } else {
            lv_obj_set_width(seg, parent_is_grid ? lv_pct(100) : LV_SIZE_CONTENT);
        }
        segment_add_labels(seg, items[i].icon, items[i].name);
        apply_segment_style(seg, i == meta->selected);
        apply_segment_label_colors(seg, i == meta->selected);
        lv_obj_add_event_cb(seg, segment_click, LV_EVENT_CLICKED, meta);
    }

    return outer;
}

uint32_t ui_single_selector_tab_1_get_selected(const lv_obj_t *widget)
{
    if (widget == NULL) {
        return 0;
    }
    sel_meta_t *meta = lv_obj_get_user_data((lv_obj_t *)widget);
    if (meta == NULL) {
        return 0;
    }
    return meta->selected;
}

void ui_single_selector_tab_1_set_selected(lv_obj_t *widget, uint32_t index, bool notify)
{
    if (widget == NULL) {
        return;
    }
    sel_meta_t *meta = lv_obj_get_user_data(widget);
    if (meta == NULL || meta->count == 0) {
        return;
    }
    if (index >= meta->count) {
        return;
    }
    if (index == meta->selected) {
        return;
    }
    meta->selected = index;
    selector_refresh(meta);
    if (notify && meta->on_change != NULL) {
        meta->on_change(index, meta->user_data);
    }
}
