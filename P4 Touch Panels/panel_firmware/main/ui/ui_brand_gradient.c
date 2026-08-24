#include "ui/ui_brand_gradient.h"

#include "bsp/display.h"
#include "ui/components/ui_taskbar.h"
#include "ui/ui_colors.h"

static lv_grad_dsc_t s_brand_scr_grad;
static bool s_brand_scr_grad_inited;

static void ui_brand_gradient_init_once(void)
{
    if (s_brand_scr_grad_inited) {
        return;
    }
    const lv_color_t colors[] = { UI_COLOR_BRAND_VIOLET, UI_COLOR_BRAND_MAGENTA };
    lv_grad_init_stops(&s_brand_scr_grad, colors, NULL, NULL, 2);
    lv_grad_linear_init(&s_brand_scr_grad, 0, 0, lv_pct(100), lv_pct(100), LV_GRAD_EXTEND_PAD);
    s_brand_scr_grad_inited = true;
}

void ui_brand_gradient_apply(lv_obj_t *obj)
{
    if (obj == NULL) {
        return;
    }
    ui_brand_gradient_init_once();
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_grad(obj, &s_brand_scr_grad, LV_PART_MAIN);
}

lv_obj_t *ui_brand_framed_panel_create(lv_obj_t *scr)
{
    if (scr == NULL) {
        return NULL;
    }

    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    const int32_t bg_w = BSP_LCD_H_RES - (2 * UI_BRAND_FRAME_MARGIN_PX);
    const int32_t bg_h = BSP_LCD_V_RES - (2 * UI_BRAND_FRAME_MARGIN_PX);
    lv_obj_t *bg = lv_obj_create(scr);
    lv_obj_remove_style_all(bg);
    lv_obj_set_size(bg, bg_w, bg_h);
    lv_obj_align(bg, LV_ALIGN_TOP_LEFT, UI_BRAND_FRAME_MARGIN_PX, UI_BRAND_FRAME_MARGIN_PX);
    ui_brand_gradient_apply(bg);
    lv_obj_set_style_radius(bg, UI_TASKBAR_DOCK_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(bg, true, LV_PART_MAIN);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_CLICKABLE);
    return bg;
}
