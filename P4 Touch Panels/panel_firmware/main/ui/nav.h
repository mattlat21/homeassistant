#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

typedef enum {
    APP_HOME = 0,
    APP_OLLIE_ROOM,
    APP_DASHBOARD,
    APP_FRONT_GATE,
    APP_PIPBOY,
    APP_SETTINGS,
    APP_STUDY,
    APP_ABOUT,
    APP_FRONT_DOOR,
    APP_KITCHEN,
    APP_STUDIO,
    APP_COUNT,
} app_id_t;

void nav_init(lv_display_t *disp);

void nav_register_screen(app_id_t id, lv_obj_t *screen);

lv_obj_t *nav_screen_get(app_id_t id);

/** Resolved from the active LVGL screen; unknown screens fall back to `APP_HOME`. */
app_id_t nav_get_current_app(void);

/** True if the active screen is one registered with `nav_register_screen` (excludes loading, OTA UI, etc.). */
bool nav_is_on_registered_screen(void);

/** Load a registered screen; clears any MQTT temporary-screen timer first. */
void nav_go_to(app_id_t id);

/**
 * LVGL-thread only: show `target`, then restore the screen that was active when this was called after `duration_ms`.
 * Cancels any previous temporary-screen timer (including from MQTT).
 */
void nav_go_to_temporarily(app_id_t target, uint32_t duration_ms);

void nav_go_home(void);

void nav_next(void);

void nav_prev(void);

void nav_install_gesture_on_screen(lv_obj_t *screen);
