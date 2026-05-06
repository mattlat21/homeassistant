#include "ui/screens/screen_ota_progress.h"

#include <stdint.h>
#include <stdlib.h>

#include "bsp/display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "ui/nav.h"
#include "sdkconfig.h"

#if CONFIG_SCREEN_TEST_OTA_ENABLE

static lv_obj_t *s_scr;
static lv_obj_t *s_bar;
static int32_t s_last_sent_pct = -1;

typedef struct {
    SemaphoreHandle_t done;
} show_async_msg_t;

static void show_cb(void *ud)
{
    show_async_msg_t *m = (show_async_msg_t *)ud;
    s_last_sent_pct = -1;
    if (s_scr != NULL) {
        lv_screen_load(s_scr);
    }
    if (s_bar != NULL) {
        lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
    }
    if (m != NULL && m->done != NULL) {
        xSemaphoreGive(m->done);
    }
}

static void set_pct_cb(void *ud)
{
    int p = (int)(intptr_t)ud;
    if (s_bar == NULL) {
        return;
    }
    if (p < 0) {
        p = 0;
    }
    if (p > 100) {
        p = 100;
    }
    lv_bar_set_value(s_bar, p, LV_ANIM_OFF);
}

static void dismiss_cb(void *ud)
{
    (void)ud;
    nav_go_home();
}

lv_obj_t *screen_ota_progress_create(lv_display_t *disp)
{
    (void)disp;
    if (s_scr != NULL) {
        return s_scr;
    }

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_obj_set_style_bg_color(scr, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *col = lv_obj_create(scr);
    lv_obj_remove_style_all(col);
    lv_obj_set_width(col, LV_PCT(72));
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 20, LV_PART_MAIN);
    lv_obj_center(col);

    s_bar = lv_bar_create(col);
    lv_bar_set_range(s_bar, 0, 100);
    lv_obj_set_width(s_bar, LV_PCT(100));
    /* Outer track height; radius = half height gives a stadium-shaped surround with rounded ends. */
    const int32_t bar_outer_h = 22;
    const int32_t bar_pad = 5;
    lv_obj_set_height(s_bar, bar_outer_h);
    lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_bar, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_border_opa(s_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_bar, bar_pad, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(s_bar, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_bar, lv_color_black(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    /* Pill-shaped fill; radius matches inner channel height so ends stay rounded with a gap from the border. */
    const int32_t inner_h = bar_outer_h - 2 * bar_pad;
    lv_obj_set_style_radius(s_bar, inner_h > 0 ? inner_h / 2 : 0, LV_PART_INDICATOR);

    lv_obj_t *lbl = lv_label_create(col);
    lv_label_set_text(lbl, "Updating...");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_black(), LV_PART_MAIN);

    s_scr = scr;
    return s_scr;
}

bool screen_ota_progress_show_and_wait_for_display(uint32_t timeout_ms)
{
    if (s_scr == NULL) {
        return false;
    }
    show_async_msg_t *m = (show_async_msg_t *)malloc(sizeof(show_async_msg_t));
    if (m == NULL) {
        return false;
    }
    m->done = xSemaphoreCreateBinary();
    if (m->done == NULL) {
        free(m);
        return false;
    }
    if (lv_async_call(show_cb, m) != LV_RESULT_OK) {
        vSemaphoreDelete(m->done);
        free(m);
        return false;
    }
    bool ok = xSemaphoreTake(m->done, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
    vSemaphoreDelete(m->done);
    free(m);
    return ok;
}

void screen_ota_progress_set_percent_async(int32_t percent_0_100)
{
    if (s_bar == NULL) {
        return;
    }
    if (percent_0_100 < 0) {
        percent_0_100 = 0;
    }
    if (percent_0_100 > 100) {
        percent_0_100 = 100;
    }
    if (percent_0_100 == s_last_sent_pct) {
        return;
    }
    s_last_sent_pct = percent_0_100;
    if (lv_async_call(set_pct_cb, (void *)(intptr_t)percent_0_100) != LV_RESULT_OK) {
        s_last_sent_pct = -1;
    }
}

void screen_ota_progress_dismiss_async(void)
{
    s_last_sent_pct = -1;
    (void)lv_async_call(dismiss_cb, NULL);
}

#else /* !CONFIG_SCREEN_TEST_OTA_ENABLE */

lv_obj_t *screen_ota_progress_create(lv_display_t *disp)
{
    (void)disp;
    return NULL;
}

bool screen_ota_progress_show_and_wait_for_display(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return false;
}

void screen_ota_progress_set_percent_async(int32_t percent_0_100)
{
    (void)percent_0_100;
}

void screen_ota_progress_dismiss_async(void)
{
}

#endif
