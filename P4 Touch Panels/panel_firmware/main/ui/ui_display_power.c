#include "ui/ui_display_power.h"

#include "app_prefs.h"
#include "bsp/esp32_p4_wifi6_touch_lcd_4b.h"

#define FADE_TIMER_PERIOD_MS 50U

typedef enum {
    DISP_PWR_NORMAL,
    DISP_PWR_DIM,
    DISP_PWR_OFF,
} disp_pwr_state_t;

static lv_display_t *s_disp;
static lv_timer_t *s_timer;
static lv_timer_t *s_fade_timer;
static disp_pwr_state_t s_state = DISP_PWR_NORMAL;
static uint8_t s_normal_pct;
static uint8_t s_dim_pct;
static uint32_t s_dim_sec;
static uint32_t s_off_sec;
static uint32_t s_fade_sec;
static uint8_t s_current_pct;
static uint8_t s_target_pct;
static uint8_t s_fade_from_pct;
static uint16_t s_fade_steps_total;
static uint16_t s_fade_step;
static bool s_fading;

static void apply_brightness_immediate(uint8_t pct)
{
    (void)bsp_display_brightness_set(pct);
    s_current_pct = pct;
}

static uint8_t brightness_for_state(disp_pwr_state_t state)
{
    switch (state) {
    case DISP_PWR_OFF:
        return 0;
    case DISP_PWR_DIM:
        return s_dim_pct;
    default:
        return s_normal_pct;
    }
}

static void fade_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_fading) {
        return;
    }
    s_fade_step++;
    if (s_fade_step >= s_fade_steps_total) {
        apply_brightness_immediate(s_target_pct);
        s_fading = false;
        lv_timer_pause(s_fade_timer);
        return;
    }
    int32_t delta = (int32_t)s_target_pct - (int32_t)s_fade_from_pct;
    int32_t pct = (int32_t)s_fade_from_pct + (delta * (int32_t)s_fade_step) / (int32_t)s_fade_steps_total;
    if (pct < 0) {
        pct = 0;
    } else if (pct > 100) {
        pct = 100;
    }
    apply_brightness_immediate((uint8_t)pct);
}

static void set_brightness_target(uint8_t target)
{
    if (s_fading && target == s_target_pct) {
        return;
    }
    if (!s_fading && target == s_current_pct) {
        return;
    }
    if (s_fade_sec == 0U) {
        s_fading = false;
        if (s_fade_timer != NULL) {
            lv_timer_pause(s_fade_timer);
        }
        apply_brightness_immediate(target);
        s_target_pct = target;
        return;
    }

    s_fade_from_pct = s_current_pct;
    s_target_pct = target;
    s_fade_steps_total = (uint16_t)((s_fade_sec * 1000U + FADE_TIMER_PERIOD_MS - 1U) / FADE_TIMER_PERIOD_MS);
    if (s_fade_steps_total == 0U) {
        s_fade_steps_total = 1U;
    }
    s_fade_step = 0U;
    s_fading = true;
    if (s_fade_timer != NULL) {
        lv_timer_reset(s_fade_timer);
        lv_timer_resume(s_fade_timer);
    } else {
        apply_brightness_immediate(target);
    }
}

static void refresh_hardware_brightness(void)
{
    set_brightness_target(brightness_for_state(s_state));
}

static disp_pwr_state_t desired_state(uint32_t inactive_ms)
{
    if (s_off_sec > 0U && inactive_ms >= s_off_sec * 1000U) {
        return DISP_PWR_OFF;
    }
    if (s_dim_sec > 0U && s_dim_pct != s_normal_pct && inactive_ms >= s_dim_sec * 1000U) {
        return DISP_PWR_DIM;
    }
    return DISP_PWR_NORMAL;
}

static void apply_state(disp_pwr_state_t want)
{
    if (want == s_state) {
        return;
    }
    s_state = want;
    refresh_hardware_brightness();
}

static void sync_state_from_inactivity(void)
{
    if (s_disp == NULL) {
        return;
    }
    apply_state(desired_state(lv_display_get_inactive_time(s_disp)));
}

static void display_power_timer_cb(lv_timer_t *t)
{
    (void)t;
    sync_state_from_inactivity();
}

void ui_display_power_configure(uint8_t normal_pct, uint8_t dim_pct, uint32_t dim_sec, uint32_t off_sec,
                                uint32_t fade_sec)
{
    s_normal_pct = normal_pct > 100U ? 100U : normal_pct;
    s_dim_pct = dim_pct > 100U ? 100U : dim_pct;
    s_dim_sec = dim_sec;
    s_off_sec = off_sec;
    s_fade_sec = fade_sec;
    sync_state_from_inactivity();
    refresh_hardware_brightness();
}

void ui_display_power_wake(void)
{
    if (s_disp != NULL) {
        lv_display_trigger_activity(s_disp);
    }
    s_state = DISP_PWR_NORMAL;
    refresh_hardware_brightness();
}

void ui_display_power_init(lv_display_t *disp)
{
    s_disp = disp;
    app_prefs_get_display_power(&s_normal_pct, &s_dim_pct, &s_dim_sec, &s_off_sec, &s_fade_sec);
    apply_brightness_immediate(s_normal_pct);
    s_target_pct = s_normal_pct;
    s_state = DISP_PWR_NORMAL;
    lv_display_trigger_activity(disp);
    s_timer = lv_timer_create(display_power_timer_cb, 500, NULL);
    if (s_timer != NULL) {
        lv_timer_set_repeat_count(s_timer, -1);
    }
    s_fade_timer = lv_timer_create(fade_timer_cb, FADE_TIMER_PERIOD_MS, NULL);
    if (s_fade_timer != NULL) {
        lv_timer_set_repeat_count(s_fade_timer, -1);
        lv_timer_pause(s_fade_timer);
    }
}
