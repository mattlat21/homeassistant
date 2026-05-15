#include "ui/screens/screen_front_gate.h"

#include "reolink_preview.h"
#include "ui/components/ui_gate_action.h"
#include "ui/ui_brand_gradient.h"
#include "bsp/display.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include <string.h>

#define FRONT_GATE_CORNER_RADIUS 20
#define FRONT_GATE_STATUS_LABEL_TEXT_OPA LV_OPA_90

static lv_obj_t *s_gate_state_pill;
static lv_obj_t *s_gate_state_label;
static lv_obj_t *s_gate_action_btn;
static lv_obj_t *s_gate_action_label;

static void front_gate_action_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_gate_action_on_click();
}

lv_obj_t *screen_front_gate_create(lv_display_t *disp)
{
    (void)disp;
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    ui_brand_gradient_apply(scr);

    const int32_t box_w = 720;
    const int32_t box_h = 480;

    lv_obj_t *box = lv_obj_create(scr);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, box_w, box_h);
    lv_obj_align(box, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(box, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(box, lv_color_hex(0x8E8E93), LV_PART_MAIN);
    lv_obj_set_style_radius(box, 40, LV_PART_MAIN);
    lv_obj_set_style_pad_all(box, 0, LV_PART_MAIN);

    const size_t buf_sz = (size_t)REOLINK_PREVIEW_W * REOLINK_PREVIEW_H * 3;
    uint8_t *canvas_buf =
        (uint8_t *)heap_caps_malloc(buf_sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (canvas_buf == NULL) {
        canvas_buf = (uint8_t *)heap_caps_malloc(buf_sz, MALLOC_CAP_8BIT);
    }

    lv_obj_t *canvas = NULL;
    if (canvas_buf != NULL) {
        canvas = lv_canvas_create(box);
        memset(canvas_buf, 0x55, buf_sz);
        lv_canvas_set_buffer(canvas, canvas_buf, REOLINK_PREVIEW_W, REOLINK_PREVIEW_H, LV_COLOR_FORMAT_RGB888);
        lv_obj_center(canvas);
    } else {
        lv_obj_t *lbl = lv_label_create(box);
        lv_label_set_text(lbl, "No RAM for preview");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_center(lbl);
    }

    s_gate_state_pill = lv_obj_create(box);
    lv_obj_remove_style_all(s_gate_state_pill);
    lv_obj_set_size(s_gate_state_pill, 320, 72);
    lv_obj_set_style_bg_color(s_gate_state_pill, lv_color_hex(0x1E88E5), LV_PART_MAIN);
    lv_obj_set_style_radius(s_gate_state_pill, FRONT_GATE_CORNER_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_gate_state_pill, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_gate_state_pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_gate_state_pill, LV_ALIGN_BOTTOM_LEFT, 20, -20);

    s_gate_state_label = lv_label_create(s_gate_state_pill);
    lv_label_set_text(s_gate_state_label, "Closed");
    lv_obj_set_style_text_font(s_gate_state_label, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_gate_state_label, FRONT_GATE_STATUS_LABEL_TEXT_OPA, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_gate_state_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(s_gate_state_label);

    s_gate_action_btn = lv_button_create(scr);
    lv_obj_remove_style_all(s_gate_action_btn);
    lv_obj_set_size(s_gate_action_btn, 635, 185);
    lv_obj_align_to(s_gate_action_btn, box, LV_ALIGN_OUT_BOTTOM_MID, 0, 28);
    lv_obj_set_style_radius(s_gate_action_btn, FRONT_GATE_CORNER_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_gate_action_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_gate_action_btn, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_gate_action_btn, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(s_gate_action_btn, LV_OPA_60, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_add_event_cb(s_gate_action_btn, front_gate_action_clicked, LV_EVENT_CLICKED, NULL);

    s_gate_action_label = lv_label_create(s_gate_action_btn);
    lv_obj_set_style_text_font(s_gate_action_label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_gate_action_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_gate_action_label, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_center(s_gate_action_label);

    ui_gate_action_bind(s_gate_action_btn, s_gate_action_label);
    ui_gate_action_set_status_pill(s_gate_state_pill, s_gate_state_label);

    reolink_preview_bind(scr, canvas);

    return scr;
}
