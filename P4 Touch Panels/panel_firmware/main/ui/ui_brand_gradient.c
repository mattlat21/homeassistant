#include "ui/ui_brand_gradient.h"

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
