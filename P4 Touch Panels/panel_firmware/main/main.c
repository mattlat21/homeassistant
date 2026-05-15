#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include "bsp/esp32_p4_wifi6_touch_lcd_4b.h"
#include "app_prefs.h"
#include "ha_mqtt.h"
#include "ota_update.h"
#include "ui/ui_shell.h"

static const char *TAG = "screen_test_1";

static void ui_shell_boot_async(void *user_data)
{
    ui_shell_init((lv_display_t *)user_data);
    ESP_LOGI(TAG, "UI shell started (loading → default app from NVS; swipe L/R between apps, swipe up for home)");
}

/** Same as BSP `bsp_display_start()` but larger LVGL task stack for ThorVG/Lottie (default 7 KiB overflows in tvgSwRle). */
static lv_display_t *bsp_display_start_for_lottie(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
            .buff_dma = false,
#else
            .buff_dma = true,
#endif
            .buff_spiram = false,
            .sw_rotate = true,
        },
    };
    cfg.lvgl_port_cfg.task_stack = 32 * 1024;
    /* Pin ThorVG/Lottie to CPU1: long strokes starve the idle task on that core; keep CPU0 idle under TASK_WDT. */
    cfg.lvgl_port_cfg.task_affinity = 1;
    return bsp_display_start_with_config(&cfg);
}

void app_main(void)
{
    ha_mqtt_init();
    app_prefs_init();
    ota_update_init();

    /* Backlight: do not call bsp_display_brightness_init() here — BSP does it inside
     * bsp_display_new_with_handles(). Extra inits reserve GPIO 26 twice and LEDC warns
     * ("GPIO 26 is not usable"); PWM/backlight can stay off → black screen. */
    lv_display_t *disp = bsp_display_start_for_lottie();
    if (disp == NULL) {
        ESP_LOGE(TAG, "bsp_display_start_with_config failed");
        return;
    }

    esp_err_t err = bsp_display_brightness_set(100);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "brightness set failed: %s", esp_err_to_name(err));
        return;
    }

    /* lv_lottie_set_src_data() triggers lottie_update() on the caller's thread; keep ThorVG off main's small stack. */
    if (lv_async_call(ui_shell_boot_async, disp) != LV_RESULT_OK) {
        ESP_LOGE(TAG, "lv_async_call(ui_shell_boot_async) failed");
        return;
    }
}
