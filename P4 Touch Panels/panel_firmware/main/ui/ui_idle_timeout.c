#include "ui/ui_idle_timeout.h"

#include "app_prefs.h"
#include "ui/nav.h"

static lv_display_t *s_disp;
static lv_timer_t *s_timer;
static app_id_t s_target = APP_HOME;
static uint32_t s_sec;

static void idle_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (s_sec == 0U) {
        return;
    }
    if (!nav_is_on_registered_screen()) {
        return;
    }
    if (nav_get_current_app() == s_target) {
        return;
    }
    uint32_t inactive_ms = lv_display_get_inactive_time(s_disp);
    if (inactive_ms >= s_sec * 1000U) {
        nav_go_to(s_target);
    }
}

void ui_idle_timeout_configure(app_id_t target_app, uint32_t sec)
{
    s_target = target_app;
    s_sec = sec;
}

void ui_idle_timeout_init(lv_display_t *disp)
{
    s_disp = disp;
    app_prefs_get_idle_timeout(&s_target, &s_sec);
    lv_display_trigger_activity(disp);
    s_timer = lv_timer_create(idle_timer_cb, 500, NULL);
    if (s_timer != NULL) {
        lv_timer_set_repeat_count(s_timer, -1);
    }
}
