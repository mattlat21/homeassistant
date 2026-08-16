#include "ui/screens/screen_penny.h"

#include <stdint.h>
#include <string.h>

#include "bsp/display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "ha_mqtt.h"
#include "lvgl.h"
#include "reolink_audio.h"
#include "reolink_preview.h"
#include "sdkconfig.h"
#include "ui/components/ui_button_1.h"
#include "ui/components/ui_confirm_dialog.h"
#include "ui/components/ui_ollie_fan_modal.h"
#include "ui/components/ui_taskbar.h"
#include "ui/fonts/ui_home_assistant_icon_glyphs.h"
#include "ui/nav.h"

/** Gap between the camera preview and the top/left/right screen edges. */
#define PENNY_VIDEO_MARGIN 3
/** Apparent corner radius of the camera preview. */
#define PENNY_VIDEO_RADIUS 20
/** Vertical gap separating the light/fan row from the preview above and the dock below. */
#define PENNY_ACTION_ROW_MARGIN 12
/** Gap between the light and fan buttons. */
#define PENNY_ACTION_ROW_GAP 16
/**
 * Outer radius of a corner wedge. Must exceed PENNY_VIDEO_RADIUS * sqrt(2) so the wedge reaches the
 * square corner of the canvas; the overhang past the video box is clipped away.
 */
#define PENNY_CORNER_WEDGE_OUTER (PENNY_VIDEO_RADIUS * 2)

/**
 * A canvas cannot clip itself to a rounded parent, so each corner is covered by a black
 * quarter-annulus: opaque from @ref PENNY_VIDEO_RADIUS out past the square corner, leaving the
 * rounded region showing. Reads as a rounded corner because the screen behind is also black.
 */
static void penny_add_corner_wedge(lv_obj_t *parent, lv_align_t align, int32_t x_ofs, int32_t y_ofs,
                                   int32_t start_angle)
{
    lv_obj_t *wedge = lv_arc_create(parent);
    lv_obj_remove_style_all(wedge);
    lv_obj_remove_flag(wedge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(wedge, PENNY_CORNER_WEDGE_OUTER * 2, PENNY_CORNER_WEDGE_OUTER * 2);
    lv_obj_align(wedge, align, x_ofs, y_ofs);
    lv_arc_set_bg_angles(wedge, start_angle, start_angle + 90);
    lv_obj_set_style_arc_color(wedge, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(wedge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(wedge, PENNY_CORNER_WEDGE_OUTER - PENNY_VIDEO_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(wedge, false, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(wedge, LV_OPA_TRANSP, LV_PART_INDICATOR);
}

static void penny_taskbar_nav(void *user_data)
{
    nav_go_to((app_id_t)(uintptr_t)user_data);
}

static void penny_light_confirmed(void *user_data)
{
    (void)user_data;
    (void)ha_mqtt_publish_ollie_button("light");
}

/** Toggling the light is disruptive if Ollie is asleep, so it is gated behind a confirmation. */
static void penny_light_clicked(void *user_data)
{
    (void)user_data;
    ui_confirm_dialog_open("Toggle the light?", "This will switch Ollie's bedroom light.", "Toggle",
                           penny_light_confirmed, NULL);
}

static void penny_fan_clicked(void *user_data)
{
    (void)user_data;
    ui_ollie_fan_modal_open();
}

/** Grid descriptors outlive this call: LVGL keeps the pointers for later layout passes. */
static lv_coord_t s_action_col_dsc[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
static lv_coord_t s_action_row_dsc[] = { LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };

static void penny_create_action_row(lv_obj_t *scr, int32_t width, int32_t y, int32_t height)
{
    lv_obj_t *actions = lv_obj_create(scr);
    lv_obj_remove_style_all(actions);
    lv_obj_set_size(actions, width, height);
    lv_obj_align(actions, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(actions, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(actions, s_action_col_dsc, s_action_row_dsc);
    lv_obj_set_style_pad_column(actions, PENNY_ACTION_ROW_GAP, LV_PART_MAIN);

    lv_color_t fg = lv_color_white();
    const lv_color_t *fg_p = &fg;
    (void)ui_button_1_create(actions, 0, 0, 1, 1, UI_HA_ICON_LIGHTBULB, NULL, fg_p, fg_p, penny_light_clicked, NULL);
    (void)ui_button_1_create(actions, 0, 1, 1, 1, UI_HA_ICON_FAN, NULL, fg_p, fg_p, penny_fan_clicked, NULL);
}

static void penny_screen_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SCREEN_LOADED) {
        const reolink_audio_config_t aud = {
#ifdef CONFIG_ESP_HMI_REOLINK_PENNY_HOST
            .host = CONFIG_ESP_HMI_REOLINK_PENNY_HOST,
#else
            .host = "",
#endif
            .user = CONFIG_ESP_HMI_REOLINK_USER,
            .password = CONFIG_ESP_HMI_REOLINK_PASSWORD,
        };
        reolink_audio_start(&aud);
    } else if (code == LV_EVENT_SCREEN_UNLOADED) {
        reolink_audio_stop();
        /* Both live on the top layer, so they would otherwise stay up over the next screen. */
        ui_confirm_dialog_close();
        ui_ollie_fan_modal_close();
    }
}

lv_obj_t *screen_penny_create(lv_display_t *disp)
{
    (void)disp;
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    reolink_audio_init();

    /* The canvas is a fixed-size buffer, so the margins come from a smaller box cropping it. */
    const int32_t box_w = REOLINK_PREVIEW_W - (PENNY_VIDEO_MARGIN * 2);
    const int32_t box_h = REOLINK_PREVIEW_H;

    lv_obj_t *box = lv_obj_create(scr);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, box_w, box_h);
    lv_obj_align(box, LV_ALIGN_TOP_MID, 0, PENNY_VIDEO_MARGIN);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(box, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(box, lv_color_hex(0x8E8E93), LV_PART_MAIN);
    lv_obj_set_style_radius(box, PENNY_VIDEO_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_pad_all(box, 0, LV_PART_MAIN);

    lv_obj_t *canvas = NULL;
    const bool reolink_enabled =
#ifdef CONFIG_ESP_HMI_REOLINK_PENNY_HOST
        CONFIG_ESP_HMI_REOLINK_PENNY_HOST[0] != '\0';
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
            ESP_LOGW("screen_penny", "canvas buffer alloc failed");
        }
    } else {
        lv_obj_t *preview_lbl = lv_label_create(box);
        lv_label_set_text(preview_lbl, "Camera preview disabled");
        lv_obj_set_style_text_font(preview_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_center(preview_lbl);
    }

    /* Created after the canvas so the wedges paint over it. */
    const int32_t wedge_ofs = PENNY_CORNER_WEDGE_OUTER - PENNY_VIDEO_RADIUS;
    penny_add_corner_wedge(box, LV_ALIGN_TOP_LEFT, -wedge_ofs, -wedge_ofs, 180);
    penny_add_corner_wedge(box, LV_ALIGN_TOP_RIGHT, wedge_ofs, -wedge_ofs, 270);
    penny_add_corner_wedge(box, LV_ALIGN_BOTTOM_RIGHT, wedge_ofs, wedge_ofs, 0);
    penny_add_corner_wedge(box, LV_ALIGN_BOTTOM_LEFT, -wedge_ofs, wedge_ofs, 90);

    const reolink_cam_config_t cam = {
#ifdef CONFIG_ESP_HMI_REOLINK_PENNY_HOST
        .host = CONFIG_ESP_HMI_REOLINK_PENNY_HOST,
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

    const int32_t action_y = PENNY_VIDEO_MARGIN + box_h + PENNY_ACTION_ROW_MARGIN;
    const int32_t action_h = BSP_LCD_V_RES - UI_TASKBAR_HEIGHT - PENNY_ACTION_ROW_MARGIN - action_y;
    if (action_h > 0) {
        penny_create_action_row(scr, box_w, action_y, action_h);
    }

    const ui_taskbar_item_t taskbar_items[] = {
        { LV_SYMBOL_HOME, &lv_font_montserrat_48, NULL, penny_taskbar_nav, (void *)(uintptr_t)APP_HOME },
        { UI_HA_ICON_TEDDY_BEAR, NULL, NULL, penny_taskbar_nav, (void *)(uintptr_t)APP_PENNY_ROOM },
        { UI_HA_ICON_GATE, NULL, NULL, penny_taskbar_nav, (void *)(uintptr_t)APP_FRONT_GATE },
        { UI_HA_ICON_THERMOMETER, NULL, NULL, penny_taskbar_nav, (void *)(uintptr_t)APP_HVAC },
        { LV_SYMBOL_BATTERY_FULL, &lv_font_montserrat_48, NULL, penny_taskbar_nav,
          (void *)(uintptr_t)APP_HOUSE_BATTERY },
        { LV_SYMBOL_SETTINGS, &lv_font_montserrat_48, NULL, penny_taskbar_nav, (void *)(uintptr_t)APP_SETTINGS },
    };
    (void)ui_taskbar_create(scr, taskbar_items, (uint8_t)(sizeof(taskbar_items) / sizeof(taskbar_items[0])));

    lv_obj_add_event_cb(scr, penny_screen_event, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(scr, penny_screen_event, LV_EVENT_SCREEN_UNLOADED, NULL);

    return scr;
}
