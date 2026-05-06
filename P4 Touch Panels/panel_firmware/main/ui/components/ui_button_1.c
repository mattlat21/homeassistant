#include "ui/components/ui_button_1.h"

#include "ui/components/ui_box_1.h"
#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/ui_visual_tokens.h"

typedef struct {
    ui_button_1_cb_t click_cb;
    void *click_user_data;
} button_meta_t;

static void button_meta_free(lv_event_t *e)
{
    button_meta_t *meta = lv_event_get_user_data(e);
    if (meta != NULL) {
        lv_free(meta);
    }
}

static void button_click(lv_event_t *e)
{
    button_meta_t *meta = lv_event_get_user_data(e);
    if (meta != NULL && meta->click_cb != NULL) {
        meta->click_cb(meta->click_user_data);
    }
}

static void add_centered_label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *lab = lv_label_create(parent);
    lv_label_set_text(lab, text != NULL ? text : "");
    lv_obj_set_style_text_font(lab, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(lab, color, LV_PART_MAIN);
}

/** Scale icon label to ~80% of button content (smaller side), preserving aspect; pivot at label center. */
static void apply_icon_only_scale(lv_obj_t *btn, lv_obj_t *icon)
{
    if (btn == NULL || icon == NULL) {
        return;
    }
    const lv_coord_t cw = lv_obj_get_content_width(btn);
    const lv_coord_t ch = lv_obj_get_content_height(btn);
    if (cw < 8 || ch < 8) {
        return;
    }
    const int32_t target = (int32_t)LV_MIN(cw, ch) * 80 / 100;
    lv_obj_update_layout(icon);
    const lv_coord_t iw = lv_obj_get_width(icon);
    const lv_coord_t ih = lv_obj_get_height(icon);
    if (iw < 1 || ih < 1) {
        return;
    }
    /* LV_SCALE_NONE (256) = 100%; scale so both dimensions fit in `target`. */
    int32_t sc = (target * 256) / (int32_t)LV_MAX(iw, ih);
    if (sc < 48) {
        sc = 48;
    }
    if (sc > 2048) {
        sc = 2048;
    }
    /* Never upscale bitmap font past native size (avoids blocky glyphs). */
    if (sc > 256) {
        sc = 256;
    }
    lv_obj_set_style_transform_pivot_x(icon, iw / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(icon, ih / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_x(icon, sc, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(icon, sc, LV_PART_MAIN);
    /* Font line box is taller than the glyph; shift down so the icon looks vertically centered in the button. */
    lv_obj_set_style_translate_y(icon, UI_BUTTON_1_ICON_ONLY_TRANSLATE_Y, LV_PART_MAIN);
}

static void icon_only_btn_size_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SIZE_CHANGED) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *icon = lv_event_get_user_data(e);
    apply_icon_only_scale(btn, icon);
}

lv_obj_t *ui_button_1_create(lv_obj_t *parent, uint8_t row, uint8_t col, uint8_t row_span, uint8_t col_span,
                             const char *icon_utf8, const char *name, const lv_color_t *icon_color,
                             const lv_color_t *text_color, ui_button_1_cb_t click_cb, void *click_user_data)
{
    const bool has_icon = (icon_utf8 != NULL && icon_utf8[0] != '\0');
    const bool has_name = (name != NULL && name[0] != '\0');
    const lv_color_t icol = (icon_color != NULL) ? *icon_color : UI_BOX_1_LABEL_COLOR;
    const lv_color_t tcol = (text_color != NULL) ? *text_color : UI_BOX_1_LABEL_COLOR;

    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_remove_style_all(btn);
    ui_box_1_style_apply(btn);
    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, col_span, LV_GRID_ALIGN_STRETCH, row, row_span);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(btn, 8, LV_PART_MAIN);

    lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (has_icon && has_name) {
        lv_obj_set_style_pad_column(btn, 6, LV_PART_MAIN);
    }

    if (has_icon && has_name) {
        add_centered_label(btn, icon_utf8, &ui_font_home_assistant_icons_56, icol);
        add_centered_label(btn, name, &lv_font_montserrat_16, tcol);
    } else if (has_icon) {
        lv_obj_t *icon_lab = lv_label_create(btn);
        lv_label_set_text(icon_lab, icon_utf8);
        lv_obj_set_style_text_font(icon_lab, &ui_font_home_assistant_icons_56, LV_PART_MAIN);
        lv_obj_set_style_text_color(icon_lab, icol, LV_PART_MAIN);
        lv_obj_add_event_cb(btn, icon_only_btn_size_cb, LV_EVENT_SIZE_CHANGED, icon_lab);
        lv_obj_update_layout(btn);
        apply_icon_only_scale(btn, icon_lab);
    } else if (has_name) {
        add_centered_label(btn, name, &lv_font_montserrat_16, tcol);
    }

    button_meta_t *meta = lv_malloc(sizeof(button_meta_t));
    if (meta != NULL) {
        meta->click_cb = click_cb;
        meta->click_user_data = click_user_data;
        lv_obj_add_event_cb(btn, button_click, LV_EVENT_CLICKED, meta);
        lv_obj_add_event_cb(btn, button_meta_free, LV_EVENT_DELETE, meta);
    }

    return btn;
}
