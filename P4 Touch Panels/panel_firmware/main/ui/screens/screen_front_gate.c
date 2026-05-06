#include "ui/screens/screen_front_gate.h"

#include "ha_mqtt.h"
#include "reolink_preview.h"
#include "ui/components/ui_status_bar.h"
#include "ui/ui_brand_gradient.h"
#include "bsp/display.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include <strings.h>
#include <string.h>

/** Preview frame as % of full screen; leaves lower area for future controls. */
#define FRONT_GATE_PREVIEW_W_PCT 80 // 65
#define FRONT_GATE_PREVIEW_H_PCT 60 //45

/** Shared corner radius for the status pill and the gate action button. */
#define FRONT_GATE_CORNER_RADIUS 20
/** Gate action button disabled cooldown after press (ms). */
#define FRONT_GATE_ACTION_COOLDOWN_MS 5000
/** Semi-transparent overlay on the gate status pill (video shows through). */
#define FRONT_GATE_STATUS_PILL_BG_OPA LV_OPA_60
#define FRONT_GATE_STATUS_LABEL_TEXT_OPA LV_OPA_90

typedef enum {
    FRONT_GATE_STATE_CLOSED = 0,
    FRONT_GATE_STATE_PARTIAL,
    FRONT_GATE_STATE_OPEN,
} front_gate_state_t;

static lv_obj_t *s_gate_state_pill;
static lv_obj_t *s_gate_state_label;
static lv_obj_t *s_gate_action_btn;
static lv_obj_t *s_gate_action_label;
static front_gate_state_t s_gate_state = FRONT_GATE_STATE_CLOSED;
static lv_timer_t *s_gate_action_unlock_timer;

static const char *front_gate_action_payload_for_state(front_gate_state_t state)
{
    if (state == FRONT_GATE_STATE_OPEN) {
        return "Front Gate Close";
    }
    if (state == FRONT_GATE_STATE_PARTIAL) {
        return "Front Gate Activate";
    }
    return "Front Gate Open";
}

static const char *front_gate_action_text_for_state(front_gate_state_t state)
{
    if (state == FRONT_GATE_STATE_OPEN) {
        return "Close Gate";
    }
    if (state == FRONT_GATE_STATE_PARTIAL) {
        return "Activate Gate";
    }
    return "Open Gate";
}

static void front_gate_action_refresh_ui(void)
{
    if (s_gate_action_btn == NULL || s_gate_action_label == NULL) {
        return;
    }
    lv_label_set_text(s_gate_action_label, front_gate_action_text_for_state(s_gate_state));
}

static void front_gate_action_unlock_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_gate_action_btn != NULL) {
        lv_obj_remove_state(s_gate_action_btn, LV_STATE_DISABLED);
    }
    if (s_gate_action_unlock_timer != NULL) {
        lv_timer_del(s_gate_action_unlock_timer);
        s_gate_action_unlock_timer = NULL;
    }
}

static void front_gate_action_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (s_gate_action_btn == NULL || lv_obj_has_state(s_gate_action_btn, LV_STATE_DISABLED)) {
        return;
    }

    const char *payload = front_gate_action_payload_for_state(s_gate_state);
    (void)ha_mqtt_publish_ollie_button(payload);

    lv_obj_add_state(s_gate_action_btn, LV_STATE_DISABLED);
    if (s_gate_action_unlock_timer != NULL) {
        lv_timer_del(s_gate_action_unlock_timer);
        s_gate_action_unlock_timer = NULL;
    }
    s_gate_action_unlock_timer = lv_timer_create(front_gate_action_unlock_timer_cb, FRONT_GATE_ACTION_COOLDOWN_MS, NULL);
    if (s_gate_action_unlock_timer != NULL) {
        lv_timer_set_repeat_count(s_gate_action_unlock_timer, 1);
    }
}

static void normalize_gate_state(char *state)
{
    if (state == NULL) {
        return;
    }
    while (state[0] == '"' || state[0] == '\'' || state[0] == ':') {
        size_t n = strlen(state);
        memmove(state, state + 1, n);
    }
    size_t len = strlen(state);
    while (len > 0 && (state[len - 1] == '"' || state[len - 1] == '\'')) {
        state[--len] = '\0';
    }
}

static void front_gate_state_on_mqtt(const char *state, void *user_data)
{
    (void)user_data;
    if (s_gate_state_pill == NULL || s_gate_state_label == NULL || state == NULL) {
        return;
    }

    char normalized[64];
    strncpy(normalized, state, sizeof(normalized) - 1);
    normalized[sizeof(normalized) - 1] = '\0';
    normalize_gate_state(normalized);

    lv_color_t bg = lv_color_hex(0xFBC02D); /* Closed: yellow */
    lv_color_t fg = lv_color_white();
    const char *text = "Closed";

    if (strcasecmp(normalized, "Closed") == 0) {
        s_gate_state = FRONT_GATE_STATE_CLOSED;
        text = "Closed";
        bg = lv_color_hex(0xFBC02D); /* yellow */
        fg = lv_color_black();
    } else if (strcasecmp(normalized, "Partially Open") == 0) {
        s_gate_state = FRONT_GATE_STATE_PARTIAL;
        text = "Partially Open";
        bg = lv_color_hex(0x1E88E5); /* blue */
    } else if (strcasecmp(normalized, "open") == 0) {
        s_gate_state = FRONT_GATE_STATE_OPEN;
        text = "Gate Open";
        bg = lv_color_hex(0x2E7D32); /* green */
        fg = lv_color_white();
    }

    lv_label_set_text(s_gate_state_label, text);
    lv_obj_set_style_bg_color(s_gate_state_pill, bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_gate_state_pill, FRONT_GATE_STATUS_PILL_BG_OPA, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_gate_state_label, fg, LV_PART_MAIN);
    front_gate_action_refresh_ui();
}

lv_obj_t *screen_front_gate_create(lv_display_t *disp)
{
    (void)disp;
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    ui_brand_gradient_apply(scr);

    // (void)ui_status_bar_create(scr);

    const int32_t box_w = 720; // (BSP_LCD_H_RES * FRONT_GATE_PREVIEW_W_PCT) / 100;
    const int32_t box_h = 480; // (BSP_LCD_V_RES * FRONT_GATE_PREVIEW_H_PCT) / 100;

    lv_obj_t *box = lv_obj_create(scr);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, box_w, box_h);
    lv_obj_align(box, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(box, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(box, lv_color_hex(0x8E8E93), LV_PART_MAIN);
    lv_obj_set_style_radius(box, 40, LV_PART_MAIN);
    lv_obj_set_style_pad_all(box, 0, LV_PART_MAIN);
    /* LVGL 9: children clip to parent by default; no LV_OBJ_FLAG_OVERFLOW_HIDDEN. */

    const size_t buf_sz = (size_t)REOLINK_PREVIEW_W * REOLINK_PREVIEW_H * 3;
    uint8_t *canvas_buf =
        (uint8_t *)heap_caps_malloc(buf_sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (canvas_buf == NULL) {
        canvas_buf = (uint8_t *)heap_caps_malloc(buf_sz, MALLOC_CAP_8BIT);
    }

    lv_obj_t *canvas = NULL;
    if (canvas_buf != NULL) {
        canvas = lv_canvas_create(box);
        memset(canvas_buf, 0x55, buf_sz);
        lv_canvas_set_buffer(canvas, canvas_buf, REOLINK_PREVIEW_W, REOLINK_PREVIEW_H, LV_COLOR_FORMAT_RGB888);
        lv_obj_center(canvas);
    } else {
        lv_obj_t *lbl = lv_label_create(box);
        lv_label_set_text(lbl, "No RAM for preview");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_center(lbl);
    }

    /* Gate status pill. */
    s_gate_state_pill = lv_obj_create(box);
    lv_obj_remove_style_all(s_gate_state_pill);
    lv_obj_set_size(s_gate_state_pill, 320, 72);
    lv_obj_set_style_bg_color(s_gate_state_pill, lv_color_hex(0x1E88E5), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_gate_state_pill, FRONT_GATE_STATUS_PILL_BG_OPA, LV_PART_MAIN);
    lv_obj_set_style_radius(s_gate_state_pill, FRONT_GATE_CORNER_RADIUS, LV_PART_MAIN);
    // lv_obj_set_style_min_width(s_gate_state_pill, 340, LV_PART_MAIN);
    // lv_obj_set_style_pad_ver(s_gate_state_pill, 4, LV_PART_MAIN);
    // lv_obj_set_style_pad_hor(s_gate_state_pill, 16, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_gate_state_pill, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_gate_state_pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_gate_state_pill, LV_ALIGN_BOTTOM_LEFT, 20, -20);

    /* Gate status label. */
    s_gate_state_label = lv_label_create(s_gate_state_pill);
    lv_label_set_text(s_gate_state_label, "Closed");
    lv_obj_set_style_text_font(s_gate_state_label, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_opa(s_gate_state_label, FRONT_GATE_STATUS_LABEL_TEXT_OPA, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_gate_state_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(s_gate_state_label);

    /* Gate action button: strip default theme (avoids stray 1px border/fringe around edges). */
    s_gate_action_btn = lv_button_create(scr);
    lv_obj_remove_style_all(s_gate_action_btn);
    lv_obj_set_size(s_gate_action_btn, 635, 185);
    lv_obj_align_to(s_gate_action_btn, box, LV_ALIGN_OUT_BOTTOM_MID, 0, 28);
    lv_obj_set_style_radius(s_gate_action_btn, FRONT_GATE_CORNER_RADIUS, LV_PART_MAIN);
    // lv_obj_set_style_clip_corner(s_gate_action_btn, true, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_gate_action_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_gate_action_btn, LV_OPA_20, LV_PART_MAIN);
    // lv_obj_set_style_border_width(s_gate_action_btn, 0, LV_PART_MAIN);
    // lv_obj_set_style_outline_width(s_gate_action_btn, 0, LV_PART_MAIN);
    // lv_obj_set_style_shadow_width(s_gate_action_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_gate_action_btn, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(s_gate_action_btn, LV_OPA_60, LV_PART_MAIN | LV_STATE_DISABLED);
    // lv_obj_set_style_bg_color(s_gate_action_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    // lv_obj_set_style_bg_opa(s_gate_action_btn, LV_OPA_20, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(s_gate_action_btn, front_gate_action_clicked, LV_EVENT_CLICKED, NULL);

    /* Gate action label. */
    s_gate_action_label = lv_label_create(s_gate_action_btn);
    lv_obj_set_style_text_font(s_gate_action_label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_gate_action_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_gate_action_label, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_center(s_gate_action_label);
    front_gate_action_refresh_ui();

    ha_mqtt_set_front_gate_state_callback(front_gate_state_on_mqtt, NULL);

    reolink_preview_bind(scr, canvas);

    return scr;
}
