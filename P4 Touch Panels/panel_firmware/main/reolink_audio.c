#include "reolink_audio.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "bsp/esp32_p4_wifi6_touch_lcd_4b.h"
#include "esp_aac_dec.h"
#include "esp_audio_types.h"
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_rom_md5.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include <errno.h>

static const char *TAG = "reolink_audio";

#define RTSP_RECV_BUF 4096
#define AAC_PCM_OUT_MAX (4096)
#define SPEAKER_VOLUME 70

static esp_codec_dev_handle_t s_spk;
static volatile bool s_running;
static TaskHandle_t s_task;
static bool s_spk_inited;

static char s_host[64];
static char s_user[32];
static char s_password[64];

static const char *strcasestr_local(const char *hay, const char *needle)
{
    if (hay == NULL || needle == NULL || needle[0] == '\0') {
        return hay;
    }
    size_t nlen = strlen(needle);
    for (const char *p = hay; *p; p++) {
        size_t i = 0;
        while (i < nlen && p[i] &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) {
            i++;
        }
        if (i == nlen) {
            return p;
        }
    }
    return NULL;
}

static void md5_hex(const char *in, char out_hex[33])
{
    unsigned char dig[16];
    md5_context_t ctx;
    esp_rom_md5_init(&ctx);
    esp_rom_md5_update(&ctx, (const unsigned char *)in, strlen(in));
    esp_rom_md5_final(dig, &ctx);
    for (int i = 0; i < 16; i++) {
        sprintf(out_hex + i * 2, "%02x", dig[i]);
    }
    out_hex[32] = '\0';
}

static int sock_send_all(int fd, const char *data, int len)
{
    int sent = 0;
    while (sent < len) {
        int n = send(fd, data + sent, (size_t)(len - sent), 0);
        if (n < 0) {
            return -1;
        }
        sent += n;
    }
    return sent;
}

static int rtsp_read_response(int fd, char *buf, int buf_sz, char **body_out, int *body_len)
{
    int used = 0;
    *body_out = NULL;
    *body_len = 0;
    while (used < buf_sz - 1) {
        int n = recv(fd, buf + used, (size_t)(buf_sz - 1 - used), 0);
        if (n <= 0) {
            return -1;
        }
        used += n;
        buf[used] = '\0';

        /* Reolink may emit interleaved RTCP (`$`) before the PLAY 200 headers. Skip those. */
        while (used > 0 && buf[0] == '$') {
            if (used < 4) {
                break; /* need channel + length */
            }
            int pkt_len = ((unsigned char)buf[2] << 8) | (unsigned char)buf[3];
            int frame = 4 + pkt_len;
            if (pkt_len < 0 || frame > buf_sz) {
                return -1;
            }
            while (used < frame) {
                n = recv(fd, buf + used, (size_t)(frame - used), 0);
                if (n <= 0) {
                    return -1;
                }
                used += n;
            }
            used -= frame;
            if (used > 0) {
                memmove(buf, buf + frame, (size_t)used);
            }
            buf[used] = '\0';
        }
        if (used > 0 && buf[0] == '$') {
            continue; /* still assembling `$` header */
        }

        char *hdr_end = strstr(buf, "\r\n\r\n");
        if (hdr_end == NULL) {
            continue;
        }
        int hdr_len = (int)(hdr_end - buf) + 4;
        int content_length = 0;
        for (char *p = buf; p < hdr_end;) {
            char *nl = strstr(p, "\r\n");
            if (nl == NULL || nl > hdr_end) {
                break;
            }
            if (strncasecmp(p, "Content-Length:", 15) == 0) {
                content_length = atoi(p + 15);
            }
            p = nl + 2;
        }
        while (used < hdr_len + content_length && used < buf_sz - 1) {
            n = recv(fd, buf + used, (size_t)(buf_sz - 1 - used), 0);
            if (n <= 0) {
                return -1;
            }
            used += n;
        }
        buf[used] = '\0';
        if (content_length > 0) {
            *body_out = buf + hdr_len;
            *body_len = content_length;
        }
        return used;
    }
    return -1;
}

static int rtsp_status_code(const char *resp)
{
    const char *p = strchr(resp, ' ');
    return p ? atoi(p + 1) : -1;
}

static bool parse_www_auth(const char *resp, char *realm, size_t realm_sz, char *nonce, size_t nonce_sz)
{
    const char *line = strcasestr_local(resp, "WWW-Authenticate:");
    if (line == NULL) {
        return false;
    }
    const char *r = strstr(line, "realm=\"");
    const char *n = strstr(line, "nonce=\"");
    if (r == NULL || n == NULL) {
        return false;
    }
    r += 7;
    n += 7;
    const char *re = strchr(r, '"');
    const char *ne = strchr(n, '"');
    if (re == NULL || ne == NULL) {
        return false;
    }
    size_t rl = (size_t)(re - r);
    size_t nl = (size_t)(ne - n);
    if (rl >= realm_sz || nl >= nonce_sz) {
        return false;
    }
    memcpy(realm, r, rl);
    realm[rl] = '\0';
    memcpy(nonce, n, nl);
    nonce[nl] = '\0';
    return true;
}

static void make_digest(const char *method, const char *uri, const char *realm, const char *nonce,
                        char *out, size_t out_sz)
{
    char ha1[33], ha2[33], resp[33];
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s:%s:%s", s_user, realm, s_password);
    md5_hex(tmp, ha1);
    snprintf(tmp, sizeof(tmp), "%s:%s", method, uri);
    md5_hex(tmp, ha2);
    snprintf(tmp, sizeof(tmp), "%s:%s:%s", ha1, nonce, ha2);
    md5_hex(tmp, resp);
    snprintf(out, out_sz,
             "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"", s_user, realm,
             nonce, uri, resp);
}

static bool extract_header_value(const char *resp, const char *key, char *out, size_t out_sz)
{
    size_t key_len = strlen(key);
    for (const char *p = resp; *p;) {
        const char *nl = strstr(p, "\r\n");
        if (nl == NULL) {
            break;
        }
        if (strncasecmp(p, key, key_len) == 0 && p[key_len] == ':') {
            const char *v = p + key_len + 1;
            while (*v == ' ') {
                v++;
            }
            size_t n = (size_t)(nl - v);
            /* Session may have ;timeout= */
            const char *semi = memchr(v, ';', n);
            if (semi != NULL) {
                n = (size_t)(semi - v);
            }
            if (n >= out_sz) {
                n = out_sz - 1;
            }
            memcpy(out, v, n);
            out[n] = '\0';
            return true;
        }
        if (nl[0] == '\r' && nl[1] == '\n' && nl[2] == '\r' && nl[3] == '\n') {
            break;
        }
        p = nl + 2;
    }
    return false;
}

static bool parse_sdp_audio(const char *sdp, char *control, size_t ctrl_sz, int *sample_rate)
{
    *sample_rate = 16000;
    control[0] = '\0';

    bool in_audio = false;
    for (const char *p = sdp; *p;) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        while (len > 0 && (p[len - 1] == '\r' || p[len - 1] == '\n')) {
            len--;
        }
        if (len >= 7 && strncmp(p, "m=audio", 7) == 0) {
            in_audio = true;
        } else if (len >= 2 && p[0] == 'm' && p[1] == '=') {
            in_audio = false;
        } else if (in_audio) {
            if (len > 9 && strncmp(p, "a=rtpmap:", 9) == 0) {
                const char *slash = memchr(p, '/', len);
                if (slash != NULL) {
                    *sample_rate = atoi(slash + 1);
                }
            } else if (len > 10 && strncmp(p, "a=control:", 10) == 0) {
                size_t n = len - 10;
                if (n >= ctrl_sz) {
                    n = ctrl_sz - 1;
                }
                memcpy(control, p + 10, n);
                control[n] = '\0';
            }
        }
        if (nl == NULL) {
            break;
        }
        p = nl + 1;
    }
    return control[0] != '\0';
}

static void prepend_adts(uint8_t *dst, const uint8_t *aac, int aac_len, int sample_rate, int channels)
{
    /* AAC-LC ADTS header for raw AAC access units from RTP. */
    static const int sfreq_table[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                                      16000, 12000, 11025, 8000,  7350};
    int sfi = 8; /* 16000 default */
    for (int i = 0; i < (int)(sizeof(sfreq_table) / sizeof(sfreq_table[0])); i++) {
        if (sfreq_table[i] == sample_rate) {
            sfi = i;
            break;
        }
    }
    int frame_len = aac_len + 7;
    dst[0] = 0xFF;
    dst[1] = 0xF1; /* MPEG-4, no CRC */
    dst[2] = (uint8_t)(((2 /* AAC LC */) << 6) | (sfi << 2) | ((channels >> 2) & 0x1));
    dst[3] = (uint8_t)(((channels & 0x3) << 6) | ((frame_len >> 11) & 0x3));
    dst[4] = (uint8_t)((frame_len >> 3) & 0xFF);
    dst[5] = (uint8_t)(((frame_len & 0x7) << 5) | 0x1F);
    dst[6] = 0xFC;
    memcpy(dst + 7, aac, (size_t)aac_len);
}

/** Extract AAC payload from RFC3640 AAC-hbr RTP (sizelength=13,indexlength=3,indexdeltalength=3). */
static int aac_hbr_extract(const uint8_t *rtp_payload, int payload_len, uint8_t *out, int out_cap)
{
    if (payload_len < 4) {
        return -1;
    }
    int au_headers_length_bits = (rtp_payload[0] << 8) | rtp_payload[1];
    int au_headers_bytes = (au_headers_length_bits + 7) / 8;
    int data_offset = 2 + au_headers_bytes;
    if (data_offset > payload_len) {
        return -1;
    }
    /* First AU size: 13 bits starting at bit 0 of AU headers section. */
    const uint8_t *auh = rtp_payload + 2;
    int au_size = (auh[0] << 5) | (auh[1] >> 3);
    if (au_size <= 0 || data_offset + au_size > payload_len) {
        /* Fallback: treat remaining as single AU if headers look odd. */
        au_size = payload_len - data_offset;
    }
    if (au_size <= 0 || au_size > out_cap) {
        return -1;
    }
    memcpy(out, rtp_payload + data_offset, (size_t)au_size);
    return au_size;
}

static bool ensure_speaker(void)
{
    if (s_spk_inited && s_spk != NULL) {
        return true;
    }
    s_spk = bsp_audio_codec_speaker_init();
    if (s_spk == NULL) {
        ESP_LOGE(TAG, "speaker init failed");
        return false;
    }
    (void)esp_codec_dev_set_out_vol(s_spk, SPEAKER_VOLUME);
    s_spk_inited = true;
    ESP_LOGI(TAG, "speaker ready");
    return true;
}

/** Block until STA has an IPv4 address (or stop requested). */
static bool wait_for_sta_ip(void)
{
    while (s_running) {
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif != NULL) {
            esp_netif_ip_info_t ip = {0};
            if (esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr != 0) {
                return true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    return false;
}

static void session_cleanup(int *fd, void **dec, bool *codec_open)
{
    if (*codec_open && s_spk != NULL) {
        esp_codec_dev_close(s_spk);
        *codec_open = false;
    }
    if (*dec != NULL) {
        esp_aac_dec_close(*dec);
        *dec = NULL;
    }
    if (*fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

static void reolink_audio_task(void *arg)
{
    (void)arg;
    char *recv_buf = NULL;
    uint8_t *aac_au = NULL;
    uint8_t *adts = NULL;
    uint8_t *pcm = NULL;
    void *dec = NULL;
    int fd = -1;
    bool codec_open = false;
    char play_uri[220] = {0};
    char session[96] = {0};
    int cseq = 1;

    if (!ensure_speaker()) {
        goto done;
    }

    recv_buf = (char *)heap_caps_malloc(RTSP_RECV_BUF, MALLOC_CAP_DEFAULT);
    aac_au = (uint8_t *)heap_caps_malloc(2048, MALLOC_CAP_DEFAULT);
    adts = (uint8_t *)heap_caps_malloc(2048 + 8, MALLOC_CAP_DEFAULT);
    pcm = (uint8_t *)heap_caps_malloc(AAC_PCM_OUT_MAX, MALLOC_CAP_DEFAULT);
    if (recv_buf == NULL || aac_au == NULL || adts == NULL || pcm == NULL) {
        ESP_LOGE(TAG, "alloc failed");
        goto done;
    }

    char uri[160];
    snprintf(uri, sizeof(uri), "rtsp://%s:554/h264Preview_01_sub", s_host);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(554);
    if (inet_aton(s_host, &addr.sin_addr) == 0) {
        ESP_LOGE(TAG, "bad host %s", s_host);
        goto done;
    }

reconnect:
    session_cleanup(&fd, &dec, &codec_open);
    play_uri[0] = '\0';
    session[0] = '\0';
    cseq = 1;

    if (!s_running) {
        goto done;
    }
    if (!wait_for_sta_ip()) {
        goto done;
    }

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        ESP_LOGE(TAG, "socket failed");
        vTaskDelay(pdMS_TO_TICKS(2000));
        goto reconnect;
    }
    struct timeval tv = {.tv_sec = 8, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGW(TAG, "connect %s:554 failed (errno=%d), retrying", s_host, errno);
        session_cleanup(&fd, &dec, &codec_open);
        vTaskDelay(pdMS_TO_TICKS(2000));
        goto reconnect;
    }

    char req[1024];
    char *body = NULL;
    int body_len = 0;

    /* DESCRIBE (expect 401 then digest retry). */
    snprintf(req, sizeof(req),
             "DESCRIBE %s RTSP/1.0\r\n"
             "CSeq: %d\r\n"
             "Accept: application/sdp\r\n"
             "User-Agent: ESP-HMI\r\n"
             "\r\n",
             uri, cseq++);
    if (sock_send_all(fd, req, (int)strlen(req)) < 0 ||
        rtsp_read_response(fd, recv_buf, RTSP_RECV_BUF, &body, &body_len) < 0) {
        ESP_LOGE(TAG, "DESCRIBE failed");
        goto teardown;
    }

    char realm[96] = {0}, nonce[96] = {0}, auth[384] = {0};
    if (rtsp_status_code(recv_buf) == 401) {
        if (!parse_www_auth(recv_buf, realm, sizeof(realm), nonce, sizeof(nonce))) {
            ESP_LOGE(TAG, "no digest challenge");
            goto teardown;
        }
        make_digest("DESCRIBE", uri, realm, nonce, auth, sizeof(auth));
        snprintf(req, sizeof(req),
                 "DESCRIBE %s RTSP/1.0\r\n"
                 "CSeq: %d\r\n"
                 "Accept: application/sdp\r\n"
                 "Authorization: %s\r\n"
                 "User-Agent: ESP-HMI\r\n"
                 "\r\n",
                 uri, cseq++, auth);
        if (sock_send_all(fd, req, (int)strlen(req)) < 0 ||
            rtsp_read_response(fd, recv_buf, RTSP_RECV_BUF, &body, &body_len) < 0 ||
            rtsp_status_code(recv_buf) != 200) {
            ESP_LOGE(TAG, "DESCRIBE auth failed: %.80s", recv_buf);
            goto teardown;
        }
    } else if (rtsp_status_code(recv_buf) != 200) {
        ESP_LOGE(TAG, "DESCRIBE status %d", rtsp_status_code(recv_buf));
        goto teardown;
    }

    char content_base[160] = {0};
    extract_header_value(recv_buf, "Content-Base", content_base, sizeof(content_base));
    char control[64] = {0};
    int sample_rate = 16000;
    if (body == NULL || !parse_sdp_audio(body, control, sizeof(control), &sample_rate)) {
        ESP_LOGE(TAG, "SDP has no audio control");
        goto teardown;
    }

    char setup_uri[288];
    if (strncmp(control, "rtsp://", 7) == 0) {
        snprintf(setup_uri, sizeof(setup_uri), "%s", control);
    } else if (content_base[0] != '\0') {
        size_t bl = strlen(content_base);
        if (bl > 0 && content_base[bl - 1] == '/') {
            snprintf(setup_uri, sizeof(setup_uri), "%s%s", content_base, control);
        } else {
            snprintf(setup_uri, sizeof(setup_uri), "%s/%s", content_base, control);
        }
    } else {
        snprintf(setup_uri, sizeof(setup_uri), "rtsp://%s:554/Preview_01_sub/%s", s_host, control);
    }

    /* Reolink returns 400 (not 401) for unauthenticated SETUP — send digest on first try. */
    if (realm[0] != '\0' && nonce[0] != '\0') {
        make_digest("SETUP", setup_uri, realm, nonce, auth, sizeof(auth));
        snprintf(req, sizeof(req),
                 "SETUP %s RTSP/1.0\r\n"
                 "CSeq: %d\r\n"
                 "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
                 "Authorization: %s\r\n"
                 "User-Agent: ESP-HMI\r\n"
                 "\r\n",
                 setup_uri, cseq++, auth);
    } else {
        snprintf(req, sizeof(req),
                 "SETUP %s RTSP/1.0\r\n"
                 "CSeq: %d\r\n"
                 "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
                 "User-Agent: ESP-HMI\r\n"
                 "\r\n",
                 setup_uri, cseq++);
    }
    if (sock_send_all(fd, req, (int)strlen(req)) < 0 ||
        rtsp_read_response(fd, recv_buf, RTSP_RECV_BUF, &body, &body_len) < 0) {
        ESP_LOGE(TAG, "SETUP failed");
        goto teardown;
    }
    if (rtsp_status_code(recv_buf) == 401) {
        if (!parse_www_auth(recv_buf, realm, sizeof(realm), nonce, sizeof(nonce))) {
            ESP_LOGE(TAG, "SETUP no digest");
            goto teardown;
        }
        make_digest("SETUP", setup_uri, realm, nonce, auth, sizeof(auth));
        snprintf(req, sizeof(req),
                 "SETUP %s RTSP/1.0\r\n"
                 "CSeq: %d\r\n"
                 "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
                 "Authorization: %s\r\n"
                 "User-Agent: ESP-HMI\r\n"
                 "\r\n",
                 setup_uri, cseq++, auth);
        if (sock_send_all(fd, req, (int)strlen(req)) < 0 ||
            rtsp_read_response(fd, recv_buf, RTSP_RECV_BUF, &body, &body_len) < 0 ||
            rtsp_status_code(recv_buf) != 200) {
            ESP_LOGE(TAG, "SETUP auth failed: %.120s", recv_buf);
            goto teardown;
        }
    } else if (rtsp_status_code(recv_buf) != 200) {
        ESP_LOGE(TAG, "SETUP status %d: %.120s", rtsp_status_code(recv_buf), recv_buf);
        goto teardown;
    }

    if (!extract_header_value(recv_buf, "Session", session, sizeof(session))) {
        ESP_LOGE(TAG, "no Session header");
        goto teardown;
    }

    /* PLAY on Content-Base aggregate control when available. */
    if (content_base[0] != '\0') {
        snprintf(play_uri, sizeof(play_uri), "%s", content_base);
        size_t pl = strlen(play_uri);
        while (pl > 0 && play_uri[pl - 1] == '/') {
            play_uri[--pl] = '\0';
        }
    } else {
        snprintf(play_uri, sizeof(play_uri), "%s", uri);
    }

    /* Same as SETUP: Reolink may 400 unauthenticated PLAY — send digest when available. */
    if (realm[0] != '\0' && nonce[0] != '\0') {
        make_digest("PLAY", play_uri, realm, nonce, auth, sizeof(auth));
        snprintf(req, sizeof(req),
                 "PLAY %s RTSP/1.0\r\n"
                 "CSeq: %d\r\n"
                 "Session: %s\r\n"
                 "Authorization: %s\r\n"
                 "User-Agent: ESP-HMI\r\n"
                 "\r\n",
                 play_uri, cseq++, session, auth);
    } else {
        snprintf(req, sizeof(req),
                 "PLAY %s RTSP/1.0\r\n"
                 "CSeq: %d\r\n"
                 "Session: %s\r\n"
                 "User-Agent: ESP-HMI\r\n"
                 "\r\n",
                 play_uri, cseq++, session);
    }
    if (sock_send_all(fd, req, (int)strlen(req)) < 0 ||
        rtsp_read_response(fd, recv_buf, RTSP_RECV_BUF, &body, &body_len) < 0) {
        ESP_LOGE(TAG, "PLAY failed");
        goto teardown;
    }
    if (rtsp_status_code(recv_buf) == 401) {
        if (!parse_www_auth(recv_buf, realm, sizeof(realm), nonce, sizeof(nonce))) {
            goto teardown;
        }
        make_digest("PLAY", play_uri, realm, nonce, auth, sizeof(auth));
        snprintf(req, sizeof(req),
                 "PLAY %s RTSP/1.0\r\n"
                 "CSeq: %d\r\n"
                 "Session: %s\r\n"
                 "Authorization: %s\r\n"
                 "User-Agent: ESP-HMI\r\n"
                 "\r\n",
                 play_uri, cseq++, session, auth);
        if (sock_send_all(fd, req, (int)strlen(req)) < 0 ||
            rtsp_read_response(fd, recv_buf, RTSP_RECV_BUF, &body, &body_len) < 0 ||
            rtsp_status_code(recv_buf) != 200) {
            ESP_LOGE(TAG, "PLAY auth failed");
            goto teardown;
        }
    } else if (rtsp_status_code(recv_buf) != 200) {
        ESP_LOGE(TAG, "PLAY status %d", rtsp_status_code(recv_buf));
        goto teardown;
    }

    ESP_LOGI(TAG, "RTSP PLAY ok — listening AAC@%d from %s", sample_rate, s_host);

    esp_aac_dec_cfg_t acfg = {
        .sample_rate = sample_rate,
        .channel = 1,
        .bits_per_sample = 16,
        .no_adts_header = false,
        .aac_plus_enable = false,
    };
    if (esp_aac_dec_open(&acfg, sizeof(acfg), &dec) != ESP_AUDIO_ERR_OK || dec == NULL) {
        ESP_LOGE(TAG, "AAC decoder open failed");
        goto teardown;
    }

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
        .sample_rate = (uint32_t)sample_rate,
        .mclk_multiple = 0,
    };
    if (esp_codec_dev_open(s_spk, &fs) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "speaker open @%d failed", sample_rate);
        goto teardown;
    }
    codec_open = true;

    /* Interleaved RTP loop: $ ch len_hi len_lo | RTP */
    uint8_t sync[4];
    int sync_fill = 0;
    while (s_running) {
        uint8_t b;
        int n = recv(fd, &b, 1, 0);
        if (n <= 0) {
            ESP_LOGW(TAG, "RTSP stream ended");
            break;
        }
        if (sync_fill == 0) {
            if (b == '$') {
                sync[0] = b;
                sync_fill = 1;
            } else {
                /* Possible RTSP response interleaved; skip until $ */
            }
            continue;
        }
        sync[sync_fill++] = b;
        if (sync_fill < 4) {
            continue;
        }
        sync_fill = 0;
        int channel = sync[1];
        int pkt_len = (sync[2] << 8) | sync[3];
        if (pkt_len <= 0 || pkt_len > RTSP_RECV_BUF) {
            ESP_LOGW(TAG, "bad interleaved len %d", pkt_len);
            break;
        }
        int got = 0;
        while (got < pkt_len) {
            n = recv(fd, recv_buf + got, (size_t)(pkt_len - got), 0);
            if (n <= 0) {
                goto teardown;
            }
            got += n;
        }
        if (channel != 0) {
            continue; /* RTCP on odd channel */
        }
        if (pkt_len < 12) {
            continue;
        }
        const uint8_t *rtp = (const uint8_t *)recv_buf;
        int hdr_len = 12 + 4 * (rtp[0] & 0x0F);
        if (hdr_len >= pkt_len) {
            continue;
        }
        int aac_len = aac_hbr_extract(rtp + hdr_len, pkt_len - hdr_len, aac_au, 2048);
        if (aac_len <= 0) {
            continue;
        }
        prepend_adts(adts, aac_au, aac_len, sample_rate, 1);
        int adts_len = aac_len + 7;

        esp_audio_dec_in_raw_t raw = {
            .buffer = adts,
            .len = (uint32_t)adts_len,
            .consumed = 0,
            .frame_recover = ESP_AUDIO_DEC_RECOVERY_NONE,
        };
        esp_audio_dec_out_frame_t frame = {
            .buffer = pcm,
            .len = AAC_PCM_OUT_MAX,
            .decoded_size = 0,
        };
        esp_audio_dec_info_t info = {0};
        esp_audio_err_t er = esp_aac_dec_decode(dec, &raw, &frame, &info);
        if (er == ESP_AUDIO_ERR_OK && frame.decoded_size > 0) {
            (void)esp_codec_dev_write(s_spk, pcm, (int)frame.decoded_size);
        }
    }

teardown:
    if (fd >= 0 && session[0] != '\0') {
        snprintf(req, sizeof(req),
                 "TEARDOWN %s RTSP/1.0\r\n"
                 "CSeq: %d\r\n"
                 "Session: %s\r\n"
                 "\r\n",
                 play_uri[0] ? play_uri : uri, cseq++, session);
        (void)sock_send_all(fd, req, (int)strlen(req));
    }
    if (s_running) {
        ESP_LOGI(TAG, "RTSP session ended — reconnecting in 2s");
        vTaskDelay(pdMS_TO_TICKS(2000));
        goto reconnect;
    }

done:
    session_cleanup(&fd, &dec, &codec_open);
    heap_caps_free(recv_buf);
    heap_caps_free(aac_au);
    heap_caps_free(adts);
    heap_caps_free(pcm);
    ESP_LOGI(TAG, "audio task exit");
    s_running = false;
    s_task = NULL;
    vTaskDelete(NULL);
}

void reolink_audio_init(void)
{
    (void)ensure_speaker();
}

void reolink_audio_start(const reolink_audio_config_t *cfg)
{
    if (cfg == NULL || cfg->host == NULL || cfg->host[0] == '\0') {
        ESP_LOGW(TAG, "no host — camera audio disabled");
        return;
    }
    snprintf(s_host, sizeof(s_host), "%s", cfg->host);
    snprintf(s_user, sizeof(s_user), "%s", cfg->user != NULL ? cfg->user : "admin");
    snprintf(s_password, sizeof(s_password), "%s", cfg->password != NULL ? cfg->password : "");

    reolink_audio_init();
    s_running = true;
    if (s_task != NULL) {
        return;
    }
    BaseType_t ok = xTaskCreatePinnedToCore(reolink_audio_task, "reolink_aud", 12288, NULL, 5, &s_task, 0);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        s_running = false;
        s_task = NULL;
    }
}

void reolink_audio_stop(void)
{
    s_running = false;
}
