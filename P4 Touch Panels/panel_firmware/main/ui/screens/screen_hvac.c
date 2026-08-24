#include "ui/screens/screen_hvac.h"

#include <stdint.h>

#include "bsp/display.h"
#include "ha_mqtt.h"
#include "ui/components/ui_hvac_card_1.h"
#include "ui/components/ui_hvac_climate_modal.h"
#include "ui/components/ui_taskbar.h"

#define HVAC_GRID_COLS 4u
#define HVAC_GRID_ROWS 3u
#define HVAC_GRID_GAP 8
#define HVAC_GRID_PAD 8
#define HVAC_ZONE_COUNT 11u

typedef struct {
    const char *room_name;
    bool opens_modal;
    ui_hvac_climate_profile_t profile;
    /** NULL → Ollie primary `climate_*` buttons; otherwise e.g. `climate_bedroom_1`. */
    const char *btn_prefix;
} hvac_zone_def_t;

static const hvac_zone_def_t s_zone_defs[HVAC_ZONE_COUNT] = {
    { "Main Room", true, UI_HVAC_CLIMATE_PROFILE_HEAT_COOL_FAN_DRY, "climate_main_room" },
    { "Lounge Room", false, UI_HVAC_CLIMATE_PROFILE_HEATER, NULL },
    { "Our Bedroom", true, UI_HVAC_CLIMATE_PROFILE_HEATER, "climate_bedroom_1" },
    { "Penny's Room", false, UI_HVAC_CLIMATE_PROFILE_HEATER, NULL },
    { "Ollie's Room", true, UI_HVAC_CLIMATE_PROFILE_HEATER, NULL },
    { "Spare Room", false, UI_HVAC_CLIMATE_PROFILE_HEATER, NULL },
    { "Bathroom", false, UI_HVAC_CLIMATE_PROFILE_HEATER, NULL },
    { "Laundry", false, UI_HVAC_CLIMATE_PROFILE_HEATER, NULL },
    { "Upstairs Bedroom", true, UI_HVAC_CLIMATE_PROFILE_HEAT_COOL_FAN, "climate_upstairs_bedroom" },
    { "Study", true, UI_HVAC_CLIMATE_PROFILE_HEAT_COOL_FAN, "climate_study" },
    { "Studio", true, UI_HVAC_CLIMATE_PROFILE_HEAT_COOL, "climate_studio" },
};

static lv_obj_t *s_cards[HVAC_ZONE_COUNT];

static void hvac_apply_card(lv_obj_t *card, float setpoint_c, float current_c, int8_t hvac_mode)
{
    if (card == NULL) {
        return;
    }
    ui_hvac_card_1_set_current_temp(card, current_c);
    ui_hvac_card_1_set_setpoint(card, setpoint_c);
    ui_hvac_card_1_set_mode(card, hvac_mode);
}

static void hvac_modal_sync_from_card(unsigned card_idx, float setpoint_c, int8_t hvac_mode)
{
    if (card_idx >= HVAC_ZONE_COUNT || s_cards[card_idx] == NULL) {
        return;
    }
    ui_hvac_climate_modal_update_if_open(card_idx, setpoint_c, hvac_mode,
                                         ui_hvac_card_1_has_setpoint(s_cards[card_idx]));
}

static void hvac_climate_apply_ollie(float setpoint_c, float current_c, bool heater_on, bool climate_control_on,
                                     int8_t hvac_mode, void *user_data)
{
    (void)heater_on;
    (void)user_data;
    /* Ollie is heater-only; prefer heat/off from control when mode is unknown. */
    int8_t mode = hvac_mode;
    if (mode == HA_MQTT_CLIMATE_HVAC_UNKNOWN) {
        mode = climate_control_on ? HA_MQTT_CLIMATE_HVAC_HEAT : HA_MQTT_CLIMATE_HVAC_OFF;
    }
    hvac_apply_card(s_cards[4], setpoint_c, current_c, mode);
    hvac_modal_sync_from_card(4, setpoint_c, mode);
}

static void hvac_climate_apply_zone(float setpoint_c, float current_c, bool heater_on, bool climate_control_on,
                                    int8_t hvac_mode, void *user_data)
{
    (void)heater_on;
    const unsigned card_idx = (unsigned)(uintptr_t)user_data;
    if (card_idx >= HVAC_ZONE_COUNT) {
        return;
    }
    int8_t mode = hvac_mode;
    /* Heater zones may only publish bool-ish control; map like Ollie when unknown. */
    if (mode == HA_MQTT_CLIMATE_HVAC_UNKNOWN && s_zone_defs[card_idx].profile == UI_HVAC_CLIMATE_PROFILE_HEATER) {
        mode = climate_control_on ? HA_MQTT_CLIMATE_HVAC_HEAT : HA_MQTT_CLIMATE_HVAC_OFF;
    }
    hvac_apply_card(s_cards[card_idx], setpoint_c, current_c, mode);
    hvac_modal_sync_from_card(card_idx, setpoint_c, mode);
}

static void hvac_room_temp_apply(float temp_c, void *user_data)
{
    const unsigned card_idx = (unsigned)(uintptr_t)user_data;
    if (card_idx >= HVAC_ZONE_COUNT || s_cards[card_idx] == NULL) {
        return;
    }
    ui_hvac_card_1_set_current_temp(s_cards[card_idx], temp_c);
}

static void hvac_card_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    const unsigned card_idx = (unsigned)(uintptr_t)lv_event_get_user_data(e);
    if (card_idx >= HVAC_ZONE_COUNT || !s_zone_defs[card_idx].opens_modal) {
        return;
    }
    lv_obj_t *card = s_cards[card_idx];
    if (card == NULL) {
        return;
    }
    const hvac_zone_def_t *def = &s_zone_defs[card_idx];
    const bool have_set = ui_hvac_card_1_has_setpoint(card);
    const float setpoint = have_set ? ui_hvac_card_1_get_setpoint(card) : 21.0f;
    ui_hvac_climate_modal_open(def->room_name, def->profile, def->btn_prefix, card_idx, setpoint,
                               ui_hvac_card_1_get_mode(card), have_set);
}

static void hvac_screen_unloaded(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_SCREEN_UNLOADED) {
        return;
    }
    ui_hvac_climate_modal_close();
}

lv_obj_t *screen_hvac_create(lv_display_t *disp)
{
    (void)disp;
    for (unsigned i = 0; i < HVAC_ZONE_COUNT; i++) {
        s_cards[i] = NULL;
    }

    ha_mqtt_configure_hvac_zone(HA_MQTT_HVAC_ZONE_BEDROOM_1, "esp_hmi/data/bedroom1/climate/setpoint",
                                "esp_hmi/data/bedroom1/climate/current", "esp_hmi/data/bedroom1/climate/heater_on",
                                "esp_hmi/data/bedroom1/climate/control");
    ha_mqtt_configure_hvac_zone(HA_MQTT_HVAC_ZONE_UPSTAIRS_BEDROOM, "esp_hmi/data/upstairs_bedroom/climate/setpoint",
                                "esp_hmi/data/upstairs_bedroom/climate/current",
                                "esp_hmi/data/upstairs_bedroom/climate/heater_on",
                                "esp_hmi/data/upstairs_bedroom/climate/control");
    ha_mqtt_configure_hvac_zone(HA_MQTT_HVAC_ZONE_STUDIO, "esp_hmi/data/studio/climate/setpoint",
                                "esp_hmi/data/studio/climate/current", "esp_hmi/data/studio/climate/heater_on",
                                "esp_hmi/data/studio/climate/control");
    ha_mqtt_configure_hvac_zone(HA_MQTT_HVAC_ZONE_STUDY, "esp_hmi/data/study/climate/setpoint",
                                "esp_hmi/data/study/climate/current", "esp_hmi/data/study/climate/heater_on",
                                "esp_hmi/data/study/climate/control");
    ha_mqtt_configure_hvac_zone(HA_MQTT_HVAC_ZONE_MAIN_ROOM, "esp_hmi/data/main_room/climate/setpoint",
                                "esp_hmi/data/main_room/climate/current", "esp_hmi/data/main_room/climate/heater_on",
                                "esp_hmi/data/main_room/climate/control");
    ha_mqtt_configure_room_temp(HA_MQTT_ROOM_TEMP_PENNY, "esp_hmi/data/penny_room/temperature");

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, hvac_screen_unloaded, LV_EVENT_SCREEN_UNLOADED, NULL);

    const lv_coord_t grid_h = (lv_coord_t)(BSP_LCD_V_RES - UI_TASKBAR_HEIGHT);

    lv_obj_t *grid = lv_obj_create(scr);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, BSP_LCD_H_RES, grid_h);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_all(grid, HVAC_GRID_PAD, LV_PART_MAIN);
    lv_obj_set_style_pad_row(grid, HVAC_GRID_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_column(grid, HVAC_GRID_GAP, LV_PART_MAIN);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    static lv_coord_t col_dsc[HVAC_GRID_COLS + 1];
    static lv_coord_t row_dsc[HVAC_GRID_ROWS + 1];
    for (unsigned c = 0; c < HVAC_GRID_COLS; c++) {
        col_dsc[c] = LV_GRID_FR(1);
    }
    col_dsc[HVAC_GRID_COLS] = LV_GRID_TEMPLATE_LAST;
    for (unsigned r = 0; r < HVAC_GRID_ROWS; r++) {
        row_dsc[r] = LV_GRID_FR(1);
    }
    row_dsc[HVAC_GRID_ROWS] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

    for (unsigned i = 0; i < HVAC_ZONE_COUNT; i++) {
        const uint8_t row = (uint8_t)(i / HVAC_GRID_COLS);
        const uint8_t col = (uint8_t)(i % HVAC_GRID_COLS);
        s_cards[i] = ui_hvac_card_1_create(grid, row, col, s_zone_defs[i].room_name);
        if (s_cards[i] != NULL && s_zone_defs[i].opens_modal) {
            ui_hvac_card_1_set_click_cb(s_cards[i], hvac_card_clicked, (void *)(uintptr_t)i);
        }
    }
    /* Temp-only cards until climate entities exist. */
    if (s_cards[3] != NULL) {
        ui_hvac_card_1_set_setpoint_visible(s_cards[3], false);
    }

    ha_mqtt_add_ollie_climate_state_callback(hvac_climate_apply_ollie, NULL);
    ha_mqtt_add_hvac_zone_climate_callback(HA_MQTT_HVAC_ZONE_BEDROOM_1, hvac_climate_apply_zone, (void *)(uintptr_t)2);
    ha_mqtt_add_hvac_zone_climate_callback(HA_MQTT_HVAC_ZONE_UPSTAIRS_BEDROOM, hvac_climate_apply_zone,
                                           (void *)(uintptr_t)8);
    ha_mqtt_add_hvac_zone_climate_callback(HA_MQTT_HVAC_ZONE_STUDIO, hvac_climate_apply_zone, (void *)(uintptr_t)10);
    ha_mqtt_add_hvac_zone_climate_callback(HA_MQTT_HVAC_ZONE_STUDY, hvac_climate_apply_zone, (void *)(uintptr_t)9);
    ha_mqtt_add_hvac_zone_climate_callback(HA_MQTT_HVAC_ZONE_MAIN_ROOM, hvac_climate_apply_zone, (void *)(uintptr_t)0);
    ha_mqtt_add_room_temp_callback(HA_MQTT_ROOM_TEMP_PENNY, hvac_room_temp_apply, (void *)(uintptr_t)3);

    (void)ui_taskbar_attach_standard(scr);

    return scr;
}
