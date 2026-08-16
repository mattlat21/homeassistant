#include "ui/components/ui_confirm_dialog.h"

#include "ui/components/ui_box_1.h"
#include "ui/ui_visual_tokens.h"

#define CONFIRM_BACKDROP_OPA ((lv_opa_t)(255 * 82 / 100))
#define CONFIRM_PANEL_OPA ((lv_opa_t)(255 * 93 / 100))
#define CONFIRM_CANCEL_BTN_OPA ((lv_opa_t)(255 * 88 / 100))
#define CONFIRM_BTN_MIN_HEIGHT 76

/** Lives on `lv_layer_top()`, which is shared by all screens, so one instance is enough. */
static lv_obj_t *s_dialog;
static ui_confirm_dialog_cb_t s_confirm_cb;
static void *s_confirm_user_data;

void ui_confirm_dialog_close(void)
{
    if (s_dialog != NULL) {
        lv_obj_del(s_dialog);
        s_dialog = NULL;
    }
    s_confirm_cb = NULL;
    s_confirm_user_data = NULL;
}

static void confirm_on_deleted(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }
    s_dialog = NULL;
}

static void confirm_bg_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (lv_event_get_target(e) != s_dialog) {
        return;
    }
    ui_confirm_dialog_close();
}

static void confirm_cancel_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_confirm_dialog_close();
}

static void confirm_accept_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    /* Snapshot before closing: the close path clears the pending callback. */
    ui_confirm_dialog_cb_t cb = s_confirm_cb;
    void *user_data = s_confirm_user_data;
    ui_confirm_dialog_close();
    if (cb != NULL) {
        cb(user_data);
    }
}

static lv_obj_t *add_choice_button(lv_obj_t *row, const char *text, bool accent, lv_event_cb_t click_cb)
{
    lv_obj_t *btn = lv_button_create(row);
    lv_obj_remove_style_all(btn);
    if (accent) {
        lv_obj_set_style_bg_color(btn, UI_ACCENT_COLOR, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(btn, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, CONFIRM_CANCEL_BTN_OPA, LV_PART_MAIN);
    }
    lv_obj_set_style_radius(btn, UI_SINGLE_SELECTOR_SEGMENT_RADIUS, LV_PART_MAIN);
    lv_obj_set_height(btn, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(btn, CONFIRM_BTN_MIN_HEIGHT, LV_PART_MAIN);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_style_pad_ver(btn, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(btn, 16, LV_PART_MAIN);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(btn, click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lab = lv_label_create(btn);
    lv_label_set_text(lab, text);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(lab, accent ? UI_ACCENT_TEXT_COLOR : UI_BOX_1_LABEL_COLOR, LV_PART_MAIN);
    return btn;
}

void ui_confirm_dialog_open(const char *title, const char *message, const char *confirm_text,
                            ui_confirm_dialog_cb_t confirm_cb, void *user_data)
{
    ui_confirm_dialog_close();

    lv_display_t *disp = lv_display_get_default();
    if (disp == NULL) {
        return;
    }
    const int32_t dw = lv_display_get_horizontal_resolution(disp);
    const int32_t dh = lv_display_get_vertical_resolution(disp);

    s_confirm_cb = confirm_cb;
    s_confirm_user_data = user_data;

    lv_obj_t *ov = lv_obj_create(lv_layer_top());
    s_dialog = ov;
    lv_obj_remove_style_all(ov);
    lv_obj_set_size(ov, (lv_coord_t)dw, (lv_coord_t)dh);
    lv_obj_set_style_bg_color(ov, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ov, CONFIRM_BACKDROP_OPA, LV_PART_MAIN);
    lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ov, confirm_bg_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ov, confirm_on_deleted, LV_EVENT_DELETE, NULL);

    lv_obj_t *panel = lv_obj_create(ov);
    lv_obj_remove_style_all(panel);
    ui_box_1_style_apply(panel);
    lv_obj_set_style_bg_opa(panel, CONFIRM_PANEL_OPA, LV_PART_MAIN);
    lv_obj_set_width(panel, (lv_coord_t)LV_MIN(dw - 48, 420));
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    /* LVGL 9: no LV_FLEX_ALIGN_STRETCH; children use lv_pct(100) width. */
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(panel, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_row(panel, 12, LV_PART_MAIN);

    lv_obj_t *title_lab = lv_label_create(panel);
    lv_label_set_text(title_lab, (title != NULL) ? title : "Are you sure?");
    lv_label_set_long_mode(title_lab, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(title_lab, lv_pct(100));
    lv_obj_set_style_text_font(title_lab, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_lab, UI_BOX_1_LABEL_COLOR, LV_PART_MAIN);

    if (message != NULL && message[0] != '\0') {
        lv_obj_t *msg_lab = lv_label_create(panel);
        lv_label_set_text(msg_lab, message);
        lv_label_set_long_mode(msg_lab, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(msg_lab, lv_pct(100));
        lv_obj_set_style_text_font(msg_lab, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(msg_lab, UI_BOX_1_LABEL_COLOR, LV_PART_MAIN);
    }

    lv_obj_t *row = lv_obj_create(panel);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 12, LV_PART_MAIN);

    (void)add_choice_button(row, "Cancel", false, confirm_cancel_clicked);
    (void)add_choice_button(row, (confirm_text != NULL && confirm_text[0] != '\0') ? confirm_text : "Confirm", true,
                            confirm_accept_clicked);
}
