#pragma once

#include <stdint.h>

#include "lvgl.h"

/**
 * Full-screen OTA progress UI (not registered in nav / not on home launcher).
 * Created from ui_shell when CONFIG_SCREEN_TEST_OTA_ENABLE.
 */
lv_obj_t *screen_ota_progress_create(lv_display_t *disp);

/**
 * Show OTA screen and block until LVGL has loaded it. Call only from the OTA worker task.
 * @return false if UI missing or timeout
 */
bool screen_ota_progress_show_and_wait_for_display(uint32_t timeout_ms);

/** Update progress bar 0–100 from the OTA worker (LVGL thread via internal async). */
void screen_ota_progress_set_percent_async(int32_t percent_0_100);

/** After a failed/aborted OTA, return to Home on the LVGL thread. */
void screen_ota_progress_dismiss_async(void);
