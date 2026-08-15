#include "ui/components/ui_taskbar.h"

#include "ui/fonts/ui_home_assistant_icon_glyphs.h"

typedef struct {
    ui_taskbar_item_cb_t click_cb;
    void *click_user_data;
} taskbar_item_meta_t;

static void taskbar_item_meta_free(lv_event_t *e)
{
    taskbar_item_meta_t *meta = lv_event_get_user_data(e);
    if (meta != NULL) {
        lv_free(meta);
    }
}

static void taskbar_item_click(lv_event_t *e)
{
    taskbar_item_meta_t *meta = lv_event_get_user_data(e);
    if (meta != NULL && meta->click_cb != NULL) {
        meta->click_cb(meta->click_user_data);
    }
}

static lv_obj_t *taskbar_dock(lv_obj_t *bar)
{
    if (bar == NULL || lv_obj_get_child_cnt(bar) == 0) {
        return NULL;
    }
    return lv_obj_get_child(bar, 0);
}

void ui_taskbar_set_bg(lv_obj_t *bar, lv_color_t color, lv_opa_t opa)
{
    lv_obj_t *dock = taskbar_dock(bar);
    if (dock == NULL) {
        return;
    }
    lv_obj_set_style_bg_color(dock, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dock, opa, LV_PART_MAIN);
}

void ui_taskbar_raise(lv_obj_t *bar)
{
    if (bar != NULL) {
        lv_obj_move_foreground(bar);
    }
}

lv_obj_t *ui_taskbar_get_item(lv_obj_t *bar, uint8_t index)
{
    lv_obj_t *dock = taskbar_dock(bar);
    if (dock == NULL || index >= lv_obj_get_child_cnt(dock)) {
        return NULL;
    }
    return lv_obj_get_child(dock, index);
}

static void taskbar_add_item(lv_obj_t *dock, const ui_taskbar_item_t *item)
{
    lv_obj_t *btn = lv_button_create(dock);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, UI_TASKBAR_ITEM_SIZE, UI_TASKBAR_ITEM_SIZE);
    lv_obj_set_style_radius(btn, UI_TASKBAR_ITEM_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, (lv_opa_t)(255 * 30 / 100), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = lv_label_create(btn);
    lv_label_set_text(icon, (item->icon_utf8 != NULL) ? item->icon_utf8 : LV_SYMBOL_DUMMY);
    lv_obj_set_style_text_font(icon, (item->icon_font != NULL) ? item->icon_font
                                                              : &ui_font_home_assistant_icons_56,
                               LV_PART_MAIN);
    lv_obj_set_style_text_color(icon, (item->icon_color != NULL) ? *item->icon_color : UI_TASKBAR_ICON_COLOR,
                                LV_PART_MAIN);
    lv_obj_center(icon);

    taskbar_item_meta_t *meta = lv_malloc(sizeof(taskbar_item_meta_t));
    if (meta != NULL) {
        meta->click_cb = item->click_cb;
        meta->click_user_data = item->click_user_data;
        lv_obj_add_event_cb(btn, taskbar_item_click, LV_EVENT_CLICKED, meta);
        lv_obj_add_event_cb(btn, taskbar_item_meta_free, LV_EVENT_DELETE, meta);
    }
}

lv_obj_t *ui_taskbar_create(lv_obj_t *parent, const ui_taskbar_item_t *items, uint8_t count)
{
    if (parent == NULL || (count > 0 && items == NULL)) {
        return NULL;
    }
    if (count > UI_TASKBAR_MAX_ITEMS) {
        count = (uint8_t)UI_TASKBAR_MAX_ITEMS;
    }

    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, lv_pct(100), UI_TASKBAR_HEIGHT);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    /* Stay bottom-aligned even when the parent uses a flex/grid layout. */
    lv_obj_add_flag(bar, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(bar, UI_TASKBAR_SIDE_MARGIN, LV_PART_MAIN);
    lv_obj_set_style_pad_right(bar, UI_TASKBAR_SIDE_MARGIN, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(bar, UI_TASKBAR_BOTTOM_MARGIN, LV_PART_MAIN);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *dock = lv_obj_create(bar);
    lv_obj_remove_style_all(dock);
    lv_obj_set_size(dock, lv_pct(100), UI_TASKBAR_DOCK_HEIGHT);
    lv_obj_set_style_radius(dock, UI_TASKBAR_DOCK_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dock, UI_TASKBAR_BG_COLOR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dock, UI_TASKBAR_BG_OPA, LV_PART_MAIN);
    lv_obj_set_style_border_width(dock, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(dock, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_opa(dock, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(dock, 24, LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_y(dock, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(dock, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(dock, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(dock, (UI_TASKBAR_DOCK_HEIGHT - UI_TASKBAR_ITEM_SIZE) / 2, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(dock, (UI_TASKBAR_DOCK_HEIGHT - UI_TASKBAR_ITEM_SIZE) / 2, LV_PART_MAIN);
    lv_obj_clear_flag(dock, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(dock, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dock, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (uint8_t i = 0; i < count; i++) {
        taskbar_add_item(dock, &items[i]);
    }

    ui_taskbar_raise(bar);
    return bar;
}
