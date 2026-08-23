#include "ui/screens/screen_front_gate.h"

#include "reolink_preview.h"
#include "sdkconfig.h"
#include "ui/components/ui_gate_action.h"
#include "ui/components/ui_taskbar.h"
#include "bsp/display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdint.h>
#include <string.h>

#define FRONT_GATE_CORNER_RADIUS 20
#define FRONT_GATE_STATUS_LABEL_TEXT_OPA LV_OPA_90
/** Gap between the camera preview and the top/left/right screen edges. */
#define FRONT_GATE_VIDEO_MARGIN 3
/** Vertical gap separating the action button from the preview above and the dock below. */
#define FRONT_GATE_ACTION_MARGIN 12
/** Match taskbar dock width (side margins from @ref UI_TASKBAR_SIDE_MARGIN). */
#define FRONT_GATE_ACTION_WIDTH (BSP_LCD_H_RES - (2 * UI_TASKBAR_SIDE_MARGIN))
/** Apparent corner radius of the camera preview. */
#define FRONT_GATE_VIDEO_RADIUS 20
/**
 * Outer radius of a corner wedge. Must exceed FRONT_GATE_VIDEO_RADIUS * sqrt(2) so the wedge reaches
 * the square corner of the canvas; the overhang past the video box is clipped away.
 */
#define FRONT_GATE_CORNER_WEDGE_OUTER (FRONT_GATE_VIDEO_RADIUS * 2)

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

/**
 * A canvas cannot clip itself to a rounded parent, so each corner is covered by a black
 * quarter-annulus: opaque from @ref FRONT_GATE_VIDEO_RADIUS out past the square corner, leaving the
 * rounded region showing. Reads as a rounded corner because the screen behind is also black.
 */
static void front_gate_add_corner_wedge(lv_obj_t *parent, lv_align_t align, int32_t x_ofs, int32_t y_ofs,
                                        int32_t start_angle)
{
    lv_obj_t *wedge = lv_arc_create(parent);
    lv_obj_remove_style_all(wedge);
    lv_obj_remove_flag(wedge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(wedge, FRONT_GATE_CORNER_WEDGE_OUTER * 2, FRONT_GATE_CORNER_WEDGE_OUTER * 2);
    lv_obj_align(wedge, align, x_ofs, y_ofs);
    lv_arc_set_bg_angles(wedge, start_angle, start_angle + 90);
    lv_obj_set_style_arc_color(wedge, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(wedge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(wedge, FRONT_GATE_CORNER_WEDGE_OUTER - FRONT_GATE_VIDEO_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(wedge, false, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(wedge, LV_OPA_TRANSP, LV_PART_INDICATOR);
}

lv_obj_t *screen_front_gate_create(lv_display_t *disp)
{
    (void)disp;
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    /* The canvas is a fixed-size buffer, so the margins come from a smaller box cropping it. */
    const int32_t box_w = REOLINK_PREVIEW_W - (FRONT_GATE_VIDEO_MARGIN * 2);
    const int32_t box_h = REOLINK_PREVIEW_H;

    lv_obj_t *box = lv_obj_create(scr);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, box_w, box_h);
    lv_obj_align(box, LV_ALIGN_TOP_MID, 0, FRONT_GATE_VIDEO_MARGIN);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(box, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(box, lv_color_hex(0x8E8E93), LV_PART_MAIN);
    lv_obj_set_style_radius(box, FRONT_GATE_VIDEO_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_pad_all(box, 0, LV_PART_MAIN);

    lv_obj_t *canvas = NULL;
    const bool reolink_enabled =
#ifdef CONFIG_ESP_HMI_REOLINK_HOST
        CONFIG_ESP_HMI_REOLINK_HOST[0] != '\0';
#else
        false;
#endif
    if (reolink_enabled) {
    const size_t buf_sz = (size_t)REOLINK_PREVIEW_W * REOLINK_PREVIEW_H * 3;
    uint8_t *canvas_buf =
        (uint8_t *)heap_caps_malloc(buf_sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (canvas_buf == NULL) {
        canvas_buf = (uint8_t *)heap_caps_malloc(buf_sz, MALLOC_CAP_8BIT);
    }
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
        ESP_LOGW("screen_front_gate", "canvas buffer alloc failed");
    }
    } else {
        lv_obj_t *preview_lbl = lv_label_create(box);
        lv_label_set_text(preview_lbl, "Camera preview disabled");
        lv_obj_set_style_text_font(preview_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_center(preview_lbl);
    }

    /* Created after the canvas so the wedges paint over it. */
    const int32_t wedge_ofs = FRONT_GATE_CORNER_WEDGE_OUTER - FRONT_GATE_VIDEO_RADIUS;
    front_gate_add_corner_wedge(box, LV_ALIGN_TOP_LEFT, -wedge_ofs, -wedge_ofs, 180);
    front_gate_add_corner_wedge(box, LV_ALIGN_TOP_RIGHT, wedge_ofs, -wedge_ofs, 270);
    front_gate_add_corner_wedge(box, LV_ALIGN_BOTTOM_RIGHT, wedge_ofs, wedge_ofs, 0);
    front_gate_add_corner_wedge(box, LV_ALIGN_BOTTOM_LEFT, -wedge_ofs, wedge_ofs, 90);

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

    /* Fill whatever is left between the preview and the dock. */
    const int32_t action_y = FRONT_GATE_VIDEO_MARGIN + box_h + FRONT_GATE_ACTION_MARGIN;
    const int32_t action_h = BSP_LCD_V_RES - UI_TASKBAR_HEIGHT - FRONT_GATE_ACTION_MARGIN - action_y;
    s_gate_action_btn = lv_button_create(scr);
    lv_obj_remove_style_all(s_gate_action_btn);
    lv_obj_set_size(s_gate_action_btn, FRONT_GATE_ACTION_WIDTH, action_h);
    lv_obj_align(s_gate_action_btn, LV_ALIGN_TOP_MID, 0, action_y);
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

    const reolink_cam_config_t cam = {
#ifdef CONFIG_ESP_HMI_REOLINK_HOST
        .host = CONFIG_ESP_HMI_REOLINK_HOST,
#else
        .host = "",
#endif
        .port = CONFIG_ESP_HMI_REOLINK_PORT,
#ifdef CONFIG_ESP_HMI_REOLINK_USE_HTTPS
        .use_https = true,
#else
        .use_https = false,
#endif
        .user = CONFIG_ESP_HMI_REOLINK_USER,
        .password = CONFIG_ESP_HMI_REOLINK_PASSWORD,
        .channel = CONFIG_ESP_HMI_REOLINK_CHANNEL,
    };
    reolink_preview_bind(scr, canvas, &cam);

    (void)ui_taskbar_attach_standard(scr);

    return scr;
}
