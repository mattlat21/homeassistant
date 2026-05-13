#include "ui/ui_watchdog.h"

#include "sdkconfig.h"

#if CONFIG_SCREEN_TEST_UI_WATCHDOG_ENABLE

#include <stdatomic.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "ui_watchdog";

/** Microseconds from esp_timer_get_time(); updated from LVGL timer callback only if UI loop runs. */
static _Atomic int64_t s_last_heartbeat_us;

static void heartbeat_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    atomic_store_explicit(&s_last_heartbeat_us, esp_timer_get_time(), memory_order_relaxed);
}

static void freeze_monitor_task(void *arg)
{
    (void)arg;
    const int64_t timeout_us = (int64_t)CONFIG_SCREEN_TEST_UI_WATCHDOG_TIMEOUT_S * 1000000LL;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        const int64_t last = atomic_load_explicit(&s_last_heartbeat_us, memory_order_relaxed);
        const int64_t now = esp_timer_get_time();
        if (now - last > timeout_us) {
            ESP_LOGE(TAG, "UI freeze: LVGL heartbeat stale > %ds — rebooting",
                     CONFIG_SCREEN_TEST_UI_WATCHDOG_TIMEOUT_S);
            esp_restart();
        }
    }
}

void ui_watchdog_init(void)
{
    static bool started;

    if (started) {
        return;
    }
    started = true;

    atomic_store_explicit(&s_last_heartbeat_us, esp_timer_get_time(), memory_order_relaxed);

    lv_timer_t *t = lv_timer_create(heartbeat_timer_cb, 250, NULL);
    if (t == NULL) {
        ESP_LOGW(TAG, "lv_timer_create(heartbeat) failed — freeze watchdog inactive");
        return;
    }

    const BaseType_t ok =
        xTaskCreatePinnedToCore(freeze_monitor_task, "ui_freeze_mon", 3072, NULL, 3, NULL, 0);
    if (ok != pdPASS) {
        ESP_LOGW(TAG, "ui_freeze_mon task create failed — freeze watchdog inactive");
    }
}

#else /* !CONFIG_SCREEN_TEST_UI_WATCHDOG_ENABLE */

void ui_watchdog_init(void) {}

#endif
