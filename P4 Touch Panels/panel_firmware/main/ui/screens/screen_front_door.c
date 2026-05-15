#include "ui/screens/screen_front_door.h"

#include "ui/components/ui_button_1.h"
#include "ui/components/ui_gate_action.h"
#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/ui_brand_gradient.h"
#include "ui/ui_screen_template.h"

static lv_obj_t *s_btn_gate;

static void front_door_gate_click(void *user_data)
{
    (void)user_data;
    ui_gate_action_on_click();
}

lv_obj_t *screen_front_door_create(lv_display_t *disp)
{
    ui_screen_template_params_t params;
    ui_screen_template_params_init_defaults(&params);
    params.status_bar = true;
    params.grid_cols = 3;
    params.grid_rows = 3;
    params.pad_top = 24;
    params.pad_right = 24;
    params.pad_bottom = 24;
    params.pad_left = 24;
    params.row_gap = 16;
    params.col_gap = 16;

    ui_screen_template_result_t layout;
    if (!ui_screen_template_create(disp, &params, &layout)) {
        return NULL;
    }

    ui_brand_gradient_apply(layout.screen);

    lv_color_t fg = lv_color_white();
    const lv_color_t *fg_p = &fg;

    s_btn_gate = ui_button_1_create(layout.grid, 1, 1, 1, 1, UI_HA_ICON_GATE, "Open Gate", fg_p, fg_p,
                                    front_door_gate_click, NULL);

    if (s_btn_gate != NULL && lv_obj_get_child_cnt(s_btn_gate) >= 2) {
        lv_obj_t *name_lab = lv_obj_get_child(s_btn_gate, 1);
        ui_gate_action_bind(s_btn_gate, name_lab);
    }

    return layout.screen;
}
