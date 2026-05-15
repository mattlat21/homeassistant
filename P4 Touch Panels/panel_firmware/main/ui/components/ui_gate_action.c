#include "ui/components/ui_gate_action.h"

#include "ha_mqtt.h"
#include <strings.h>
#include <string.h>

#define UI_GATE_ACTION_COOLDOWN_MS 5000
#define UI_GATE_ACTION_BIND_MAX 2
#define UI_GATE_ACTION_STATUS_PILL_BG_OPA LV_OPA_60

typedef enum {
    UI_GATE_ACTION_STATE_CLOSED = 0,
    UI_GATE_ACTION_STATE_PARTIAL,
    UI_GATE_ACTION_STATE_OPEN,
} ui_gate_action_state_t;

typedef struct {
    lv_obj_t *btn;
    lv_obj_t *label;
} ui_gate_action_binding_t;

static ui_gate_action_binding_t s_bindings[UI_GATE_ACTION_BIND_MAX];
static size_t s_binding_count;

static lv_obj_t *s_status_pill;
static lv_obj_t *s_status_label;

static ui_gate_action_state_t s_gate_state = UI_GATE_ACTION_STATE_CLOSED;
static lv_timer_t *s_unlock_timer;

static const char *s_status_text = "Closed";
static lv_color_t s_status_bg;
static lv_color_t s_status_fg;

static const char *action_payload_for_state(ui_gate_action_state_t state)
{
    if (state == UI_GATE_ACTION_STATE_OPEN) {
        return "Front Gate Close";
    }
    if (state == UI_GATE_ACTION_STATE_PARTIAL) {
        return "Front Gate Activate";
    }
    return "Front Gate Open";
}

static const char *action_text_for_state(ui_gate_action_state_t state)
{
    if (state == UI_GATE_ACTION_STATE_OPEN) {
        return "Close Gate";
    }
    if (state == UI_GATE_ACTION_STATE_PARTIAL) {
        return "Activate Gate";
    }
    return "Open Gate";
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

static void refresh_action_labels(void)
{
    const char *text = action_text_for_state(s_gate_state);
    for (size_t i = 0; i < s_binding_count; i++) {
        if (s_bindings[i].label != NULL) {
            lv_label_set_text(s_bindings[i].label, text);
        }
    }
}

static void set_buttons_disabled(bool disabled)
{
    for (size_t i = 0; i < s_binding_count; i++) {
        if (s_bindings[i].btn == NULL) {
            continue;
        }
        if (disabled) {
            lv_obj_add_state(s_bindings[i].btn, LV_STATE_DISABLED);
        } else {
            lv_obj_remove_state(s_bindings[i].btn, LV_STATE_DISABLED);
        }
    }
}

static bool any_bound_button_disabled(void)
{
    for (size_t i = 0; i < s_binding_count; i++) {
        if (s_bindings[i].btn != NULL && lv_obj_has_state(s_bindings[i].btn, LV_STATE_DISABLED)) {
            return true;
        }
    }
    return false;
}

static void refresh_status_pill_widgets(void)
{
    if (s_status_pill == NULL || s_status_label == NULL) {
        return;
    }
    lv_label_set_text(s_status_label, s_status_text);
    lv_obj_set_style_bg_color(s_status_pill, s_status_bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_status_pill, UI_GATE_ACTION_STATUS_PILL_BG_OPA, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_status_label, s_status_fg, LV_PART_MAIN);
}

static void apply_parsed_state(const char *normalized)
{
    s_status_text = "Closed";
    s_status_bg = lv_color_hex(0xFBC02D);
    s_status_fg = lv_color_black();

    if (strcasecmp(normalized, "Closed") == 0) {
        s_gate_state = UI_GATE_ACTION_STATE_CLOSED;
    } else if (strcasecmp(normalized, "Partially Open") == 0) {
        s_gate_state = UI_GATE_ACTION_STATE_PARTIAL;
        s_status_text = "Partially Open";
        s_status_bg = lv_color_hex(0x1E88E5);
        s_status_fg = lv_color_white();
    } else if (strcasecmp(normalized, "open") == 0) {
        s_gate_state = UI_GATE_ACTION_STATE_OPEN;
        s_status_text = "Gate Open";
        s_status_bg = lv_color_hex(0x2E7D32);
        s_status_fg = lv_color_white();
    }

    refresh_action_labels();
    refresh_status_pill_widgets();
}

static void unlock_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    set_buttons_disabled(false);
    if (s_unlock_timer != NULL) {
        lv_timer_del(s_unlock_timer);
        s_unlock_timer = NULL;
    }
}

static void gate_state_on_mqtt(const char *state, void *user_data)
{
    (void)user_data;
    if (state == NULL) {
        return;
    }

    char normalized[64];
    strncpy(normalized, state, sizeof(normalized) - 1);
    normalized[sizeof(normalized) - 1] = '\0';
    normalize_gate_state(normalized);
    apply_parsed_state(normalized);
}

void ui_gate_action_init(void)
{
    s_binding_count = 0;
    s_status_pill = NULL;
    s_status_label = NULL;
    s_status_bg = lv_color_hex(0xFBC02D);
    s_status_fg = lv_color_black();
    ha_mqtt_set_front_gate_state_callback(gate_state_on_mqtt, NULL);
}

void ui_gate_action_bind(lv_obj_t *btn, lv_obj_t *label)
{
    if (btn == NULL || label == NULL || s_binding_count >= UI_GATE_ACTION_BIND_MAX) {
        return;
    }
    s_bindings[s_binding_count].btn = btn;
    s_bindings[s_binding_count].label = label;
    s_binding_count++;
    lv_label_set_text(label, action_text_for_state(s_gate_state));
    if (any_bound_button_disabled()) {
        lv_obj_add_state(btn, LV_STATE_DISABLED);
    }
}

void ui_gate_action_set_status_pill(lv_obj_t *pill, lv_obj_t *label)
{
    s_status_pill = pill;
    s_status_label = label;
    refresh_status_pill_widgets();
}

void ui_gate_action_on_click(void)
{
    for (size_t i = 0; i < s_binding_count; i++) {
        if (s_bindings[i].btn != NULL && lv_obj_has_state(s_bindings[i].btn, LV_STATE_DISABLED)) {
            return;
        }
    }

    (void)ha_mqtt_publish_ollie_button(action_payload_for_state(s_gate_state));

    set_buttons_disabled(true);
    if (s_unlock_timer != NULL) {
        lv_timer_del(s_unlock_timer);
        s_unlock_timer = NULL;
    }
    s_unlock_timer = lv_timer_create(unlock_timer_cb, UI_GATE_ACTION_COOLDOWN_MS, NULL);
    if (s_unlock_timer != NULL) {
        lv_timer_set_repeat_count(s_unlock_timer, 1);
    }
}
