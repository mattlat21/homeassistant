#include "ui/screens/screen_penny.h"

#include "reolink_preview.h"
#include "sdkconfig.h"
#include "ui/ui_brand_gradient.h"
#include "bsp/display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include <string.h>

lv_obj_t *screen_penny_create(lv_display_t *disp)
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

    return scr;
}
