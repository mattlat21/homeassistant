#include "ui/ui_display_power.h"

#include "app_prefs.h"
#include "bsp/esp32_p4_wifi6_touch_lcd_4b.h"

typedef enum {
    DISP_PWR_NORMAL,
    DISP_PWR_DIM,
    DISP_PWR_OFF,
} disp_pwr_state_t;

static lv_display_t *s_disp;
static lv_timer_t *s_timer;
static disp_pwr_state_t s_state = DISP_PWR_NORMAL;
static uint8_t s_normal_pct;
static uint8_t s_dim_pct;
static uint32_t s_dim_sec;
static uint32_t s_off_sec;

static void apply_brightness(uint8_t pct)
{
    (void)bsp_display_brightness_set(pct);
}

static void refresh_hardware_brightness(void)
{
    switch (s_state) {
    case DISP_PWR_OFF:
        apply_brightness(0);
        break;
    case DISP_PWR_DIM:
        apply_brightness(s_dim_pct);
        break;
    default:
        apply_brightness(s_normal_pct);
        break;
    }
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

void ui_display_power_configure(uint8_t normal_pct, uint8_t dim_pct, uint32_t dim_sec, uint32_t off_sec)
{
    s_normal_pct = normal_pct > 100U ? 100U : normal_pct;
    s_dim_pct = dim_pct > 100U ? 100U : dim_pct;
    s_dim_sec = dim_sec;
    s_off_sec = off_sec;
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
    app_prefs_get_display_power(&s_normal_pct, &s_dim_pct, &s_dim_sec, &s_off_sec);
    apply_brightness(s_normal_pct);
    s_state = DISP_PWR_NORMAL;
    lv_display_trigger_activity(disp);
    s_timer = lv_timer_create(display_power_timer_cb, 500, NULL);
    if (s_timer != NULL) {
        lv_timer_set_repeat_count(s_timer, -1);
    }
}
