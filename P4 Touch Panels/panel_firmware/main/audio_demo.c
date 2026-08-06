#include "audio_demo.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "bsp/esp32_p4_wifi6_touch_lcd_4b.h"
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "audio_demo";

#define AUDIO_DEMO_SAMPLE_RATE 22050
#define AUDIO_DEMO_TONE_HZ 440.0f
#define AUDIO_DEMO_VOLUME 70
#define AUDIO_DEMO_BEEP_MS 250
#define AUDIO_DEMO_GAP_MS 750
#define AUDIO_DEMO_AMPLITUDE 12000

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static esp_codec_dev_handle_t s_spk;
static volatile bool s_running;
static TaskHandle_t s_task;
static bool s_inited;

static void fill_sine(int16_t *dst, int samples)
{
    const float step = 2.0f * (float)M_PI * AUDIO_DEMO_TONE_HZ / (float)AUDIO_DEMO_SAMPLE_RATE;
    float phase = 0.0f;
    for (int i = 0; i < samples; i++) {
        dst[i] = (int16_t)(sinf(phase) * (float)AUDIO_DEMO_AMPLITUDE);
        phase += step;
        if (phase > 2.0f * (float)M_PI) {
            phase -= 2.0f * (float)M_PI;
        }
    }
}

static void audio_demo_task(void *arg)
{
    (void)arg;

    if (s_spk == NULL) {
        ESP_LOGE(TAG, "speaker codec not ready — aborting play task");
        s_running = false;
        s_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    const int beep_samples = (AUDIO_DEMO_SAMPLE_RATE * AUDIO_DEMO_BEEP_MS) / 1000;
    const int beep_bytes = beep_samples * (int)sizeof(int16_t);
    int16_t *tone = (int16_t *)heap_caps_malloc((size_t)beep_bytes, MALLOC_CAP_DEFAULT);
    int16_t *write_buf = (int16_t *)heap_caps_malloc((size_t)beep_bytes, MALLOC_CAP_DEFAULT);
    if (tone == NULL || write_buf == NULL) {
        ESP_LOGE(TAG, "tone buffer alloc failed");
        heap_caps_free(tone);
        heap_caps_free(write_buf);
        s_running = false;
        s_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    fill_sine(tone, beep_samples);

    while (s_running) {
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = AUDIO_DEMO_SAMPLE_RATE,
            .mclk_multiple = 0,
        };

        int ret = esp_codec_dev_open(s_spk, &fs);
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "esp_codec_dev_open failed: %d", ret);
            break;
        }

        ESP_LOGI(TAG, "playing 440 Hz beep loop (250 ms on / 750 ms off)");

        while (s_running) {
            memcpy(write_buf, tone, (size_t)beep_bytes);
            ret = esp_codec_dev_write(s_spk, write_buf, beep_bytes);
            if (ret != ESP_CODEC_DEV_OK) {
                ESP_LOGW(TAG, "esp_codec_dev_write failed: %d", ret);
                break;
            }

            for (int waited = 0; s_running && waited < AUDIO_DEMO_GAP_MS; waited += 50) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }

        esp_codec_dev_close(s_spk);
        ESP_LOGI(TAG, "playback stopped");

        /* If start() raced during close, reopen instead of exiting. */
        if (!s_running) {
            break;
        }
    }

    heap_caps_free(tone);
    heap_caps_free(write_buf);
    s_running = false;
    s_task = NULL;
    vTaskDelete(NULL);
}

void audio_demo_init(void)
{
    if (s_inited) {
        return;
    }

    s_spk = bsp_audio_codec_speaker_init();
    if (s_spk == NULL) {
        ESP_LOGE(TAG, "bsp_audio_codec_speaker_init failed — no speaker codec");
        return;
    }

    int ret = esp_codec_dev_set_out_vol(s_spk, AUDIO_DEMO_VOLUME);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "set_out_vol failed: %d (continuing)", ret);
    }

    s_inited = true;
    ESP_LOGI(TAG, "speaker codec ready (vol=%d)", AUDIO_DEMO_VOLUME);
}

void audio_demo_start(void)
{
    audio_demo_init();
    if (s_spk == NULL) {
        return;
    }

    s_running = true;
    if (s_task != NULL) {
        /* Previous task still winding down or already looping — keep flag set. */
        return;
    }

    BaseType_t ok = xTaskCreatePinnedToCore(audio_demo_task, "audio_demo", 4096, NULL, 4, &s_task, 0);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "play task create failed");
        s_running = false;
        s_task = NULL;
    }
}

void audio_demo_stop(void)
{
    s_running = false;
}
