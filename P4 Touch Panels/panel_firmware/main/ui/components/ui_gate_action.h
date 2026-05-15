#pragma once

#include "lvgl.h"

/** Shared front-gate state + action button logic (Front Gate + Front Door screens). */
void ui_gate_action_init(void);

/** Register an action button + label updated on MQTT and disabled during cooldown. */
void ui_gate_action_bind(lv_obj_t *btn, lv_obj_t *label);

/** Optional status pill on the Front Gate screen (MQTT-driven colors/text). */
void ui_gate_action_set_status_pill(lv_obj_t *pill, lv_obj_t *label);

/** Publish the action for the current gate state and start the shared cooldown. */
void ui_gate_action_on_click(void);
