#include <stdint.h>
#include "ui/screens/screen_loading.h"
#include "app_prefs.h"
#include "ui/nav.h"
#include "bsp/display.h"

#if LV_USE_LOTTIE
/* EMBED_FILES "assets/lottie_startup.json" → linker symbols use the file basename */
extern const uint8_t _binary_lottie_startup_json_start[];
extern const uint8_t _binary_lottie_startup_json_end[];
#endif

static bool s_loading_nav_done = false;

static void loading_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_loading_nav_done) {
        return;
    }
    s_loading_nav_done = true;
    lv_obj_t *old_scr = lv_screen_active();
    nav_go_to(app_prefs_get_default_app());
    lv_obj_t *new_scr = lv_screen_active();
    if (old_scr != NULL && old_scr != new_scr) {
        lv_obj_delete_async(old_scr);
    }
}

void screen_loading_begin(lv_obj_t *screen)
{
    (void)screen;
    s_loading_nav_done = false;
    lv_timer_t *t = lv_timer_create(loading_timer_cb, UI_LOADING_DURATION_MS, NULL);
    lv_timer_set_repeat_count(t, 1);
}

lv_obj_t *screen_loading_create(lv_display_t *disp)
{
    (void)disp;
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *lottie = NULL;
#if LV_USE_LOTTIE
    lottie = lv_lottie_create(scr);
    /* Keep source animation unchanged; reduce raster workload on this target. */
    static uint8_t s_lottie_px[96 * 96 * 4];
    lv_lottie_set_buffer(lottie, 96, 96, s_lottie_px);
    const size_t json_sz =
        (size_t)(_binary_lottie_startup_json_end - _binary_lottie_startup_json_start);
    lv_lottie_set_src_data(lottie, _binary_lottie_startup_json_start, json_sz);
    lv_anim_t *lottie_anim = lv_lottie_get_anim(lottie);
    if (lottie_anim != NULL) {
        /* Keep the same asset; slow frame advancement to avoid long continuous lock hold on heavy vectors. */
        lv_anim_set_duration(lottie_anim, 1500);
        lv_anim_set_repeat_count(lottie_anim, 1);
    }
    lv_obj_center(lottie);
#endif

    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "Made by Matt");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x3C3C43), LV_PART_MAIN);
#if LV_USE_LOTTIE
    if (lottie != NULL) {
        lv_obj_align_to(lbl, lottie, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    } else {
        lv_obj_center(lbl);
    }
#else
    lv_obj_center(lbl);
#endif

    return scr;
}
