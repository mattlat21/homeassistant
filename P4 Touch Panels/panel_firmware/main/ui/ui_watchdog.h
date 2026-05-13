#pragma once

/**
 * LVGL heartbeat + monitor task: reboot if the UI task stops advancing timers (freeze detection).
 * Must be called from the LVGL thread once after display is ready (e.g. start of ui_shell_init).
 * No-op when CONFIG_SCREEN_TEST_UI_WATCHDOG_ENABLE is disabled.
 */
void ui_watchdog_init(void);
