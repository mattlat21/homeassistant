#include "ota_update.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_app_format.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"
#include "sdkconfig.h"

#if CONFIG_SCREEN_TEST_OTA_ENABLE
#include "ui/screens/screen_ota_progress.h"
#endif

static const char *TAG = "ota_update";

static void maybe_mark_app_valid(void)
{
#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL) {
        return;
    }
    esp_ota_img_states_t st = ESP_OTA_IMG_UNDEFINED;
    if (esp_ota_get_state_partition(running, &st) != ESP_OK) {
        return;
    }
    if (st == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "running app marked valid (rollback cancelled)");
        } else {
            ESP_LOGW(TAG, "esp_ota_mark_app_valid_cancel_rollback: %s", esp_err_to_name(err));
        }
    }
#endif
}

#if CONFIG_SCREEN_TEST_OTA_ENABLE

#define OTA_JSON_QUEUE_MAX 768
#define OTA_WORKER_STACK (1024 * 12)
#define OTA_WORKER_PRIO 5

typedef struct {
    char json[OTA_JSON_QUEUE_MAX];
} ota_json_msg_t;

static QueueHandle_t s_ota_queue;
static volatile bool s_ota_busy;

static bool hex32_from_string(const char *hex, uint8_t out[32])
{
    size_t len = strlen(hex);
    if (len != 64) {
        return false;
    }
    for (size_t i = 0; i < 32; i++) {
        unsigned int b = 0;
        if (sscanf(hex + (i * 2), "%2x", &b) != 1) {
            return false;
        }
        out[i] = (uint8_t)b;
    }
    return true;
}

static esp_err_t verify_partition_sha256_prefix(const esp_partition_t *part, size_t length, const uint8_t expected[32])
{
    if (part == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (length > part->size) {
        return ESP_ERR_INVALID_SIZE;
    }

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    if (mbedtls_sha256_starts(&ctx, 0) != 0) {
        mbedtls_sha256_free(&ctx);
        return ESP_FAIL;
    }

    uint8_t buf[1024];
    size_t off = 0;
    while (off < length) {
        size_t chunk = length - off;
        if (chunk > sizeof(buf)) {
            chunk = sizeof(buf);
        }
        esp_err_t err = esp_partition_read(part, off, buf, chunk);
        if (err != ESP_OK) {
            mbedtls_sha256_free(&ctx);
            return err;
        }
        if (mbedtls_sha256_update(&ctx, buf, chunk) != 0) {
            mbedtls_sha256_free(&ctx);
            return ESP_FAIL;
        }
        off += chunk;
    }

    uint8_t digest[32];
    if (mbedtls_sha256_finish(&ctx, digest) != 0) {
        mbedtls_sha256_free(&ctx);
        return ESP_FAIL;
    }
    mbedtls_sha256_free(&ctx);

    if (memcmp(digest, expected, 32) != 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t run_https_ota(const char *url, const uint8_t *expected_sha256, bool verify_sha, size_t verify_len)
{
    bool ui_ok = screen_ota_progress_show_and_wait_for_display(5000);
    if (!ui_ok) {
        ESP_LOGW(TAG, "OTA progress UI did not appear (timeout or screen missing); continuing download");
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *target_before = esp_ota_get_next_update_partition(running);

    esp_http_client_config_t http_cfg = {
        .url = url,
        .timeout_ms = CONFIG_SCREEN_TEST_OTA_HTTP_TIMEOUT_MS,
        .keep_alive_enable = true,
    };

#if CONFIG_SCREEN_TEST_OTA_SKIP_CERT_VERIFY
    http_cfg.skip_cert_common_name_check = true;
    http_cfg.crt_bundle_attach = NULL;
#else
    http_cfg.crt_bundle_attach = esp_crt_bundle_attach;
#endif

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    esp_https_ota_handle_t h = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_cfg, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_begin: %s", esp_err_to_name(err));
        screen_ota_progress_dismiss_async();
        return err;
    }

    for (;;) {
        err = esp_https_ota_perform(h);
        {
            const int total = esp_https_ota_get_image_size(h);
            const int rd = esp_https_ota_get_image_len_read(h);
            int pct = 0;
            if (total > 0 && rd >= 0) {
                pct = (int)(((int64_t)rd * 100) / (int64_t)total);
                if (pct > 100) {
                    pct = 100;
                }
            }
            screen_ota_progress_set_percent_async(pct);
        }
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_perform: %s", esp_err_to_name(err));
        esp_https_ota_abort(h);
        screen_ota_progress_dismiss_async();
        return err;
    }

    if (verify_sha && expected_sha256 != NULL) {
        screen_ota_progress_set_percent_async(99);
        const esp_partition_t *upd = target_before;
        if (upd == NULL) {
            ESP_LOGE(TAG, "no update partition for SHA verify");
            esp_https_ota_abort(h);
            screen_ota_progress_dismiss_async();
            return ESP_ERR_NOT_FOUND;
        }
        if (verify_len > upd->size) {
            ESP_LOGE(TAG, "OTA size %" PRIu32 " exceeds partition %" PRIu32, (uint32_t)verify_len,
                     (uint32_t)upd->size);
            esp_https_ota_abort(h);
            screen_ota_progress_dismiss_async();
            return ESP_ERR_INVALID_SIZE;
        }
        err = verify_partition_sha256_prefix(upd, verify_len, expected_sha256);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "SHA256 verify failed (image may not match sha256/size)");
            esp_https_ota_abort(h);
            screen_ota_progress_dismiss_async();
            return err;
        }
        ESP_LOGI(TAG, "SHA256 verify OK (%zu bytes)", verify_len);
    }

    screen_ota_progress_set_percent_async(100);
    err = esp_https_ota_finish(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_finish: %s", esp_err_to_name(err));
        screen_ota_progress_dismiss_async();
        return err;
    }

    ESP_LOGI(TAG, "OTA success, rebooting…");
    esp_restart();
    return ESP_OK;
}

static void ota_worker_task(void *arg)
{
    (void)arg;
    ota_json_msg_t msg;

    for (;;) {
        if (xQueueReceive(s_ota_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        cJSON *root = cJSON_Parse(msg.json);
        if (root == NULL) {
            ESP_LOGW(TAG, "OTA JSON parse failed");
            s_ota_busy = false;
            continue;
        }

        const cJSON *j_url = cJSON_GetObjectItemCaseSensitive(root, "url");
        const cJSON *j_ver = cJSON_GetObjectItemCaseSensitive(root, "version");
        const cJSON *j_sha = cJSON_GetObjectItemCaseSensitive(root, "sha256");
        const cJSON *j_sz = cJSON_GetObjectItemCaseSensitive(root, "size");

        if (!cJSON_IsString(j_url) || j_url->valuestring == NULL || j_url->valuestring[0] == '\0') {
            ESP_LOGW(TAG, "OTA JSON missing url");
            cJSON_Delete(root);
            s_ota_busy = false;
            continue;
        }

        const esp_app_desc_t *cur = esp_app_get_description();
        if (cJSON_IsString(j_ver) && j_ver->valuestring != NULL && j_ver->valuestring[0] != '\0' &&
            cur != NULL && strncmp(j_ver->valuestring, cur->version, sizeof(cur->version)) == 0) {
            ESP_LOGI(TAG, "OTA skipped (already running version %s)", cur->version);
            cJSON_Delete(root);
            s_ota_busy = false;
            continue;
        }

        bool has_sha = cJSON_IsString(j_sha) && j_sha->valuestring != NULL && strlen(j_sha->valuestring) == 64;
        bool has_sz = cJSON_IsNumber(j_sz) && cJSON_GetNumberValue(j_sz) > 0;
        if (has_sha != has_sz) {
            ESP_LOGW(TAG, "OTA: provide both sha256 (64 hex) and size (bytes), or neither");
            cJSON_Delete(root);
            s_ota_busy = false;
            continue;
        }

        uint8_t expect_sha[32];
        size_t verify_len = 0;
        bool verify = false;
        if (has_sha && has_sz) {
            if (!hex32_from_string(j_sha->valuestring, expect_sha)) {
                ESP_LOGW(TAG, "OTA: invalid sha256 hex");
                cJSON_Delete(root);
                s_ota_busy = false;
                continue;
            }
            verify_len = (size_t)cJSON_GetNumberValue(j_sz);
            verify = true;
        }

        ESP_LOGI(TAG, "OTA start url=%s", j_url->valuestring);
        (void)run_https_ota(j_url->valuestring, expect_sha, verify, verify_len);

        cJSON_Delete(root);
        s_ota_busy = false;
    }
}

void ota_update_init(void)
{
    maybe_mark_app_valid();

    if (s_ota_queue != NULL) {
        return;
    }
    s_ota_queue = xQueueCreate(1, sizeof(ota_json_msg_t));
    if (s_ota_queue == NULL) {
        ESP_LOGE(TAG, "OTA queue create failed");
        return;
    }
    BaseType_t ok = xTaskCreatePinnedToCore(ota_worker_task, "ota_worker", OTA_WORKER_STACK, NULL, OTA_WORKER_PRIO, NULL,
                                            tskNO_AFFINITY);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "OTA worker task create failed");
    }
}

bool ota_update_request_from_mqtt_json(const char *json)
{
    if (json == NULL) {
        return false;
    }
    if (s_ota_queue == NULL) {
        ESP_LOGW(TAG, "OTA not initialized");
        return false;
    }
    if (s_ota_busy) {
        ESP_LOGW(TAG, "OTA already in progress");
        return false;
    }

    size_t n = strlen(json);
    if (n >= OTA_JSON_QUEUE_MAX) {
        ESP_LOGW(TAG, "OTA JSON too long (%zu)", n);
        return false;
    }

    ota_json_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    memcpy(msg.json, json, n + 1);

    if (xQueueSend(s_ota_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "OTA queue full");
        return false;
    }
    s_ota_busy = true;
    return true;
}

#else /* !CONFIG_SCREEN_TEST_OTA_ENABLE */

void ota_update_init(void)
{
    maybe_mark_app_valid();
}

bool ota_update_request_from_mqtt_json(const char *json)
{
    (void)json;
    return false;
}

#endif /* CONFIG_SCREEN_TEST_OTA_ENABLE */
