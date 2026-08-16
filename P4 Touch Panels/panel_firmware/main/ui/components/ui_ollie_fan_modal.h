#pragma once

#include "lvgl.h"

/**
 * Fan-speed picker for Ollie's room, shown on the top layer (Ollie's Room + Ollie's Room Legacy).
 * Selecting a row publishes `fan_off` / `fan_1` / `fan_2` / `fan_3` via `ha_mqtt_publish_ollie_button`
 * and closes the modal. Tapping the backdrop dismisses without publishing.
 * Calling this while already open closes it (button acts as a toggle).
 */
void ui_ollie_fan_modal_open(void);

/** Close if open; safe to call unconditionally (e.g. on `LV_EVENT_SCREEN_UNLOADED`). */
void ui_ollie_fan_modal_close(void);
