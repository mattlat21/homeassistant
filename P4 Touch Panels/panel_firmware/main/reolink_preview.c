#include "reolink_preview.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#if __has_include("esp_jpeg_dec.h")
#include "esp_jpeg_dec.h"
#else
#include "jpeg_decoder.h"
#endif

static const char *TAG = "reolink_preview";

#define JPEG_BODY_MAX (256 * 1024)
#define FAST_POLL_US (1000000ULL)
#define SLOW_POLL_US (300000000ULL)
#define REOLINK_PREVIEW_MAX 2

typedef struct {
    uint8_t *buf;
    int cap;
    int len;
    char content_type[96];
    int content_length;
} http_jpeg_accum_t;

typedef struct {
    int http_status;
    int content_length;
    char content_type[96];
} snap_http_probe_meta_t;

typedef struct {
    reolink_cam_config_t cfg;
    lv_obj_t *canvas;
    esp_timer_handle_t timer_fast;
    esp_timer_handle_t timer_slow;
    bool active;
} reolink_instance_t;

typedef struct {
    uint8_t *rgb;
    uint16_t w;
    uint16_t h;
    lv_obj_t *canvas;
} snap_lvgl_ud_t;

static snap_http_probe_meta_t s_snap_http_probe_meta;
static QueueHandle_t s_queue;
static TaskHandle_t s_worker;
static reolink_instance_t s_inst[REOLINK_PREVIEW_MAX];
static int s_inst_count;

/** Down/upscale RGB888 with nearest neighbor; output allocated with jpeg_calloc_align. */
static uint8_t *reolink_scale_rgb888_nn(const uint8_t *src, uint16_t sw, uint16_t sh, uint16_t dw, uint16_t dh)
{
    if (src == NULL || sw == 0 || sh == 0 || dw == 0 || dh == 0) {
        return NULL;
    }
    const int out_bytes = (int)dw * (int)dh * 3;
    uint8_t *dst = jpeg_calloc_align(out_bytes, 16);
    if (dst == NULL) {
        return NULL;
    }
    for (uint32_t y = 0; y < dh; y++) {
        uint32_t sy = y * (uint32_t)sh / (uint32_t)dh;
        if (sy >= (uint32_t)sh) {
            sy = (uint32_t)sh - 1U;
        }
        for (uint32_t x = 0; x < dw; x++) {
            uint32_t sx = x * (uint32_t)sw / (uint32_t)dw;
            if (sx >= (uint32_t)sw) {
                sx = (uint32_t)sw - 1U;
            }
            size_t si = ((size_t)sy * (size_t)sw + (size_t)sx) * 3U;
            size_t di = ((size_t)y * (size_t)dw + (size_t)x) * 3U;
            dst[di + 0] = src[si + 0];
            dst[di + 1] = src[si + 1];
            dst[di + 2] = src[si + 2];
        }
    }
    return dst;
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_jpeg_accum_t *acc = (http_jpeg_accum_t *)evt->user_data;
    if (acc == NULL) {
        return ESP_OK;
    }
    switch (evt->event_id) {
    case HTTP_EVENT_ON_HEADER:
        if (evt->header_key != NULL && evt->header_value != NULL) {
            if (strcasecmp(evt->header_key, "Content-Type") == 0) {
                snprintf(acc->content_type, sizeof(acc->content_type), "%s", evt->header_value);
            } else if (strcasecmp(evt->header_key, "Content-Length") == 0) {
                acc->content_length = atoi(evt->header_value);
            }
        }
        break;
    case HTTP_EVENT_ON_DATA:
        if (evt->data_len > 0 && acc->buf != NULL) {
            if (acc->len + evt->data_len <= acc->cap) {
                memcpy(acc->buf + acc->len, evt->data, (size_t)evt->data_len);
                acc->len += evt->data_len;
            }
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

static bool reolink_cfg_host_ok(const reolink_cam_config_t *cfg)
{
    return cfg != NULL && cfg->host != NULL && cfg->host[0] != '\0';
}

static esp_err_t reolink_fetch_jpeg(const reolink_cam_config_t *cam, uint8_t **out_buf, int *out_len)
{
    if (!reolink_cfg_host_ok(cam)) {
        return ESP_ERR_INVALID_STATE;
    }

    char url[384];
    const char *scheme = cam->use_https ? "https" : "http";
    unsigned rs = (unsigned)esp_random();
    const char *user = cam->user != NULL ? cam->user : "";
    const char *password = cam->password != NULL ? cam->password : "";
    int n = snprintf(url, sizeof(url),
                     "%s://%s:%d/cgi-bin/api.cgi?cmd=Snap&channel=%d&rs=%08x&user=%s&password=%s&width=%d&height=%d",
                     scheme, cam->host, cam->port, cam->channel, rs, user, password, REOLINK_PREVIEW_W,
                     REOLINK_PREVIEW_H);
    if (n <= 0 || n >= (int)sizeof(url)) {
        return ESP_ERR_INVALID_SIZE;
    }

    memset(&s_snap_http_probe_meta, 0, sizeof(s_snap_http_probe_meta));
    s_snap_http_probe_meta.content_length = -1;

    uint8_t *body = (uint8_t *)heap_caps_malloc(JPEG_BODY_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (body == NULL) {
        body = (uint8_t *)heap_caps_malloc(JPEG_BODY_MAX, MALLOC_CAP_8BIT);
    }
    if (body == NULL) {
        return ESP_ERR_NO_MEM;
    }

    http_jpeg_accum_t acc = {
        .buf = body,
        .cap = JPEG_BODY_MAX,
        .len = 0,
        .content_type = {0},
        .content_length = -1,
    };

    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = &acc,
        .timeout_ms = 15000,
        .buffer_size = 2048,
        .skip_cert_common_name_check = cam->use_https,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        heap_caps_free(body);
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        s_snap_http_probe_meta.http_status = esp_http_client_get_status_code(client);
        s_snap_http_probe_meta.content_length = acc.content_length;
        memcpy(s_snap_http_probe_meta.content_type, acc.content_type, sizeof(acc.content_type));
        ESP_LOGW(TAG, "HTTP perform failed (%s): %s", cam->host, esp_err_to_name(err));
        esp_http_client_cleanup(client);
        heap_caps_free(body);
        return err;
    }

    int code = esp_http_client_get_status_code(client);
    s_snap_http_probe_meta.http_status = code;
    s_snap_http_probe_meta.content_length = acc.content_length;
    memcpy(s_snap_http_probe_meta.content_type, acc.content_type, sizeof(acc.content_type));
    esp_http_client_cleanup(client);

    if (code != 200 || acc.len < 100) {
        ESP_LOGW(TAG, "bad response (%s): status=%d len=%d", cam->host, code, acc.len);
        heap_caps_free(body);
        return ESP_FAIL;
    }

    *out_buf = body;
    *out_len = acc.len;
    return ESP_OK;
}

static jpeg_error_t decode_jpeg_to_rgb(const uint8_t *jpeg_data, int jpeg_len, uint8_t **out_rgb, int *out_rgb_len,
                                       uint16_t *out_w, uint16_t *out_h)
{
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB888;
    config.rotate = JPEG_ROTATE_0D;

    jpeg_dec_handle_t dec = NULL;
    jpeg_dec_io_t *io = NULL;
    jpeg_dec_header_info_t *info = NULL;
    uint8_t *rgb = NULL;

    jpeg_error_t ret = jpeg_dec_open(&config, &dec);
    if (ret != JPEG_ERR_OK) {
        return ret;
    }

    io = calloc(1, sizeof(jpeg_dec_io_t));
    info = calloc(1, sizeof(jpeg_dec_header_info_t));
    if (io == NULL || info == NULL) {
        ret = JPEG_ERR_NO_MEM;
        goto out;
    }

    io->inbuf = (uint8_t *)jpeg_data;
    io->inbuf_len = jpeg_len;
    ret = jpeg_dec_parse_header(dec, io, info);
    if (ret != JPEG_ERR_OK) {
        goto out;
    }

    *out_w = info->width;
    *out_h = info->height;
    *out_rgb_len = (int)info->width * (int)info->height * 3;
    rgb = jpeg_calloc_align(*out_rgb_len, 16);
    if (rgb == NULL) {
        ret = JPEG_ERR_NO_MEM;
        goto out;
    }
    io->outbuf = rgb;
    *out_rgb = rgb;

    ret = jpeg_dec_process(dec, io);
    if (ret != JPEG_ERR_OK) {
        jpeg_free_align(rgb);
        *out_rgb = NULL;
    }

out:
    if (dec != NULL) {
        jpeg_dec_close(dec);
    }
    free(io);
    free(info);
    return ret;
}

static void apply_snap_on_lvgl(void *user_data)
{
    snap_lvgl_ud_t *ud = (snap_lvgl_ud_t *)user_data;
    if (ud == NULL) {
        return;
    }
    if (ud->canvas != NULL && ud->rgb != NULL && ud->w == REOLINK_PREVIEW_W && ud->h == REOLINK_PREVIEW_H) {
        uint8_t *dst = (uint8_t *)lv_canvas_get_buf(ud->canvas);
        if (dst != NULL) {
            /* esp_new_jpeg RGB888 is R,G,B; LVGL LV_COLOR_FORMAT_RGB888 stores B,G,R. */
            const uint8_t *src = ud->rgb;
            const size_t pixels = (size_t)REOLINK_PREVIEW_W * (size_t)REOLINK_PREVIEW_H;
            for (size_t i = 0; i < pixels; i++) {
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
                dst += 3;
                src += 3;
            }
            lv_obj_invalidate(ud->canvas);
        }
    } else if (ud->rgb != NULL && (ud->w != REOLINK_PREVIEW_W || ud->h != REOLINK_PREVIEW_H)) {
        ESP_LOGW(TAG, "snap size %" PRIu16 "x%" PRIu16 " (expected %dx%d)", ud->w, ud->h, REOLINK_PREVIEW_W,
                 REOLINK_PREVIEW_H);
    }
    if (ud->rgb != NULL) {
        jpeg_free_align(ud->rgb);
    }
    free(ud);
}

static void run_fetch_and_schedule_lvgl(int idx)
{
    if (idx < 0 || idx >= s_inst_count) {
        return;
    }
    reolink_instance_t *inst = &s_inst[idx];
    if (!inst->active || !reolink_cfg_host_ok(&inst->cfg) || inst->canvas == NULL) {
        return;
    }

    uint8_t *jpeg = NULL;
    int jpeg_len = 0;
    if (reolink_fetch_jpeg(&inst->cfg, &jpeg, &jpeg_len) != ESP_OK) {
        return;
    }

    uint8_t *rgb = NULL;
    int rgb_len = 0;
    uint16_t w = 0, h = 0;
    jpeg_error_t jret = decode_jpeg_to_rgb(jpeg, jpeg_len, &rgb, &rgb_len, &w, &h);

    if (jret != JPEG_ERR_OK || rgb == NULL) {
        ESP_LOGW(TAG, "jpeg decode failed: %d", (int)jret);
        heap_caps_free(jpeg);
        if (rgb) {
            jpeg_free_align(rgb);
        }
        return;
    }
    heap_caps_free(jpeg);

    if (w != REOLINK_PREVIEW_W || h != REOLINK_PREVIEW_H) {
        uint8_t *scaled = reolink_scale_rgb888_nn(rgb, w, h, REOLINK_PREVIEW_W, REOLINK_PREVIEW_H);
        if (scaled == NULL) {
            ESP_LOGW(TAG, "scale failed %" PRIu16 "x%" PRIu16 " -> %dx%d", w, h, REOLINK_PREVIEW_W,
                     REOLINK_PREVIEW_H);
            jpeg_free_align(rgb);
            return;
        }
        jpeg_free_align(rgb);
        rgb = scaled;
        w = REOLINK_PREVIEW_W;
        h = REOLINK_PREVIEW_H;
    }

    snap_lvgl_ud_t *ud = (snap_lvgl_ud_t *)calloc(1, sizeof(snap_lvgl_ud_t));
    if (ud == NULL) {
        jpeg_free_align(rgb);
        return;
    }
    ud->rgb = rgb;
    ud->w = w;
    ud->h = h;
    ud->canvas = inst->canvas;

    if (lv_async_call(apply_snap_on_lvgl, ud) != LV_RESULT_OK) {
        jpeg_free_align(rgb);
        free(ud);
    }
}

static void reolink_worker_task(void *arg)
{
    (void)arg;
    uint8_t idx = 0;
    for (;;) {
        if (xQueueReceive(s_queue, &idx, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        run_fetch_and_schedule_lvgl((int)idx);
    }
}

static void enqueue_instance(int idx)
{
    if (s_queue == NULL || idx < 0 || idx >= REOLINK_PREVIEW_MAX) {
        return;
    }
    uint8_t m = (uint8_t)idx;
    (void)xQueueSend(s_queue, &m, 0);
}

static void timer_cb(void *arg)
{
    enqueue_instance((int)(intptr_t)arg);
}

static void preview_screen_event(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_inst_count) {
        return;
    }
    reolink_instance_t *inst = &s_inst[idx];
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SCREEN_LOADED) {
        if (inst->timer_slow != NULL) {
            esp_timer_stop(inst->timer_slow);
        }
        if (inst->timer_fast != NULL) {
            esp_timer_start_periodic(inst->timer_fast, FAST_POLL_US);
        }
    } else if (code == LV_EVENT_SCREEN_UNLOADED) {
        if (inst->timer_fast != NULL) {
            esp_timer_stop(inst->timer_fast);
        }
        if (inst->timer_slow != NULL) {
            esp_timer_start_periodic(inst->timer_slow, SLOW_POLL_US);
        }
    }
}

static bool ensure_shared_worker(void)
{
    if (s_queue != NULL) {
        return true;
    }

    s_queue = xQueueCreate(8, sizeof(uint8_t));
    if (s_queue == NULL) {
        ESP_LOGE(TAG, "queue create failed");
        return false;
    }

    if (xTaskCreatePinnedToCore(reolink_worker_task, "reolink_snap", 12288, NULL, 5, &s_worker, 0) != pdPASS) {
        ESP_LOGE(TAG, "worker task create failed");
        vQueueDelete(s_queue);
        s_queue = NULL;
        return false;
    }
    return true;
}

void reolink_preview_bind(lv_obj_t *screen, lv_obj_t *canvas, const reolink_cam_config_t *cfg)
{
    if (!reolink_cfg_host_ok(cfg)) {
        ESP_LOGI(TAG, "Reolink host empty — preview disabled");
        return;
    }

    if (canvas == NULL || screen == NULL) {
        ESP_LOGW(TAG, "no canvas/screen — preview disabled");
        return;
    }

    if (s_inst_count >= REOLINK_PREVIEW_MAX) {
        ESP_LOGE(TAG, "too many Reolink previews (max %d)", REOLINK_PREVIEW_MAX);
        return;
    }

    if (!ensure_shared_worker()) {
        return;
    }

    const int idx = s_inst_count;
    reolink_instance_t *inst = &s_inst[idx];
    memset(inst, 0, sizeof(*inst));
    inst->cfg = *cfg;
    inst->canvas = canvas;
    inst->active = true;

    static const char *const fast_names[REOLINK_PREVIEW_MAX] = {"rl_fast_0", "rl_fast_1"};
    static const char *const slow_names[REOLINK_PREVIEW_MAX] = {"rl_slow_0", "rl_slow_1"};

    const esp_timer_create_args_t fast_args = {
        .callback = &timer_cb,
        .arg = (void *)(intptr_t)idx,
        .name = fast_names[idx],
    };
    const esp_timer_create_args_t slow_args = {
        .callback = &timer_cb,
        .arg = (void *)(intptr_t)idx,
        .name = slow_names[idx],
    };
    if (esp_timer_create(&fast_args, &inst->timer_fast) != ESP_OK ||
        esp_timer_create(&slow_args, &inst->timer_slow) != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_create failed for %s", cfg->host);
        inst->active = false;
        return;
    }

    s_inst_count++;

    lv_obj_add_event_cb(screen, preview_screen_event, LV_EVENT_SCREEN_LOADED, (void *)(intptr_t)idx);
    lv_obj_add_event_cb(screen, preview_screen_event, LV_EVENT_SCREEN_UNLOADED, (void *)(intptr_t)idx);

    esp_timer_start_periodic(inst->timer_slow, SLOW_POLL_US);
    enqueue_instance(idx);

    ESP_LOGI(TAG, "Reolink preview bound host=%s (fast 1s on screen, slow 5m off screen)", cfg->host);
}
