#include "ui/nav.h"

#include <stdint.h>

static lv_obj_t *s_screens[APP_COUNT];

static lv_timer_t *s_temp_restore_timer;
static app_id_t s_temp_restore_app;

static void cancel_temp_restore_timer_only(void)
{
    if (s_temp_restore_timer != NULL) {
        lv_timer_del(s_temp_restore_timer);
        s_temp_restore_timer = NULL;
    }
}

static bool nav_try_load_registered(app_id_t id)
{
    if (id >= APP_COUNT || s_screens[id] == NULL) {
        return false;
    }
    lv_screen_load(s_screens[id]);
    /* Idle timeout uses LVGL inactivity time; resetting here counts programmatic nav (MQTT, swipe, timers). */
    lv_display_t *d = lv_obj_get_display(lv_screen_active());
    if (d != NULL) {
        lv_display_trigger_activity(d);
    }
    return true;
}

static void nav_temp_restore_cb(lv_timer_t *t)
{
    (void)t;
    s_temp_restore_timer = NULL;
    (void)nav_try_load_registered(s_temp_restore_app);
}

void nav_init(lv_display_t *disp)
{
    (void)disp;
    for (int i = 0; i < APP_COUNT; i++) {
        s_screens[i] = NULL;
    }
}

void nav_register_screen(app_id_t id, lv_obj_t *screen)
{
    if (id < APP_COUNT) {
        s_screens[id] = screen;
    }
}

lv_obj_t *nav_screen_get(app_id_t id)
{
    if (id >= APP_COUNT) {
        return NULL;
    }
    return s_screens[id];
}

app_id_t nav_get_current_app(void)
{
    lv_obj_t *active = lv_screen_active();
    for (app_id_t i = 0; i < APP_COUNT; i++) {
        if (s_screens[i] == active) {
            return i;
        }
    }
    return APP_HOME;
}

bool nav_is_on_registered_screen(void)
{
    lv_obj_t *active = lv_screen_active();
    for (app_id_t i = 0; i < APP_COUNT; i++) {
        if (s_screens[i] != NULL && s_screens[i] == active) {
            return true;
        }
    }
    return false;
}

void nav_go_to_temporarily(app_id_t target, uint32_t duration_ms)
{
    cancel_temp_restore_timer_only();
    app_id_t from = nav_get_current_app();
    if (!nav_try_load_registered(target)) {
        return;
    }
    s_temp_restore_app = from;

    uint32_t wait = duration_ms;
    if (wait < 250U) {
        wait = 250U;
    }
    if (wait > 86400000U) {
        wait = 86400000U;
    }

    s_temp_restore_timer = lv_timer_create(nav_temp_restore_cb, wait, NULL);
    if (s_temp_restore_timer != NULL) {
        lv_timer_set_repeat_count(s_temp_restore_timer, 1);
    }
}

void nav_go_to(app_id_t id)
{
    cancel_temp_restore_timer_only();
    (void)nav_try_load_registered(id);
}

void nav_go_home(void)
{
    nav_go_to(APP_HOME);
}

void nav_next(void)
{
    app_id_t cur = nav_get_current_app();
    for (int step = 1; step <= APP_COUNT; step++) {
        app_id_t next = (app_id_t)((cur + step) % APP_COUNT);
        if (s_screens[next] != NULL) {
            nav_go_to(next);
            return;
        }
    }
}

void nav_prev(void)
{
    app_id_t cur = nav_get_current_app();
    for (int step = 1; step <= APP_COUNT; step++) {
        app_id_t prev = (app_id_t)((cur + APP_COUNT - step) % APP_COUNT);
        if (s_screens[prev] != NULL) {
            nav_go_to(prev);
            return;
        }
    }
}

static void nav_gesture_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) {
        return;
    }
    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) {
        return;
    }
    if (lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER) {
        return;
    }
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir == LV_DIR_TOP) {
        nav_go_home();
    } else if (dir == LV_DIR_LEFT) {
        nav_next();
    } else if (dir == LV_DIR_RIGHT) {
        nav_prev();
    }
}

void nav_install_gesture_on_screen(lv_obj_t *screen)
{
    if (screen == NULL) {
        return;
    }
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, nav_gesture_cb, LV_EVENT_GESTURE, NULL);
}
